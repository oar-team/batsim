# Introduction #

Batsim is run as a (Linux) process and communicates with a scheduler via a socket.
This socket is an Unix Domain Socket one in STREAM mode.

Since the C++ version (commit b9b5e8543c3b6172 of the C++ branch, 2015-11-20),
Batsim handles the server side of the socket (previously, Batsim was client-sided).

Batsim opens a server socket on a file (not on a port since Unix Domain Sockets are used).
Batsim then listens for 1 client socket and accepts its connection.

The used protocol is a simple synchronous semi-textual protocol. Its behaviour may be summarized
by a request-reply protocol. When Batsim needs a scheduling decision, the following events occur:
    1. Batsim stops the simulation.
    2. Batsim sends a request to the scheduler.
    3. Batsim waits for a reply from the scheduler.
    4. Batsim receives and reads the reply.
    5. Batsim resumes the simulation.

# Message Composition #

All messages sent in this protocol are assumed to have the format MSG_SIZE MSG_CONTENT where:
    - MSG_SIZE is a 32-bit native-endianness unsigned integer, which stores the number of bytes of the MSG_CONTENT part.
    - MSG_CONTENT is the real message content. It is a (non null-terminated) string: a sequence of bytes interpreted as characters thanks to the ASCII table.

Each MSG_CONTENT follows this syntax:
{PROTO_VERSION}:{TIME_MSG}|{TIME_EVENT1}:{STAMP1}[:{STAMP_DEPENDENT_CONTENT1}][|{TIME_EVENT2}:{STAMP2}[:{STAMP_DEPENDENT_CONTENT2}][...]] with :
    - PROTO_VERSION is the protocol version used in the message
    - TIME_MSG is the simulation time at which the message has been sent by the scheduler
    - TIME_EVENT1, TIME_EVENT2, ... TIME_EVENTn are the simulation time at which the event n was supposed to be sent to the scheduler. These values must be before the corresponding TIME_MSG time. Furthermore, all events must be in chronological order.
    - STAMP1, STAMP2, ... STAMPn are the stamp of each event: it allows to know what event type should be parsed. They are 1-character long.
    - STAMP_DEPENDENT_CONTENT1, STAMP_DEPENDENT_CONTENT2, ... STAMP_DEPENDENT_CONTENTn stores additional information about the event. For example, when a job is completed, this field stores
    the job ID of the job which just completed. This part is not mandatory, it depends on the used
    stamp.

# Message Stamps #

| Proto. version  | Stamp | Direction     | Content syntax           | Meaning
|---------------- |-------|-------------- |------------------------- |-------------
|        0+       |   S   | Bastim->Sched | JOB_ID                   | Job submission: one (static) job is available and can now be allocated tor resources.
|        0+       |   C   | Batsim->Sched | JOB_ID                   | Job completion: one (static) job finished its execution.
|        0+       |   J   | Sched->Batsim | JID1=MID1,MID2,MIDn[;...]| Job allocation: tells to put job JID1 on machines MID1, ..., MIDn. Many jobs might be allocated in the same event.              |           |
|        0+       |   N   | Both          | No content               | NOP: tells to do nothing / nothing happened.
|        1+       |   P   | Sched->Batsim | MACHINE_ID=PSTATE        | Ask to change the pstate of a machine.
|        1+       |   p   | Batsim->Sched | MACHINE_ID=PSTATE        | Tells the scheduler the pstate of a machine has changed.
|        1+       |   R   | Sched->Batsim | JOB_ID                   | Job rejection: the scheduler tells that one (static) job will not be computed.
|        1+       |   n   | Sched->Batim  | TIME                     | NOP me later: the scheduler asks to be awaken at simulation time TIME.


# Message Examples #

## Static Job Submission ##
    Batsim -> Scheduler
    0:10.000015|10.000015:S:1
    0:13|12:S:2|12.5:S:3|13:S:4

## Static Job Completion ##
    Batsim -> Scheduler
    0:15.836694|15.836694:C:1
    0:40.001320|25:C:2|38.002565:C:3

## Static Job Allocation ##
    Scheduler -> Batsim
    0:15.000015|15.000015:J:1=1,2,0,3;2=3
    0:45.00132|45.00132:J:4=3,1,2,0

## Static Job Rejection ##
    Scheduler -> Batsim
    0:50|50:R:5

## NOP ##
    Scheduler -> Batsim or Batsim -> Scheduler
    0:2|2:N
    0:42|10:N|20:N|30:N|40:N

## PState Modification Request ##
    Scheduler -> Batsim
    1:50|50:P:0=2
    1:70|60:P:0=0|60:P:1=2|70:P:2=0

## PState Modification Acknowledgement ##
    Batsim -> Scheduler
    1:50.000001|50.000001:p:0=2
    1:60.000001|60.000001:p:0=0|60.000001:p:1=2

## NOP Me Later ##
    Scheduler -> Batsim
    1:100|100:n:142

    Expected result (Batsim -> Scheduler):
        1:142.000001|142.000001:N