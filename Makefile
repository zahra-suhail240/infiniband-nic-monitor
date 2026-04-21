CC = gcc
CFLAGS = -g -Wall -Wextra -Wpedantic -Wconversion -Wdouble-promotion -Wunused -Wshadow -Wsign-conversion -fsanitize=undefined
INCLUDES = -I.
SRCS = infiniband.c main.c extra.c ncurses_display.c
OBJDIR = build
OBJS = $(SRCS:%.c=$(OBJDIR)/%.o)
TARGET = ib_test
LDFLAGS = -lncurses

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(OBJDIR)/%.o: %.c | $(OBJDIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

$(OBJDIR):
	mkdir -p $(OBJDIR)

clean:
	rm -rf $(OBJDIR) $(TARGET)

run: $(TARGET)
	@if ! command -v tmux > /dev/null 2>&1; then \
		echo "ERROR: tmux is not installed. Run: sudo apt install tmux"; \
		exit 1; \
	elif tmux has-session -t ib_monitor 2>/dev/null; then \
		tmux attach -t ib_monitor; \
	elif [ -n "$$TMUX" ]; then \
		tmux new-window "./$(TARGET)"; \
	else \
		tmux new-session -d -s ib_monitor "./$(TARGET)" && tmux attach -t ib_monitor; \
	fi
	