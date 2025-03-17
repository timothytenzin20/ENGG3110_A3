/*
 * Hamilton-Wright, Andrew (2001)
 *
 * This small demo program implements the producer/consumer problem
 * using POSIX threads (Pthreads for short) co-operate within the process.
 * and semaphores for synchronization.
 * It uses one producer thread and NCONSUMER consumer threads.
 * It must be linked to the Pthreads library, so the compile line is:
 * % cc semaphore_pthreads_demo.c -lpthread
 * under Linux.
 * If it is compiled with "cc -DDEBUG semaphore_pthreads_demo.c -lpthread" you
 * get a bunch of debugging output.
 * If you run semaphore_process_demo.c, you will see that the alphabet will
 * not print out in order. Since the algorithm here is the same, that would
 * be theoretically possible, but doesn't occur under Linux, due to the
 * implementation of Pthreads. Take a look at the code and figure out why?
 * (Big hint: Look at which consumer thread gets stuff out of the queue,
 *  as listed by the -DDEBUG output.)
 *
 * For all the nitty gritty on Pthreads and Pthread semaphores under Linux,
 * read the following man pages. pthread_create, pthread_attr_init,
 *   pthread_join, pthread_exit and sem_wait.
 * (For example: man sem_wait)
 */
 #include <stdio.h>
 #include <stdlib.h>
 #include <pthread.h>
 #include <semaphore.h>
 /*
  * Define the queue size and number of threads.
  */
 #define	QSIZE		5
 #define	NCONSUMER	3
 
 /*
  * Define the global/shared data for the queue.
  */
 char	que[QSIZE];	/* The queue, a ring buffer	*/
 int	start_pos;	/* Start position of queue	*/
 int	end_pos;	/* End position of queue	*/
 
 /*
  * and the semaphores to be used for sychronization.
  */
 sem_t available, used, critical;
 
 /*
  * and the producer and consumer thread functions.
  */
 void * producer_thread(void *param);
 void * consumer_thread(void *param);
 
 /*
  * The routines add_queue() and del_queue() use semaphores to add/delete
  * entries from the queue.
  * Since there is only one producer thread, there is no critical
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
     sem_wait(&available);
 
     /*
      * Add an element to the queue.
      */
 #ifdef DEBUG
     fprintf(stderr, "add queue %c\n", buf);
 #endif
     que[end_pos] = buf;
     end_pos = (end_pos + 1) % QSIZE;
 
     /*
      * Signal the used semaphore.
      * (For some reason, the Pthread semaphore signal operation is called
      *  sem_post() and not sem_signal()?)
      */
 #ifdef DEBUG
     fprintf(stderr, "in add queue, signalling used\n");
 #endif
     sem_post(&used);
 }
 
 /*
  * Called by several threads. As such manipulation of start_pos and
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
     sem_wait(&used);
 
     /*
      * and wait for the critical semaphore.
      */
 #ifdef DEBUG
     fprintf(stderr, "del que wait critical\n");
 #endif
     sem_wait(&critical);
 
     /*
      * Delete an entry in the queue, putting it in temp_buf.
      */
     temp_buf = que[start_pos];
     start_pos = (start_pos + 1) % QSIZE;
 #ifdef DEBUG
     fprintf(stderr, "del que got %c\n", temp_buf);
 #endif
 
     /*
      * Signal the critical and available semaphores.
      */
     sem_post(&critical);
     sem_post(&available);
 
     /*
      * and return the value.
      */
     return (temp_buf);
 }
 
 /*
  * The main program creates the Pthreads and then simply waits for
  * them to complete producing and consuming. It destroys the semaphores,
  * which is a part of the POSIX Pthreads standard, but is actually a no-op
  * for Linux systems.
  */
 int
 main()
 {
     int i;
     pthread_t producer_threadid, consumer_threadid[NCONSUMER];
     pthread_attr_t pthread_attr;
     char thread_name[NCONSUMER][40];
 
     /*
      * Set the Pthread attributes to the defaults, which are fine for
      * most situations.
      */
     pthread_attr_init(&pthread_attr);
 
     /*
      * and initialize the semaphores.
      * Semaphores are meaningless if they are not initialized.
      * For Linux Pthreads, the second argument to sem_init() is
      * always 0 and the third argument is the initial value for
      * the semaphore.
      */
     sem_init(&available, 0, QSIZE);
     sem_init(&used, 0, 0);
     sem_init(&critical, 0, 1);
 
     /*
      * Initialize the queue.
      */
     start_pos = end_pos = 0;
 #ifdef DEBUG
     fprintf(stderr, "main after initialization\n");
 #endif
 
     /*
      * Create the producer and consumer Pthreads.
      */
     for (i = 0; i < NCONSUMER; i++) {
         sprintf(&thread_name[i][0], "cons%d", i);
         pthread_create(&consumer_threadid[i], &pthread_attr,
             consumer_thread, (void *)&thread_name[i][0]);
     }
     pthread_create(&producer_threadid, &pthread_attr,
         producer_thread, (void *)0);
 
 #ifdef DEBUG
     fprintf(stderr, "main, wait for threads to terminate\n");
 #endif
     /*
      * Wait for the producer thread to terminate.
      */
     pthread_join(producer_threadid, NULL);
 
     /*
      * And wait for the consumer threads to terminate.
      */
     for (i = 0; i < NCONSUMER; i++)
         pthread_join(consumer_threadid[i], NULL);
 
     printf("\n");
 #ifdef DEBUG
     fprintf(stderr, "main at destroy semaphores\n");
 #endif
     sem_destroy(&available);
     sem_destroy(&used);
     sem_destroy(&critical);
 
     /*
      * The C compiler inserts an exit() call at the end of main(),
      * so we don't need an explicit exit() call here to terminate
      * the process.
      */
 }
 
 /*
  * This function is executed by each consumer thread.
  * (The void * argument is actually a pointer to the thread_name.)
  */
 void *
 consumer_thread(void *param)
 {
     char retc, *thread_name;
 
     thread_name = (char *)param;
     /*
      * A consumer. Just call del_queue() in a loop
      * until the end of the alphabet is reached.
      */
     do {
         retc = del_queue();
         if (retc != 'X')
             printf("%c ", retc);
 #ifdef DEBUG
         fprintf(stderr,"consumer thread %s got %c\n",thread_name,retc);
 #endif
     } while (retc != 'X');
 
     /*
      * This call to pthread_exit() terminates this thread.
      */
     pthread_exit(0);
     /*
      * Never gets here, because the thread terminates
      * just above us at the pthread_exit().
      */
 }
 
 /*
  * This function is the producer thread.
  */
 void *
 producer_thread(void *param)
 {
     int i;
     char cval;
 
     /*
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
      * A cheezy way to stop the consumer threads. Send one 'X' for each
      * consumer thread.
      */
     for (i = 0; i < NCONSUMER; i++)
         add_queue('X');
 
     /*
      * and pthread_exit() when done.
      */
     pthread_exit(0);
 }
 
 