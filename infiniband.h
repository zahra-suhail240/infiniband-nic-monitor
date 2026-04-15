#ifndef INFINIBAND_H
#define INFINIBAND_H

#include <stdio.h>

#define INTERFACE_NUMBER 16
#define DEVICE_NAME 64

struct interfaces {
    //Counters folder <- currently working on
    long int port_rcv_data; //done
    long int port_rcv_packets;//done
    long int port_multicast_rcv_packets; //done
    long int unicast_rcv_packets; //done
    long int port_xmit_data;//done
    long int port_xmit_packets;//done
    long int port_rcv_switch_relay_errors;//done
    long int port_rcv_errors; //done
    long int port_rcv_constraint_errors; //done
    long int local_link_intgrity_errors;//done
    long int port_xmit_wait; //done
    long int port_multicast_xmit_packets;//done
    long int port_unicast_xmit_packets;//done
    long int port_xmit_discards;//done
    long int port_xmit_constraint_errors;//donne
    long int port_rcv_remote_physical_errors; //done
    long int symbol_error; //done
    long int VL15_dropped;
    long int link_error_recovery;//done
    long int link_downed;//done
    
    //Local ID only used in IB, 0 for RoCE
    long int lid; //done

    //hw_counter folder
    long int duplicate_request;
    long int implied_nak_seq_err;
    long int lifespan;
    long int local_ack_timeout_err;
    long int np_cnp_sent;
    long int np_ecn_marked_roce_packets;
    long int np_ecn_marked_packets;
    long int out_of_buffer;
    long int out_of_sequence;
    long int packet_seq_err;
    long int req_cqe_error;
    long int req_cqe_flush_error;
    long int resp_cqe_error;
    long int resp_cqe_flush_error;
    long int resp_remot_access_errors;
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
    
    char link_layer[BUFSIZ]; //DONEEE
    char name_of_interface[DEVICE_NAME];//done
    char phys_state[BUFSIZ]; //DONE
    char state[BUFSIZ]; //doneee
    char rate[BUFSIZ];  //done
};

//Each port will have it's own metrics, so we need an array of interfaces
struct ib_metrics {
    struct interfaces ib_interfaces[INTERFACE_NUMBER];
};

extern int get_ib_metrics(struct ib_metrics *input_metrics, int ether_flag);     


#endif