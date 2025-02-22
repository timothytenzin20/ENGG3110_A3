#ifndef __IPC_HEADER__
#define	__IPC_HEADER__

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <stdlib.h>
#include <unistd.h>


/*
 * According to POSIX, we have to define this structure
 * ourselves; historically, however, it has been defined
 * for us.  What we do, .'. is define it ourselves based
 * on using some macros to guess whether we are being
 * compiled by an older compiler.
 *
 * This guessing code taken from the Linux man pages
 */

# if defined(__GNU_LIBRARY__) && !defined(_SEM_SEMUN_UNDEFINED)
# include <linux/sem.h>
    /* union semun is defined by including <sys/sem.h> */ 
    union semun semargs;
# else
#  if !defined(_POSIX_C_SOURCE) || defined(_DARWIN_C_SOURCE)
	/*
	 * On a newer compiler on Darwin/MacOSX, Apple has decided
	 * to again define this for us through sys/sem.h, so now
	 * we detect that situation here to avoid redefinition
	 * (and do nothing in this branch of the if)
	 */
#  else
    /* according to X/OPEN we have to define it ourselves */
    union semun {
	int val;                    /* value for SETVAL */
	struct semid_ds *buf;       /* buffer for IPC_STAT, IPC_SET */
	unsigned short int *array;  /* array for GETALL, SETALL */
	struct seminfo *__buf;      /* buffer for IPC_INFO */
    };
#  endif
# endif


/*
 *	Some handy macros for use with the semaphores.  These
 *	are essentially "syntactic sugar" to make the code
 *	read better when you use the semaphores
 */

/* Wait for the semaphore to become available */
#define	SEM_WAIT(sem, index) \
    do { \
	struct sembuf ops; \
	ops.sem_num = (index); \
	ops.sem_op  = (-1); \
	ops.sem_flg = 0; \
	if (semop((sem), &ops, 1) < 0) { \
	    fprintf(stderr, \
		"Semaphore op   (WAIT-[%s]) at %s(%d) failed : errno [%d]\n", \
		#sem, \
		__FILE__, __LINE__, \
		errno);  \
	    exit (4); \
	} \
    } while (0)



/* signal that the semaphore is free */
#define	SEM_SIGNAL(sem, index) \
    do { \
	struct sembuf ops; \
	ops.sem_num = (index); \
	ops.sem_op  = 1; \
	ops.sem_flg = 0; \
	if (semop((sem), &ops, 1) < 0) { \
	    fprintf(stderr, \
		"Semaphore op (SIGNAL-[%s]) at %s(%d) failed : (%d) %s\n", \
		#sem, \
		__FILE__, __LINE__, \
		errno, \
		strerror(errno)); \
	    exit (4); \
	} \
    } while (0)



/*
 * Initialize a semaphore at a given index to the given value
 */
#define INITIALIZE_SEM(sem, index, value) \
    do { \
	union semun semarg; \
	semarg.val = (value); \
	if (semctl((sem), (index), SETVAL, semarg) < 0) { \
	    fprintf(stderr, \
		"Semaphore [%s] init at %s(%d) failed : errno [%d]\n", \
		#sem, \
		__FILE__, __LINE__, \
		errno); \
	    exit(4); \
	} \
    } while (0)

/*
 * Dump out the values of a semaphore set, given the semid
 * and the number of sems in the set
 */
#define SEM_DUMP(sem, numSems) semDump((sem), (numSems))

/* function prototypes for ANSI compliance */
void semDump(int semId, int nSems);

#endif	/* __IPC_HEADER__ */
