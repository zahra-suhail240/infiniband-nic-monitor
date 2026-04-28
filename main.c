#include <errno.h>
#include <ncurses.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <unistd.h>
#include "infiniband.h"
#include "ncurses_display.h"
#include "extra.h"

/* Must match ncurses_display.c */
#define PAD_TOTAL_ROWS(n)  (6*(n) + (8+4+4+4+4+4+4) + 6)
#define PAD_COLS           500 //was 220

static volatile sig_atomic_t break_flag = 0;
static void sigint_handler(int signo) { (void)signo; break_flag = 1; }

int main(void) {
    long int refresh_second = 5;
    int ether_flag   = 0;
    int error_count  = 0;

    if (is_linux() != 1) {
        fprintf(stderr, "ERROR: this tool only runs on Linux\n");
        return EXIT_FAILURE;
    }

    /* signal setup */
    struct sigaction sa;
    sigset_t signal_empty_set, signal_block_set;
    sigemptyset(&signal_empty_set);
    sigemptyset(&signal_block_set);
    sigaddset(&signal_block_set, SIGINT);
    sigprocmask(SIG_BLOCK, &signal_block_set, NULL);
    sa.sa_handler = sigint_handler;
    sa.sa_flags   = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);

    /* initial fetch */
    struct ib_metrics cur, prev;
    ib_results res = get_ib_metrics(&cur, ether_flag);
    if (res.count < 0) { fprintf(stderr, "ERROR: unable to retrieve IB metrics\n"); return EXIT_FAILURE; }
    if (res.count == 0) { fprintf(stderr, "ERROR: no InfiniBand device found\n");   return EXIT_FAILURE; }

    /* ncurses */
    initscr();
    cbreak();
    noecho();
    curs_set(0);
    keypad(stdscr, TRUE);
    init_colors();
    nodelay(stdscr, TRUE);

    WINDOW *pad = newpad(PAD_TOTAL_ROWS(res.count), PAD_COLS);
    if (!pad) { endwin(); fprintf(stderr, "ERROR: failed to create pad\n"); return EXIT_FAILURE; }

    int scroll_y = 0, scroll_x = 0;
    int prev_flag = 0, prev_count = 0;
    char error_msg[BUFSIZ] = {0};
    int error_flag = 0;

    /* baseline / soft-reset state */
    struct ib_metrics baseline;
    int baseline_flag = 0;

    while (1) {
        /* count total error events across all interfaces */
        error_count = 0;
        for (int i = 0; i < res.count; ++i) {
            error_count += (int)(
                cur.ib_interfaces[i].symbol_error +
                cur.ib_interfaces[i].port_rcv_errors +
                cur.ib_interfaces[i].link_downed +
                cur.ib_interfaces[i].link_error_recovery);
        }
        //debug
        if (prev_flag) {
            FILE *dbg = fopen("/tmp/ib_debug.txt", "a");
            if (dbg) {
                fprintf(dbg, "prev=%ld cur=%ld diff=%ld prev_count=%d n=%d\n",
                    prev.ib_interfaces[0].port_rcv_packets,
                    cur.ib_interfaces[0].port_rcv_packets,
                    cur.ib_interfaces[0].port_rcv_packets - prev.ib_interfaces[0].port_rcv_packets,
                    prev_count,
                    res.count);
                fclose(dbg);
            }
        }

        draw_screen(pad, &cur, &prev, &baseline, res.active_count,
                    res.count, prev_count,
                    prev_flag, baseline_flag,
                    refresh_second, error_count);

        /* clamp scroll */
        int max_y = PAD_TOTAL_ROWS(res.count) - LINES;
        int max_x = PAD_COLS - COLS;
        if (max_y < 0) max_y = 0;
        if (max_x < 0) max_x = 0;
        if (scroll_y < 0) scroll_y = 0;
        if (scroll_x < 0) scroll_x = 0;
        if (scroll_y > max_y) scroll_y = max_y;
        if (scroll_x > max_x) scroll_x = max_x;

        prefresh(pad, scroll_y, scroll_x, 0, 0, LINES-1, COLS-1);

        /* sleep until refresh or keypress */
        struct timespec ts = { refresh_second, 0 };
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(STDIN_FILENO, &rfds);
        int ret = pselect(STDIN_FILENO+1, &rfds, NULL, NULL, &ts, &signal_empty_set);

        if (ret < 0 && errno == EINTR && break_flag) break;

        if (ret > 0) {
            int ch, scrolled = 0;
            while ((ch = getch()) != ERR) {
                switch (ch) {
                    case KEY_DOWN:  case 'j': scroll_y += 3;        scrolled = 1; break;
                    case KEY_UP:    case 'k': scroll_y -= 3;        scrolled = 1; break;
                    case KEY_RIGHT: case 'l': scroll_x += 8;        scrolled = 1; break;
                    case KEY_LEFT:  case 'h': scroll_x -= 8;        scrolled = 1; break;
                    case KEY_NPAGE: case ' ': scroll_y += LINES-2;  scrolled = 1; break;
                    case KEY_PPAGE:           scroll_y -= LINES-2;  scrolled = 1; break;
                    case KEY_HOME:            scroll_y = 0; scroll_x = 0; scrolled = 1; break;
                    case KEY_END:             scroll_y = max_y;     scrolled = 1; break;
                    case 'r': case 'R':
                        /* snapshot current counters as the new zero baseline */
                        baseline      = cur;
                        baseline_flag = 1;
                        break;
                    case 'c': case 'C':
                        /* clear baseline — go back to raw cumulative values */
                        baseline_flag = 0;
                        break;
                    case 'q': case 'Q':       goto done;
                    default: break;
                }
            }
            if (scrolled) {
                if (scroll_y < 0) scroll_y = 0;
                if (scroll_x < 0) scroll_x = 0;
                if (scroll_y > max_y) scroll_y = max_y;
                if (scroll_x > max_x) scroll_x = max_x;
                prefresh(pad, scroll_y, scroll_x, 0, 0, LINES-1, COLS-1);
                continue;
            }
        }

        /* fetch new data */
        prev       = cur;
        prev_count = res.count;
        prev_flag  = 1;

        ib_results res_new = get_ib_metrics(&cur, ether_flag);
        if (res_new.count < 0) {
            strncpy(error_msg, "ERROR: unable to retrieve IB metrics", BUFSIZ-1);
            ++error_flag; break;
        }
        if (res_new.count == 0) {
            strncpy(error_msg, "ERROR: no InfiniBand device found", BUFSIZ-1);
            ++error_flag; break;
        }

        
        res.count  = res_new.count;
        res.active_count = res_new.active_count;
        res.status       = res_new.status;
        
    }

done:
    delwin(pad);
    endwin();
    if (error_flag > 0) { fprintf(stderr, "%s\n", error_msg); return EXIT_FAILURE; }
    return EXIT_SUCCESS;
}