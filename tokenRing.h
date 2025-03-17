#ifndef __TOKEN_CONTROL_HEADER__
#define __TOKEN_CONTROL_HEADER__
#include <pthread.h>
#include <semaphore.h>
/*
 * Define any handy constants and structures.
 * Also define the functions.
 */
#define	MAX_DATA	250
#define	TOKEN_FLAG	1
#define	TO		2
#define	FROM		3
#define	LEN		4
#define	DATA		5
#define	DONE		6


#define	N_NODES		7


struct data_pkt {
	char		token_flag;	/* '1' for token, '0' for data	*/
	char		to;		/* Destination node #		*/
	char		from;		/* Source node #		*/
	unsigned char	length;		/* Data length 1<->MAX_DATA	*/
	char		data[MAX_DATA];	/* Up to MAX_DATA bytes of data	*/
};

/*
 * The shared memory region includes a byte used to transfer data between
 * the nodes, a packet structure for each node with a packet to send and
 * sent/received counts for the nodes.
 */
struct node_data {
	unsigned char	data_xfer;
	struct data_pkt	to_send;
	int		sent;
	int		received;
	int		terminate;
};

struct shared_data {
	struct node_data node[N_NODES];
    volatile int cleanup_in_progress;  
};

/*
 * Assign a number/name to each semaphore.
 * Semaphores are used to co-ordinate access to the data_xfer and
 * to_send shared data structures and also to indicate when data transfers
 * occur between nodes.
 * Macros with the node # as argument are used to access the sets of
 * semaphores.
 */
#define	EMPTY0		0
#define	FILLED0		(N_NODES)
#define	TO_SEND0	(FILLED0 + N_NODES)
#define	CRIT		(TO_SEND0 + N_NODES)
#define	NUM_SEM		(CRIT + 1)

#define	EMPTY(n)	(EMPTY0 + (n))
#define	FILLED(n)	(FILLED0 + (n))
#define	TO_SEND(n)	(TO_SEND0 + (n))

typedef struct TokenRingData {
    sem_t *sems;  
    int snd_state;
    struct shared_data *shared_ptr;  
    pthread_t threads[N_NODES];
    int node_numbers[N_NODES];
    struct token_args *thread_args;
    pthread_mutex_t mutex;  
    volatile int termination_flag;  
} TokenRingData;

struct token_args {
    struct TokenRingData *control;
    int node_num;
};

/** prototypes */
void panic(const char *fmt, ...);

struct TokenRingData *setupSystem();
int runSimulation(struct TokenRingData *simulationData, int numPackets);
int cleanupSystem(struct TokenRingData *simulationData);

unsigned char rcv_byte(struct TokenRingData *control, int num);
void send_byte(struct TokenRingData *control, int num, unsigned byte);
void send_pkt(struct TokenRingData *control, int num);
void *token_node(void *arg);

#endif /* __TOKEN_CONTROL_HEADER__ */