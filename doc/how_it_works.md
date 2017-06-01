# Case study 1: j1 completion -> execute j2 and j3
In a real system, the scheduling algorithm is called from time to time to
make some decisions.
This case study consists in making the decision to execute two jobs (j2 and j3)
on job j1 completion. However, the decision-making procedure takes some time and
makes the decision in an online fashion: the decision to execute j1 is made
before the decision to execute j2.

![case1_overview_figure](protocol_img/case1_overview.png)

## Protocol
In a Batsim simulation, most decisions are taken in another (Linux) process.
This decision-making process will simply be referred to as the Scheduler from now on.

The two processes communicate via a protocol. In this protocol, the Scheduler
must answer one message to each Batsim message. The messages may contain
multiple events. In this case, the messages would be:

![case1_protocol_figure](protocol_img/case1_protocol.png)

### Request message Batsim -> Scheduler
The message from Batsim to the Scheduler is:
``` JSON
{
  "now": 10.000000,
  "events": [
    {
      "timestamp": 10.000000,
      "type": "JOB_COMPLETED",
      "data": {
        "job_id": "d4c32e!1",
        "status": "SUCCESS"
      }
    }
  ]
}
```

The message means that:
- at time 10, job 1 (from workload d4c32e) completed successfully.
- the scheduler has been called at time 10 (``"now": 10.000000``),
  which means that the decisions can be made at time 10 or later on.

### Reply message Scheduler -> Batsim
The scheduler answer is the following:
``` JSON
{
  "now": 15.0,
  "events": [
    {
      "timestamp": 13.0,
      "type": "EXECUTE_JOB",
      "data": {
        "job_id": "d4c32e!2",
        "alloc": "0-1"
      }
    },
    {
      "timestamp": 14.0,
      "type": "EXECUTE_JOB",
      "data": {
        "job_id": "d4c32e!3",
        "alloc": "2-3"
      }
    }
  ]
}
```

This message means that the scheduler:
- (did something until time 13, since the request has been sent at time 10
  and that the first event is at time 13)
- first chose, at time 13, to execute job 2 on machines 0 and 1
- (did something until time 14, since the next event is at time 14)
- then chose, at time 14, to execute job 3 on machines 2 and 3
- (did something until time 15, since the reply has been received at
  ``"now": 15.0``)
- finally chose to stop making decisions for now, at time 15

## What happens within Batsim?
Batsim can be seen as a distributed application composed of different processes.
These processes may communicate with each other, and spawn other processes.

The main process is the **server**. It is started at the beginning of the
simulation, and it ends when the simulation has finished. It orchestrates
most of the other processes:
- **request reply** processes, in charge of communicating with the scheduler
- **job executor** processes, in charge of executing jobs
- **waiter** processes, in charge of handling
  [CALL_ME_LATER](proto_description.md#call_me_later) events

What happens within Batsim for the case study 1 is the following:
![case1_inner_figure](protocol_img/case1_inner.png)

First, a **job executor** process finishes to execute job 1. It sends a message
about it to the **server** then terminates. When the server receives the message,
it spawns a **request reply** process to forward that j1 has completed.

The newly spawned **request reply** process sends a network message to
the scheduler, forwarding that j1 has completed. The **request reply** process
then waits for the scheduler reply (the simulation is *stopped* as long as
the reply has not been received).
Once the reply from the scheduler has been received, the **request reply**
process role is to forward the events to the server at the right times.
For this purpose, it sends the events in order, sleeping between events if
needed.

Once all the events have been forwarded, the **request reply** process tells the
**server** that it has finished, which will allow the server to spawn another
**request reply** process later on. Only one **request reply** process can be
executing at a given time. Whenever the server receives an event which must be
sent to the scheduler, it stores it an event queue. If the scheduler is ready,
the message is sent immediately. Otherwise, the message will be sent as soon
as possible, i.e. as soon as the next ``SCHED_READY`` event is received.
