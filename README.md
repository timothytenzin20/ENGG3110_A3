# ENGG3110 Assignment 3 Submission

**Student:** Timothy Khan (1239165)  
**Instructor:** Andrew Hamilton-Wright  
**Date:** March 17, 2025

## Assignment Completion
- [x] Name is in a comment at the beginning of the program
- [x] Completed thread-based implementation
- [x] Included a functional makefile - with debug statements
- [x] Compiles and runs on linux.socs.uoguelph.ca
- [x] Submission contains a brief discussion describing program design

# Token Ring Simulation Design Overview

## Core Components

### Setup (tokenRing_setup.c)
The system initialization now uses `malloc()` instead of shared memory segments, since threads share the same address space. Key components:

- Dynamic memory allocation for control structure and thread management
- POSIX semaphore initialization for synchronization
- Thread argument preparation for each node
- Node data structure initialization

### Node Simulation (tokenRing_simulate.c)

Thread-based implementation using `pthread_create()` instead of `fork()`. Each node runs as a thread within the same process, sharing:

- Common address space for data structures
- Semaphore-based synchronization
- Debug output controlled by DEBUG flag

#### Thread Synchronization
Careful management of shared resources using:
```c
sem_wait(&control->sems[CRIT]) 
// Critical section operations
sem_post(&control->sems[CRIT])
```

#### Termination Handling
Cleanup implemented through:
- cleanup_in_progress flag
- terminate flags for each node
- Proper semaphore cleanup
- Thread joining with timeout support

### Notable Changes from Assignment 2
1. Replaced process forking with thread creation
2. Removed shared memory segments in favor of malloc
3. Added conditional debug output
4. Improved termination handling
5. Added thread synchronization primitives
6. Implemented thread-safe cleanup procedures

Overall, the completed assignment is functional and executes using `./tokensim *num*`. The program accurately creates nodes, transfers data, uses threads, terminates threadsd, and joins them correctly. Joining, cleaning, and termination can be staggered due to time delay and other causes, however functiona remains as expected.
