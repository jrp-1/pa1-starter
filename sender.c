#include "sender.h"

#include <assert.h>

void init_sender(Sender* sender, int id) {
    pthread_cond_init(&sender->buffer_cv, NULL);
    pthread_mutex_init(&sender->buffer_mutex, NULL);
    sender->send_id = id;
    sender->input_cmdlist_head = NULL;
    sender->input_framelist_head = NULL;
    sender->active = 1;
    sender->awaiting_msg_ack = 0;
    sender->awaiting_handshake = 0;
    sender->last_ack_recv = 0;

    sender->awaiting_handshake = 0;
    for (int i = 0; i < MAX_HOSTS; i++) {
        sender->handshake[i] = 0; // no handshake
    }

    // TODO: You should fill in this function as necessary
    sender->lfs = 0; //last frame sent
    gettimeofday(&sender->time_sent, NULL);
    sender->seq_no = 0;
    sender->msg_sent = 1; // initialize to 1

    // .01 s timer for nanosleep
    sender->ts.tv_sec = 0;
    sender->ts.tv_nsec = 10000000; // 10 milliseconds
}

struct timeval* sender_get_next_expiring_timeval(Sender* sender) {
    // TODO: You should fill in this function so that it returns the 
    // timeval when next timeout should occur
    // timeval for each frame in window (change from .09 to .01=10ms) -- SYN still .09
    if (sender->awaiting_handshake) {
        // SYN-ACK only uses .09s
        return &sender->timeout;
    }
    // if (sender->awaiting_msg_ack)
        // fprintf(stderr, "Getting Q timeout expiring_timeval\n");
    return &sender->SendQ[(sender->lfs) % SWS].timeout;
}

void set_timeout(Sender* sender) {
    // add .09s to time_sent
    // if (sender->awaiting_handshake) {
        sender->timeout.tv_sec = sender->time_sent.tv_sec;
        sender->timeout.tv_usec = sender->time_sent.tv_usec + 90000;
        if (sender->timeout.tv_usec > 1000000) { // check for overlow
            sender->timeout.tv_sec++;
            sender->timeout.tv_usec -= 1000000;
            //fprintf(stderr, "TIMEOUT OVERFLOW\n");
        }
    // }
    //fprintf(stderr, "SET TIMEOUT\n");

}

void build_frame(Sender* sender, LLnode** outgoing_frames_head_ptr, Frame* outgoing_frame, char* message, uint8_t src, uint8_t dst, uint8_t sequence_no, uint16_t remaining_bytes) {
    // builds a frame
    assert(outgoing_frame);
    outgoing_frame->remaining_msg_bytes = remaining_bytes;
    outgoing_frame->src_id = src;
    outgoing_frame->dst_id = dst;
    outgoing_frame->seq_no = sequence_no;
    outgoing_frame->crc8 = 0x0; // remembmber to 0 out crc

    memcpy(outgoing_frame->data, message, FRAME_PAYLOAD_SIZE);

    // Add CRC
    char* outgoing_charbuf = convert_frame_to_char(outgoing_frame);
    outgoing_frame->crc8 = compute_crc8(outgoing_charbuf);
    free(outgoing_charbuf);

    // printf("Messages:%s\n", message);

    // printf("Built Frame: remaining_bytes: %d src:%d dst:%d seq_no:%d|\nmessage: %s||\n|crc8:%d\n", outgoing_frame->remaining_msg_bytes, outgoing_frame->src_id, outgoing_frame->dst_id, outgoing_frame->seq_no, outgoing_frame->data, outgoing_frame->crc8);
    // At this point, we don't need the outgoing_cmd
    // free(message);
}

void add_syn(Sender* sender, LLnode** outgoing_frames_head_ptr, Frame* outgoing_frame) {
    // USE ONLY FOR SYN-ACK
    // set last frame sent
    sender->lfs = outgoing_frame->seq_no;
    // set time frame sent
    gettimeofday(&sender->time_sent, NULL);
    // set timeout
    set_timeout(sender);

    // adds frame to queue
    // TODO: crc, ack
    // set we are waiting for ack
    sender->awaiting_msg_ack = 1;

    // fprintf(stderr, "Sending: SND_%d,RCV%d,MSG:%s,seq_no:%d\n",outgoing_frame->src_id,outgoing_frame->dst_id,outgoing_frame->data,outgoing_frame->seq_no);
    char* outgoing_charbuf = convert_frame_to_char(outgoing_frame);
    ll_append_node(outgoing_frames_head_ptr, outgoing_charbuf);
    // free(outgoing_frame);
}

void send_syn(Sender* sender, LLnode** outgoing_frames_head_ptr, uint8_t dst) {
    Frame* outgoing_frame = malloc(sizeof(Frame));
    sender->frames[0] = outgoing_frame;             // don't lose the frame if need to resend
    build_frame(sender, outgoing_frames_head_ptr, outgoing_frame, "SYN", sender->send_id, dst, 0, 0);
    sender->awaiting_handshake = 1;
    add_syn(sender, outgoing_frames_head_ptr, outgoing_frame);
}

void handle_incoming_acks(Sender* sender, LLnode** outgoing_frames_head_ptr) {
    // TODO: Suggested steps for handling incoming ACKs
    //    1) Dequeue the ACK from the sender->input_framelist_head
    //    2) Convert the incoming frame from char* to Frame* data type
    //    3) Implement logic as per stop and wait ARQ to track ACK for what frame is expected,
    //       and what to do when ACK for expected frame is receivedr

    if (sender->awaiting_msg_ack) {
        int incoming_frames_length = ll_get_length(sender->input_framelist_head);
        while (incoming_frames_length > 0) {
            // Pop a node off the front of the link list and update the count
            LLnode* ll_inmsg_node = ll_pop_node(&sender->input_framelist_head);
            incoming_frames_length = ll_get_length(sender->input_framelist_head);

            char* raw_char_buf = ll_inmsg_node->value;
            Frame* inframe = convert_char_to_frame(raw_char_buf);

            fprintf(stderr, "AWAITING ACK\n");
            // fprintf(stderr, "INCOMING ACK: %s\n", )

            // if (!compute_crc8(raw_char_buf)) { // check crc

            // HANDLE SYN-ACK
                if(!(strcmp(inframe->data, "SYN-ACK"))) {
                    // we're acked
                    fprintf(stderr, "SYN-ACK from sender host %d\tto receiver host %d\n", inframe->dst_id, inframe->src_id);
                    sender->awaiting_handshake = 0;
                    sender->awaiting_msg_ack = 0;
                    sender->handshake[inframe->src_id] = 1;

                    // free our outgoing frame
                    free(sender->frames[sender->lfs]);
                }

                if(!(strcmp(inframe->data, "ACK"))) { 
                    // check for ACK
                    sender->last_ack_recv = inframe->seq_no;            // should handle cumulative ack

                    fprintf(stderr, "WEHAVEACK\t\t%dlastack\t\tseq_no %d\t\tmessageend: %d\t\twindowend:%d\n", sender->last_ack_recv, inframe->seq_no, sender->message_end, sender->window_end);

                    if (sender->last_ack_recv == sender->message_end - 1) {

                        fprintf(stderr, "PAYLOADGONE\n");
                        // full payload sent & we have ack
                        sender->awaiting_msg_ack = 0;
                    }

                    sender->window_start = sender->last_ack_recv;

                    if (sender->window_end < sender->message_end - 1) {
                        sender->window_end = (uint8_t)(sender->last_ack_recv + SWS - 1);
                        // sender->window_start = sender->last_ack_recv;
                        // moving window end to 
                    }



                    // free(sender->SendQ[sender->last_ack_recv % SWS].frame); // free the acked payload
                    if (sender->last_ack_recv == sender->message_end) {  // we need to move the window & free the send q

                        // sender->window_end = sender->window_end + SWS - 1;
                        // if (sender->window_end > sender->message_end) {
                        //     sender->window_end = sender->message_end;
                        // }
                        sender->window_start = sender->last_ack_recv;
                        sender->awaiting_msg_ack = 0;

                        if (sender->seq_no == sender->message_end) {
                            sender->msg_sent = 1;   // last ack?
                        }
                    }
                 }
            // }
            // else {
            //     print_frame(inframe);
            //     fprintf(stderr, "Computed CRC: %d\n", compute_crc8(raw_char_buf));
            //     // drop ACK frame for CRC mismatch -- times out the frame
            //     fprintf(stderr, "\nACK CRC MISMATCH\n");
            // }

            // Free raw_char_buf
            free(raw_char_buf);
        }
    }
}

void handle_input_cmds(Sender* sender, LLnode** outgoing_frames_head_ptr) {
    // TODO: Suggested steps for handling input cmd
    //    1) Dequeue the Cmd from sender->input_cmdlist_head
    //    2) Convert to Frame
    //    3) Set up the frame according to the protocol

    int input_cmd_length = ll_get_length(sender->input_cmdlist_head);

    // Recheck the command queue length to see if stdin_thread dumped a command
    // on us
    input_cmd_length = ll_get_length(sender->input_cmdlist_head);

    while (input_cmd_length > 0 && sender->msg_sent && !(sender->awaiting_handshake) && !(sender->awaiting_msg_ack)) {

        // peek to check if receiver has SYN-ACKed
        LLnode* ll_peeked_input = sender->input_cmdlist_head;
        Cmd* peeked_command = (Cmd* )ll_peeked_input->value;

        // fprintf(stderr, "handshake status:%d\n", sender->handshake[peeked_command->dst_id]);
        // if no handshake, SYN-ACK
        if (sender->handshake[peeked_command->dst_id] == 0) {
            // SYN-ACK and sleep for syn-ack
            send_syn(sender, outgoing_frames_head_ptr, peeked_command->dst_id);
            sender->awaiting_handshake = 1;
            sender->awaiting_msg_ack = 1;
            // fprintf(stderr, "SENT SYN to %d\n", peeked_command->dst_id);
        }

        else {
            sender->msg_sent = 0;   // we want to send a message before next command

            // Pop a node off and update the input_cmd_length
            LLnode* ll_input_cmd_node = ll_pop_node(&sender->input_cmdlist_head);
            input_cmd_length = ll_get_length(sender->input_cmdlist_head);

            // Cast to Cmd type and free up the memory for the node
            Cmd* outgoing_cmd = (Cmd*) ll_input_cmd_node->value;
            free(ll_input_cmd_node);

            int msg_length = strlen(outgoing_cmd->message) + 1;


            if (msg_length > (FRAME_PAYLOAD_SIZE * UINT8_MAX)) {
                printf(
                "<SEND_%d>: sending messages of length greater than %d is not "
                "implemented\n",
                sender->send_id, MAX_FRAME_SIZE * UINT8_MAX);
            }

            // number of new frames
            uint8_t new_frames = (msg_length / FRAME_PAYLOAD_SIZE) + 1;
            // fprintf(stderr, "NEW FRAMES:%d\t\n", new_frames);

            sender->window_start = sender->seq_no;
            if (new_frames < SWS) {
                // less frame than window size
                sender->window_end = sender->seq_no + new_frames;
            }
            else {
                sender->window_end = sender->seq_no + SWS - 1;
            }

            // fprintf(stderr, "NEW FRAMES:%d\tWINDOW END:%d\tSEQ NO:%d\n", new_frames, sender->window_end, sender->seq_no);

            uint16_t remaining_bytes = msg_length - FRAME_PAYLOAD_SIZE;

            if (msg_length - FRAME_PAYLOAD_SIZE < 0) {
                remaining_bytes = 0; // when payloads only need 1 frame
            }
            // printf("Sending%dmessages\n", sender->frame_ctr);

            char* str_pos = outgoing_cmd->message; // pointer to where we are

            fprintf(stderr, "NEW FRAMES:%d\n", new_frames);

            while (new_frames > 0) {

                sender->frames[sender->seq_no] = malloc(sizeof(Frame));
                char char_buf[FRAME_PAYLOAD_SIZE]; // buffer for each payload
                
                memcpy(char_buf, str_pos, FRAME_PAYLOAD_SIZE);

                // increment our position pointer
                str_pos += FRAME_PAYLOAD_SIZE;

                assert(sender->frames[sender->seq_no]);

                build_frame(sender, outgoing_frames_head_ptr, sender->frames[sender->seq_no], char_buf, outgoing_cmd->src_id, outgoing_cmd->dst_id, sender->seq_no, remaining_bytes);
                fprintf(stderr, "message_length:%d, remaining_bytes:%d,seq_no:%d\n", msg_length, remaining_bytes, sender->seq_no);

                if (remaining_bytes - FRAME_PAYLOAD_SIZE > 0) {
                    remaining_bytes -= FRAME_PAYLOAD_SIZE;
                }
                else {
                    remaining_bytes = 0;
                }
                
                fprintf(stderr, "SEQ_NO:%u\t", sender->seq_no);
                sender->seq_no = (uint8_t)(sender->seq_no + 1);    // overflow should set 255 to 0

                new_frames --;                          // decrement to 0
                
            }
            free(outgoing_cmd);
            free(outgoing_cmd->message);

            sender->message_end = sender->seq_no;   // last frame in message;

            fprintf(stderr, "NEW FRAMES:%d\tWINDOW END:%d\tSEQ NO:%d\tMESSAGE END:%d\n", new_frames, sender->window_end, sender->seq_no, sender->message_end);


            sender->seq_no = sender->window_start;  // set next frame in sequence to window start
            sender->last_ack_recv = sender->seq_no; // set last ack received so we don't send old payloads

            fprintf(stderr, "SEQ_NO RESET:%d\n", sender->seq_no);
        }
    }
}

void add_frame(Sender* sender, LLnode** outgoing_frames_head_ptr) {
    struct timeval send_time;
    if (!(sender->msg_sent) && !(sender->awaiting_handshake)) {
        // verify we haven't sent whole message

        // MOVE WINDOW IN HANDLE ACKS

        if(sender->seq_no < sender->window_end && sender->seq_no < sender->message_end) {
            gettimeofday(&send_time, NULL);
            sender->SendQ[(sender->seq_no) % SWS].frame = sender->frames[sender->seq_no];

            sender->SendQ[(sender->seq_no) % SWS].timeout.tv_usec = send_time.tv_usec + 90000; // 0.09s timeout 
            sender->SendQ[(sender->seq_no) % SWS].timeout.tv_sec = send_time.tv_sec;
            if (sender->SendQ[(sender->seq_no) % SWS].timeout.tv_usec > 1000000) { // check for overflow
                sender->SendQ[(sender->seq_no) % SWS].timeout.tv_sec++;
                sender->SendQ[(sender->seq_no) % SWS].timeout.tv_usec -= 1000000;
            //fprintf(stderr, "TIMEOUT OVERFLOW\n");
            }
            char* outgoing_charbuf = convert_frame_to_char(sender->SendQ[(sender->seq_no) % SWS].frame);
            // SEND THE FRAME
            ll_append_node(outgoing_frames_head_ptr, outgoing_charbuf);

            sender->awaiting_msg_ack = 1;   // wait for ack of 8 frames

            sender->lfs = sender->seq_no;   // update last frame sent
            // increment seq_no
            sender->seq_no = sender->seq_no + 1;

            fprintf(stderr, "Sent frame seq_no: %d\n", sender->lfs);

            // sleep for .01s
            nanosleep(&sender->ts, NULL); // part of #include <“time.h”>

            if (sender->seq_no == sender->message_end) {
                // last frame sent
                sender->msg_sent = 1;
                fprintf(stderr, "LAST FRAME SENT\n");
            }
        }
    }
}

void resend_frames(Sender* sender, LLnode** outgoing_frames_head_ptr) {
    // last ack_received to seq_no
    uint8_t seq = sender->last_ack_recv;
    fprintf(stderr, "RESENDING: from ");
    while (seq != sender->seq_no) {
        char* outgoing_charbuf = convert_frame_to_char(sender->SendQ[seq % SWS].frame);
        // SEND THE FRAME
        ll_append_node(outgoing_frames_head_ptr, outgoing_charbuf);
        fprintf(stderr, "%d\t", seq);
        seq++; // increment until we get to our position in window
    }
    fprintf(stderr, "done\n");
    // sleep for .01s
}

void handle_timedout_frames(Sender* sender, LLnode** outgoing_frames_head_ptr) {
    // TODO: Handle frames that have timed out
    // check time
    struct timeval curr_time;
    gettimeofday(&curr_time, NULL);
    long timer = timeval_usecdiff(&curr_time, &(sender->timeout));
    if (timer < 0) { // check  to see if timed out
        if (sender->awaiting_msg_ack && sender->awaiting_handshake) { // make sure we're waiting for a SYNACK
            // SYN resend only
            Frame* outgoing_frame = sender->frames[sender->lfs];
            add_syn(sender, outgoing_frames_head_ptr, outgoing_frame);
        }
    }
    if (sender-> awaiting_msg_ack) {
        timer = timeval_usecdiff(&curr_time, sender_get_next_expiring_timeval(sender));
        // fprintf(stderr, "Timer in handle_timedout_frames: %d\n", timer);
        // fprintf(stderr, "Time ms expiring_timeval %ld\n", sender_get_next_expiring_timeval(sender)->tv_usec);
        if ( timer < 0) {
        resend_frames(sender, outgoing_frames_head_ptr);
        }
    }
}

void* run_sender(void* input_sender) {
    struct timespec time_spec;
    struct timeval curr_timeval;
    const int WAIT_SEC_TIME = 0;
    const long WAIT_USEC_TIME = 100000;
    Sender* sender = (Sender*) input_sender;
    LLnode* outgoing_frames_head;
    struct timeval* expiring_timeval;
    long sleep_usec_time, sleep_sec_time;

    // This incomplete sender thread, at a high level, loops as follows:
    // 1. Determine the next time the thread should wake up
    // 2. Grab the mutex protecting the input_cmd/inframe queues
    // 3. Dequeues commands and frames from the input_cmdlist and input_framelist 
    //    respectively. Adds frames to outgoing_frames list as needed.
    // 4. Releases the lock
    // 5. Sends out the frames

    while (1) {
        outgoing_frames_head = NULL;

        // Get the current time
        gettimeofday(&curr_timeval, NULL);

        // time_spec is a data structure used to specify when the thread should wake up.
        time_spec.tv_sec = curr_timeval.tv_sec;
        time_spec.tv_nsec = curr_timeval.tv_usec * 1000;

        // Check for the next event we should handle
        expiring_timeval = sender_get_next_expiring_timeval(sender);

        if (expiring_timeval == NULL) {
            time_spec.tv_sec += WAIT_SEC_TIME;
            time_spec.tv_nsec += WAIT_USEC_TIME * 1000;
        } else {
            // Take the difference between the next event and the current time
            sleep_usec_time = timeval_usecdiff(&curr_timeval, expiring_timeval);

            // Sleep if the difference is positive
            if (sleep_usec_time > 0) {
                sleep_sec_time = sleep_usec_time / 1000000;
                sleep_usec_time = sleep_usec_time % 1000000;
                time_spec.tv_sec += sleep_sec_time;
                time_spec.tv_nsec += sleep_usec_time * 1000;
            }
        }

        // Check to make sure we didn't "overflow" the nanosecond field
        if (time_spec.tv_nsec >= 1000000000) {
            time_spec.tv_sec++;
            time_spec.tv_nsec -= 1000000000;
        }

        //*****************************************************************************************
        // NOTE: Anything that involves dequeing from the input_framelist or input_cmdlist 
        // should go between the mutex lock and unlock, because other threads
        //      CAN/WILL access these structures
        //*****************************************************************************************
        pthread_mutex_lock(&sender->buffer_mutex);

        // Check whether anything has arrived
        int input_cmd_length = ll_get_length(sender->input_cmdlist_head);
        int inframe_queue_length = ll_get_length(sender->input_framelist_head);

        // Nothing (cmd nor incoming frame) has arrived, so do a timed wait on
        // the sender's condition variable (releases lock) A signal on the
        // condition variable will wakeup the thread and reaquire the lock
        if (input_cmd_length == 0 && inframe_queue_length == 0) {
            pthread_cond_timedwait(&sender->buffer_cv, &sender->buffer_mutex,
                                   &time_spec);
        }
        // Implement this
        handle_incoming_acks(sender, &outgoing_frames_head);

        // Implement this
        handle_input_cmds(sender, &outgoing_frames_head);
        add_frame(sender, &outgoing_frames_head);

        sender->active = (ll_get_length(outgoing_frames_head) > 0 || sender->awaiting_msg_ack) ? 1:0;

        // Implement this
        handle_timedout_frames(sender, &outgoing_frames_head);

        pthread_mutex_unlock(&sender->buffer_mutex);

        // DO NOT CHANGE BELOW CODE
        // Send out all the frames
        int ll_outgoing_frame_length = ll_get_length(outgoing_frames_head);

        while (ll_outgoing_frame_length > 0) {
            LLnode* ll_outframe_node = ll_pop_node(&outgoing_frames_head);
            char* char_buf = (char*) ll_outframe_node->value;

            // The following function will free the memory for the char_buf object.
            // The function will convert char_buf to frame and deliver it to 
            // the receiver having receiver->recv_id = frame->dst_id.
            send_msg_to_receiver(char_buf);
            // Free up the ll_outframe_node
            free(ll_outframe_node);

            ll_outgoing_frame_length = ll_get_length(outgoing_frames_head);
        }
    }
    pthread_exit(NULL);
    return 0;
}
