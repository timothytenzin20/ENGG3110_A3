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
	struct data_pkt pkt;

	/*
	 * If this is node #0, start the ball rolling by creating the
	 * token.
	 */
	if (num == 0) {
		send_byte(control, num, 0x01);  // send token
	}
	/*
	 * Loop around processing data, until done.
	 */
	while (not_done) {
		byte = rcv_byte(control, num); // processes incoming bytes
		/*
		 * Handle the byte, based upon current state.
		 */
		switch (rcv_state) {
		case TOKEN_FLAG:
			if (byte == 0x01) {  // token received
				WAIT_SEM(control, CRIT);
				if (control->shared_ptr->node[num].to_send.length > 0) {
					control->snd_state = TOKEN_FLAG;
					sending = 1;
					send_pkt(control, num);
				} else {
					send_byte(control, num, byte);  // Forward token
				}
				SIGNAL_SEM(control, CRIT);
			} else {
				pkt.token_flag = byte;
				rcv_state = TO;
			}
			break;

		case TO:
			pkt.to = byte;
			rcv_state = FROM;
			if (!sending) send_byte(control, num, byte);
			break;

		case FROM:
			pkt.from = byte;
			if (pkt.from == num) {
				sending = 0;  // Our packet came back
				send_byte(control, num, 0x01);  // Send token
				SIGNAL_SEM(control, TO_SEND(num));
			} else if (!sending) {
				send_byte(control, num, byte);
			}
			rcv_state = LEN;
			break;

		case LEN:
			len = byte;
			pkt.length = byte;
			if (!sending) send_byte(control, num, byte);
			if (len == 0) {
				rcv_state = TOKEN_FLAG;
			} else {
				rcv_state = DATA;
			}
			break;

		case DATA:
			if (!sending) send_byte(control, num, byte);
			len--;
			if (len == 0) {
				rcv_state = TOKEN_FLAG;
				if (pkt.to == num) {
					WAIT_SEM(control, CRIT);
					control->shared_ptr->node[num].received++;
					SIGNAL_SEM(control, CRIT);
				}
			}
			break;
		}

		// check termination condtions
		WAIT_SEM(control, CRIT);
		not_done = !control->shared_ptr->node[num].terminate;
		SIGNAL_SEM(control, CRIT);
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
	static int sndpos = 0;
    static int sndlen;

	switch (control->snd_state) {
	case TOKEN_FLAG:
		send_byte(control, num, 0x00);  // data packet
		control->snd_state = TO;
		break;

	case TO:
		send_byte(control, num, control->shared_ptr->node[num].to_send.to);
		control->snd_state = FROM;
		break;

	case FROM:
		send_byte(control, num, control->shared_ptr->node[num].to_send.from);
		control->snd_state = LEN;
		break;

	case LEN:
		sndlen = control->shared_ptr->node[num].to_send.length;
		sndpos = 0;
		send_byte(control, num, sndlen);
		control->snd_state = DATA;
		break;

	case DATA:
		send_byte(control, num, control->shared_ptr->node[num].to_send.data[sndpos++]);
		if (sndpos >= sndlen) {
			control->snd_state = DONE;
			control->shared_ptr->node[num].sent++;
			control->shared_ptr->node[num].to_send.length = 0;  // clear packet
			SIGNAL_SEM(control, TO_SEND(num));  // signal to accept another packet
		}
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
	int next = (num + 1) % N_NODES;
	
	WAIT_SEM(control, EMPTY(next));
	control->shared_ptr->node[next].data_xfer = byte;
	SIGNAL_SEM(control, FILLED(next));
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

	WAIT_SEM(control, FILLED(num));
	byte = control->shared_ptr->node[num].data_xfer;
	SIGNAL_SEM(control, EMPTY(num));
	
	return byte;
}

