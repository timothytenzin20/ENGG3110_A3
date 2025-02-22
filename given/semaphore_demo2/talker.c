/*
 * This program is half of a one-way "talk" system -- it is a
 * simple consumer of data from the "blackboard".
 */
#include <stdio.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <string.h>
#include <errno.h>

/**
 * Include the header files we have made
 */
#include "ipc.h"
#include "blackboard.h"

/**
 * Simple "blackboard" example with one writer.
 *
 * To run this program, compile it, and open two windows.
 * Run with an ID number (in both windows) (your student ID
 * will be a good choice) like so:
 *     ./talker 1234567
 *
 * The second program run will accept lines of text and send
 * them to the first program (via a shared memory page)
 * which will then print them.
 *
 * You can enter the string "bye" to quit.
 *
 * For Assignment 2 you can extend this code or use your own.
 */


/**
 * In the "first" program, we are just printing lines out
 * (for now).  Read from the shared page (the "blackboard")
 * to see what has been added.  We use a producer-consumer
 * semaphore setup to control the two processes.
 */
int printOutWhatWeGet(struct blackboard *bboard, int semId)
{
	int keepGoing;
	int nLinesProcessed = 0;


	/**
	 * Now we simply wait for data to arrive, looking
	 * for lines that say "bye"
	 */
	keepGoing = 1;
	while (keepGoing) {
		SEM_WAIT(semId, FULL);

		SEM_WAIT(semId, CRIT);
		/** check to see if we are done */
		if (strncmp(bboard->data[ bboard->nextFull ],
			    	"bye", 3) == 0) {
		    keepGoing = 0;
		}
		printf("BB:[%s]\n", bboard->data[ bboard->nextFull ]); 

		/** increment and wrap if we are at the end */
		bboard->nextFull = (bboard->nextFull + 1) % BBOARD_MAX_LINES;
		SEM_SIGNAL(semId, CRIT);

		/**
		 * Uncomment this and type a bunch of lines.
		 * What happens?  How does this relate to
		 * BBOARD_MAX_LINES in blackboard.h?
		 */
		/* sleep(2); */

		nLinesProcessed++;

		SEM_SIGNAL(semId, EMPTY);
	}

	return nLinesProcessed;
}



/**
 * The producer for the above consumer.  This function
 * takes lines of text typed on the console and transfers
 * them to the companion program, using the shared memory
 * segment and semaphore set.
 */
int sendWhatIsTyped(struct blackboard *bboard, int semId)
{
	char inputBuffer[BBOARD_MAX_LINE_LENGTH];
	int nLinesProcessed = 0;

	/**
	 * Now we simply wait for data to arrive, looking
	 * for lines that say "bye"
	 */
	printf("Type some text. Type \"bye\" to quit.\n");
	do {
	    	
		printf("Line:");
		fflush(stdout);
		/** make prompt appear, even though there is no \n */


		fgets(inputBuffer, BBOARD_MAX_LINE_LENGTH, stdin);

		/** get rid of trailing \n in input buffer */
		inputBuffer[strlen(inputBuffer) - 1] = 0;

		SEM_WAIT(semId, EMPTY);
		SEM_WAIT(semId, CRIT);
		strcpy(bboard->data[ bboard->nextEmpty ], inputBuffer);
		bboard->nextEmpty = (bboard->nextEmpty + 1) % BBOARD_MAX_LINES;
		SEM_SIGNAL(semId, CRIT);

		SEM_SIGNAL(semId, FULL);

		nLinesProcessed++;

	} while (strncmp(inputBuffer, "bye", 3) != 0);

	return nLinesProcessed;
}



/*
 * The main program creates the shared memory region and
 * a semaphore to govern it, then waits for data to
 * show up.
 */
int
main(int argc, char **argv)
{
	int shmId;
	int semId;
	long idLong;
	key_t idKey;
	int sharedMemoryCreator = 0;
	int nLinesProcessed;
	struct blackboard	*bboard;

	if (argc < 2) {
	    fprintf(stderr, "Need ID number for key\n");
	    exit (1);
	}

	if ( sscanf(argv[1], "%ld", &idLong) != 1) {
	    fprintf(stderr, "Cannot parse ID number from '%s'\n",
		    argv[1]);
	    exit (1);
	}

	/**
	 * Convert given ID to a "key_t" -- shift
	 * up to ensure lowest 9 bits are free,
	 */
	idKey = (key_t) (idLong << 9);


	/*
	 * Create the shared memory region, with
	 * exclusion.  This will fail if we are
	 * not the "first one in", so we can then
	 * try attaching to an existing one
	 */
	shmId = shmget(idKey,
			sizeof(struct blackboard),
			IPC_CREAT|IPC_EXCL|0600)	/** rw for owner */;

	if (shmId < 0 && errno == EEXIST) {
	    /**
	     * this means that we are not the first one, so just attach
	     * the existing segment
	     */

	    sharedMemoryCreator = 0;
	    shmId = shmget(idKey,
			sizeof(struct blackboard),
			0);

	} else {
	    /** record that we created the shared memory segment */
	    sharedMemoryCreator = 1;
	    printf("This process is the \"First one In\"\n");
	}


	if (shmId < 0) {
		fprintf(stderr,
			"Can't create shared memory region "
			": errno = %d : %s\n",
			errno, strerror(errno));
		exit (1);
	}
	printf("Successfully created shared memory "
		"blackboard for key %ld\n",
		idLong);

	/*
	 * and map the shared data region into our address space at an
	 * address selected by Linux.
	 */
	bboard = (struct blackboard *)shmat(shmId, (char *)0, 0);
	if (bboard == NULL) {
		fprintf(stderr,
			"Can't map shared blackboard "
			": errno = %d : %s\n",
			errno, strerror(errno));
		exit (2);
	}

	/*
	 * Now, create the semaphores, by creating the semaphore set.
	 * Under Linux, semaphore sets are stored in an area of memory
	 * handled much like a shared region. (A semaphore set is simply
	 * a bunch of semaphores allocated to-gether.)
	 *
	 * We will allocate three semaphores, and use them as
	 * full(0), empty(1) and crit(2)
	 */
	semId = semget(idKey, 3, IPC_CREAT|0600);
	if (semId < 0) {
		fprintf(stderr,
			"Can't create semaphore set "
			": errno = %d : %s\n",
			errno, strerror(errno));
		exit(3);
	}

	/*
	 * and initialize them.
	 * Semaphores are meaningless if they are not initialized.
	 */
	INITIALIZE_SEM(semId, FULL, 0);
	INITIALIZE_SEM(semId, EMPTY, BBOARD_MAX_LINES);
	INITIALIZE_SEM(semId, CRIT, 1);

	/*
	 * And initialize the shared data
	 */
	bboard->nextEmpty = 0;
	bboard->nextFull = 0;

	if (sharedMemoryCreator == 1) {
	    nLinesProcessed = printOutWhatWeGet(bboard, semId);
	} else {
	    nLinesProcessed = sendWhatIsTyped(bboard, semId);
	}

	printf("Processed %d lines\n", nLinesProcessed);

	if (sharedMemoryCreator) {
	    printf("Cleaning up . . .\n");
	    /*
	     * And destroy the shared data area and semaphore set.
	     * First detach the shared memory segment via shmdt()
	     * and then destroy it with shmctl() using the IPC_RMID
	     * command.
	     *
	     * Destroy the semaphore set in a similar manner using a
	     * semctl() call with the IPC_RMID command.
	     */
	    shmdt((char *)bboard);
	    shmctl(shmId, IPC_RMID, NULL);
	    semctl(semId, 0, IPC_RMID, NULL);
	}

	printf("Done\n");
	return 0;
}

