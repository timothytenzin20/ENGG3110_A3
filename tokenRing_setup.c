/* References
• author: Andrew Hamilton-Wright
• name: semaphore_process_demo.c
• year: 2001
• source: Course Directory

• author: Andrew Hamilton-Wright
• name: semaphore_process2
• year: 2001
• source: Course Directory

• author: geeksforgeeks
• name: How to use POSIX semaphores in C language
• year: 2025
• source: https://www.geeksforgeeks.org/use-posix-semaphores-c/

• author: stackoverflow
• name: How to create semaphores in shared memory in C?
• year: 2023
• source: https://stackoverflow.com/questions/32153151/how-to-create-semaphores-in-shared-memory-in-c

• author: GitHub - kernohad
• name: Token-ring-communication-simulation
• year: 2018
• source: https://github.com/kernohad/Token-ring-communication-simulation/blob/master/simulation.c
*/
/*
*	CIS3110: Assignment 2
*	Submitted To: Andrew-Hamilton Wright
* 	Submitted By: Timothy Khan 1239165
*	Date: February 24, 2025
*/
/*
 * The program simulates a Token Ring LAN by forking off a process
 * for each LAN node, that communicate via shared memory, instead
 * of network cables. To keep the implementation simple, it jiggles
 * out bytes instead of bits.
 *
 * It keeps a count of packets sent and received for each node.
 */
#include <stdio.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include "tokenRing.h"

/*
 * The main program creates the shared memory region and forks off the
 * processes to emulate the token ring nodes.
 * This process generates packets at random and inserts them in
 * the to_send field of the nodes. When done it waits for each process
 * to be done receiving and then tells them to terminate and waits
 * for them to die and prints out the sent/received counts.
 */
struct TokenRingData *
setupSystem()
{
	register int i;
	struct TokenRingData *control;

	control = (struct TokenRingData *) malloc(sizeof(struct TokenRingData));
	if (!control) {
		fprintf(stderr, "Failed to allocate control structure\n");
		return NULL;
	}

	// allocate semaphore array
	control->sems = malloc(NUM_SEM * sizeof(sem_t));
	if (!control->sems) {
		fprintf(stderr, "Failed to allocate semaphore array\n");
		free(control);
		return NULL;
	}

	// allocate shared data
	control->shared_ptr = (struct shared_data *)malloc(sizeof(struct shared_data));
	if (!control->shared_ptr) {
		fprintf(stderr, "Failed to allocate shared data\n");
		free(control->sems);
		free(control);
		return NULL;
	}

	// allocate thread arguments
	control->thread_args = malloc(N_NODES * sizeof(struct token_args));
	if (!control->thread_args) {
		fprintf(stderr, "Failed to allocate thread arguments\n");
		goto FAIL;
	}

	// initialize semaphores
	for (i = 0; i < NUM_SEM; i++) {
		if (sem_init(&control->sems[i], 0, (i < N_NODES || i >= FILLED0 + N_NODES) ? 1 : 0) < 0) {
			fprintf(stderr, "Failed to initialize semaphore %d\n", i);
			goto FAIL;
		}
	}

	// initialize node 
	for (i = 0; i < N_NODES; i++) {
		control->shared_ptr->node[i].sent = 0;
		control->shared_ptr->node[i].received = 0;
		control->shared_ptr->node[i].terminate = 0;
		control->shared_ptr->node[i].to_send.length = 0;
		control->shared_ptr->node[i].data_xfer = 0;
		control->shared_ptr->node[i].to_send.token_flag = '1';
		control->node_numbers[i] = i; 
	}

	// initialize thread 
	for (i = 0; i < N_NODES; i++) {
		control->thread_args[i].control = control;
		control->thread_args[i].node_num = i;
	}

	srandom(time(0));
	return control;

FAIL:
	if (control) {
		if (control->thread_args) free(control->thread_args);
		if (control->sems) {
			// destroy initialized semaphores
			for (int j = 0; j < i; j++) {
				sem_destroy(&control->sems[j]);
			}
			free(control->sems);
		}
		if (control->shared_ptr) free(control->shared_ptr);
		free(control);
	}
	return NULL;
}

int
runSimulation(control, numberOfPackets)
	struct TokenRingData *control;
	int numberOfPackets;
{
	int i;

	/*
	 * Create threads that simulate the nodes.
	 * Store thread IDs and node numbers for each thread
	 */
	for (i = 0; i < N_NODES; i++) {
		control->node_numbers[i] = i;  
		if (pthread_create(&control->threads[i], NULL, 
			token_node, &control->thread_args[i]) != 0) {
			panic("Thread creation failed for node %d\n", i);
		}
#ifdef DEBUG
		fprintf(stderr, "Created thread for node %d\n", i);
#endif
	}

	/*
	 * Loop around generating packets at random.
	 */
	for (i = 0; i < numberOfPackets; i++) {
#ifdef DEBUG
		fprintf(stderr, "Main in generate packets\n");
#endif
		int num = random() % N_NODES;

		if (sem_wait(&control->sems[TO_SEND(num)]) < 0) {
			panic("Wait sem failed errno=%d\n", errno);
		}
		if (sem_wait(&control->sems[CRIT]) < 0) {
			panic("Wait sem failed errno=%d\n", errno);
		}

		if (control->shared_ptr->node[num].to_send.length > 0) {
			sem_post(&control->sems[CRIT]);  
			panic("to_send filled\n");
		}

		control->shared_ptr->node[num].to_send.token_flag = '0';

		int to;
		do {
			to = random() % N_NODES;
		} while (to == num);

		control->shared_ptr->node[num].to_send.to = (char)to;
		control->shared_ptr->node[num].to_send.from = (char)num;
		control->shared_ptr->node[num].to_send.length = (random() % MAX_DATA) + 1;
		
		// initialize packet data with test content
		for (int j = 0; j < control->shared_ptr->node[num].to_send.length; j++) {
			control->shared_ptr->node[num].to_send.data[j] = 'A' + (j % 26);
		}

		if (sem_post(&control->sems[CRIT]) < 0) {
			panic("Signal sem failed errno=%d\n", errno);
		}
		if (sem_post(&control->sems[TO_SEND(num)]) < 0) {
			panic("Signal sem failed errno=%d\n", errno);
		}
	}

#ifdef DEBUG
    fprintf(stderr, "Setting termination flags for all nodes\n");
#endif
    if (sem_wait(&control->sems[CRIT]) < 0) {
        panic("Wait sem failed errno=%d\n", errno);
    }
    
    control->shared_ptr->cleanup_in_progress = 1;
    for (i = 0; i < N_NODES; i++) {
        control->shared_ptr->node[i].terminate = 1;
#ifdef DEBUG
        fprintf(stderr, "Set termination flag for node %d\n", i);
#endif
        
        // release blocked semaphores
        sem_post(&control->sems[FILLED(i)]);
        sem_post(&control->sems[EMPTY(i)]);
        sem_post(&control->sems[TO_SEND(i)]);
    }
    
    if (sem_post(&control->sems[CRIT]) < 0) {
        panic("Signal sem failed errno=%d\n", errno);
    }
#ifdef DEBUG
    fprintf(stderr, "Released CRIT after setting termination flags\n");
#endif

    // ensure nodes can check termination
    send_byte(control, 0, '0');
#ifdef DEBUG
    fprintf(stderr, "Sent final token to initiate termination\n");
#endif

#ifdef DEBUG
    fprintf(stderr, "Waiting for threads to terminate...\n");
#endif
    
    // wait for threads with timeout
    for (i = 0; i < N_NODES; i++) {
        if (pthread_join(control->threads[i], NULL) != 0) {
#ifdef DEBUG
            fprintf(stderr, "Warning: Thread %d join failed\n", i);
#endif
        } else {
#ifdef DEBUG
            fprintf(stderr, "Thread %d joined successfully\n", i);
#endif
        }
    }

    return 1;
}

int
cleanupSystem(control)
    struct TokenRingData *control;
{
    int i;

    // print results
    for (i = 0; i < N_NODES; i++) {
#ifdef DEBUG
        fprintf(stderr, "Node %d: sent=%d received=%d\n", i,
            control->shared_ptr->node[i].sent,
            control->shared_ptr->node[i].received);
#endif
    }

    fflush(stdout);
    fflush(stderr);

    // semaphores and memory
    for (i = 0; i < NUM_SEM; i++) {
        sem_destroy(&control->sems[i]);
    }

    free(control->thread_args);
    free(control->sems);
    free(control->shared_ptr);
    free(control);

    return 1;
}

/*
 * Panic: Just print out the message and exit.
 */
void
panic(const char *fmt, ...)
{
    	va_list vargs;

	va_start(vargs, fmt);
	(void) vfprintf(stdout, fmt, vargs);
	va_end(vargs);

	exit(5);
}
