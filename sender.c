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
    // TODO: You should fill in this function as necessary
    sender->lfs = malloc(sizeof(Frame)); //last frame sent
    gettimeofday(&sender->time_sent, NULL);
}

struct timeval* sender_get_next_expiring_timeval(Sender* sender) {
    // TODO: You should fill in this function so that it returns the 
    // timeval when next timeout should occur

    return &sender->timeout;

}

void set_timeout(Sender* sender) {
    // add .09s to time_sent
    sender->timeout.tv_sec = sender->time_sent.tv_sec;
    sender->timeout.tv_usec = sender->time_sent.tv_usec + 90000;
    if (sender->timeout.tv_usec > 1000000) { // check for overlow
        sender->timeout.tv_sec--;
        sender->timeout.tv_usec -= 1000000;
    }
    printf("SET TIMEOUT\n");

}

void rebuild_frame(Sender* sender, Frame* outgoing_frame) {
    //rebuild for last frame sent
    assert(outgoing_frame);
    memcpy(outgoing_frame->data, sender->lfs->data, FRAME_PAYLOAD_SIZE);
    outgoing_frame->src_id = sender->lfs->src_id;
    outgoing_frame->dst_id = sender->lfs->dst_id;
    outgoing_frame->crc8 = sender->lfs->crc8;
}

void build_frame(Sender* sender, LLnode** outgoing_frames_head_ptr, Frame* outgoing_frame, char* message, uint8_t src, uint8_t dst) {
    // builds a frame
    assert(outgoing_frame);
    memcpy(outgoing_frame->data, message, FRAME_PAYLOAD_SIZE);
    outgoing_frame->src_id = src;
    outgoing_frame->dst_id = dst;

    // Add CRC
    char* outgoing_charbuf = convert_frame_to_char(outgoing_frame);
    outgoing_frame->crc8 = compute_crc8(outgoing_charbuf);
    outgoing_charbuf = convert_frame_to_char(outgoing_frame);
    // At this point, we don't need the outgoing_cmd
    free(message);
}

void add_frame(Sender* sender, LLnode** outgoing_frames_head_ptr, Frame* outgoing_frame) {
    // set last frame sent
    sender->lfs->src_id = outgoing_frame->src_id;
    sender->lfs->dst_id = outgoing_frame->dst_id;
    memcpy(sender->lfs->data, outgoing_frame->data, FRAME_PAYLOAD_SIZE);
    sender->lfs->crc8 = outgoing_frame->crc8;

    // set time frame sent
    gettimeofday(&sender->time_sent, NULL);
    // set timeout
    set_timeout(sender);

    // adds frame to queue
    // TODO: crc, ack
    // set we are waiting for ack
    sender->awaiting_msg_ack = 1;

    char* outgoing_charbuf = convert_frame_to_char(outgoing_frame);
    ll_append_node(outgoing_frames_head_ptr, outgoing_charbuf);
    free(outgoing_frame);
}

void handle_incoming_acks(Sender* sender, LLnode** outgoing_frames_head_ptr) {
    // TODO: Suggested steps for handling incoming ACKs
    //    1) Dequeue the ACK from the sender->input_framelist_head
    //    2) Convert the incoming frame from char* to Frame* data type
    //    3) Implement logic as per stop and wait ARQ to track ACK for what frame is expected,
    //       and what to do when ACK for expected frame is received

    // 
    if (sender->awaiting_msg_ack) {
        int incoming_frames_length = ll_get_length(sender->input_framelist_head);
        while (incoming_frames_length > 0) {
            // Pop a node off the front of the link list and update the count
            LLnode* ll_inmsg_node = ll_pop_node(&sender->input_framelist_head);
            incoming_frames_length = ll_get_length(sender->input_framelist_head);

            char* raw_char_buf = ll_inmsg_node->value;
            Frame* inframe = convert_char_to_frame(raw_char_buf);

            if(!strcmp(inframe->data, "ACK")) { // 0 if equal
                // check for ACK
                printf("ACK RECV_%d>:[%s]\n", sender->send_id, inframe->data);
                sender->awaiting_msg_ack = 0;
            }

            // Free raw_char_buf
            free(raw_char_buf);
        }

        // printf("<RECV_%d>:[%s]\n", receiver->recv_id, inframe->data);
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
    while (input_cmd_length > 0) {
        // Pop a node off and update the input_cmd_length
        LLnode* ll_input_cmd_node = ll_pop_node(&sender->input_cmdlist_head);
        input_cmd_length = ll_get_length(sender->input_cmdlist_head);

        // Cast to Cmd type and free up the memory for the node
        Cmd* outgoing_cmd = (Cmd*) ll_input_cmd_node->value;
        free(ll_input_cmd_node);

        int msg_length = strlen(outgoing_cmd->message) + 1;
        if (msg_length > FRAME_PAYLOAD_SIZE) {
            // Do something about messages that exceed the frame size
            printf(
                // split message intoframes
                "<SEND_%d>: sending messages of length greater than %d is not "
                "implemented\n",
                sender->send_id, MAX_FRAME_SIZE);
        } else {
            Frame* outgoing_frame = malloc(sizeof(Frame));

            build_frame(sender, outgoing_frames_head_ptr, outgoing_frame, outgoing_cmd->message, outgoing_cmd->src_id, outgoing_cmd->dst_id);
            // Frame* outgoing_frame = malloc(sizeof(Frame));
            // assert(outgoing_frame);
            // strcpy(outgoing_frame->data, outgoing_cmd->message);
            // outgoing_frame->src_id = outgoing_cmd->src_id;
            // outgoing_frame->dst_id = outgoing_cmd->dst_id;
            // // At this point, we don't need the outgoing_cmd
            // free(outgoing_cmd->message);
            free(outgoing_cmd);

            add_frame(sender, outgoing_frames_head_ptr, outgoing_frame);
            // Convert the message to the outgoing_charbuf
            // char* outgoing_charbuf = convert_frame_to_char(outgoing_frame);
            // ll_append_node(outgoing_frames_head_ptr, outgoing_charbuf);
            // free(outgoing_frame);
        }
    }
}

void handle_timedout_frames(Sender* sender, LLnode** outgoing_frames_head_ptr) {
    // TODO: Handle frames that have timed out
    // check time
    struct timeval curr_time;
    gettimeofday(&curr_time, NULL);
    long timer = timeval_usecdiff(&curr_time, &(sender->timeout));
    if (timer < 0 && sender->awaiting_msg_ack) {
        Frame* outgoing_frame = malloc(sizeof(Frame));
        rebuild_frame(sender, outgoing_frame);
        printf("attempting resend\n");
        printf("time since last send in usec:%d\n", curr_time.tv_usec - sender->time_sent.tv_usec);
        add_frame(sender, outgoing_frames_head_ptr, outgoing_frame);
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
