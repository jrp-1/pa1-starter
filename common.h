#ifndef __COMMON_H__
#define __COMMON_H__

#include <math.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#define MAX_COMMAND_LENGTH 16
#define AUTOMATED_FILENAME 512
// Window Sizes
#define WINDOW_SIZE 8
#define SWS WINDOW_SIZE
#define RWS WINDOW_SIZE

// max hosts
#define MAX_HOSTS 256

typedef unsigned char uchar_t;
typedef struct Frame_t Frame;

// System configuration information
struct SysConfig_t {
    uint16_t link_bandwidth;
    float corrupt_prob;
    unsigned char automated;
    char automated_file[AUTOMATED_FILENAME];
};
typedef struct SysConfig_t SysConfig;

// Command line input information
struct Cmd_t {
    uint16_t src_id;
    uint16_t dst_id;
    char* message;
};
typedef struct Cmd_t Cmd;

// Linked list information
enum LLtype { llt_string, llt_frame, llt_integer, llt_head } LLtype;

struct LLnode_t {
    struct LLnode_t* prev;
    struct LLnode_t* next;
    enum LLtype type;

    void* value;
};
typedef struct LLnode_t LLnode;

// windows typedefs
typedef struct RecvQ_slot RecvQ;
typedef struct SendQ_slot SendQ;

// Receiver and Sender data structures
struct Receiver_t {
    /* DO NOT CHANGE:
        1) buffer_mutex
        2) buffer_cv
        3) input_framelist_head
        4) recv_id
    */
    pthread_mutex_t buffer_mutex;
    pthread_cond_t buffer_cv;
    LLnode* input_framelist_head;

    int recv_id;
    int active;

    //  check handshake status
    int handshake[MAX_HOSTS];

    uint8_t last_frame_recv;            // last successful frame received
    uint8_t seq_no;                     // sequence number
    uint8_t largest_acc_frame;          // largest acceptable frame

    char* message[MAX_HOSTS];           // message for hosts

    struct RecvQ_slot {
        Frame* frame;
    } RecvQ[RWS];
    RecvQ recvQ[MAX_HOSTS][RWS];

    uint8_t end_of_last_pl;             // end of last payload seq no

};

struct Sender_t {
    /* DO NOT CHANGE:
        1) buffer_mutex
        2) buffer_cv
        3) input_cmdlist_head
        4) input_framelist_head
        5) send_id
    */    
    pthread_mutex_t buffer_mutex;
    pthread_cond_t buffer_cv;
    LLnode* input_cmdlist_head;
    LLnode* input_framelist_head;
    int send_id;
    int active;
    int awaiting_msg_ack;

    // track per receiver
    uint8_t seq_no;         // sequence number
    uint8_t window_start;   // first frame in window
    uint8_t window_end;     // last frame in window
    uint8_t message_end;    // last frame in message
    uint8_t last_ack_recv;
    uint8_t lfs;
    int msg_sent;
    struct timeval time_sent; // time last frame was sent
    struct timeval timeout;   // timeout (expiring)
    Frame* frames[MAX_HOSTS]; // array of frames

    //  check handshake status
    int awaiting_handshake;
    int handshake[MAX_HOSTS];

    struct timespec ts; // part of #include <“time.h”>

    struct SendQ_slot {
        struct timeval timeout;
        Frame* frame;
    } SendQ[SWS];

};

enum SendFrame_DstType { ReceiverDst, SenderDst } SendFrame_DstType;

typedef struct Sender_t Sender;
typedef struct Receiver_t Receiver;

// DO NOT CHANGE
#define MAX_FRAME_SIZE 64

/* TODO: Add fields to your frame that help you achieve the desired functionality.
   Please note that the first three fields of your frame (remaining_msg_bytes, 
   dst_id, src_id) are fixed. DO NOT CHANGE their order or names or data types.

   In the rest portions of the frame, you can add fields and change FRAME_PAYLOAD_SIZE as
   you want. However, MAX_FRAME_SIZE is fixed (i.e. 64 bytes).
*/
#define FRAME_PAYLOAD_SIZE 58
struct Frame_t {
    uint16_t remaining_msg_bytes; // DO NOT CHANGE
    uint8_t dst_id; // DO NOT CHANGE
    uint8_t src_id; // DO NOT CHANGE
    uint8_t seq_no; // sequence number
    char data[FRAME_PAYLOAD_SIZE];
    uint8_t crc8; // added crc
};

/*
Global variables declared below.
DO NOT CHANGE:
  1) glb_senders_array
  2) glb_receivers_array
  3) glb_senders_array_length
  4) glb_receivers_array_length
  5) glb_sysconfig
*/
Sender* glb_senders_array;
Receiver* glb_receivers_array;
int glb_senders_array_length;
int glb_receivers_array_length;
SysConfig glb_sysconfig;

#endif
