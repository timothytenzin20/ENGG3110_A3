# ENGG3110 Assignment 2 Submission

**Student:** Timothy Khan (1239165)  
**Instructor:** Andrew Hamilton-Wright  
**Date:** February 24, 2025

## Assignment Completion
- [x] Name is in a comment at the beginning of the program
- [x] Completed unfilled code segments
- [x] Included a functional makefile - with debug statements
- [x] Compiles and runs on linux.socs.uoguelph.ca
- [x] Submission contains a brief discussion describing program design

# Token Ring Simulation Design Overview

## Core Components

### Setup (tokenRing_setup.c)
System initialization was handled using the provided `setupSystem()` function, which created shared memory and initialized semaphores for our use. Specifically, each node gets three semaphores: EMPTY(i), FILLED(i), and TO_SEND(i) for coordinating data transfer. 

- `EMPTY(i)`: Indicates if a node has empty space for data.
- `FILLED(i)`: Signals that a node has received data.
- `TO_SEND(i)`: Controls data transfer between nodes.

Here it was important to initalize the semaphores to be empty as well as a critical semaphore to manage shared data access. Using `data_xfer` I have a single byte buffer used for transferring data between nodes and since there's no data to transfer, it is initialized to be 0. Meanwhile, `token_flag` is set to '1' since it is free and ready to be used for node transmission.

The simulation control in `runSimulation()` forks child processes using `fork()` and uses variable `pid_t pid`. Then I manage packet generation through a random number generator and also add random packet data to help visualize on execution.  Cleanup is then handled by `cleanupSystem()` which ensures proper termination of all processes and deallocation.

### Node Simulation (tokenRing_simulate.c) 

Token passing is implemented in the `token_node()` function. The function maintains `rcv_state` and tracks token status through  `token_flag` in the packet structure. Here, node 0 initializes the ring by creating a token ready to be used. Packet processing in `send_pkt()` then uses the relevant cases to manage its processing: TOKEN_FLAG, TO, FROM, LEN, DATA. The `send_byte()` and `rcv_byte()` functions handle the actual byte transfers between nodes using semaphores for synchronization.

#### Packet Processing (`send_pkt()`)
Manages data processing through different packet fields:

```c
case TOKEN_FLAG:
    // Token handling logic, checking if data can be sent
    ...
case TO:
    // Destination processing, determining where data should be sent
    ...
case FROM:
    // Source processing to retain sender address
    ...
case LEN:
    // Length processing to define packet size
    ...
case DATA:
    // Data transfer logic, handling byte transmission
    ...
```

#### Producer/consumer behaviour (`token_node()`):
```c
if (byte == '0') {
    if (producer == 1 && consumer == 0) {
        control->snd_state = TOKEN_FLAG;
        send_pkt(control, num);
        rcv_state = TO;
    }
}
```

This ensures that only valid nodes send packets and ensures other nodes receive and process the packet. However, it can be noted that for some executions of the program using `./tokensim *num*` the node recieves data sometimes does not actually do so and it is recieved by a node later in the program. This is like a synchronization error but any attempt to fix it left me with a deadlock. Still, I believe this behaviour is still comprehensible and therefore not technically flawed. Thus, I conclude my assignment submission.
