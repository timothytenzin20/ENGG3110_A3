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
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>

#include "tokenRing.h"

/*
 * This function is the body of a child process emulating a node.
 */
void *token_node(void *arg) {
    struct TokenRingData *control = ((struct token_args *)arg)->control;
    int num = ((struct token_args *)arg)->node_num;
    
    // state tracking variables
    int rcv_state = TOKEN_FLAG, not_done = 1, sending = 0, len;
    unsigned char byte;
    // node role flags
    char producer, consumer;

    /*
     * If this is node #0, start the ball rolling by creating the
     * token.
     */
    if (num == 0) {
        send_byte(control, num, '0');  
#ifdef DEBUG
    fprintf(stderr, "YUH FIRST TOKEN @ THE NODE #%d.\n", num);
#endif
    }
    /*
     * Loop around processing data, until done.
     */
    while (not_done) {
        byte = rcv_byte(control, num); // get byte from previous node
#ifdef DEBUG
        fprintf(stderr, "@ Node %d: Received byte 0x%02X in state %d\n", num, byte, rcv_state);
#endif
        /*
         * Handle the byte, based upon current state.
         */
        switch (rcv_state) {
        case TOKEN_FLAG:
            // check if node can send data
            if (sem_wait(&control->sems[CRIT]) < 0) {
                panic("Wait sem failed errno=%d\n", errno);
            }
#ifdef DEBUG
            fprintf(stderr, "@ Node %d: Token check - current token_flag=%c\n", 
                    num, control->shared_ptr->node[num].to_send.token_flag);
#endif
            if (byte == '0'){
                if (control->shared_ptr->node[num].to_send.token_flag == '0') {
                    producer = 1;
                    consumer = 0;
                } 
                else {
                    producer = 0;
                    consumer = 1;
                }
            }

            if (sem_post(&control->sems[CRIT]) < 0) {
                panic("Signal sem failed errno=%d\n", errno);
            }
            
            if (byte == '0') {
                if (producer == 1 && consumer == 0) {
#ifdef DEBUG
                fprintf(stderr, "@ Node %d: Starting to send packet\n", num);
#endif
                    control->snd_state = TOKEN_FLAG;
                    send_pkt(control, num);
                    rcv_state = TO;
                }
                else {
                    if (sem_wait(&control->sems[CRIT]) < 0) {
                        panic("Wait sem failed errno=%d\n", errno);
                    }

                    if (control->shared_ptr->node[num].terminate == 1) {
                        printf("KILLING MY SON/CHILD: %d\n", num);
                        not_done = 0;
                    }
                    if (sem_post(&control->sems[CRIT]) < 0) {
                        panic("Signal sem failed errno=%d\n", errno);
                    }
                    send_byte(control, num, byte);
                    rcv_state = TOKEN_FLAG;
                }
            } 
            else {
                send_byte(control, num, byte);
                rcv_state = TO;
            }
            break;

        case TO:
            // handle destination address
            rcv_state = FROM;
            if (producer == 1 && consumer == 0) {
                send_pkt(control, num);
            } 
            else {
                send_byte(control, num, byte);
            }
            break;

        case FROM:
            // handle source address
            rcv_state = LEN;
            if (producer == 1 && consumer == 0) {
                send_pkt(control, num);
            } 
            else {
                send_byte(control, num, byte);
            }
            break;

        case LEN:
            // process packet length and prepare for data
            if (producer == 1 && consumer == 0) {
                send_pkt(control, num);
                if (sem_wait(&control->sems[CRIT]) < 0) {
                    panic("Wait sem failed errno=%d\n", errno);
                }
                len = control->shared_ptr->node[num].to_send.length;
                if (sem_post(&control->sems[CRIT]) < 0) {
                    panic("Signal sem failed errno=%d\n", errno);
                }
            }
            else {
                send_byte(control, num, byte);
                len = (int) byte;
            }
            sending = 0;
            if (len > 0) {
                rcv_state = DATA;
            }
            else {
                rcv_state = TOKEN_FLAG;
            }
            break;

        case DATA:
            // transfer packet data bytes
#ifdef DEBUG
            fprintf(stderr, "@ Node %d: Processing DATA, sending=%d, len=%d\n", 
                    num, sending, len);
#endif
            if (sending < (len-1)) {
                if (producer == 1 && consumer == 0) {
                    send_pkt(control, num);
                }
                else {
                    send_byte(control, num, byte);
                }    
                rcv_state = DATA;
            }
            else {
                if (producer == 1 && consumer == 0) {
                    send_pkt(control, num);
                }
                else {
                    send_byte(control, num, byte);
                }
                producer = 0;
                consumer = 1;
                rcv_state = TOKEN_FLAG;
            }
            sending++;
            break;
        };
    }
#ifdef DEBUG
    fprintf(stderr, "@ Node %d: Terminating\n", num);
#endif
    return NULL;
}

/*
 * This function sends a data packet followed by the token, one byte each
 * time it is called.
 */
void
send_pkt(control, num)
    struct TokenRingData *control;
    int num;
{
    // packet sending state variables
    static int sndpos, sndlen, node_index;  

    switch (control->snd_state) {
    case TOKEN_FLAG:
        // start packet transmission with token
#ifdef DEBUG
        fprintf(stderr, "@ Node %d: Sending packet header\n", num);
#endif
        if (sem_wait(&control->sems[CRIT]) < 0) {
            panic("Wait sem failed errno=%d\n", errno);
        }
        control->shared_ptr->node[num].sent++;
        node_index = (int) control->shared_ptr->node[num].to_send.to; // get destination node
        control->shared_ptr->node[node_index].received++; 
        control->shared_ptr->node[num].to_send.token_flag = '1';
        if (sem_post(&control->sems[CRIT]) < 0) {
            panic("Signal sem failed errno=%d\n", errno);
        }
        
        send_byte(control, num, control->shared_ptr->node[num].to_send.token_flag);
        control->snd_state = TO;
        sndpos = 0;
        
        if (sem_wait(&control->sems[CRIT]) < 0) {
            panic("Wait sem failed errno=%d\n", errno);
        }
        sndlen = control->shared_ptr->node[num].to_send.length;
        if (sem_post(&control->sems[CRIT]) < 0) {
            panic("Signal sem failed errno=%d\n", errno);
        }
        break;

    case TO:
        // send destination node id
        send_byte(control, num, control->shared_ptr->node[num].to_send.to);
        control->snd_state = FROM;
        break;

    case FROM:
        // send source node id
        send_byte(control, num, control->shared_ptr->node[num].to_send.from);
        control->snd_state = LEN;
        break;

    case LEN:
        // send packet length
        send_byte(control, num, control->shared_ptr->node[num].to_send.length);
        control->snd_state = DATA;
        break;

    case DATA:
        // transmit packet data bytes
#ifdef DEBUG
        fprintf(stderr, "@ Node %d: Sending data byte %d of %d\n", 
                num, sndpos, sndlen);
#endif
        if (sndpos < (sndlen-1)) {
            send_byte(control, num, control->shared_ptr->node[num].to_send.data[sndpos]);
            sndpos++;
            control->snd_state = DATA;
            break;
        } else {
            if (sem_wait(&control->sems[CRIT]) < 0) {
                panic("Wait sem failed errno=%d\n", errno);
            }
            control->snd_state = DONE;
            if (sem_post(&control->sems[CRIT]) < 0) {
                panic("Signal sem failed errno=%d\n", errno);
            }
        }

    case DONE:
        // complete transmission and release token
#ifdef DEBUG
        fprintf(stderr, "@ Node %d: Packet transmission complete\n", num);
#endif
        if (sem_wait(&control->sems[CRIT]) < 0) {
            panic("Wait sem failed errno=%d\n", errno);
        }
        printf("\ncontents at node: %d is: ", num);
        for (node_index = 0; node_index < control->shared_ptr->node[num].to_send.length; node_index++) { 
            printf("%c", control->shared_ptr->node[num].to_send.data[node_index]);
        }
        printf("\n\n");
        control->shared_ptr->node[num].to_send.token_flag = '1';
        
        control->snd_state = TOKEN_FLAG;
        send_byte(control, num, '0');
        if (sem_post(&control->sems[TO_SEND(num)]) < 0) {
            panic("Signal sem failed errno=%d\n", errno);
        }
        if (sem_post(&control->sems[CRIT]) < 0) {
            panic("Signal sem failed errno=%d\n", errno);
        }
        break;
    };
}

/*
 * Send a byte to the next node on the ring.
 */
void
send_byte(control, num, byte)
    struct TokenRingData *control;
    int num;
    unsigned byte;
{
    int next = (num + 1) % N_NODES;

#ifdef DEBUG
    fprintf(stderr, "@ Node %d: Sending byte 0x%02X to node %d\n", num, byte, next);
#endif

    if (sem_wait(&control->sems[EMPTY(next)]) < 0) {
        panic("Wait sem failed errno=%d\n", errno);
    }

    // Critical section
    control->shared_ptr->node[next].data_xfer = byte;
    
    if (sem_post(&control->sems[FILLED(next)]) < 0) {
        panic("Signal sem failed errno=%d\n", errno);
    }
}

/*
 * Receive a byte for this node.
 */
unsigned char
rcv_byte(control, num)
    struct TokenRingData *control;
    int num;
{
    unsigned char byte;
    int previous = (num + N_NODES - 1) % N_NODES;

    if (sem_wait(&control->sems[FILLED(num)]) < 0) {
        panic("Wait sem failed errno=%d\n", errno);
    }
    
    byte = control->shared_ptr->node[num].data_xfer;

#ifdef DEBUG
    fprintf(stderr, "@ Node %d: Received byte 0x%02X\n", num, byte);
#endif

    if (sem_post(&control->sems[EMPTY(num)]) < 0) {
        panic("Signal sem failed errno=%d\n", errno);
    }
    
    return byte;
}
