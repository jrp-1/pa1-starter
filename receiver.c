#include "receiver.h"

void init_receiver(Receiver* receiver, int id) {
    pthread_cond_init(&receiver->buffer_cv, NULL);
    pthread_mutex_init(&receiver->buffer_mutex, NULL);
    receiver->recv_id = id;
    receiver->input_framelist_head = NULL;
    receiver->active = 1;

    receiver->last_frame_recv = 0;
    receiver->seq_no = 0;
    receiver->largest_acc_frame = WINDOW_SIZE - 1;

    for (int i = 0; i < MAX_HOSTS; i++) {
        receiver->handshake[i] = 0;
    }

    receiver->end_of_last_pl = 0;
    receiver->pl_printed = 0;
    // receiver->message = malloc(FRAME_PAYLOAD_SIZE * UINT8_MAX); // huge string
}

void send_ack(Receiver* receiver, LLnode** outgoing_frames_head_ptr, uint8_t sequence_no, uint8_t send_id) {
    Frame* outgoing_frame = malloc(sizeof(Frame));
    outgoing_frame->remaining_msg_bytes = 0;
    outgoing_frame->dst_id = send_id;
    outgoing_frame->src_id = receiver->recv_id;
    outgoing_frame->seq_no = sequence_no;
    strcpy(outgoing_frame->data, "ACK");
    // Add CRC
    char* outgoing_charbuf = convert_frame_to_char(outgoing_frame);
    outgoing_frame->crc8 = compute_crc8(outgoing_charbuf);
    outgoing_charbuf = convert_frame_to_char(outgoing_frame);

    ll_append_node(outgoing_frames_head_ptr, outgoing_charbuf);
    free(outgoing_frame);
}


void send_synack(Receiver* receiver, LLnode** outgoing_frames_head_ptr, uint8_t sequence_no, uint8_t send_id) {
    Frame* outgoing_frame = malloc(sizeof(Frame));
    outgoing_frame->remaining_msg_bytes = 0;
    outgoing_frame->dst_id = send_id;
    outgoing_frame->src_id = receiver->recv_id;
    outgoing_frame->seq_no = sequence_no;
    strcpy(outgoing_frame->data, "SYN-ACK");
    // Add CRC
    char* outgoing_charbuf = convert_frame_to_char(outgoing_frame);
    outgoing_frame->crc8 = compute_crc8(outgoing_charbuf);
    outgoing_charbuf = convert_frame_to_char(outgoing_frame);

    if (receiver->message[send_id] == NULL) {
        receiver->message[send_id] = malloc(sizeof(char)  * FRAME_PAYLOAD_SIZE * UINT8_MAX);
    }
   

    // fprintf(stderr, "Sending SYN-ACK to%d \n", outgoing_frame->dst_id);

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


        // check crc8
        if (!compute_crc8(raw_char_buf)) {
            // crc 0 -- frame not corrupted
            Frame* inframe = convert_char_to_frame(raw_char_buf);

            if (!(strcmp(inframe->data, "SYN"))) {
                // fprintf(stderr, "SYNPAYLOADREC\n");

                // establish handshake
                send_synack(receiver, outgoing_frames_head_ptr, inframe->seq_no, inframe->src_id);
                // make receive Q
                receiver->handshake[inframe->src_id] = 1;

                // make a string on handshake
                receiver->message[inframe->src_id] = malloc(FRAME_PAYLOAD_SIZE * UINT8_MAX);

                free(raw_char_buf);
            }
            else if (receiver->handshake[inframe->src_id] == 0) {
                // error : NO HANDSHAKE
                fprintf(stderr, "NO HANDSHAKE FROM %d to receiver %d\n", inframe->src_id, receiver->recv_id);
            }
            else if (inframe->seq_no <= receiver->largest_acc_frame || ((uint8_t)(inframe->seq_no + 8) <= (uint8_t)(receiver->largest_acc_frame + 8))) {
                // TODO: edge case largest_acc_frame becomes 0 and seq_no is 255, etc. -- add 8 to each to check so they rotate around


                
                // print_frame(inframe);
                // CHECK if we already have the frame! -- ALSO need to check our buffer
                if (receiver->last_frame_recv == inframe->seq_no) {
                    // SAME FRAME AS LAST TIME
                    if (receiver->recvQ[inframe->src_id][inframe->seq_no % RWS].frame == NULL) {
                        // it's not saved
                        receiver->recvQ[inframe->src_id][receiver->seq_no % RWS].frame = malloc(sizeof(Frame));
                    }
                    copy_frame(receiver->recvQ[inframe->src_id][receiver->seq_no % RWS].frame, inframe);
                    // update our message
                    memcpy((receiver->message[inframe->src_id] + inframe->seq_no * FRAME_PAYLOAD_SIZE), inframe->data, FRAME_PAYLOAD_SIZE);
                }
                else  {

                    receiver->seq_no = inframe->seq_no;
                    if (receiver->seq_no > receiver->last_frame_recv + 1) {
                        // out of order frame sent -- 
                        if (receiver->seq_no <= receiver->largest_acc_frame) {
                            memcpy((receiver->message[inframe->src_id] + receiver->seq_no * FRAME_PAYLOAD_SIZE), inframe->data, FRAME_PAYLOAD_SIZE);
                        }
                    }
                    else if (receiver->seq_no < receiver->last_frame_recv) {
                        // WE SHOULDNT HAVE TO DO ANYTHING
                        // out of order frame sent but valid frame as long as > laf - 8
                        // check if we have frames in window
                        // uint8_t seq = receiver->seq_no;
                        // while (seq < receiver->largest_acc_frame) {
                            // go through the window
                            if (receiver->recvQ[inframe->src_id][receiver->seq_no % RWS].frame != NULL) {
                                // seq++;
                                // update our message
                                memcpy((receiver->message[inframe->src_id] + receiver->seq_no * FRAME_PAYLOAD_SIZE), inframe->data, FRAME_PAYLOAD_SIZE);
                            }
                        // }
                        // cumulative ACK to most recent frame
                    }

                    else {
                        // frame in order, update largest acc frame?
                        receiver->last_frame_recv = inframe->seq_no;
                        receiver->largest_acc_frame = receiver->last_frame_recv + RWS - 1;

                        // update our message
                        memcpy((receiver->message[inframe->src_id] + inframe->seq_no * FRAME_PAYLOAD_SIZE), inframe->data, FRAME_PAYLOAD_SIZE);
                        // memcpy(str_pos, receiver->frames[inframe->src_id][i]->data, FRAME_PAYLOAD_SIZE);

                        // fprintf(stderr, "Largest acc frame set to %d\n", receiver->largest_acc_frame);

                    }
                }

                // Free raw_char_buf
                free(raw_char_buf);

                // add to window
                if (receiver->recvQ[inframe->src_id][receiver->seq_no % RWS].frame == NULL) {
                    receiver->recvQ[inframe->src_id][receiver->seq_no % RWS].frame = malloc(sizeof(Frame));
                    // receiver->recvQ[inframe->src_id][receiver->seq_no % RWS].frame = receiver->frames[inframe->src_id][receiver->seq_no];
                }
                copy_frame(receiver->recvQ[inframe->src_id][receiver->seq_no % RWS].frame, inframe);


                // receiver->frames[inframe->src_id][receiver->seq_no] = malloc(sizeof(Frame));
                // copy_frame(receiver->frames[inframe->src_id][receiver->seq_no], inframe);

                // fprintf(stderr, "<RECV_%d>:[%s]\n", receiver->recv_id, inframe->data);


                // printf("<RECV_%d>:[%s]\n", receiver->recv_id, inframe->data);

                if (inframe->remaining_msg_bytes == 0) { // && !receiver->pl_printed) {
                    // fprintf(stderr, "LAST FRAME RECV?\n");
                    // fprintf(stderr, "FIRST FRAME? %d\n", first_frame);
                    // we need to make sure all payloads have been receieved
                    // while ()

                    // print our message
                    printf("<RECV_%d>:[%s]\n", receiver->recv_id, (receiver->message[inframe->src_id] + receiver->end_of_last_pl * FRAME_PAYLOAD_SIZE));

                    // set our last payload position
                    receiver->end_of_last_pl = inframe->seq_no + 1;
                    // receiver->pl_printed = 1;
                }

                // only ack the last in-order frame received
                send_ack(receiver, outgoing_frames_head_ptr, receiver->last_frame_recv, inframe->src_id);


                free(inframe);

                // when we get to the end of the window we need to move the window forwards and clear the queue
                // we also need to stop moving the window when we receive EOF/last message


                // // fprintf(stderr, "ACKING recv_%d, send_%d, seq_no%d, remaining bytes:%d\n", receiver->recv_id, inframe->src_id, receiver->seq_no, inframe->remaining_msg_bytes);
                // // send ack
                // send_ack(receiver, outgoing_frames_head_ptr, receiver->last_frame_recv, inframe->src_id);

                // check if last frame, if so print
                // if (inframe->remaining_msg_bytes == 0) { // PRINT
                //        char char_buf[FRAME_PAYLOAD_SIZE * UINT8_MAX]; // huge string

                //     char* str_pos = char_buf;
                //     // for (int i = first_frame; i <= receiver->last_frame_recv; i++) {
                // //         // printf("<RECV_%d>:[%s]\t", receiver->recv_id, receiver->frames[i]->data);
                //         // memcpy(str_pos, receiver->frames[inframe->src_id][i]->data, FRAME_PAYLOAD_SIZE);

                //         // fprintf(stderr, "copied string%d\n", i);
                //         // printf("|||%s\n", str_pos);
                //         // free(receiver->frames[inframe->src_id][i]);
                //         str_pos += FRAME_PAYLOAD_SIZE;
                //     }

                //     printf("<RECV_%d>:[%s]\n", receiver->recv_id, char_buf);

                //     // set inactive?
                //     // printf("LENGTH OF RECEIVER:%d\n", ll_get_length(*outgoing_frames_head_ptr));

                // }

                // free(inframe);
                // free(ll_inmsg_node);
            } else {
                // fprintf(stderr, "Dropped frame seq_no: %d (not in range?) -- %d largest_acc_frame\n", inframe->seq_no, receiver->largest_acc_frame);
                // drop frame -- seq_no not within acceptable range
            }
        }
        else {
            // drop frame
            // fprintf(stderr, "\nCRC MISMATCH: wait for resend\n");
            free(raw_char_buf);
            // free(ll_inmsg_node);
        }
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
