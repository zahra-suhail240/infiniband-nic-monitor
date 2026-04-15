#include "extra.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/utsname.h>

int read_file_char(char *filename, char *value) {
    FILE *file_handle;
    file_handle = NULL;

    file_handle = fopen(filename, "r");
    if (file_handle == NULL) {
        goto handle_error;
    }

    if (fgets(value, BUFSIZ, file_handle) != NULL) {
        size_t value_length = strlen(value);
        if (value_length > 0 && value[value_length - 1] == '\n') {
            value[value_length - 1] = '\0';
        }
    } else {
        goto handle_error;
    }

    fclose(file_handle);

    return 0;

handle_error:
    if (file_handle != NULL) {
        fclose(file_handle);
    }

    return -1;
}