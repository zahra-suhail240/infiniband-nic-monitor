#ifndef INFINIBAND_H
#define INFINIBAND_H

#include <stdio.h>

#define INTERFACE_NUMBER 16
#define DEVICE_NAME 64

typedef struct {
    int count;
    int active_count;
    int status;
} ib_results;

struct interfaces {
    //Counters folder 
    long int port_rcv_data; 
    long int port_rcv_packets;
    long int port_multicast_rcv_packets; 
    long int unicast_rcv_packets; 
    long int port_xmit_data;
    long int port_xmit_packets;
    long int port_rcv_switch_relay_errors;
    long int port_rcv_errors; 
    long int port_rcv_constraint_errors; 
    long int local_link_intgrity_errors;
    long int port_xmit_wait; 
    long int port_multicast_xmit_packets;
    long int port_unicast_xmit_packets;
    long int port_xmit_discards;
    long int port_xmit_constraint_errors;
    long int port_rcv_remote_physical_errors; 
    long int symbol_error; 
    long int VL15_dropped;
    long int link_error_recovery;
    long int link_downed;
    
    //Local ID only used in IB, 0 for RoCE
    long int lid; 

    //hw_counter folder
    long int duplicate_request;
    long int implied_nak_seq_err;
    long int lifespan;
    long int local_ack_timeout_err;
    long int np_cnp_sent;
    long int np_ecn_marked_roce_packets; 
    long int out_of_buffer;
    long int out_of_sequence;
    long int packet_seq_err;
    long int req_cqe_error;
    long int req_cqe_flush_error;
    long int resp_cqe_error; 
    long int resp_cqe_flush_error;
    long int resp_remote_access_errors;
    long int rnr_nak_retry_err;
    long int roce_adp_retrans; 
    long int roce_adp_rtrans_to;
    long int roce_slow_restart;
    long int roce_slow_restart_cnps;
    long int roce_slow_restart_trans;
    long int rp_cnp_handled;
    long int rp_cnp_ignored;
    long int rx_atomic_requests;
    long int rx_dct_connect;
    long int rx_icrc_encapsulatd;
    long int rx_read_requests;
    long int rx_write_requests;
    
    char link_layer[BUFSIZ]; 
    char name_of_interface[DEVICE_NAME];
    char phys_state[BUFSIZ];
    char state[BUFSIZ]; 
    char rate[BUFSIZ]; 
};

//Each port will have it's own metrics, so we need an array of interfaces
struct ib_metrics {
    struct interfaces ib_interfaces[INTERFACE_NUMBER];
};

extern ib_results get_ib_metrics(struct ib_metrics *input_metrics, int ether_flag);     



#endif