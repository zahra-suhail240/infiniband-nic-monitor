#include "infiniband.h"
#include "extra.h"
#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>

#include <sys/stat.h>

int get_ib_metrics(struct ib_metrics *input_metrics, int ether_flag) {
    //number of interfaces processed
    int count = 0;
    
    //temp buffers for reading file contents
    char character_value[BUFSIZ];
    long int integer_value;

    DIR *dir_handle = NULL;
    DIR *device_handle = NULL;

    struct dirent *dir_entry;
    struct dirent *device_entry;

    //Extra
    size_t path_length;
    int snprintf_result;
    int ret_read_file;

    //Open the directory containing the IB metrics
    dir_handle = opendir("/sys/class/infiniband");
    if(dir_handle == NULL){
        fprintf(stderr, "Unable to open the following directory: /sys/class/infiniband: %s\n", strerror(errno));
        return -1;
    }

    //printf("Devices\n");

    while((dir_entry = readdir(dir_handle)) != NULL){
       
        //Skip the . and .. entries
        if(strcmp(dir_entry->d_name, ".") == 0 || strcmp(dir_entry->d_name, "..") == 0){
            continue;
        }
        //Construct the path to the current device's metrics
        path_length = strlen("/sys/class/infiniband/") + strlen(dir_entry->d_name) + strlen("/ports") + 1;
        char sysfs_devices_path[path_length];

        snprintf_result =snprintf(sysfs_devices_path, path_length, "/sys/class/infiniband/%s/ports", dir_entry->d_name);

        if(snprintf_result < 0){
            continue;    
        }
        printf("%d\n", snprintf_result);

       // printf("%s\n", dir_entry->d_name);
       //count++;

        //Need to open the port file now
        device_handle = opendir(sysfs_devices_path);
        if(device_handle == NULL){
            continue;
        }


        while((device_entry = readdir(device_handle)) != NULL){
            if(strcmp(device_entry->d_name, ".") == 0 || strcmp(device_entry->d_name, "..") == 0){
                continue;
            }

            path_length = strlen(sysfs_devices_path) + strlen("/") +strlen(device_entry->d_name) + 1;
            char sysfs_port_path[path_length];

            snprintf_result = snprintf(sysfs_port_path, path_length, "%s/%s", sysfs_devices_path, device_entry->d_name);

            if(snprintf_result < 0){
                continue;
            }

            //Begin reading the metrics from the port file starting with the link layer
            path_length = strlen(sysfs_port_path) + strlen("/link layer") + 1;
            char path_to_link_layer[path_length];

            snprintf_result = snprintf(path_to_link_layer, path_length, "%s/link_layer", sysfs_port_path);

            printf("%s\n", path_to_link_layer);
            
            ret_read_file = read_file_char(path_to_link_layer, character_value);

            printf("%d\n", ret_read_file);

            if(snprintf_result < 0 || ret_read_file < 0){
                continue;
            }

            

            if(ether_flag <=0){
                if(strcmp(character_value, "Ethernet") != 0){
                    printf("DEBUG link_layer raw: '%s'\n", character_value);
                    continue;
                }
            }
            

            //Currently look into the counters folder
        

            size_t device_path_length = strlen(sysfs_port_path);
            path_length = device_path_length  + strlen("/counters") + 1;
            char sysfs_device_port_counters_path[path_length];
            
            snprintf_result = snprintf(sysfs_device_port_counters_path, path_length, "%s/counters", sysfs_port_path);
            if(snprintf_result < 0){
                continue;
            }

            //Now im inside the counters folder
            printf("%s\n",sysfs_device_port_counters_path); 

            struct stat counters_stats;

            if(stat(sysfs_device_port_counters_path, &counters_stats) !=0 || !S_ISDIR(counters_stats.st_mode)){
                continue;
            }


            size_t interface_name_length = strlen(device_entry->d_name) +strlen(dir_entry->d_name) + strlen(":")+1;
            char interface_name[interface_name_length];
            snprintf_result = snprintf(interface_name, interface_name_length, "%s:%s", dir_entry->d_name, device_entry->d_name);
            if(snprintf_result < 0){
                continue;
            }

            printf("%s\n", interface_name);
           
           strcpy(input_metrics->ib_interfaces[count].name_of_interface, interface_name);
           strcpy(input_metrics->ib_interfaces[count].link_layer, character_value);

           path_length = strlen(sysfs_port_path) + strlen("/state") + 1;
           char state_file_path[path_length];
          
           snprintf_result = snprintf(state_file_path, path_length, "%s/state", sysfs_port_path);
           ret_read_file = read_file_char(state_file_path, character_value);
           
           printf("%s\n", state_file_path);

            if(snprintf_result < 0 || ret_read_file < 0){
                continue;
            }
           
           














            //----- SMAE FOR HW_COUNTERS FOLDER -----//
            size_t hw_counters_path_length = strlen(sysfs_port_path);
            path_length = hw_counters_path_length  + strlen("/hw_counters") + 1;
            char sysfs_device_port_HWcounters_path[path_length];
            
            snprintf_result = snprintf(sysfs_device_port_HWcounters_path, path_length, "%s/hw_counters", sysfs_port_path);
            if(snprintf_result < 0){
                continue;
            }

            //Now im inside the counters folder
            printf("%s\n",sysfs_device_port_HWcounters_path); 
            
            //------------------------------------------//
        
        }
        
    }

}   
  

    
