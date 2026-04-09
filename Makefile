CC = gcc
CFLAGS = -g -Wall -Wextra -Wpedantic -Wconversion -Wdouble-promotion -Wunused -Wshadow -Wsign-conversion -fsanitize=undefined
INCLUDES = -I.
SRCS = infiniband.c main.c
OBJDIR = build
OBJS =  $(SRCS:%.c=$(OBJDIR)/%.o)
TARGET = ib_test
LDFLAGS = -lncurses

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(OBJDIR)/%.o: %.c | $(OBJDIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

$(OBJDIR):
	mkdir -p $(OBJDIR)

clean:
	rm -f $(OBJS) $(TARGET)