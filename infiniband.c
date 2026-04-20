#include "infiniband.h"
#include "extra.h"
#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>

#include <sys/stat.h>
#include <linux/limits.h>

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

            ret_read_file = read_file_char(path_to_link_layer, character_value);


            if(snprintf_result < 0 || ret_read_file < 0){
                continue;
            }

            if(ether_flag <=0){
                if(strcmp(character_value, "Ethernet") != 0){
                    continue;
                }
            }
            
            size_t device_path_length = strlen(sysfs_port_path);
            path_length = device_path_length  + strlen("/counters") + 1;
            char sysfs_device_port_counters_path[path_length];
            
            snprintf_result = snprintf(sysfs_device_port_counters_path, path_length, "%s/counters", sysfs_port_path);
            if(snprintf_result < 0){
                continue;
            }
 

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

           
           strcpy(input_metrics->ib_interfaces[count].name_of_interface, interface_name);
           strcpy(input_metrics->ib_interfaces[count].link_layer, character_value);

           path_length = strlen(sysfs_port_path) + strlen("/state") + 1;
           char state_file_path[path_length];
          
           snprintf_result = snprintf(state_file_path, path_length, "%s/state", sysfs_port_path);
           ret_read_file = read_file_char(state_file_path, character_value);
           

            if(snprintf_result < 0 || ret_read_file < 0){
                continue;
            }

            strcpy(input_metrics->ib_interfaces[count].state, character_value);
            

            //Phys_state
            path_length = strlen(sysfs_port_path) + strlen("/phys_state") + 1;
            char phys_state_file_path[path_length];

            snprintf_result = snprintf(phys_state_file_path, path_length, "%s/phys_state", sysfs_port_path);
            ret_read_file = read_file_char(phys_state_file_path, character_value);

            if(snprintf_result < 0 || ret_read_file < 0){
                continue;
            }
            strcpy(input_metrics->ib_interfaces[count].phys_state, character_value);
            

            //rate
            path_length = strlen(sysfs_port_path) + strlen("/rate") + 1;
            char rate_file_path[path_length];
            
            snprintf_result = snprintf(rate_file_path, path_length, "%s/rate", sysfs_port_path);
            ret_read_file = read_file_char(rate_file_path, character_value);

            if(snprintf_result < 0 || ret_read_file < 0){
                continue;
            }
            strcpy(input_metrics->ib_interfaces[count].rate, character_value);
            

            //lid
            path_length = strlen(sysfs_port_path) + strlen("/lid") + 1;
            char lid_file_path[path_length];

            snprintf_result = snprintf(lid_file_path, path_length, "%s/lid", sysfs_port_path);
            ret_read_file = read_file_int(lid_file_path, character_value);

            if(snprintf_result < 0 || ret_read_file < 0){
                continue;
            }

            input_metrics->ib_interfaces[count].lid = integer_value;
            

            //COUNTER FOLDER
            
           char counters_folder_path[PATH_MAX];
           
           //symbol_error
           snprintf_result = snprintf(counters_folder_path, PATH_MAX, "%s/symbol_error", sysfs_port_path); //double check if im suppoed to use this sysfs_port_path
           

           ret_read_file = read_file_int(counters_folder_path, &integer_value);
           if(snprintf_result < 0 || ret_read_file < 0){
                input_metrics->ib_interfaces[count].symbol_error = 0;
            }else{
                input_metrics->ib_interfaces[count].symbol_error = integer_value;
            }
            
        
            snprintf_result = snprintf(counters_folder_path, PATH_MAX, "%s/port_rcv_errors", sysfs_port_path);
            ret_read_file = read_file_int(counters_folder_path, &integer_value);
            if(snprintf_result < 0 || ret_read_file < 0){
                input_metrics->ib_interfaces[count].port_rcv_errors = 0;
            }else{
                input_metrics->ib_interfaces[count].port_rcv_errors = integer_value;
            }
            

            snprintf_result = snprintf(counters_folder_path, PATH_MAX, "%s/port_rcv_data", sysfs_port_path);
            ret_read_file = read_file_int(counters_folder_path, &integer_value);
            if(snprintf_result < 0 || ret_read_file < 0){
                input_metrics->ib_interfaces[count].port_rcv_data = 0;
            }else{
                input_metrics->ib_interfaces[count].port_rcv_data = integer_value;
            }
              

            snprintf_result = snprintf(counters_folder_path, PATH_MAX, "%s/port_rcv_packets", sysfs_port_path);
            ret_read_file = read_file_int(counters_folder_path, &integer_value);
            if(snprintf_result < 0 || ret_read_file < 0){
                input_metrics->ib_interfaces[count].port_rcv_packets = 0;       
            }else{
                input_metrics->ib_interfaces[count].port_rcv_packets = integer_value;
            }
              

            snprintf_result = snprintf(counters_folder_path, PATH_MAX, "%s/port_multicast_rcv_packets", sysfs_port_path);
            ret_read_file = read_file_int(counters_folder_path, &integer_value);
            if(snprintf_result < 0 || ret_read_file < 0){
                input_metrics->ib_interfaces[count].port_multicast_rcv_packets = 0;
            }else{  
                input_metrics->ib_interfaces[count].port_multicast_rcv_packets = integer_value;
            }
            

            snprintf_result = snprintf(counters_folder_path, PATH_MAX, "%s/unicast_rcv_packets", sysfs_port_path);
            ret_read_file = read_file_int(counters_folder_path, &integer_value);
            if(snprintf_result < 0 || ret_read_file < 0){
                input_metrics->ib_interfaces[count].unicast_rcv_packets = 0;        
            }else{
                input_metrics->ib_interfaces[count].unicast_rcv_packets = integer_value;
            }
             

            snprintf_result = snprintf(counters_folder_path, PATH_MAX, "%s/port_xmit_data", sysfs_port_path);
            ret_read_file = read_file_int(counters_folder_path, &integer_value);
            if(snprintf_result < 0 || ret_read_file < 0){
                input_metrics->ib_interfaces[count].port_xmit_data = 0;     
            }else{  
                input_metrics->ib_interfaces[count].port_xmit_data = integer_value;
            }
            

            snprintf_result = snprintf(counters_folder_path, PATH_MAX, "%s/port_xmit_packets", sysfs_port_path);
            ret_read_file = read_file_int(counters_folder_path, &integer_value);
            if(snprintf_result < 0 || ret_read_file < 0){
                input_metrics->ib_interfaces[count].port_xmit_packets = 0;     
            }else{  
                input_metrics->ib_interfaces[count].port_xmit_packets = integer_value;
            }
            


            snprintf_result = snprintf(counters_folder_path, PATH_MAX, "%s/port_rcv_switch_relay_errors", sysfs_port_path);
            ret_read_file = read_file_int(counters_folder_path, &integer_value);
            if(snprintf_result < 0 || ret_read_file < 0){       
                input_metrics->ib_interfaces[count].port_rcv_switch_relay_errors = 0;
            }else{
                input_metrics->ib_interfaces[count].port_rcv_switch_relay_errors = integer_value;
            }
            
            snprintf_result = snprintf(counters_folder_path, PATH_MAX, "%s/port_rcv_constraint_errors", sysfs_port_path);
            ret_read_file = read_file_int(counters_folder_path, &integer_value);
            if(snprintf_result < 0 || ret_read_file < 0){       
                input_metrics->ib_interfaces[count].port_rcv_constraint_errors = 0;
            }else{
                input_metrics->ib_interfaces[count].port_rcv_constraint_errors = integer_value;
            }

            snprintf_result = snprintf(counters_folder_path, PATH_MAX, "%s/local_link_intgrity_errors", sysfs_port_path);
            ret_read_file = read_file_int(counters_folder_path, &integer_value);
            if(snprintf_result < 0 || ret_read_file < 0){
                input_metrics->ib_interfaces[count].local_link_intgrity_errors = 0;
            }else{
                input_metrics->ib_interfaces[count].local_link_intgrity_errors = integer_value;
            }
            

            snprintf_result = snprintf(counters_folder_path, PATH_MAX, "%s/port_xmit_wait", sysfs_port_path);
            ret_read_file = read_file_int(counters_folder_path, &integer_value);
            if(snprintf_result < 0 || ret_read_file < 0){       
                input_metrics->ib_interfaces[count].port_xmit_wait = 0;
            }else{
                input_metrics->ib_interfaces[count].port_xmit_wait = integer_value;
            }
           

            snprintf_result = snprintf(counters_folder_path, PATH_MAX, "%s/port_multicast_xmit_packets", sysfs_port_path);
            ret_read_file = read_file_int(counters_folder_path, &integer_value);
            if(snprintf_result < 0 || ret_read_file < 0){       
                input_metrics->ib_interfaces[count].port_multicast_xmit_packets = 0;    
            }else{
                input_metrics->ib_interfaces[count].port_multicast_xmit_packets = integer_value;
            }
            

            snprintf_result = snprintf(counters_folder_path, PATH_MAX, "%s/port_unicast_xmit_packets", sysfs_port_path);
            ret_read_file = read_file_int(counters_folder_path, &integer_value);        
            if(snprintf_result < 0 || ret_read_file < 0){       
                input_metrics->ib_interfaces[count].port_unicast_xmit_packets = 0;
            }else{
                input_metrics->ib_interfaces[count].port_unicast_xmit_packets = integer_value;  
            }
            

            snprintf_result = snprintf(counters_folder_path, PATH_MAX, "%s/port_xmit_discards", sysfs_port_path);
            ret_read_file = read_file_int(counters_folder_path, &integer_value);
            if(snprintf_result < 0 || ret_read_file < 0){
                input_metrics->ib_interfaces[count].port_xmit_discards = 0;
            }else{
                input_metrics->ib_interfaces[count].port_xmit_discards = integer_value;
            }
           

            snprintf_result = snprintf(counters_folder_path, PATH_MAX, "%s/port_xmit_constraint_errors", sysfs_port_path);
            ret_read_file = read_file_int(counters_folder_path, &integer_value);
            if(snprintf_result < 0 || ret_read_file < 0){       
                input_metrics->ib_interfaces[count].port_xmit_constraint_errors = 0;
            }else{
                input_metrics->ib_interfaces[count].port_xmit_constraint_errors = integer_value;
            }
           

            snprintf_result = snprintf(counters_folder_path, PATH_MAX, "%s/port_rcv_remote_physical_errors", sysfs_port_path);
            ret_read_file = read_file_int(counters_folder_path, &integer_value);
            if(snprintf_result < 0 || ret_read_file < 0){       
                input_metrics->ib_interfaces[count].port_rcv_remote_physical_errors = 0;
            }else{      
                input_metrics->ib_interfaces[count].port_rcv_remote_physical_errors = integer_value;
            }
            

            snprintf_result = snprintf(counters_folder_path, PATH_MAX, "%s/link_error_recovery", sysfs_port_path);
            ret_read_file = read_file_int(counters_folder_path, &integer_value);
            if(snprintf_result < 0 || ret_read_file < 0){       
                input_metrics->ib_interfaces[count].link_error_recovery = 0;
            }else{      
                input_metrics->ib_interfaces[count].link_error_recovery = integer_value;
            }
           
            snprintf_result = snprintf(counters_folder_path, PATH_MAX, "%s/link_downed", sysfs_port_path);
            ret_read_file = read_file_int(counters_folder_path, &integer_value);
            if(snprintf_result < 0 || ret_read_file < 0){       
                input_metrics->ib_interfaces[count].link_downed = 0;
            }else{      
                input_metrics->ib_interfaces[count].link_downed = integer_value;
            }
              

            snprintf_result = snprintf(counters_folder_path, PATH_MAX, "%s/VL15_dropped", sysfs_port_path);
            ret_read_file = read_file_int(counters_folder_path, &integer_value);        
            if(snprintf_result < 0 || ret_read_file < 0){       
                input_metrics->ib_interfaces[count].VL15_dropped = 0;
            }else{
                input_metrics->ib_interfaces[count].VL15_dropped = integer_value;  
            }
            


            //------------------- SAME FOR HW_COUNTERS FOLDER ----------------------//
            size_t hw_counters_path_length = strlen(sysfs_port_path);
            path_length = hw_counters_path_length  + strlen("/hw_counters") + 1;
            char sysfs_device_port_HWcounters_path[path_length];
            
            snprintf_result = snprintf(sysfs_device_port_HWcounters_path, path_length, "%s/hw_counters", sysfs_port_path);
            if(snprintf_result < 0){
                continue;
            }

            //Now im inside the counters folder
           

            char hw_counters_folder_path[PATH_MAX];

            snprintf_result = snprintf(hw_counters_folder_path, PATH_MAX, "%s/duplicate_request", sysfs_device_port_HWcounters_path);
            ret_read_file = read_file_int(hw_counters_folder_path, &integer_value);
            if(snprintf_result < 0 || ret_read_file < 0){
                  input_metrics->ib_interfaces[count].duplicate_request = 0;   
            }
            else{
                input_metrics->ib_interfaces[count].duplicate_request = integer_value;
            }
            

            snprintf_result = snprintf(hw_counters_folder_path, PATH_MAX, "%s/implied_nak_seq_err", sysfs_port_path);
            ret_read_file = read_file_int(hw_counters_folder_path, &integer_value);
            if(snprintf_result < 0 || ret_read_file < 0){
                  input_metrics->ib_interfaces[count].implied_nak_seq_err = 0;   
            }
            else{
                input_metrics->ib_interfaces[count].implied_nak_seq_err = integer_value;
            }
            

            snprintf_result = snprintf(hw_counters_folder_path, PATH_MAX, "%s/lifespan", sysfs_device_port_HWcounters_path);
            ret_read_file = read_file_int(hw_counters_folder_path, &integer_value);
            if(snprintf_result < 0 || ret_read_file < 0){
                  input_metrics->ib_interfaces[count].lifespan = 0;   
            }
            else{       
                input_metrics->ib_interfaces[count].lifespan = integer_value;
            }
           

            snprintf_result = snprintf(hw_counters_folder_path, PATH_MAX, "%s/local_ack_timeout_err", sysfs_device_port_HWcounters_path);
            ret_read_file = read_file_int(hw_counters_folder_path, &integer_value);
            if(snprintf_result < 0 || ret_read_file < 0){
                  input_metrics->ib_interfaces[count].local_ack_timeout_err = 0;   
            }
            else{       
                input_metrics->ib_interfaces[count].local_ack_timeout_err = integer_value;
            }
           
            snprintf_result = snprintf(hw_counters_folder_path, PATH_MAX, "%s/np_cnp_sent", sysfs_device_port_HWcounters_path);
            ret_read_file = read_file_int(hw_counters_folder_path, &integer_value);
            if(snprintf_result < 0 || ret_read_file < 0){
                  input_metrics->ib_interfaces[count].np_cnp_sent = 0;   
            }
            else{       
                input_metrics->ib_interfaces[count].np_cnp_sent = integer_value;
            }
           
            
            snprintf_result = snprintf(hw_counters_folder_path, PATH_MAX, "%s/np_ecn_marked_roce_packets", sysfs_device_port_HWcounters_path);
            ret_read_file = read_file_int(hw_counters_folder_path, &integer_value);
            if(snprintf_result < 0 || ret_read_file < 0){
                  input_metrics->ib_interfaces[count].np_ecn_marked_roce_packets = 0;        
            }
            else{
                input_metrics->ib_interfaces[count].np_ecn_marked_roce_packets = integer_value;
            }
            
            
            snprintf_result = snprintf(hw_counters_folder_path, PATH_MAX, "%s/out_of_buffer", sysfs_device_port_HWcounters_path);
            ret_read_file = read_file_int(hw_counters_folder_path, &integer_value);
            if(snprintf_result < 0 || ret_read_file < 0){
                  input_metrics->ib_interfaces[count].out_of_buffer = 0;        
            }       
            else{
                input_metrics->ib_interfaces[count].out_of_buffer = integer_value;
            }
              

            snprintf_result = snprintf(hw_counters_folder_path, PATH_MAX, "%s/out_of_sequence", sysfs_device_port_HWcounters_path);
            ret_read_file = read_file_int(hw_counters_folder_path, &integer_value);
            if(snprintf_result < 0 || ret_read_file < 0){
                  input_metrics->ib_interfaces[count].out_of_sequence = 0;        
            }       
            else{
                input_metrics->ib_interfaces[count].out_of_sequence = integer_value;
            }
            
            
            snprintf_result = snprintf(hw_counters_folder_path, PATH_MAX, "%s/packet_seq_err", sysfs_device_port_HWcounters_path);
            ret_read_file = read_file_int(hw_counters_folder_path, &integer_value);
            if(snprintf_result < 0 || ret_read_file < 0){
                  input_metrics->ib_interfaces[count].packet_seq_err = 0;        
            }       
            else{       
                input_metrics->ib_interfaces[count].packet_seq_err = integer_value;
            }
             

            snprintf_result = snprintf(hw_counters_folder_path, PATH_MAX, "%s/req_cqe_error", sysfs_device_port_HWcounters_path);
            ret_read_file = read_file_int(hw_counters_folder_path, &integer_value);
            if(snprintf_result < 0 || ret_read_file < 0){       
                    input_metrics->ib_interfaces[count].req_cqe_error = 0;        
                }       
                else{       
                    input_metrics->ib_interfaces[count].req_cqe_error = integer_value;
                }   
           

            snprintf_result = snprintf(hw_counters_folder_path, PATH_MAX, "%s/req_cqe_flush_error", sysfs_device_port_HWcounters_path);
            ret_read_file = read_file_int(hw_counters_folder_path, &integer_value);
            if(snprintf_result < 0 || ret_read_file < 0){       
                    input_metrics->ib_interfaces[count].req_cqe_flush_error = 0;        
                }       
                else{    
                    input_metrics->ib_interfaces[count].req_cqe_flush_error = integer_value;        
                }
            

            snprintf_result = snprintf(hw_counters_folder_path, PATH_MAX, "%s/resp_cqe_error", sysfs_device_port_HWcounters_path);
            ret_read_file = read_file_int(hw_counters_folder_path, &integer_value);     
            if(snprintf_result < 0 || ret_read_file < 0){       
                input_metrics->ib_interfaces[count].resp_cqe_error = 0;        
            }       
            else{    
                input_metrics->ib_interfaces[count].resp_cqe_error = integer_value;        
            }
            

            snprintf_result = snprintf(hw_counters_folder_path, PATH_MAX, "%s/resp_cqe_flush_error", sysfs_device_port_HWcounters_path);
            ret_read_file = read_file_int(hw_counters_folder_path, &integer_value);     
            if(snprintf_result < 0 || ret_read_file < 0){       
                input_metrics->ib_interfaces[count].resp_cqe_flush_error = 0;        
            }       
            else{    
                input_metrics->ib_interfaces[count].resp_cqe_flush_error = integer_value;        
            }
             

            snprintf_result = snprintf(hw_counters_folder_path, PATH_MAX, "%s/resp_remote_access_errors", sysfs_device_port_HWcounters_path);
            ret_read_file = read_file_int(hw_counters_folder_path, &integer_value);
            if(snprintf_result < 0 || ret_read_file < 0){    
                input_metrics->ib_interfaces[count].resp_remote_access_errors = 0;   
            }
            else{
                input_metrics->ib_interfaces[count].resp_remote_access_errors = integer_value;
            }
           

            
            snprintf_result = snprintf(hw_counters_folder_path, PATH_MAX, "%s/rnr_nak_retry_err", sysfs_device_port_HWcounters_path);
            ret_read_file = read_file_int(hw_counters_folder_path, &integer_value);
            if(snprintf_result < 0 || ret_read_file < 0){

                input_metrics->ib_interfaces[count].rnr_nak_retry_err = 0;

            }else{
                input_metrics->ib_interfaces[count].rnr_nak_retry_err = integer_value;
            }
 
            
            snprintf_result = snprintf(hw_counters_folder_path, PATH_MAX, "%s/roce_adp_retrans", sysfs_device_port_HWcounters_path);
            ret_read_file = read_file_int(hw_counters_folder_path, &integer_value);
            if(snprintf_result < 0 || ret_read_file < 0){
                input_metrics->ib_interfaces[count].roce_adp_retrans = 0;
            }else{      
                input_metrics->ib_interfaces[count].roce_adp_retrans = integer_value;
            }
          
            
            snprintf_result = snprintf(hw_counters_folder_path, PATH_MAX, "%s/roce_adp_rtrans_to", sysfs_device_port_HWcounters_path);
            ret_read_file = read_file_int(hw_counters_folder_path, &integer_value);
            if(snprintf_result < 0 || ret_read_file < 0){
                input_metrics->ib_interfaces[count].roce_adp_rtrans_to = 0;
            }else{      
                input_metrics->ib_interfaces[count].roce_adp_rtrans_to = integer_value;
            }
            

            snprintf_result = snprintf(hw_counters_folder_path, PATH_MAX, "%s/roce_slow_restart", sysfs_device_port_HWcounters_path);
            ret_read_file = read_file_int(hw_counters_folder_path, &integer_value); 
            if(snprintf_result < 0 || ret_read_file < 0){
                input_metrics->ib_interfaces[count].roce_slow_restart =0;
            }else{
                input_metrics->ib_interfaces[count].roce_slow_restart = integer_value;
            }
           

            snprintf_result = snprintf(hw_counters_folder_path, PATH_MAX, "%s/roce_slow_restart_cnps", sysfs_device_port_HWcounters_path);
            ret_read_file = read_file_int(hw_counters_folder_path, &integer_value); 
            if(snprintf_result < 0 || ret_read_file < 0){
                input_metrics->ib_interfaces[count].roce_slow_restart_cnps = 0;
            }else{
                input_metrics->ib_interfaces[count].roce_slow_restart_cnps = integer_value;
            }   
           


            snprintf_result = snprintf(hw_counters_folder_path, PATH_MAX, "%s/roce_slow_restart_retrans", sysfs_device_port_HWcounters_path);
            ret_read_file = read_file_int(hw_counters_folder_path, &integer_value);
            if(snprintf_result < 0 || ret_read_file < 0){
                input_metrics->ib_interfaces[count].roce_slow_restart_trans = 0;
            }else{
                input_metrics->ib_interfaces[count].roce_slow_restart_trans = integer_value;      
            }
             
            snprintf_result = snprintf(hw_counters_folder_path, PATH_MAX, "%s/rp_cnp_handled", sysfs_device_port_HWcounters_path);
            ret_read_file = read_file_int(hw_counters_folder_path, &integer_value);
            if(snprintf_result < 0 || ret_read_file < 0){
                input_metrics->ib_interfaces[count].rp_cnp_handled = 0;
            }else{
                input_metrics->ib_interfaces[count].rp_cnp_handled = integer_value;
            }
           

            snprintf_result = snprintf(hw_counters_folder_path, PATH_MAX, "%s/rp_cnp_ignored", sysfs_device_port_HWcounters_path);  
            ret_read_file = read_file_int(hw_counters_folder_path, &integer_value);
            if(snprintf_result < 0 || ret_read_file < 0){
                input_metrics->ib_interfaces[count].rp_cnp_ignored = 0;
            }else{
                input_metrics->ib_interfaces[count].rp_cnp_ignored = integer_value;
            }
           
            
            snprintf_result = snprintf(hw_counters_folder_path, PATH_MAX, "%s/rx_atomic_requests", sysfs_device_port_HWcounters_path);
            ret_read_file = read_file_int(hw_counters_folder_path, &integer_value);
            if(snprintf_result < 0 || ret_read_file < 0){
                input_metrics->ib_interfaces[count].rx_atomic_requests = 0;
            }else{  
                input_metrics->ib_interfaces[count].rx_atomic_requests = integer_value;
            }
            
            
            snprintf_result = snprintf(hw_counters_folder_path, PATH_MAX, "%s/rx_dct_connect ", sysfs_device_port_HWcounters_path);
            ret_read_file = read_file_int(hw_counters_folder_path, &integer_value);
            if(snprintf_result < 0 || ret_read_file < 0){   
                input_metrics->ib_interfaces[count].rx_dct_connect = 0;
            }else{
                input_metrics->ib_interfaces[count].rx_dct_connect = integer_value;
            }


            snprintf_result = snprintf(hw_counters_folder_path, PATH_MAX, "%s/rx_icrc_encapsulatd", sysfs_device_port_HWcounters_path);
            ret_read_file = read_file_int(hw_counters_folder_path, &integer_value);
            if(snprintf_result < 0 || ret_read_file < 0){   
                input_metrics->ib_interfaces[count].rx_icrc_encapsulatd = 0;
            }else{
                input_metrics->ib_interfaces[count].rx_icrc_encapsulatd = integer_value;
            }
            
            
            snprintf_result = snprintf(hw_counters_folder_path, PATH_MAX, "%s/rx_read_requests", sysfs_device_port_HWcounters_path);
            ret_read_file = read_file_int(hw_counters_folder_path, &integer_value);
            if(snprintf_result < 0 || ret_read_file < 0){
                input_metrics->ib_interfaces[count].rx_read_requests = 0;
            }else{
                input_metrics->ib_interfaces[count].rx_read_requests = integer_value;
            }

            snprintf_result = snprintf(hw_counters_folder_path, PATH_MAX, "%s/rx_write_requests", sysfs_device_port_HWcounters_path);   
            ret_read_file = read_file_int(hw_counters_folder_path, &integer_value);
            if(snprintf_result < 0 || ret_read_file < 0){
                input_metrics->ib_interfaces[count].rx_write_requests = 0;
            }else{
                input_metrics->ib_interfaces[count].rx_write_requests = integer_value;
            }

            //----------------------------------------------------------------------//

            ++count;

            if(count >= INTERFACE_NUMBER){
                break;
            }
        
        }

        closedir(device_handle);
        
    }

    closedir(dir_handle);
    return count;

}   
  

    
