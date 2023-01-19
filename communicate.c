#include "communicate.h"

//*********************************************************************
// NOTE: We will overwrite this file, so whatever changes you put here
//      WILL NOT persist
//*********************************************************************
void send_frame(char* char_buffer, enum SendFrame_DstType dst_type) {
    Frame* inframe = convert_char_to_frame(char_buffer);
    uint8_t dst_id = inframe->dst_id;
    // this ensures that only MAX_FRAME_SIZE bytes are sent via send_frame(). Bytes exceeding that are not sent.
    char* recv_char_buffer = convert_frame_to_char(inframe);
    
    // Multiply out the probabilities to some degree of precision
    int prob_prec = 1000;
    int corrupt_prob = (int) prob_prec * glb_sysconfig.corrupt_prob;
    int num_corrupt_bits = CORRUPTION_BITS;
    int corrupt_indices[num_corrupt_bits];

    // Pick a random number
    int random_num = rand() % prob_prec;
    int random_index;

    // Determine whether to corrupt bits
    random_num = rand() % prob_prec;
    if (random_num < corrupt_prob) {
        // Pick random indices to corrupt
        for (int i = 0; i < num_corrupt_bits; i++) {
            random_index = rand() % MAX_FRAME_SIZE;
            corrupt_indices[i] = random_index;
        }
    }

    // Corrupt the bits (inefficient, should just corrupt one copy and
    // memcpy it
    if (random_num < corrupt_prob) {
        // Corrupt bits at the chosen random indices
        for (int j = 0; j < num_corrupt_bits; j++) {
            random_index = corrupt_indices[j];
            recv_char_buffer[random_index] =
                ~recv_char_buffer[random_index];
        }
    }

    if (dst_type == ReceiverDst) {
        Receiver* dst = &glb_receivers_array[dst_id];
        pthread_mutex_lock(&dst->buffer_mutex);
        ll_append_node(&dst->input_framelist_head,
                        (void*) recv_char_buffer);
        pthread_cond_signal(&dst->buffer_cv);
        pthread_mutex_unlock(&dst->buffer_mutex);
    } else if (dst_type == SenderDst) {
        Sender* dst = &glb_senders_array[dst_id];
        pthread_mutex_lock(&dst->buffer_mutex);
        ll_append_node(&dst->input_framelist_head,
                        (void*) recv_char_buffer);
        pthread_cond_signal(&dst->buffer_cv);
        pthread_mutex_unlock(&dst->buffer_mutex);
    }
    free(char_buffer);
    return;
}
// NOTE: You should use the following method to transmit messages from a sender to receiver
void send_msg_to_receiver(char* char_buffer) {
    send_frame(char_buffer, ReceiverDst);
    return;
}

// NOTE: You should use the following method to transmit messages from a receiver to sender
void send_msg_to_sender(char* char_buffer) {
    send_frame(char_buffer, SenderDst);
    return;
}