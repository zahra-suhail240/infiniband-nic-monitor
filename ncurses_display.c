#include <ncurses.h>
#include <string.h>
#include <time.h>
#include "infiniband.h"
#include "ncurses_display.h"


typedef struct {
    int start;
    int width;
} col_t;

static void print_centered(WINDOW *w, int row, int col_start, int width,
                           const char *fmt, long val, int color_pair, int is_bold)
{
    char buf[64];
    snprintf(buf, sizeof(buf), fmt, val);

    int len = (int)strlen(buf);
    int col = col_start + (width - len) / 2;

    wattron(w, COLOR_PAIR(color_pair));
    if (is_bold) wattron(w, A_BOLD);

    mvwprintw(w, row, col, "%s", buf);

    if (is_bold) wattroff(w, A_BOLD);
    wattroff(w, COLOR_PAIR(color_pair));
}

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

/* Print a long value: white if zero, cyan+bold otherwise */
#define PRINT_VAL(w, row, col, fmt, val) \
    do { \
        if ((val) == 0) { CON(w, CLR_ZERO); } \
        else            { CON(w, CLR_VALUE); wattron(w, A_BOLD); } \
        mvwprintw((w), (row), (col), (fmt), (val)); \
        if ((val) == 0) { COFF(w, CLR_ZERO); } \
        else            { wattroff(w, A_BOLD); COFF(w, CLR_VALUE); } \
    } while (0)

/* Print an error value: red+bold if non-zero, dim white if zero */
#define PRINT_ERR(w, row, col, fmt, val) \
    do { \
        if ((val) == 0) { CON(w, CLR_ZERO); } \
        else            { CON(w, CLR_ERROR); wattron(w, A_BOLD); } \
        mvwprintw((w), (row), (col), (fmt), (val)); \
        if ((val) == 0) { COFF(w, CLR_ZERO); } \
        else            { wattroff(w, A_BOLD); COFF(w, CLR_ERROR); } \
    } while (0)

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


/* Interface status */
static col_t status_cols[] = {
    {1,17}, {18,10}, {28,18}, {46,18}, {64,18}, {82,20}
};

/* IO throughput */
static col_t io_cols[] = {
    {1,17}, {18,13}, {31,11}, {42,13}, {55,11},
    {66,17}, {83,17}, {100,17}, {117,17}
};

/* Link errors */
static col_t link_cols[] = {
    {1,17}, {18,16}, {34,16}, {50,12}
};

/* Port errors (right side) */
static col_t port_cols[] = {
    {0,17}, {18,10}, {29,10}, {39,15}, {54,15}
};

/* RX ops */
static col_t rx_cols[] = {
    {1,17}, {18,14}, {33,14}, {48,14}, {62,12}, {75,12}, {88,12}
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
void print_header(WINDOW *w, int interface_count,int active_count, int error_count,
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

    /* RESET ACTIVE badge — shown next to title when baseline is armed */
    if (baseline_flag) {
       // CON(w, CLR_RESET); wattron(w, A_BOLD);
        //mvwprintw(w, 0, 31, " ⟳ COUNTERS RESET  press c to clear ");
        //wattroff(w, A_BOLD); COFF(w, CLR_RESET);
    }

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
    CON(w, CLR_SUBTEXT); wattron(w, A_DIM);
   // mvwprintw(w, y+3, card_w+2, baseline_flag ? "since r pressed" : "since last reset");
    wattroff(w, A_DIM); COFF(w, CLR_SUBTEXT);

    /* card 3 — Total TX */
    draw_box(w, y, card_w*2, 5, card_w - 1);
    CON(w, CLR_CARD_TITLE);
    mvwprintw(w, y+1, card_w*2+2, baseline_flag ? "Total TX" : "Total TX");
    COFF(w, CLR_CARD_TITLE);
    CON(w, CLR_CARD_VALUE); wattron(w, A_BOLD);
    mvwprintw(w, y+2, card_w*2+2, "%.1f GB", tx_gb);
    wattroff(w, A_BOLD); COFF(w, CLR_CARD_VALUE);
    CON(w, CLR_SUBTEXT); wattron(w, A_DIM);
    //mvwprintw(w, y+3, card_w*2+2, baseline_flag ? "since r pressed" : "since last reset");
    wattroff(w, A_DIM); COFF(w, CLR_SUBTEXT);

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
    CON(w, CLR_SUBTEXT); wattron(w, A_DIM);
   // mvwprintw(w, y+3, card_w*3+2, baseline_flag ? "since r pressed" : "cumulative");
    wattroff(w, A_DIM); COFF(w, CLR_SUBTEXT);
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
        "Interface         | Atomic req | DCT connect | ICRC encap | Read req | Write req | Lifespan");
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
 * print_delimiter                                                     *
 * ================================================================== */
void print_delimiter(WINDOW *w, int row, int *cols, size_t n) {
    CON(w, CLR_SEPARATOR);
    wattron(w, A_BOLD);
    for (size_t i = 0; i < n; ++i)
        mvwprintw(w, row, cols[i], "|");
    wattroff(w, A_BOLD);
    COFF(w, CLR_SEPARATOR);
}

/* ================================================================== *
 * Data row printers                                                   *
 * ================================================================== */

void print_interface_status(WINDOW *w, int row, const struct interfaces *iface)
{
    CON(w, CLR_IFACE_NAME); wattron(w, A_BOLD);
    mvwprintw(w, row, status_cols[0].start, "%-17s", iface->name_of_interface);
    wattroff(w, A_BOLD); COFF(w, CLR_IFACE_NAME);

    print_centered(w, row, status_cols[1].start, status_cols[1].width,"%ld", iface->lid, CLR_VALUE, 1);
    //mvwprintw(w, row, status_cols[1].start + 1, "%-16ld", iface->lid);

    //mvwprintw(w, row, status_cols[2].start + 1, "%-16s", iface->link_layer);
    print_centered(w, row, status_cols[2].start, status_cols[2].width,"%-16s", iface->link_layer, CLR_VALUE, 1);

    //mvwprintw(w, row, status_cols[3].start + 1, "%-16s", iface->state);
    print_centered(w, row, status_cols[3].start, status_cols[3].width,"%-16s", iface->state, CLR_VALUE, 1);
    
    //mvwprintw(w, row, status_cols[4].start + 1, "%-16s", iface->phys_state);
    print_centered(w, row, status_cols[4].start, status_cols[4].width,"%-16s", iface->phys_state, CLR_VALUE, 1);
   
    mvwprintw(w, row, status_cols[5].start + 1, "%-18s", iface->rate);
    //print_centered(w, row, status_cols[5].start, status_cols[5].width,"%-18s", iface->rate, CLR_VALUE, 1);
}

/* ------------------------------------------------------------------ */

void print_io_throughput(WINDOW *w, int row,
                         const struct interfaces *cur,
                         const struct interfaces *prev,
                         long int refresh_second)
{
    if (refresh_second <= 0) refresh_second = 1;

    long int rx_pkt = (cur->port_rcv_packets - prev->port_rcv_packets) / refresh_second; //long int rx_pkt = (cur->port_rcv_packets - prev->port_rcv_packets) / refresh_second;
    long int rx_mb  = (cur->port_rcv_data - prev->port_rcv_data) * 4 / 1024 / 1024 / refresh_second;
    long int tx_pkt = (cur->port_xmit_packets - prev->port_xmit_packets) / refresh_second;
    long int tx_mb  = (cur->port_xmit_data - prev->port_xmit_data) * 4 / 1024 / 1024 / refresh_second;
    long int uc_rx  = (cur->unicast_rcv_packets - prev->unicast_rcv_packets) / refresh_second;
    long int uc_tx  = (cur->port_unicast_xmit_packets - prev->port_unicast_xmit_packets) / refresh_second;
    long int mc_rx  = (cur->port_multicast_rcv_packets - prev->port_multicast_rcv_packets) / refresh_second;
    long int mc_tx  = (cur->port_multicast_xmit_packets - prev->port_multicast_xmit_packets) / refresh_second;

    CON(w, CLR_IFACE_NAME); wattron(w, A_BOLD);
    mvwprintw(w, row, io_cols[0].start, "%-17s", cur->name_of_interface);
    wattroff(w, A_BOLD); COFF(w, CLR_IFACE_NAME);

    print_centered(w, row, io_cols[1].start, io_cols[1].width, "%ld", rx_pkt, CLR_VALUE, 1);
    print_centered(w, row, io_cols[2].start, io_cols[2].width, "%ld", rx_mb,  CLR_VALUE, 1);
    print_centered(w, row, io_cols[3].start, io_cols[3].width, "%ld", tx_pkt, CLR_VALUE, 1);
    print_centered(w, row, io_cols[4].start, io_cols[4].width, "%ld", tx_mb,  CLR_VALUE, 1);
    print_centered(w, row, io_cols[5].start, io_cols[5].width, "%ld", uc_rx,  CLR_VALUE, 1);
    print_centered(w, row, io_cols[6].start, io_cols[6].width, "%ld", uc_tx,  CLR_VALUE, 1);
    print_centered(w, row, io_cols[7].start, io_cols[7].width, "%ld", mc_rx,  CLR_VALUE, 1);
    print_centered(w, row, io_cols[8].start, io_cols[8].width, "%ld", mc_tx,  CLR_VALUE, 1);
}

/* ------------------------------------------------------------------ */
void print_link_errors(WINDOW *w, int row,
                       const struct interfaces *iface,
                       const struct interfaces *base,
                       int baseline_flag) {
    //print_delimiter(w, row, cols_link, (size_t)NCOLS(cols_link));
    CON(w, CLR_IFACE_NAME); wattron(w, A_BOLD);
    mvwprintw(w, row, 1, "%-17s", iface->name_of_interface);
    wattroff(w, A_BOLD); COFF(w, CLR_IFACE_NAME);
    PRINT_ERR(w, row, 19, "%13ld",
              BVAL(baseline_flag, iface->link_error_recovery,       base->link_error_recovery));
    PRINT_ERR(w, row, 35, "%13ld",
              BVAL(baseline_flag, iface->local_link_intgrity_errors, base->local_link_intgrity_errors));
    PRINT_ERR(w, row, 51, "%8ld",
              BVAL(baseline_flag, iface->link_downed,               base->link_downed));
}

/* ------------------------------------------------------------------ */
void print_port_errors(WINDOW *w, int row,
                       const struct interfaces *iface,
                       const struct interfaces *base,
                       int baseline_flag) {
    int b = COLS / 2 + 1;
    //print_delimiter(w, row, cols_port, (size_t)NCOLS(cols_port));
    CON(w, CLR_IFACE_NAME); wattron(w, A_BOLD);
    mvwprintw(w, row, b, "%-17s", iface->name_of_interface);
    wattroff(w, A_BOLD); COFF(w, CLR_IFACE_NAME);
    PRINT_ERR(w, row, b+18, "%6ld",
              BVAL(baseline_flag, iface->symbol_error,                      base->symbol_error));
    PRINT_ERR(w, row, b+28, "%6ld",
              BVAL(baseline_flag, iface->port_rcv_errors,                   base->port_rcv_errors));
    PRINT_ERR(w, row, b+40, "%10ld",
              BVAL(baseline_flag, iface->port_rcv_remote_physical_errors,   base->port_rcv_remote_physical_errors));
    PRINT_ERR(w, row, b+55, "%10ld",
              BVAL(baseline_flag, iface->port_xmit_discards,                base->port_xmit_discards));
}

/* ------------------------------------------------------------------ */
void print_roce_congestion(WINDOW *w, int row,
                           const struct interfaces *iface,
                           const struct interfaces *base,
                           int baseline_flag) {
    //print_delimiter(w, row, cols_roce, (size_t)NCOLS(cols_roce));
    CON(w, CLR_IFACE_NAME); wattron(w, A_BOLD);
    mvwprintw(w, row, 1, "%-17s", iface->name_of_interface);
    wattroff(w, A_BOLD); COFF(w, CLR_IFACE_NAME);
    PRINT_VAL(w, row,  19, "%12ld",
              BVAL(baseline_flag, iface->np_cnp_sent,                   base->np_cnp_sent));
    PRINT_VAL(w, row,  34, "%12ld",
              BVAL(baseline_flag, iface->np_ecn_marked_roce_packets,    base->np_ecn_marked_roce_packets));
    PRINT_VAL(w, row,  49, "%14ld",
              BVAL(baseline_flag, iface->rp_cnp_handled,                base->rp_cnp_handled));
    PRINT_VAL(w, row,  66, "%14ld",
              BVAL(baseline_flag, iface->rp_cnp_ignored,                base->rp_cnp_ignored));
    PRINT_VAL(w, row,  83, "%12ld",
              BVAL(baseline_flag, iface->roce_adp_retrans,              base->roce_adp_retrans));
    PRINT_VAL(w, row,  98, "%14ld",
              BVAL(baseline_flag, iface->roce_adp_rtrans_to,            base->roce_adp_rtrans_to));
    PRINT_VAL(w, row, 114, "%12ld",
              BVAL(baseline_flag, iface->roce_slow_restart,             base->roce_slow_restart));
}

/* ------------------------------------------------------------------ */
void print_cqe_errors(WINDOW *w, int row,
                      const struct interfaces *iface,
                      const struct interfaces *base,
                      int baseline_flag) {
    //print_delimiter(w, row, cols_cqe, (size_t)NCOLS(cols_cqe));
    CON(w, CLR_IFACE_NAME); wattron(w, A_BOLD);
    mvwprintw(w, row, 1, "%-17s", iface->name_of_interface);
    wattroff(w, A_BOLD); COFF(w, CLR_IFACE_NAME);
    PRINT_ERR(w, row, 19, "%8ld",
              BVAL(baseline_flag, iface->req_cqe_error,       base->req_cqe_error));
    PRINT_ERR(w, row, 32, "%10ld",
              BVAL(baseline_flag, iface->req_cqe_flush_error,  base->req_cqe_flush_error));
    PRINT_ERR(w, row, 45, "%9ld",
              BVAL(baseline_flag, iface->resp_cqe_error,       base->resp_cqe_error));
    PRINT_ERR(w, row, 59, "%10ld",
              BVAL(baseline_flag, iface->resp_cqe_flush_error, base->resp_cqe_flush_error));
}

/* ------------------------------------------------------------------ */
void print_seq_timeout_errors(WINDOW *w, int row,
                              const struct interfaces *iface,
                              const struct interfaces *base,
                              int baseline_flag) {
    int b = COLS / 2 + 1;
    //print_delimiter(w, row, cols_seq, (size_t)NCOLS(cols_seq));
    CON(w, CLR_IFACE_NAME); wattron(w, A_BOLD);
    mvwprintw(w, row, b, "%-17s", iface->name_of_interface);
    wattroff(w, A_BOLD); COFF(w, CLR_IFACE_NAME);
    PRINT_ERR(w, row, b+18, "%8ld",
              BVAL(baseline_flag, iface->duplicate_request,      base->duplicate_request));
    PRINT_ERR(w, row, b+31, "%9ld",
              BVAL(baseline_flag, iface->implied_nak_seq_err,    base->implied_nak_seq_err));
    PRINT_ERR(w, row, b+45, "%13ld",
              BVAL(baseline_flag, iface->local_ack_timeout_err,  base->local_ack_timeout_err));
    PRINT_ERR(w, row, b+60, "%6ld",
              BVAL(baseline_flag, iface->out_of_buffer,          base->out_of_buffer));
}

/* ------------------------------------------------------------------ */
void print_rx_operations(WINDOW *w, int row,
                         const struct interfaces *iface,
                         const struct interfaces *base,
                         int baseline_flag) {
    //print_delimiter(w, row, cols_rxops, (size_t)NCOLS(cols_rxops));
    CON(w, CLR_IFACE_NAME); wattron(w, A_BOLD);
    mvwprintw(w, row, 1, "%-17s", iface->name_of_interface);
    wattroff(w, A_BOLD); COFF(w, CLR_IFACE_NAME);
    PRINT_VAL(w, row, 19, "%11ld",
              BVAL(baseline_flag, iface->rx_atomic_requests,  base->rx_atomic_requests));
    PRINT_VAL(w, row, 34, "%12ld",
              BVAL(baseline_flag, iface->rx_dct_connect,      base->rx_dct_connect));
    PRINT_VAL(w, row, 49, "%11ld",
              BVAL(baseline_flag, iface->rx_icrc_encapsulatd, base->rx_icrc_encapsulatd));
    PRINT_VAL(w, row, 63, "%9ld",
              BVAL(baseline_flag, iface->rx_read_requests,    base->rx_read_requests));
    PRINT_VAL(w, row, 76, "%9ld",
              BVAL(baseline_flag, iface->rx_write_requests,   base->rx_write_requests));
    PRINT_VAL(w, row, 89, "%9ld",
              BVAL(baseline_flag, iface->lifespan,            base->lifespan));
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

    print_header(pad, interface_count,active_count, error_count, baseline_flag);
    print_summary_cards(pad, metrics, baseline, interface_count, active_count, error_count, baseline_flag);
    construct_window_layout(pad, interface_count);

    int n = interface_count;
    for (int i = 0; i < n; ++i) {
        const struct interfaces *iface = &metrics->ib_interfaces[i];
        const struct interfaces *base  = baseline_flag
                                         ? &baseline->ib_interfaces[i]
                                         : iface;   /* unused when flag=0 */
        int dr = i + 1;
        print_interface_status   (pad, ROW_STATUS(n) + 2 + dr, iface);
        print_link_errors        (pad, ROW_LINK(n)   + 2 + dr, iface, base, 0); //last param was baseline_flag
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
                    print_io_throughput(pad,ROW_IO(n) + 2 + dr, &metrics->ib_interfaces[i], &prev_metrics->ib_interfaces[j], refresh_second); //print_io_throughput(pad,ROW_IO(n) + 2 + i + 1, &metrics->ib_interfaces[i], &prev_metrics->ib_interfaces[j], refresh_second);
                    break;
                }
            }
        }
    }
}
