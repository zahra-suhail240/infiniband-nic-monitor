#ifndef EXTRA_H
#define EXTRA_H

extern int read_file_char(char *filename, char *value);
extern int read_file_int(char *filename, long int *value);

extern int is_linux(void);
#define SIZEOF(arr) (sizeof(arr) / sizeof((arr)[0]))

#endif