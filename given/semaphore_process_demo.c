/*
 * Hamilton-Wright, Andrew (2001)
 *
 * This small demo program implements the producer/consumer problem
 * using processes that co-operate via a shared memory region and
 * semaphores.
 * It uses one producer process and NCONSUMER consumer processes.
 * It is compiled via:
 * % cc semaphore_process_demo.c
 * under Linux.
 * If it is compiled with "cc -DDEBUG semaphore_process_demo.c" you get a bunch
 * of debugging output.
 * At this point, you will notice that the alphabet doesn't print out in
 * order. I suggest you take a look at the code and fix it so that it does
 * print the alphabet properly.
 *
 * For all the nitty gritty on shared memory and semaphores under Linux,
 * read the following man pages. shmget, shmctl, shmop, semget, semctl, semop.
 * (For example: man semop)
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/wait.h>
/*
 * Define the queue size and number of processes.
 */
#define	QSIZE		5
#define	NCONSUMER	3

/*
 * Define a data structure for the shared memory region.
 */
struct shared_data {
	char	que[QSIZE];	/* The queue, a ring buffer	*/
	int	start_pos;	/* Start position of queue	*/
	int	end_pos;	/* End position of queue	*/
};

/*
 * The Linux semaphore ops are done using the fields of sembuf.
 * sem_num - Which semaphore of the set, as defined below
 * sem_op  - set to 1 for "signal" and -1 for "wait"
 * sem_flg - set to 0 for our purposes
 * The elements of the semaphore set are assigned per the #defines
 * The macros WAIT_SEM, SIGNAL_SEM and INITIALIZE_SEM are defined
 * to, hopefully, simplify the code.
 */
#define	AVAILABLE_SEM	0
#define	USED_SEM	1
#define	CRITICAL_SEM	2
#define	NUM_SEM		3

/*
 * POSIX Now says this can't be in sem.h, so we have to put it in
 * ourselves? (It was in sys/sem.h in RedHat 5.2, and Linux seems
 * to put it in linux/sem.h, so let's use that one if it exists.)
 */
union semun {
	int val;
	struct semid_ds *buf;
	unsigned short int *array;
	struct seminfo *__buf;
};

#define	WAIT_SEM(s) { \
	struct sembuf sb; \
	sb.sem_num = (s); \
	sb.sem_op = -1; \
	sb.sem_flg = 0; \
	if (semop(semid, &sb, 1) < 0) { \
		fprintf(stderr, "Wait sem failed\n"); \
		exit(4); \
	} }

#define	SIGNAL_SEM(s) { \
	struct sembuf sb; \
	sb.sem_num = (s); \
	sb.sem_op = 1; \
	sb.sem_flg = 0; \
	if (semop(semid, &sb, 1) < 0) { \
		fprintf(stderr, "Signal sem failed\n"); \
		exit(4); \
	} }

#define INITIALIZE_SEM(s, n) { \
	union semun semarg; \
	semarg.val = (n); \
	if (semctl(semid, (s), SETVAL, semarg) < 0) { \
		fprintf(stderr, "Initialize sem failed\n"); \
		exit(4); \
	} }

/*
 * Global data, (not shared between processes).
 */
int semid;
struct shared_data *shared_ptr;

/*
 * The routines add_queue() and del_queue() use semaphores to add/delete
 * entries from the queue.
 * Since there is only one producer process, there is no critical
 * section for end_pos and que[end_pos].
 */
void
add_queue(char buf)
{

	/*
	 * Wait for space available.
	 */
#ifdef DEBUG
	fprintf(stderr,"in add_queue wait avail\n");
#endif
	WAIT_SEM(AVAILABLE_SEM);

	/*
	 * Add an element to the queue.
	 */
#ifdef DEBUG
	fprintf(stderr, "add queue %c\n", buf);
#endif
	shared_ptr->que[shared_ptr->end_pos] = buf;
	shared_ptr->end_pos = (shared_ptr->end_pos + 1) % QSIZE;

	/*
	 * Signal the used semaphore.
	 */
#ifdef DEBUG
	fprintf(stderr, "in add queue, signalling used\n");
#endif
	SIGNAL_SEM(USED_SEM);
}

/*
 * Called by several processes. As such manipulation of start_pos and
 * que[start_pos] forms a critical section.
 */
char
del_queue()
{
	char temp_buf;

	/*
	 * Wait for an element used.
	 */
#ifdef DEBUG
	fprintf(stderr, "del que wait used\n");
#endif
	WAIT_SEM(USED_SEM);

	/*
	 * and wait for the critical semaphore.
	 */
#ifdef DEBUG
	fprintf(stderr, "del que wait critical\n");
#endif
	WAIT_SEM(CRITICAL_SEM);

	/*
	 * Delete an entry in the queue, putting it in temp_buf.
	 */
	temp_buf = shared_ptr->que[shared_ptr->start_pos];
	shared_ptr->start_pos = (shared_ptr->start_pos + 1) % QSIZE;
#ifdef DEBUG
	fprintf(stderr, "del que got %c\n", temp_buf);
#endif

	/*
	 * Signal the critical and available semaphores.
	 */
	SIGNAL_SEM(CRITICAL_SEM);
	SIGNAL_SEM(AVAILABLE_SEM);

	/*
	 * and return the value.
	 */
	return (temp_buf);
}

/*
 * The main program creates the shared memory region and forks off the
 * processes to consume. This process does the producing, and then it
 * waits for the children to finish consuming and deletes the shared
 * data.
 */
int
main()
{
	int i;
	int shmid, child_status;
	char retc, cval;

	/*
	 * Create the shared memory region.
	 */
	shmid = shmget(IPC_PRIVATE, sizeof (struct shared_data), 0600);
	if (shmid < 0) {
		fprintf(stderr, "Can't create shared memory region\n");
		exit (1);
	}

	/*
	 * and map the shared data region into our address space at an
	 * address selected by Linux.
	 */
	shared_ptr = (struct shared_data *)shmat(shmid, (char *)0, 0);
	if (shared_ptr == (struct shared_data *)0) {
		fprintf(stderr, "Can't map shared memory region\n");
		exit (2);
	}

	/*
	 * Now, create the semaphores, by creating the semaphore set.
	 * Under Linux, semaphore sets are stored in an area of memory
	 * handled much like a shared region. (A semaphore set is simply
	 * a bunch of semaphores allocated to-gether.)
	 */
	semid = semget(IPC_PRIVATE, NUM_SEM, 0600);
	if (semid < 0) {
		fprintf(stderr, "Can't create semaphore set\n");
		exit(3);
	}

	/*
	 * and initialize them.
	 * Semaphores are meaningless if they are not initialized.
	 */
	INITIALIZE_SEM(AVAILABLE_SEM, QSIZE);
	INITIALIZE_SEM(USED_SEM, 0);
	INITIALIZE_SEM(CRITICAL_SEM, 1);

	/*
	 * Initialize the queue.
	 */
	shared_ptr->start_pos = shared_ptr->end_pos = 0;
#ifdef DEBUG
	fprintf(stderr, "main after initialization\n");
#endif

	/*
	 * Fork off the processes and let them do their stuff.
	 * Each fork() produces a child process that is a photocopy of
	 * the parent, except that the fork() call returns 0 in the
	 * child. Each of these children will run as consumer
	 * processes.
	 */
	for (i = 0; i < NCONSUMER; i++)
		if (fork() == 0) {
			int mypid = getpid();

			/*
			 * A child. Just call del_queue() in a loop
			 * until the end of the alphabet is reached.
			 * mypid is the Linux process id number of this
			 * process.
			 */
			do {
				retc = del_queue();
				if (retc != 'X')
					printf("%c ", retc);
#ifdef DEBUG
				fprintf(stderr, "child %d got %c\n",mypid,retc);
#endif
			} while (retc != 'X');

			/*
			 * This call to exit() terminates this process.
			 */
			exit(0);
			/*
			 * Never gets here, because the process terminates
			 * just above us at the exit().
			 */
		}
	/*
	 * The parent.
	 * Produce the alphabet in a loop, calling add_queue() to put the
	 * characters in the queue.
	 */
#ifdef DEBUG
	fprintf(stderr, "producer at alphabet loop\n");
#endif

	/*
	 * Just loop around calling add_queue() to insert characters in
	 * the alphabet.
	 */
	for (cval = 'a'; cval <= 'z'; cval++)
		add_queue(cval);

	/*
	 * A cheezy way to stop the children. Send one 'X' for each child.
	 */
	for (i = 0; i < NCONSUMER; i++)
		add_queue('X');

#ifdef DEBUG
	fprintf(stderr, "producer (parent) wait for children to terminate\n");
#endif
	/*
	 * And wait for the children to terminate.
	 * A Linux wait() call waits for one child process to terminate
	 * and then returns with its termination status. (The termination
	 * status is the value of the argument passed to exit() in the
	 * child.)
	 */
	for (i = 0; i < NCONSUMER; i++)
		wait(&child_status);

	printf("\n");
#ifdef DEBUG
	fprintf(stderr, "parent at destroy shared memory\n");
#endif
	/*
	 * And destroy the shared data area and semaphore set.
	 * First detach the shared memory segment via shmdt() and then
	 * destroy it with shmctl() using the IPC_RMID command.
	 * Destroy the semaphore set in a similar manner using a semctl()
	 * call with the IPC_RMID command.
	 */
	shmdt((char *)shared_ptr);
	shmctl(shmid, IPC_RMID, (struct shmid_ds *)0);
	semctl(semid, 0, IPC_RMID, (union semun)0);
	/*
	 * The C compiler inserts an exit() call at the end of main(),
	 * so we don't need an explicit exit() call here to terminate
	 * the producer (parent) process.
	 */
}
