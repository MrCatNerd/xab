# xab IPC protocol

> [!NOTE]
> the IPC protocol is not finished yet but im treating it as it is in this document cuz I don't wanna change it later, don't expect this document to be a reliable source of information until its merged to main!, also why tf are you trying to implement an IPC client for xab go do something else lol

## The problems
The original problems I tackled when creating the xab IPC protocol were:
- I was bored and I wanted to try out IPC for a long time
- I had time to burn (not a problem btw)
- I wanted to create a fancy GUI for xab
- xab didn't have a way to be controlled externally once it's executed

## The solution?
Introducing IPC:
- [x] Boredom busted
- [x] Time burned
- [x] Fancy GUI achieved
- [x] xab externally communicated
- [x] 2000 segfaults
- [ ] world peace :(

## Introduction
In computer science, [inter-process communication](https://en.wikipedia.org/wiki/Inter-process_communication) (IPC)
is the sharing of data between running processes in a computer system, or between multiple such systems.

xab utilizes IPC to receive commands from different processes and communicate back to them.
to communicate, both processes need to share the same way of communicating AKA a communication protocol.

> A [communication protocol](https://en.wikipedia.org/wiki/Communication_protocol)
> is a system of rules that allows two or more entities of a communications system to transmit information. The protocol defines the rules, syntax, semantics, and synchronization of communication.

there are many different ways of implementing IPC, one of them is via a Unix domain socket
> A [Unix domain socket](https://en.wikipedia.org/wiki/Unix_domain_socket) (UDS)
> is a communication endpoint for exchanging data between processes executing in the same Unix or Unix-like operating system.

### Honorable mentions
- DihBus
- pipes and FIFOs
- message queues
- signals

I decided to name the protocol: `the xab inter-process communication protocol` or `xab IPC protocol` for short, very creative I know...

## Actually implementing this
> "To communicate with another process, you must first become the process" - Sun Tzu, The art of War

> "I never said that shi" - Sun Tzu, The art of War

first, we gotta make a specification, as mentioned, I want xab to receive commands, so the best way to start is by listing out the commands I want xab to receive:
I will segment all commands to 2 different categories:
- set state - control xab's state
- get state - get information about xab's current state

### Set state commands
- restart xab
- shutdown xab
- disconnect from xab's IPC server
- change a background to a different one
- delete a background
- pause a background's video
- unpause a background's video
- toggle the background's video pause state

### Get state commands
- get all of the current monitors (if capable)
- get all of the backgrounds' data
- get xab's capabilities (custom positioning, get monitors, etc...)

great! we defined our commands, but there's a big problem, we're using a network socket to communicate,
if you have some experience with sockets you know that **ya gotta know** the size of the data being sent before reading it, or else you might end up truncating the data, over allocating, or even memory leaking! which is very bad for RAM :(
so we have four choices:
1. hopes and dreams: pray the program doesn't segfault (recommended).
2. be flexible: send the size of the rest of your message in a single 32-bit integer at the beginning. (tuff)
3. be consistent: use the same size for the message, only works if the message is structured with no dynamic data (tuffer)
4. over allocate:  just allocate 2048 bytes cuz there aint no way the message size is exceeding that (boooring)

I chose 2 and 3 for different states of the protocol

> A [network socket](https://en.wikipedia.org/wiki/Network_socket)
> is a software structure within a network node of a computer network that serves as an endpoint for sending and receiving data across the network. The structure and properties of a socket are defined by an application programming interface (API) for the networking architecture. Sockets are created only during the lifetime of a process of an application running in the node.

### Is it just me or is this state machine kinda THICC

if you have been paying close attention, you may have noticed that the protocol can be represented as a deterministic finite state machine

> A [state machine](https://en.wikipedia.org/wiki/Finite-state_machine)
> is a mathematical model of computation. It is an abstract machine that can be in exactly one of a finite number of states at any given time. The FSM can change from one state to another in response to some inputs; the change from one state to another is called a transition. An FSM is defined by a list of its states, its initial state, and the inputs that trigger each transition. Finite-state machines are of two types—deterministic finite-state machines and non-deterministic finite-state machines. For any non-deterministic finite-state machine, an equivalent deterministic one can be constructed.


I'm not a mathematician and may have fucked up the state machine, pls create an issue if you find one
<br>
This is the state machine for the server:

NOTE: this is WIP and probably not deterministic, I might sacrifice
it being technically deterministic for while loops cuz its more convenient

<pre>
0. <strong>Mainloop</strong>:
    mainloop_stuff()
    next = 1

1. <strong>Scan</strong>: scan for new connections
    if new_connections > 0: next = 2
    else: next = 3

2. <strong>ProcessNewConnections</strong>:
    while connections remain:
        if connection is valid: next = 5
        else:
            log_error()
            client_cleanup()
            next = 3
    else: next = 2

3. <strong>ScanNewRequests</strong>:
    scan for new requests from existing clients
    if new requests exist: next = 5
    else: next = 0

4. <strong>DisconnectFromClient</strong>:
    if client is valid:
        send IPC_CLIENT_DISCONNECT
        disconnect
    else:
        log_error()
        client_cleanup()
        return

5. <strong>ValidateCompatibility</strong>:
    protocol matching: server sends protocol, client responds
    if protocol mismatch: next = 4
</pre>


#### TL;DR: server go bRRR

---

The entire header specification can be found at `src/ipc_spec.h`, there is also
a rust one [here](https://github.com/MrCatNerd/xab-gui/blob/main/src/ipc_spec.rs)

implementation files:
* `src/ipc.c` - main implementation file
* `src/ipc.h` - main header file for the implementation
* - specification file, describes constants, enums etc

a couple more things about the xab IPC protocol:
* uses unix domain sockets for IPC
* supports multiple connected clients at once via epolls and a simple event loop
* uses big endianness

