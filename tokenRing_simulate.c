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
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include "tokenRing.h"

/*
 * This function is the body of a child process emulating a node.
 */
void
token_node(control, num)
	struct TokenRingData *control;
	int num;
{
	int rcv_state = TOKEN_FLAG, not_done = 1, sending = 0, len;
	unsigned char byte;

	/*
	 * If this is node #0, start the ball rolling by creating the
	 * token.
	 */
	...

	/*
	 * Loop around processing data, until done.
	 */
	while (not_done) {
		...

		/*
		 * Handle the byte, based upon current state.
		 */
		switch (rcv_state) {
		case TOKEN_FLAG:
			...
			break;

		case TO:
			...
			break;

		case FROM:
			...
			break;

		case LEN:
			...
			break;

		case DATA:
			...
			break;
		};
	}
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
	static int sndpos, sndlen;

	switch (control->snd_state) {
	case TOKEN_FLAG:
		...
		break;

	case TO:
		...
		break;

	case FROM:
		...
		break;

	case LEN:
		...
		break;

	case DATA:
		...
		break;

	case DONE:
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
	...
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

	...
}

