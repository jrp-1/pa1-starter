#include "common.h"
#include "communicate.h"
#include "input.h"
#include "receiver.h"
#include "sender.h"
#include "util.h"

#include <assert.h>
#include <math.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

int main(int argc, char* argv[]) {
    pthread_t stdin_thread;
    pthread_t* sender_threads;
    pthread_t* receiver_threads;
    int i;
    unsigned char print_usage = 0;

    // DO NOT CHANGE THIS
    // Prepare the glb_sysconfig object
    glb_sysconfig.link_bandwidth = 0;
    glb_sysconfig.corrupt_prob = 0;
    glb_sysconfig.automated = 0;
    memset(glb_sysconfig.automated_file, 0, AUTOMATED_FILENAME);

    // DO NOT CHANGE THIS
    // Prepare other variables and seed the psuedo random number generator
    glb_receivers_array_length = -1;
    glb_senders_array_length = -1;
    srand(time(NULL));

    // Parse out the command line arguments
    for (i = 1; i < argc;) {
        if (strcmp(argv[i], "-s") == 0) {
            sscanf(argv[i + 1], "%d", &glb_senders_array_length);
            i += 2;
        }

        else if (strcmp(argv[i], "-r") == 0) {
            sscanf(argv[i + 1], "%d", &glb_receivers_array_length);
            i += 2;
        } else if (strcmp(argv[i], "-b") == 0) {
            sscanf(argv[i + 1], "%hd", &glb_sysconfig.link_bandwidth);
            i += 2;
        } else if (strcmp(argv[i], "-c") == 0) {
            sscanf(argv[i + 1], "%f", &glb_sysconfig.corrupt_prob);
            i += 2;
        } else if (strcmp(argv[i], "-a") == 0) {
            int filename_len = strlen(argv[i + 1]);
            if (filename_len < AUTOMATED_FILENAME) {
                glb_sysconfig.automated = 1;
                strcpy(glb_sysconfig.automated_file, argv[i + 1]);
            }
            i += 2;
        } else if (strcmp(argv[i], "-h") == 0) {
            print_usage = 1;
            i++;
        } else {
            i++;
        }
    }

    // Spot check the input variables
    if (glb_senders_array_length <= 0 || glb_receivers_array_length <= 0 ||
        (glb_sysconfig.corrupt_prob < 0 || glb_sysconfig.corrupt_prob > 1) ||
        print_usage) {
        fprintf(
            stderr,
            "USAGE: %s \n   -r int [# of receivers] \n   -s int [# of senders] "
            "\n   -c float [0 <= corruption prob <= 1] \n   -b uint16_t [no. of frames on each link]\n",
            argv[0]);
        exit(1);
    }

    // DO NOT CHANGE THIS
    // Init the pthreads data structure
    sender_threads = malloc(sizeof(pthread_t) * glb_senders_array_length);
    assert(sender_threads);
    receiver_threads = malloc(sizeof(pthread_t) * glb_receivers_array_length);
    assert(receiver_threads);

    // Init the global senders array
    glb_senders_array = malloc(glb_senders_array_length * sizeof(Sender));
    assert(glb_senders_array);
    glb_receivers_array = malloc(glb_receivers_array_length * sizeof(Receiver));
    assert(glb_receivers_array);

    fprintf(stderr, "Messages will be corrupted with probability=%f\n",
            glb_sysconfig.corrupt_prob);
    fprintf(stderr, "Available sender id(s):\n");

    // Init sender objects, assign ids
    // NOTE: Do whatever initialization you want here or inside the init_sender
    // function
    for (i = 0; i < glb_senders_array_length; i++) {
        init_sender(&glb_senders_array[i], i);
        fprintf(stderr, "   send_id=%d\n", i);
    }

    // Init receiver objects, assign ids
    // NOTE: Do whatever initialization you want here or inside the
    // init_receiver function
    fprintf(stderr, "Available receiver id(s):\n");
    for (i = 0; i < glb_receivers_array_length; i++) {
        init_receiver(&glb_receivers_array[i], i);
        fprintf(stderr, "   recv_id=%d\n", i);
    }

    // DO NOT CHANGE THIS
    // Create the standard input thread
    int rc = pthread_create(&stdin_thread, NULL, run_stdinthread, (void*) 0);
    if (rc) {
        fprintf(stderr, "ERROR; return code from pthread_create() is %d\n", rc);
        exit(-1);
    }

    // Spawn sender threads
    for (i = 0; i < glb_senders_array_length; i++) {
        rc = pthread_create(sender_threads + i, NULL, run_sender,
                            (void*) &glb_senders_array[i]);
        if (rc) {
            fprintf(stderr, "ERROR; return code from pthread_create() is %d\n",
                    rc);
            exit(-1);
        }
    }

    // Spawn receiver threads
    for (i = 0; i < glb_receivers_array_length; i++) {
        rc = pthread_create(receiver_threads + i, NULL, run_receiver,
                            (void*) &glb_receivers_array[i]);
        if (rc) {
            fprintf(stderr, "ERROR; return code from pthread_create() is %d\n",
                    rc);
            exit(-1);
        }
    }
    pthread_join(stdin_thread, NULL);

// Graceful exit
    int active = 1;
    int lena = 0;
    int lenb = 0;
    int sender_active = 1;
    int receiver_active = 1;

    while(active == 1){
        active = 0;
        for(i = 0; i< glb_senders_array_length; i++){
            Sender* a = &glb_senders_array[i];
            pthread_mutex_lock(&a->buffer_mutex);
            lena = ll_get_length(a->input_cmdlist_head);
            sender_active = a->active;
            pthread_mutex_unlock(&a->buffer_mutex);
            if(lena > 0 || sender_active){
                active = 1;
            }
        }
        for(i = 0; i< glb_senders_array_length; i++){
            Sender* a = &glb_senders_array[i];
            pthread_mutex_lock(&a->buffer_mutex);
            lena = ll_get_length(a->input_framelist_head);
            sender_active = a->active;
            pthread_mutex_unlock(&a->buffer_mutex);
            if(lena > 0  || sender_active){
                active = 1;
            }
        }
        
        for(i=0; i< glb_receivers_array_length; i++){
            Receiver* b = &glb_receivers_array[i];
            pthread_mutex_lock(&b->buffer_mutex);
            lenb = ll_get_length(b->input_framelist_head);
            receiver_active = b->active;
            pthread_mutex_unlock(&b->buffer_mutex);
            if(lenb > 0 || receiver_active){
                active = 1;
            }
        }
        if(active == 0){
            sleep(2);
            for(i = 0; i< glb_senders_array_length; i++){
                Sender* a = &glb_senders_array[i];
                pthread_mutex_lock(&a->buffer_mutex);
                lena = ll_get_length(a->input_cmdlist_head);
                sender_active = a->active;
                pthread_mutex_unlock(&a->buffer_mutex);
                if(lena > 0 || sender_active){
                    active = 1;
                }
            }
            for(i = 0; i< glb_senders_array_length; i++){
                Sender* a = &glb_senders_array[i];
                pthread_mutex_lock(&a->buffer_mutex);
                lena = ll_get_length(a->input_framelist_head);
                sender_active = a->active;
                pthread_mutex_unlock(&a->buffer_mutex);
                if(lena > 0 || sender_active){
                    active = 1;
                }
            }
            
            for(i=0; i< glb_receivers_array_length; i++){
                Receiver* b = &glb_receivers_array[i];
                pthread_mutex_lock(&b->buffer_mutex);
                lenb = ll_get_length(b->input_framelist_head);
                receiver_active = b->active;
                pthread_mutex_unlock(&b->buffer_mutex);
                if(lenb > 0 || receiver_active){
                    active = 1;
                }
            }
        }
    }
    
    for (i = 0; i < glb_senders_array_length; i++) {
        pthread_cancel(sender_threads[i]);
        pthread_join(sender_threads[i], NULL);
    }

    for (i = 0; i < glb_receivers_array_length; i++) {
        pthread_cancel(receiver_threads[i]);
        pthread_join(receiver_threads[i], NULL);
    }

    free(sender_threads);
    free(receiver_threads);
    free(glb_senders_array);
    free(glb_receivers_array);

    return 0;
}
