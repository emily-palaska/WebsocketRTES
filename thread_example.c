#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

// Function executed by the thread
void *print_message(void *threadid) {
    long tid;
    tid = (long)threadid;
    printf("Thread ID: %ld is running\n", tid);
    pthread_exit(NULL);
}

int main() {
    pthread_t threads[2];
    int rc;
    long t;

    // Create two threads
    for(t = 0; t < 2; t++) {
        printf("Creating thread %ld\n", t);
        rc = pthread_create(&threads[t], NULL, print_message, (void *)t);
        
        if (rc) {
            printf("Error: unable to create thread, %d\n", rc);
            exit(-1);
        }
    }

    // Join the threads to ensure they complete before program exits
    for(t = 0; t < 2; t++) {
        pthread_join(threads[t], NULL);
    }

    return 0;
}

