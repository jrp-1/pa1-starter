#include "receiver.h"

const char* ack_msg = "ACK";

void init_receiver(Receiver* receiver, int id) {
    pthread_cond_init(&receiver->buffer_cv, NULL);
    pthread_mutex_init(&receiver->buffer_mutex, NULL);
    receiver->recv_id = id;
    receiver->input_framelist_head = NULL;
    receiver->active = 1;
}

void send_ack(Receiver* receiver, LLnode** outgoing_frames_head_ptr, uint8_t sequence_no, uint8_t send_id) {
    Frame* outgoing_frame = malloc(sizeof(Frame));
    outgoing_frame->remaining_msg_bytes = 0;
    outgoing_frame->dst_id = send_id;
    outgoing_frame->src_id = receiver->recv_id;
    outgoing_frame->seq_no = sequence_no;
    strcpy(outgoing_frame->data, ack_msg);
    // Add CRC
    char* outgoing_charbuf = convert_frame_to_char(outgoing_frame);
    outgoing_frame->crc8 = compute_crc8(outgoing_charbuf);
    outgoing_charbuf = convert_frame_to_char(outgoing_frame);

    ll_append_node(outgoing_frames_head_ptr, outgoing_charbuf);
    free(outgoing_frame);
}

void handle_incoming_frames(Receiver* receiver,
                          LLnode** outgoing_frames_head_ptr) {
    // TODO: Suggested steps for handling incoming frames
    //    1) Dequeue the Frame from the sender->input_framelist_head
    //    2) Compute CRC of incoming frame to know whether it is corrupted
    //    3) If frame is corrupted, drop it and move on.
    //    4) If frame is not corrupted, convert incoming frame from char* to Frame* data type
    //    5) Implement logic to check if the expected frame has come
    //    6) Implement logic to combine payload received from all frames belonging to a message
    //       and print the final message when all frames belonging to a message have been received.
    //    7) ACK the received frame
    int incoming_frames_length = ll_get_length(receiver->input_framelist_head);
    while (incoming_frames_length > 0) {
        // Pop a node off the front of the link list and update the count
        LLnode* ll_inmsg_node = ll_pop_node(&receiver->input_framelist_head);
        incoming_frames_length = ll_get_length(receiver->input_framelist_head);

        char* raw_char_buf = ll_inmsg_node->value;
        Frame* inframe = convert_char_to_frame(raw_char_buf);

        // Free raw_char_buf
        free(raw_char_buf);

        printf("<RECV_%d>:[%s]\n", receiver->recv_id, inframe->data);

        // send ack
        send_ack(receiver, outgoing_frames_head_ptr, 0, inframe->src_id);

        free(inframe);
        free(ll_inmsg_node);
    }
}

void* run_receiver(void* input_receiver) {
    struct timespec time_spec;
    struct timeval curr_timeval;
    const int WAIT_SEC_TIME = 0;
    const long WAIT_USEC_TIME = 100000;
    Receiver* receiver = (Receiver*) input_receiver;
    LLnode* outgoing_frames_head;

    // This incomplete receiver thread. At a high level, it loops as follows:
    // 1. Determine the next time the thread should wake up if there is nothing
    // in the input_framelist
    // 2. Grab the mutex protecting the input_framelist 
    // 3. Dequeues frames from the input_framelist and prints them
    // 4. Releases the lock
    // 5. Sends out any outgoing frames

    while (1) {
        // NOTE: Add outgoing frames to the outgoing_frames_head pointer
        outgoing_frames_head = NULL;
        gettimeofday(&curr_timeval, NULL);

        // Either timeout or get woken up because you've received a frame
        // NOTE: You don't really need to do anything here, but it might be
        // useful for debugging purposes to have the receivers periodically
        // wakeup and print info
        time_spec.tv_sec = curr_timeval.tv_sec;
        time_spec.tv_nsec = curr_timeval.tv_usec * 1000;
        time_spec.tv_sec += WAIT_SEC_TIME;
        time_spec.tv_nsec += WAIT_USEC_TIME * 1000;
        if (time_spec.tv_nsec >= 1000000000) {
            time_spec.tv_sec++;
            time_spec.tv_nsec -= 1000000000;
        }

        //*****************************************************************************************
        // NOTE: Anything that involves dequeing from the input frames should go
        //      between the mutex lock and unlock, because other threads
        //      CAN/WILL access these structures
        //*****************************************************************************************
        pthread_mutex_lock(&receiver->buffer_mutex);

        // Check whether anything arrived
        int incoming_frames_length =
            ll_get_length(receiver->input_framelist_head);
        if (incoming_frames_length == 0) {
            // Nothing has arrived, do a timed wait on the condition variable
            // (which releases the mutex). Again, you don't really need to do
            // the timed wait. A signal on the condition variable will wake up
            // the thread and reacquire the lock
            pthread_cond_timedwait(&receiver->buffer_cv,
                                   &receiver->buffer_mutex, &time_spec);
        }

        handle_incoming_frames(receiver, &outgoing_frames_head);
        receiver->active = ll_get_length(outgoing_frames_head) > 0 ? 1:0;
        pthread_mutex_unlock(&receiver->buffer_mutex);

        // DO NOT CHANGE BELOW CODE
        // Send out all the frames user has appended to the outgoing_frames list
        int ll_outgoing_frame_length = ll_get_length(outgoing_frames_head);
        while (ll_outgoing_frame_length > 0) {
            LLnode* ll_outframe_node = ll_pop_node(&outgoing_frames_head);
            char* char_buf = (char*) ll_outframe_node->value;

            // The following function will free the memory for the char_buf object.
            // The function will convert char_buf to frame and deliver it to 
            // the sender having sender->send_id = frame->dst_id.
            send_msg_to_sender(char_buf);
            // Free up the ll_outframe_node
            free(ll_outframe_node);

            ll_outgoing_frame_length = ll_get_length(outgoing_frames_head);
        }
    }
    pthread_exit(NULL);
}
