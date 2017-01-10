# Introduction #

Batsim is run as a process and communicates with a scheduler via a socket.
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

This protocol is used for synchronization purpose. Metadata associated to the
jobs are shared via Redis, as described [here](data_storage_description.md)

# Message Composition #

All messages sent in this protocol are assumed to have the format MSG_SIZE MSG_CONTENT where:
- MSG_SIZE is a 32-bit native-endianness unsigned integer, which stores the number of bytes of the MSG_CONTENT part.
- MSG_CONTENT is the real message content. It is a (non null-terminated) string: a sequence of bytes interpreted as characters thanks to the ASCII table.

Each MSG_CONTENT follows this syntax:
```
{PROTO_VERSION}:{TIME_MSG}|{TIME_EVENT1}:{STAMP1}[:{STAMP_DEPENDENT_CONTENT1}][|{TIME_EVENT2}:{STAMP2}[:{STAMP_DEPENDENT_CONTENT2}][...]]
```
with :
- PROTO_VERSION is the protocol version used in the message
- TIME_MSG is the simulation time at which the message has been sent by the scheduler
- TIME_EVENT1, TIME_EVENT2, ... TIME_EVENTn are the simulation times at which the events were supposed to be sent to the scheduler. These values must be lower than or equal to the corresponding TIME_MSG simulation time. Furthermore, all events must be in chronological order.
- STAMP1, STAMP2, ... STAMPn are the stamps of each event: They allow to know what event type should be parsed. They are 1-character long.
- STAMP_DEPENDENT_CONTENT1, STAMP_DEPENDENT_CONTENT2, ... STAMP_DEPENDENT_CONTENTn store additional information about the events. For example, when a job is completed, this field stores
the job ID of the job which just completed. This part is not mandatory, it depends on the used stamp.

# Message Stamps #

| Proto. version  | Stamp | Direction     | Content syntax                  | Meaning
|---------------- |-------|-------------- |-------------------------------- |-------------
|        2+       |   S   | Bastim->Sched | WLOAD!JOB_ID                    | Job submission: job JOB_ID of workload WLOAD is available and can now be allocated to resources.
|        2+       |   C   | Batsim->Sched | WLOAD!JOB_ID                    | Job completion: job JOB_ID of workload WLOAD finished its execution.
|        2+       |   J   | Sched->Batsim | WLOAD!JID1=MID1,MID2,MIDn[;...] | Job allocation: tells to put job JID1 of workload WLOAD on machines MID1, ..., MIDn. Many jobs might be allocated in the same event. Each MIDk part can be a single machine ID or a closed interval MIDa-MIDb where MIDa <= MIDb
|        0+       |   N   | Both          | No content                      | NOP: tells to do nothing / nothing happened.
|        1+       |   P   | Sched->Batsim | MID1,MID2,MIDn=PSTATE           | Asks to change the power state of some machines. Each MIDk part can be a single machine ID or a closed interval MIDa-MIDb where MIDa <= MIDb
|        1+       |   p   | Batsim->Sched | MID1,MID2,MIDn=PSTATE           | Tells the scheduler that the power state of one or several machines has changed. Each MIDk part can be a single machine ID or a closed interval MIDa-MIDb where MIDa <= MIDb. There is one and only one 'p' message for each 'P' message.
|        2+       |   R   | Sched->Batsim | WLOAD!JOB_ID                    | Job rejection: the scheduler tells that one (static) job will not be computed.
|        1+       |   n   | Sched->Batsim | TIME                            | NOP me later: the scheduler asks to be awaken at the given simulation time TIME.
|        1+       |   E   | Sched->Batsim | No content                      | Asks Batsim about the total consumed energy (from time 0 to now) in Joules. Works only in energy mode.
|        1+       |   e   | Batsim->Sched | CONSUMED_ENERGY                 | Batsim tells the total consumed energy (from time 0 to now) in Joules. Works only in energy mode. There is one and only one 'e' message for each 'E' message.
|        3+       |   A   | Batsim->Sched | No content                      | Batsim tells the scheduler that the simulation is about to begin (the Scheduler can now read information from Redis). This is the first message Batsim sends.
|        3+       |   Z   | Batsim->Sched | No content                      | Batsim tells the scheduler that the simulation is about to end (all jobs have been submitted and completed/rejected)
|        3+       |   F   | Batsim->Sched | MID1,MID2,MIDn                  | Batsim tells the scheduler that the given machines are in a failure state (crashed, no jobs can be computed on them). Each MIDk part can be a single machine ID or a closed interval MIDa-MIDb where MIDa <= MIDb
|        3+       |   f   | Batsim->Sched | MID1,MID2,MIDn                  | Batsim tells the scheduler that the given machines are no longer in a failure state (jobs can now be computed on them). Each MIDk part can be a single machine ID or a closed interval MIDa-MIDb where MIDa <= MIDb
|        4+       |   Q   | Batsim->Sched | REQ1[;...]                      | Batsim queries the scheduler about potential waiting times for requested number of processors.
|        4+       |   W   | Sched->Batsim | REQ1=WAIT1[;...]                | Scheduler notifies Batsim about potential waiting times for requested number of processors.

# Message Examples #

## Simulation starts ##
    Batsim -> Scheduler
    3:0.000000|0.000000:A

## Simulation ends ##
    Batsim -> Scheduler
    3:46.556835|46.556835:C:workload_profiles/test_workload_profile.json!2|46.556835:Z

## Static Job Submission ##
    Batsim -> Scheduler
    0:10.000015|10.000015:S:static!1
    0:13|12:S:static!2|12.5:S:static!3|13:S:static!4

## Static Job Completion ##
    Batsim -> Scheduler
    0:15.836694|15.836694:C:static!1
    0:40.001320|25:C:static!2|38.002565:C:static!3

## Static Job Allocation ##
    Scheduler -> Batsim
    0:15.000015|15.000015:J:static!1=1,2,0,3;static!2=3
    0:45.00132|45.00132:J:static!4=3,1,2,0

## NOP ##
    Scheduler -> Batsim or Batsim -> Scheduler
    0:2|2:N
    0:42|10:N|20:N|30:N|40:N

## PState Modification Request ##
    Scheduler -> Batsim
    1:50|50:P:0=2
    1:70|60:P:0-2=0

## PState Modification Acknowledgement ##
    Batsim -> Scheduler
    1:50.5|50.5:p:0=2
    1:60.5|60.5:p:0-2=0

## Static Job Rejection ##
    Scheduler -> Batsim
    0:50|50:R:static!5

## NOP Me Later ##
    Scheduler -> Batsim
    1:100|100:n:142

    Expected result (Batsim -> Scheduler):
        1:142.000001|142.000001:N

## Ask about energy consumption ##
    Scheduler -> Batsim
    1:60|60:E

    Expected result (Batsim -> Scheduler)
        1:60.000001|60.000001:e:960000
