#ifndef NCURSES_DISPLAY_H
#define NCURSES_DISPLAY_H

#include <ncurses.h>
#include "infiniband.h"

/* ------------------------------------------------------------------ *
 * Color pair IDs — init_colors() must be called once after initscr() *
 * ------------------------------------------------------------------ */
#define CLR_TITLE        1   /* bold cyan    — title, section banners  */
#define CLR_HEADER       2   /* bold yellow  — column headers          */
#define CLR_IFACE_NAME   3   /* bold white   — interface name column   */
#define CLR_VALUE        4   /* cyan         — normal numeric values   */
#define CLR_ACTIVE       5   /* bold green   — "Active" state badge    */
#define CLR_ERROR        6   /* bold red     — non-zero error values   */
#define CLR_ZERO         7   /* dim white    — zero / idle values      */
#define CLR_BADGE        8   /* black on cyan — inline badges          */
#define CLR_BADGE_GRAY   9   /* black on white — /counters badges      */
#define CLR_SEPARATOR   10   /* dim cyan     — box lines / delimiters  */
#define CLR_SUBTEXT     11   /* dim white    — path, timestamp, footer */
#define CLR_CARD_TITLE  12   /* white        — card label row          */
#define CLR_CARD_VALUE  13   /* bold green   — card big number         */
#define CLR_ACTIVE_DOT  14   /* bold green   — ● active badge          */
#define CLR_ERROR_DOT   15   /* bold red     — ● errors badge          */
#define CLR_RESET       16   /* bold yellow on red — reset active badge */

/* Call once after initscr() and start_color() */
extern void init_colors(void);

/* ------------------------------------------------------------------ *
 * Top-level draw call — call once per refresh cycle.                  *
 * baseline_flag: non-zero means subtract baseline from counters.      *
 * ------------------------------------------------------------------ */
extern void draw_screen(WINDOW *pad,
                        const struct ib_metrics *metrics,
                        const struct ib_metrics *prev_metrics,
                        const struct ib_metrics *baseline,
                        int interface_count,
                        int prev_count,
                        int prev_flag,
                        int baseline_flag,
                        long int refresh_second,
                        int error_count);

/* ------------------------------------------------------------------ *
 * Individual section printers                                         *
 * ------------------------------------------------------------------ */
extern void print_header        (WINDOW *pad, int interface_count,
                                 int error_count, int baseline_flag);
extern void print_summary_cards (WINDOW *pad,
                                 const struct ib_metrics *metrics,
                                 const struct ib_metrics *baseline,
                                 int interface_count, int error_count,
                                 int baseline_flag);
extern void construct_window_layout (WINDOW *pad, int interface_count);
extern void print_delimiter     (WINDOW *pad, int row,
                                 int *column_positions, size_t column_size);

/* status — no baseline needed (state/rate are not counters) */
extern void print_interface_status   (WINDOW *pad, int row,
                                      const struct interfaces *iface);

/* throughput — already a per-poll delta, baseline not applied */
extern void print_io_throughput      (WINDOW *pad, int row,
                                      const struct interfaces *cur,
                                      const struct interfaces *prev,
                                      long int refresh_second);

/* cumulative counter sections — accept baseline */
extern void print_link_errors        (WINDOW *pad, int row,
                                      const struct interfaces *iface,
                                      const struct interfaces *base,
                                      int baseline_flag);
extern void print_port_errors        (WINDOW *pad, int row,
                                      const struct interfaces *iface,
                                      const struct interfaces *base,
                                      int baseline_flag);
extern void print_roce_congestion    (WINDOW *pad, int row,
                                      const struct interfaces *iface,
                                      const struct interfaces *base,
                                      int baseline_flag);
extern void print_cqe_errors         (WINDOW *pad, int row,
                                      const struct interfaces *iface,
                                      const struct interfaces *base,
                                      int baseline_flag);
extern void print_seq_timeout_errors (WINDOW *pad, int row,
                                      const struct interfaces *iface,
                                      const struct interfaces *base,
                                      int baseline_flag);
extern void print_rx_operations      (WINDOW *pad, int row,
                                      const struct interfaces *iface,
                                      const struct interfaces *base,
                                      int baseline_flag);

#endif /* NCURSES_DISPLAY_H */