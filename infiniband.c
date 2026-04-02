#include "infiniband.h"
#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>

int get_ib_metrics(struct ib_metrics *metrics) {
    //number of interfaces proceessed
    int count = 0;
    
    //temp buffers for reading file contents
    char character_value[BUFSIZ];
    long int integer_value;

    DIR *dir_handle =NULL;

    struct dirent *dir_entry;

    //Open the directory containing the IB metrics
    dir_handle = opendir("/sys/class/infiniband");
    if(dir_handle == NULL){
        fprintf(stderr, "Unable to open the following directory: /sys/class/infiniband: %s\n", strerror(errno));
        return -1;
    }

    printf("Devices\n");

    while((dir_entry = readdir(dir_handle)) != NULL){
        //Skip the . and .. entries
        if(strcmp(dir_entry->d_name, ".") == 0 || strcmp(dir_entry->d_name, "..") == 0){
            continue;
        }

        printf("%s\n", dir_entry->d_name);
        count++;
    }   
    return 2;

    
}