#include <ncurses.h>
#include <string.h>
#include <time.h>
#include "infiniband.h"
#include "ncurses_display.h"


typedef struct {
    int start;
    int width;
} col_t;

/* ================================================================== *
 * Color init                                                          *
 * ================================================================== */
void init_colors(void) {
    start_color();
    use_default_colors();   /* -1 = terminal default background */

    init_pair(CLR_TITLE,      COLOR_CYAN,    -1);
    init_pair(CLR_HEADER,     COLOR_YELLOW,  -1);
    init_pair(CLR_IFACE_NAME, COLOR_WHITE,   -1);
    init_pair(CLR_VALUE,      COLOR_CYAN,    -1);
    init_pair(CLR_ACTIVE,     COLOR_GREEN,   -1);
    init_pair(CLR_ERROR,      COLOR_RED,     -1);
    init_pair(CLR_ZERO,       COLOR_WHITE,   -1);
    init_pair(CLR_BADGE,      COLOR_BLACK,   COLOR_CYAN);
    init_pair(CLR_BADGE_GRAY, COLOR_BLACK,   COLOR_WHITE);
    init_pair(CLR_SEPARATOR,  COLOR_CYAN,    -1);
    init_pair(CLR_SUBTEXT,    COLOR_WHITE,   -1);
    init_pair(CLR_CARD_TITLE, COLOR_WHITE,   -1);
    init_pair(CLR_CARD_VALUE, COLOR_GREEN,   -1);
    init_pair(CLR_ACTIVE_DOT, COLOR_GREEN,   -1);
    init_pair(CLR_ERROR_DOT,  COLOR_RED,     -1);
    init_pair(CLR_RESET,      COLOR_YELLOW,  COLOR_RED);
}

/* Convenience macros */
#define CON(w, p)  wattron((w),  COLOR_PAIR(p))
#define COFF(w, p) wattroff((w), COLOR_PAIR(p))

/*
 * BVAL — subtract baseline when active, otherwise return raw value.
 * Clamps to 0 to avoid showing negatives if baseline was taken mid-flight.
 */
#define BVAL(flag, cur_val, base_val) \
    ((flag) ? (((cur_val) - (base_val)) < 0 ? 0 : ((cur_val) - (base_val))) \
            : (cur_val))

/* ================================================================== *
 * Layout constants                                                    *
 * ================================================================== */
#define ROW_STATUS(n)   (8)
#define ROW_IO(n)       (ROW_STATUS(n)  + (n) + 4)
#define ROW_LINK(n)     (ROW_IO(n)      + (n) + 4)
#define ROW_ROCE(n)     (ROW_LINK(n)    + (n) + 4)
#define ROW_CQE(n)      (ROW_ROCE(n)    + (n) + 4)
#define ROW_RXOPS(n)    (ROW_CQE(n)     + (n) + 4)
#define PAD_TOTAL_ROWS(n)  (ROW_RXOPS(n) + (n) + 6)
#define PAD_COLS  500

/*
 * Column layout arrays — each entry's .start and .width are derived
 * directly from the pipe '|' positions in the matching header string
 * printed by construct_window_layout().
 *
 * Formula: for pipe positions p[0], p[1], ..., p[k]:
 *   col[0]   = { start:1,        width: p[0]-1        }
 *   col[i]   = { start: p[i-1]+1, width: p[i]-p[i-1]-1 }  for i=1..k
 *   col[k+1] = { start: p[k]+1,  width: (end)-p[k]-1  }
 */

/* "Interface         | LID  | Link layer | State          | Physical state |  Rate"
 *  ^                 ^      ^            ^                ^                ^
 *  0                 18     25           37               53               69      */
static col_t status_cols[] = {
    {1,  17},   /* Interface */
    {19,  6},   /* LID */
    {26, 11},   /* Link layer */
    {38, 15},   /* State */
    {54, 15},   /* Physical state */
    {70,  6},   /* Rate */
};

/* "Interface         | RX packets | RX MB/s | TX packets | TX MB/s | UC RX pkts | UC TX pkts | MC RX pkts | MC TX pkts"
 *  ^                 ^            ^         ^            ^         ^             ^             ^             ^
 *  0                 18           30        39           50        59            71            83            95          */
static col_t io_cols[] = {
    {1,  17},   /* Interface */
    {19, 11},   /* RX packets */
    {31,  8},   /* RX MB/s */
    {40, 10},   /* TX packets */
    {51,  8},   /* TX MB/s */
    {60, 11},   /* UC RX pkts */
    {72, 11},   /* UC TX pkts */
    {84, 11},   /* MC RX pkts */
    {96, 11},   /* MC TX pkts */
};

/* "Interface         | Recovery | Integrity | Downed"
 *  ^                 ^          ^            ^
 *  0                 18         28           39      */
static col_t link_cols[] = {
    {1,  17},   /* Interface */
    {19,  9},   /* Recovery */
    {29, 10},   /* Integrity */
    {40,  6},   /* Downed */
};

/* "Interface         | Symbol | RX err | RX rem PHY | TX discard"
 *  ^                 ^        ^        ^             ^
 *  0                 18       26       34            46          */
static col_t port_cols[] = {
    {1,  17},   /* Interface  (offset within right half; caller adds half) */
    {19,  7},   /* Symbol */
    {27,  7},   /* RX err */
    {35, 11},   /* RX rem PHY */
    {47, 10},   /* TX discard */
};

/* "Interface         | NP CNP sent | NP ECN pkts | RP CNP handled | RP CNP ignored | Adp retrans | Adp retrans TO | Slow restart"
 *  ^                 ^             ^             ^                ^                ^             ^                ^
 *  0                 18            31            44               59               75            87               102           */
static col_t roce_cols[] = {
    {1,  17},   /* Interface */
    {19, 12},   /* NP CNP sent */
    {32, 12},   /* NP ECN pkts */
    {45, 14},   /* RP CNP handled */
    {60, 15},   /* RP CNP ignored */
    {76, 11},   /* Adp retrans */
    {88, 14},   /* Adp retrans TO */
    {103,12},   /* Slow restart */
};

/* "Interface         | Req CQE | Req flush | Resp CQE | Resp flush"
 *  ^                 ^         ^            ^          ^
 *  0                 18        27           37         47          */
static col_t cqe_cols[] = {
    {1,  17},   /* Interface */
    {19,  8},   /* Req CQE */
    {28,  9},   /* Req flush */
    {38,  9},   /* Resp CQE */
    {48,  9},   /* Resp flush */
};

/* "Interface         | Dup req | Impl NAK | Local ACK TO | Out of Buffer"
 *  ^                 ^         ^           ^              ^
 *  0                 18        27           37             50             */
static col_t seq_cols[] = {
    {1,  17},   /* Interface  (offset within right half; caller adds half) */
    {19,  8},   /* Dup req */
    {28,  9},   /* Impl NAK */
    {38, 12},   /* Local ACK TO */
    {51, 13},   /* Out of Buffer */
};

/* "Interface         | Atomic req | DCT connect | ICRC encap | Read req | Write req | Lifespan"
 *  ^                 ^            ^             ^            ^          ^            ^
 *  0                 18           30            43           55         65           76        */
static col_t rx_cols[] = {
    {1,  17},   /* Interface */
    {19, 11},   /* Atomic req */
    {31, 12},   /* DCT connect */
    {44, 11},   /* ICRC encap */
    {56,  9},   /* Read req */
    {66, 20},   /* Write req was 10 */
    {88,  8},   /* Lifespan */
};

#define NCOLS(a) (int)(sizeof(a)/sizeof((a)[0]))

/* ================================================================== *
 * Internal helpers                                                    *
 * ================================================================== */

static void draw_box(WINDOW *w, int y, int x, int h, int width) {
    CON(w, CLR_SEPARATOR);
    mvwaddch(w, y,       x,         ACS_ULCORNER);
    mvwaddch(w, y,       x+width-1, ACS_URCORNER);
    mvwaddch(w, y+h-1,   x,         ACS_LLCORNER);
    mvwaddch(w, y+h-1,   x+width-1, ACS_LRCORNER);
    mvwhline(w, y,       x+1,       ACS_HLINE, width-2);
    mvwhline(w, y+h-1,   x+1,       ACS_HLINE, width-2);
    for (int i = 1; i < h-1; ++i) {
        mvwaddch(w, y+i, x,         ACS_VLINE);
        mvwaddch(w, y+i, x+width-1, ACS_VLINE);
    }
    COFF(w, CLR_SEPARATOR);
}

static void print_badge(WINDOW *w, int row, int col, const char *label) {
    CON(w, CLR_BADGE);
    mvwprintw(w, row, col, " %s ", label);
    COFF(w, CLR_BADGE);
}

static void print_badge_gray(WINDOW *w, int row, int col, const char *label) {
    CON(w, CLR_BADGE_GRAY);
    mvwprintw(w, row, col, " %s ", label);
    COFF(w, CLR_BADGE_GRAY);
}

/* ================================================================== *
 * print_header                                                        *
 * ================================================================== */
void print_header(WINDOW *w, int interface_count, int active_count, int error_count,
                  int baseline_flag) {
    /* title */
    CON(w, CLR_TITLE);
    wattron(w, A_BOLD);
    mvwprintw(w, 0, 1, "InfiniBand interface monitor");
    wattroff(w, A_BOLD);
    COFF(w, CLR_TITLE);

    /* ● N active  ● N errors badges */
    char active_str[32], error_str[32];
    snprintf(active_str, sizeof(active_str), " %d active ", active_count);
    snprintf(error_str,  sizeof(error_str),  " %d errors ", error_count);
    int ax = COLS - (int)strlen(active_str) - (int)strlen(error_str) - 8;

    mvwprintw(w, 0, ax, "[ ");
    CON(w, CLR_ACTIVE_DOT); wattron(w, A_BOLD);
    mvwprintw(w, 0, ax+2, "●");
    wattroff(w, A_BOLD); COFF(w, CLR_ACTIVE_DOT);
    CON(w, CLR_BADGE); mvwprintw(w, 0, ax+4, "%s", active_str); COFF(w, CLR_BADGE);
    mvwprintw(w, 0, ax+4+(int)strlen(active_str), " ]  [ ");
    CON(w, error_count > 0 ? CLR_ERROR_DOT : CLR_ACTIVE_DOT); wattron(w, A_BOLD);
    mvwprintw(w, 0, ax+4+(int)strlen(active_str)+6, "●");
    wattroff(w, A_BOLD); COFF(w, error_count > 0 ? CLR_ERROR_DOT : CLR_ACTIVE_DOT);
    CON(w, error_count > 0 ? CLR_ERROR : CLR_BADGE);
    mvwprintw(w, 0, ax+4+(int)strlen(active_str)+8, "%s", error_str);
    COFF(w, error_count > 0 ? CLR_ERROR : CLR_BADGE);
    mvwprintw(w, 0, ax+4+(int)strlen(active_str)+8+(int)strlen(error_str), " ]");

    /* sysfs path + timestamp */
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char timebuf[32];
    strftime(timebuf, sizeof(timebuf), "%H:%M:%S", t);
    CON(w, CLR_SUBTEXT); wattron(w, A_DIM);
    mvwprintw(w, 1, 1,
        "/sys/class/infiniband  —  %d interface%s  —  refreshed %s",
        interface_count,
        interface_count == 1 ? "" : "s",
        timebuf);
    wattroff(w, A_DIM); COFF(w, CLR_SUBTEXT);

    /* horizontal rule */
    CON(w, CLR_SEPARATOR); wattron(w, A_DIM);
    mvwhline(w, 2, 1, ACS_HLINE, COLS - 2);
    wattroff(w, A_DIM); COFF(w, CLR_SEPARATOR);
}

/* ================================================================== *
 * print_summary_cards                                                 *
 * ================================================================== */
void print_summary_cards(WINDOW *w,
                         const struct ib_metrics *metrics,
                         const struct ib_metrics *baseline,
                         int interface_count,
                         int active_count,
                         int error_count,
                         int baseline_flag) {
    long int total_rx = 0, total_tx = 0;
    for (int i = 0; i < interface_count; ++i) {
        long int rx = BVAL(baseline_flag,
                           metrics->ib_interfaces[i].port_rcv_data,
                           baseline->ib_interfaces[i].port_rcv_data);
        long int tx = BVAL(baseline_flag,
                           metrics->ib_interfaces[i].port_xmit_data,
                           baseline->ib_interfaces[i].port_xmit_data);
        total_rx += rx;
        total_tx += tx;
    }
    double rx_gb = (double)total_rx * 4.0 / (1024.0 * 1024.0 * 1024.0);
    double tx_gb = (double)total_tx * 4.0 / (1024.0 * 1024.0 * 1024.0);

    int card_w = (COLS - 2) / 4;
    int y = 3;

    /* card 1 — Interfaces up */
    draw_box(w, y, 1, 5, card_w - 1);
    CON(w, CLR_CARD_TITLE);
    mvwprintw(w, y+1, 3, "Interfaces up");
    COFF(w, CLR_CARD_TITLE);
    CON(w, CLR_CARD_VALUE); wattron(w, A_BOLD);
    mvwprintw(w, y+2, 3, "%d / %d", active_count, interface_count);
    wattroff(w, A_BOLD); COFF(w, CLR_CARD_VALUE);
    CON(w, CLR_SUBTEXT); wattron(w, A_DIM);
    mvwprintw(w, y+3, 3, "RoCEv2");
    wattroff(w, A_DIM); COFF(w, CLR_SUBTEXT);

    /* card 2 — Total RX */
    draw_box(w, y, card_w, 5, card_w - 1);
    CON(w, CLR_CARD_TITLE);
    mvwprintw(w, y+1, card_w+2, baseline_flag ? "Total RX " : "Total RX");
    COFF(w, CLR_CARD_TITLE);
    CON(w, CLR_CARD_VALUE); wattron(w, A_BOLD);
    mvwprintw(w, y+2, card_w+2, "%.1f GB", rx_gb);
    wattroff(w, A_BOLD); COFF(w, CLR_CARD_VALUE);

    /* card 3 — Total TX */
    draw_box(w, y, card_w*2, 5, card_w - 1);
    CON(w, CLR_CARD_TITLE);
    mvwprintw(w, y+1, card_w*2+2, baseline_flag ? "Total TX" : "Total TX");
    COFF(w, CLR_CARD_TITLE);
    CON(w, CLR_CARD_VALUE); wattron(w, A_BOLD);
    mvwprintw(w, y+2, card_w*2+2, "%.1f GB", tx_gb);
    wattroff(w, A_BOLD); COFF(w, CLR_CARD_VALUE);

    /* card 4 — Error events */
    draw_box(w, y, card_w*3, 5, card_w - 1);
    CON(w, CLR_CARD_TITLE);
    mvwprintw(w, y+1, card_w*3+2, "Error events");
    COFF(w, CLR_CARD_TITLE);
    if (error_count > 0) { CON(w, CLR_ERROR); wattron(w, A_BOLD); }
    else                 { CON(w, CLR_CARD_VALUE); wattron(w, A_BOLD); }
    mvwprintw(w, y+2, card_w*3+2, "%d", error_count);
    wattroff(w, A_BOLD);
    if (error_count > 0) COFF(w, CLR_ERROR);
    else                 COFF(w, CLR_CARD_VALUE);
}

/* ================================================================== *
 * construct_window_layout                                             *
 * ================================================================== */
void construct_window_layout(WINDOW *w, int n) {
    int half = COLS / 2;

#define BANNER(row, col, txt) \
    CON(w, CLR_TITLE); wattron(w, A_BOLD); \
    mvwprintw(w, row, col, txt); \
    wattroff(w, A_BOLD); COFF(w, CLR_TITLE)

#define HDR(row, col, txt) \
    CON(w, CLR_HEADER); wattron(w, A_BOLD); \
    mvwprintw(w, row, col, txt); \
    wattroff(w, A_BOLD); COFF(w, CLR_HEADER)

#define HLINE(row) \
    CON(w, CLR_SEPARATOR); wattron(w, A_DIM); \
    mvwhline(w, row, 1, ACS_HLINE, COLS-2); \
    wattroff(w, A_DIM); COFF(w, CLR_SEPARATOR)

    /* ---- Interface status ---------------------------------------- */
    BANNER(ROW_STATUS(n), 1, "Interface status");
    print_badge(w, ROW_STATUS(n), 20, "port info");
    HDR(ROW_STATUS(n)+1, 1,
        "Interface         | LID  | Link layer | State          | Physical state |  Rate");
    HLINE(ROW_STATUS(n)+2);

    /* ---- I/O throughput ------------------------------------------ */
    BANNER(ROW_IO(n), 1, "I/O throughput");
    print_badge(w, ROW_IO(n), 17, "per second");
    print_badge_gray(w, ROW_IO(n), COLS-12, "/counters");
    HDR(ROW_IO(n)+1, 1,
        "Interface         | RX packets | RX MB/s | TX packets | TX MB/s | UC RX pkts | UC TX pkts | MC RX pkts | MC TX pkts");
    HLINE(ROW_IO(n)+2);

    /* ---- Link errors + Port errors ------------------------------- */
    BANNER(ROW_LINK(n), 1,      "Link errors");
    print_badge(w, ROW_LINK(n), 14, "cumulative");
    print_badge_gray(w, ROW_LINK(n), half/2 - 2, "/counters");
    BANNER(ROW_LINK(n), half+1, "Port errors");
    print_badge(w, ROW_LINK(n), half+14, "cumulative");
    print_badge_gray(w, ROW_LINK(n), COLS-12, "/counters");

    HDR(ROW_LINK(n)+1, 1,
        "Interface         | Recovery | Integrity | Downed");
    HDR(ROW_LINK(n)+1, half+1,
        "Interface         | Symbol | RX err | RX rem PHY | TX discard");

    CON(w, CLR_SEPARATOR); wattron(w, A_DIM);
    mvwhline(w, ROW_LINK(n)+2, 1,      ACS_HLINE, half-2);
    mvwaddch(w, ROW_LINK(n)+2, half,   ACS_PLUS);
    mvwhline(w, ROW_LINK(n)+2, half+1, ACS_HLINE, COLS-half-2);
    for (int i = 0; i < n+3; ++i)
        mvwaddch(w, ROW_LINK(n)+i, half, ACS_VLINE);
    wattroff(w, A_DIM); COFF(w, CLR_SEPARATOR);

    /* ---- RoCE congestion ----------------------------------------- */
    BANNER(ROW_ROCE(n), 1, "RoCE congestion & retransmit");
    print_badge(w, ROW_ROCE(n), 31, "cumulative");
    print_badge_gray(w, ROW_ROCE(n), COLS-14, "/hw_counters");
    HDR(ROW_ROCE(n)+1, 1,
        "Interface         | NP CNP sent | NP ECN pkts | RP CNP handled | RP CNP ignored | Adp retrans | Adp retrans TO | Slow restart");
    HLINE(ROW_ROCE(n)+2);

    /* ---- CQE errors + Sequence & timeout ------------------------- */
    BANNER(ROW_CQE(n), 1,      "CQE errors");
    print_badge(w, ROW_CQE(n), 13, "cumulative");
    print_badge_gray(w, ROW_CQE(n), half/2 - 2, "/hw_counters");
    BANNER(ROW_CQE(n), half+1, "Sequence & timeout errors");
    print_badge(w, ROW_CQE(n), half+28, "cumulative");
    print_badge_gray(w, ROW_CQE(n), COLS-14, "/hw_counters");

    HDR(ROW_CQE(n)+1, 1,
        "Interface         | Req CQE | Req flush | Resp CQE | Resp flush");
    HDR(ROW_CQE(n)+1, half+1,
        "Interface         | Dup req | Impl NAK | Local ACK TO | Out of Buffer");

    CON(w, CLR_SEPARATOR); wattron(w, A_DIM);
    mvwhline(w, ROW_CQE(n)+2, 1,      ACS_HLINE, half-2);
    mvwaddch(w, ROW_CQE(n)+2, half,   ACS_PLUS);
    mvwhline(w, ROW_CQE(n)+2, half+1, ACS_HLINE, COLS-half-2);
    for (int i = 0; i < n+3; ++i)
        mvwaddch(w, ROW_CQE(n)+i, half, ACS_VLINE);
    wattroff(w, A_DIM); COFF(w, CLR_SEPARATOR);

    /* ---- RX operations ------------------------------------------- */
    BANNER(ROW_RXOPS(n), 1, "RX operations");
    print_badge(w, ROW_RXOPS(n), 16, "cumulative");
    print_badge_gray(w, ROW_RXOPS(n), COLS-14, "/hw_counters");
    HDR(ROW_RXOPS(n)+1, 1,
        "Interface         | Atomic req | DCT connect | ICRC encap | Read req |             Write request             | Lifespan");
    HLINE(ROW_RXOPS(n)+2);

    /* ---- section separators -------------------------------------- */
    HLINE(ROW_IO(n)   -1);
    HLINE(ROW_LINK(n) -1);
    HLINE(ROW_ROCE(n) -1);
    HLINE(ROW_CQE(n)  -1);
    HLINE(ROW_RXOPS(n)-1);

    /* ---- footer -------------------------------------------------- */
    CON(w, CLR_SUBTEXT); wattron(w, A_DIM);
    mvwprintw(w, PAD_TOTAL_ROWS(n)-1, 2,
        "↑↓/jk scroll   ←→/hl scroll   Space/PgDn   PgUp   Home   End   q quit");
    wattroff(w, A_DIM); COFF(w, CLR_SUBTEXT);

#undef BANNER
#undef HDR
#undef HLINE
}

/* ================================================================== *
 * Data row printers                                                   *
 * ================================================================== */

void print_interface_status(WINDOW *w, int row, const struct interfaces *iface)
{
    
    CON(w, CLR_IFACE_NAME); wattron(w, A_BOLD);
    mvwprintw(w, row, status_cols[0].start, "%s", iface->name_of_interface);//-17
    wattroff(w, A_BOLD); COFF(w, CLR_IFACE_NAME);
    

    mvwprintw(w, row, status_cols[1].start+3, "%-5ld", iface->lid);

    /* 3. Link layer - Left aligned */
    mvwprintw(w, row, status_cols[2].start+3, "%-13s", iface->link_layer);

    /* 4. State - Left aligned */
    mvwprintw(w, row, status_cols[3].start+5, "%-17s", iface->state);

    /* 5. Physical state - Left aligned */
    mvwprintw(w, row, status_cols[4].start+5, "%-19s", iface->phys_state);

    /* 6. Rate - Left aligned (best for long strings like "200 Gb/sec (4X HDR)") */
    mvwprintw(w, row, status_cols[5].start+7, "%-30s", iface->rate);
}

/* ------------------------------------------------------------------ */

void print_io_throughput(WINDOW *w, int row,
                         const struct interfaces *cur,
                         const struct interfaces *prev,
                         long int refresh_second)
{
    if (refresh_second <= 0) refresh_second = 1;

    long int rx_pkt = (cur->port_rcv_packets    - prev->port_rcv_packets)    / refresh_second;
    long int rx_mb  = (cur->port_rcv_data        - prev->port_rcv_data)   * 4 / 1024 / 1024 / refresh_second;
    long int tx_pkt = (cur->port_xmit_packets   - prev->port_xmit_packets)   / refresh_second;
    long int tx_mb  = (cur->port_xmit_data       - prev->port_xmit_data)  * 4 / 1024 / 1024 / refresh_second;
    long int uc_rx  = (cur->unicast_rcv_packets              - prev->unicast_rcv_packets)              / refresh_second;
    long int uc_tx  = (cur->port_unicast_xmit_packets        - prev->port_unicast_xmit_packets)        / refresh_second;
    long int mc_rx  = (cur->port_multicast_rcv_packets       - prev->port_multicast_rcv_packets)       / refresh_second;
    long int mc_tx  = (cur->port_multicast_xmit_packets      - prev->port_multicast_xmit_packets)      / refresh_second;

    CON(w, CLR_IFACE_NAME); wattron(w, A_BOLD);
    mvwprintw(w, row, io_cols[0].start, "%-17s", cur->name_of_interface);
    wattroff(w, A_BOLD); COFF(w, CLR_IFACE_NAME);

    
    mvwprintw(w, row, io_cols[1].start+5, "%-8ld", rx_pkt);

    mvwprintw(w, row, io_cols[2].start+5, "%-8ld", rx_mb);

    mvwprintw(w, row, io_cols[3].start+8, "%-8ld", tx_pkt);

    mvwprintw(w, row, io_cols[4].start+9, "%-8ld", tx_mb);

    mvwprintw(w, row, io_cols[5].start+12, "%-8ld", uc_rx);

    mvwprintw(w, row, io_cols[6].start+11, "%-8ld", uc_tx);

    mvwprintw(w, row, io_cols[7].start+15,"%-8ld", mc_rx);

    mvwprintw(w, row, io_cols[8].start+15, "%-8ld", mc_tx);
}

/* ------------------------------------------------------------------ */
void print_link_errors(WINDOW *w, int row,
                       const struct interfaces *iface,
                       const struct interfaces *base,
                       int baseline_flag)
{
    CON(w, CLR_IFACE_NAME); wattron(w, A_BOLD);
    mvwprintw(w, row, link_cols[0].start, "%-17s", iface->name_of_interface);
    wattroff(w, A_BOLD); COFF(w, CLR_IFACE_NAME);

    long v1 = BVAL(baseline_flag, iface->link_error_recovery,       base->link_error_recovery);
    long v2 = BVAL(baseline_flag, iface->local_link_intgrity_errors, base->local_link_intgrity_errors);
    long v3 = BVAL(baseline_flag, iface->link_downed,               base->link_downed);

    mvwprintw(w, row, link_cols[1].start+5, "%-6ld", v1);

    mvwprintw(w, row, link_cols[2].start+6, "%-6ld", v2);

    mvwprintw(w, row, link_cols[3].start+6, "%-6ld", v3);
}
   

/* ------------------------------------------------------------------ */
void print_port_errors(WINDOW *w, int row,
                       const struct interfaces *iface,
                       const struct interfaces *base,
                       int baseline_flag)
{
    int b = COLS / 2 + 1;   /* right-half base column */

    CON(w, CLR_IFACE_NAME); wattron(w, A_BOLD);
    mvwprintw(w, row, b + port_cols[0].start, "%-17s", iface->name_of_interface);
    wattroff(w, A_BOLD); COFF(w, CLR_IFACE_NAME);

    long v1 = BVAL(baseline_flag, iface->symbol_error,                    base->symbol_error);
    long v2 = BVAL(baseline_flag, iface->port_rcv_errors,                 base->port_rcv_errors);
    long v3 = BVAL(baseline_flag, iface->port_rcv_remote_physical_errors, base->port_rcv_remote_physical_errors);
    long v4 = BVAL(baseline_flag, iface->port_xmit_discards,              base->port_xmit_discards);

    mvwprintw(w, row, port_cols[1].start+b+4, "%-6ld", v1);

    mvwprintw(w, row, port_cols[2].start+b+4, "%-6ld", v2);
    
    mvwprintw(w, row, port_cols[3].start+b+7, "%-6ld", v3);
   
    mvwprintw(w, row,port_cols[4].start+b+8, "%-6ld", v4);

}

/* ------------------------------------------------------------------ */
void print_roce_congestion(WINDOW *w, int row,
                           const struct interfaces *iface,
                           const struct interfaces *base,
                           int baseline_flag)
{
    CON(w, CLR_IFACE_NAME); wattron(w, A_BOLD);
    mvwprintw(w, row, roce_cols[0].start, "%-17s", iface->name_of_interface);
    wattroff(w, A_BOLD); COFF(w, CLR_IFACE_NAME);

    long v1 = BVAL(baseline_flag, iface->np_cnp_sent,                base->np_cnp_sent);
    long v2 = BVAL(baseline_flag, iface->np_ecn_marked_roce_packets, base->np_ecn_marked_roce_packets);
    long v3 = BVAL(baseline_flag, iface->rp_cnp_handled,             base->rp_cnp_handled);
    long v4 = BVAL(baseline_flag, iface->rp_cnp_ignored,             base->rp_cnp_ignored);
    long v5 = BVAL(baseline_flag, iface->roce_adp_retrans,           base->roce_adp_retrans);
    long v6 = BVAL(baseline_flag, iface->roce_adp_rtrans_to,         base->roce_adp_rtrans_to);
    long v7 = BVAL(baseline_flag, iface->roce_slow_restart,          base->roce_slow_restart);

    
    mvwprintw(w, row, roce_cols[1].start+6, "%-6ld", v1);
    
    mvwprintw(w, row, roce_cols[2].start+6, "%-6ld", v2);
    
    mvwprintw(w, row, roce_cols[3].start+9, "%-6ld", v3);
    
    mvwprintw(w, row, roce_cols[4].start+10, "%-6ld", v4);
    
    mvwprintw(w, row, roce_cols[5].start+10, "%-6ld", v5);
    
    mvwprintw(w, row, roce_cols[6].start+15, "%-6ld", v6);
    
    mvwprintw(w, row, roce_cols[7].start+15, "%-6ld", v7);

}

/* ------------------------------------------------------------------ */
void print_cqe_errors(WINDOW *w, int row,
                      const struct interfaces *iface,
                      const struct interfaces *base,
                      int baseline_flag)
{
    CON(w, CLR_IFACE_NAME); wattron(w, A_BOLD);
    mvwprintw(w, row, cqe_cols[0].start, "%-17s", iface->name_of_interface);
    wattroff(w, A_BOLD); COFF(w, CLR_IFACE_NAME);

    long v1 = BVAL(baseline_flag, iface->req_cqe_error,        base->req_cqe_error);
    long v2 = BVAL(baseline_flag, iface->req_cqe_flush_error,  base->req_cqe_flush_error);
    long v3 = BVAL(baseline_flag, iface->resp_cqe_error,       base->resp_cqe_error);
    long v4 = BVAL(baseline_flag, iface->resp_cqe_flush_error, base->resp_cqe_flush_error);

    mvwprintw(w, row, cqe_cols[1].start+4, "%-6ld", v1);
    
    mvwprintw(w, row, cqe_cols[2].start+6, "%-6ld", v2);
    
    mvwprintw(w, row, cqe_cols[3].start+8, "%-6ld", v3);
    
    mvwprintw(w, row, cqe_cols[4].start+8, "%-6ld", v4);}

/* ------------------------------------------------------------------ */
void print_seq_timeout_errors(WINDOW *w, int row,
                              const struct interfaces *iface,
                              const struct interfaces *base,
                              int baseline_flag)
{
    int b = COLS / 2 + 1;   /* right-half base column */

    CON(w, CLR_IFACE_NAME); wattron(w, A_BOLD);
    mvwprintw(w, row, b + seq_cols[0].start, "%-17s", iface->name_of_interface);
    wattroff(w, A_BOLD); COFF(w, CLR_IFACE_NAME);

    long v1 = BVAL(baseline_flag, iface->duplicate_request,     base->duplicate_request);
    long v2 = BVAL(baseline_flag, iface->implied_nak_seq_err,   base->implied_nak_seq_err);
    long v3 = BVAL(baseline_flag, iface->local_ack_timeout_err, base->local_ack_timeout_err);
    long v4 = BVAL(baseline_flag, iface->out_of_buffer,         base->out_of_buffer);

    mvwprintw(w, row, seq_cols[1].start+b+4, "%-6ld", v1);
    
    mvwprintw(w, row, seq_cols[2].start+b+4, "%-6ld", v2);
    
    mvwprintw(w, row, seq_cols[3].start+b+7, "%-6ld", v3);
    
    mvwprintw(w, row, seq_cols[4].start+b+8, "%-6ld", v4);  
}

/* ------------------------------------------------------------------ */
void print_rx_operations(WINDOW *w, int row,
                         const struct interfaces *iface,
                         const struct interfaces *base,
                         int baseline_flag)
{
    CON(w, CLR_IFACE_NAME); wattron(w, A_BOLD);
    mvwprintw(w, row, rx_cols[0].start, "%-17s", iface->name_of_interface);
    wattroff(w, A_BOLD); COFF(w, CLR_IFACE_NAME);

    long v1 = BVAL(baseline_flag, iface->rx_atomic_requests,  base->rx_atomic_requests);
    long v2 = BVAL(baseline_flag, iface->rx_dct_connect,      base->rx_dct_connect);
    long v3 = BVAL(baseline_flag, iface->rx_icrc_encapsulatd, base->rx_icrc_encapsulatd);
    long v4 = BVAL(baseline_flag, iface->rx_read_requests,    base->rx_read_requests);
    long v5 = BVAL(baseline_flag, iface->rx_write_requests,   base->rx_write_requests);
    long v6 = BVAL(baseline_flag, iface->lifespan,            base->lifespan);


    mvwprintw(w, row, rx_cols[1].start+6, "%-8ld", v1);

    mvwprintw(w, row, rx_cols[2].start+6, "%-8ld", v2);

    mvwprintw(w, row, rx_cols[3].start+8, "%-8ld", v3);

    mvwprintw(w, row, rx_cols[4].start+8, "%-8ld", v4);

    mvwprintw(w, row, rx_cols[5].start+15, "%-8ld", v5);

    mvwprintw(w, row, rx_cols[6].start+28, "%-8ld", v6);
}

/* ================================================================== *
 * draw_screen                                                         *
 * ================================================================== */
void draw_screen(WINDOW *pad,
                 const struct ib_metrics *metrics,
                 const struct ib_metrics *prev_metrics,
                 const struct ib_metrics *baseline,
                 int active_count,
                 int interface_count,
                 int prev_count,
                 int prev_flag,
                 int baseline_flag,
                 long int refresh_second,
                 int error_count) {
    werase(pad);

    print_header(pad, interface_count, active_count, error_count, baseline_flag);
    print_summary_cards(pad, metrics, baseline, interface_count, active_count, error_count, baseline_flag);
    construct_window_layout(pad, interface_count);

    int n = interface_count;
    for (int i = 0; i < n; ++i) {
        const struct interfaces *iface = &metrics->ib_interfaces[i];
        const struct interfaces *base  = baseline_flag
                                         ? &baseline->ib_interfaces[i]
                                         : iface;
        int dr = i + 1;
        print_interface_status   (pad, ROW_STATUS(n) + 2 + dr, iface);
        print_link_errors        (pad, ROW_LINK(n)   + 2 + dr, iface, base, 0);
        print_port_errors        (pad, ROW_LINK(n)   + 2 + dr, iface, base, 0);
        print_roce_congestion    (pad, ROW_ROCE(n)   + 2 + dr, iface, base, 0);
        print_cqe_errors         (pad, ROW_CQE(n)    + 2 + dr, iface, base, 0);
        print_seq_timeout_errors (pad, ROW_CQE(n)    + 2 + dr, iface, base, 0);
        print_rx_operations      (pad, ROW_RXOPS(n)  + 2 + dr, iface, base, 0);
    }

    if (prev_flag > 0) {
        for (int i = 0; i < n; ++i) {
            int dr = i + 1;
            for (int j = 0; j < prev_count; ++j) {
                if (strcmp(metrics->ib_interfaces[i].name_of_interface,
                           prev_metrics->ib_interfaces[j].name_of_interface) == 0) {
                    print_io_throughput(pad, ROW_IO(n) + 2 + dr,
                                        &metrics->ib_interfaces[i],
                                        &prev_metrics->ib_interfaces[j],
                                        refresh_second);
                    break;
                }
            }
        }
    }
}