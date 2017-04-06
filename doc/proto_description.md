# Introduction

Batsim is run as a process and communicates with a scheduler via a socket.
This socket is a ZMQ REQ socket and the decision process implements a ZMQ
REP socket so they communicate with a simple request reply pattern.

The used protocol is a simple synchronous with JSON based message.
Its behaviour may be summarized by a request-reply protocol. When Batsim
needs a scheduling decision, the following events occur:

1. Batsim suspend the simulation
2. Batsim sends a request to the scheduler
3. Batsim waits for a reply from the scheduler
4. Batsim receives and reads the reply
5. Batsim resumes the simulation

This protocol is used for synchronization purpose. Metadata associated to the
jobs are shared via Redis, as described [here](data_storage_description.md)

# Message Composition

It is a JSON object that looks like this:

```json
{
  "now": 1024.24
  "events": [
    {
      "timestamp": 1000,
      "type": "EXEC",
      "data": {
        "job_id": "workload!job_1234",
        "alloc": "1 2 4-8",
      }
    },
    {
      "timestamp": 1012,
      "type": "EXEC",
      "data": {
        "job_id": "workload!job_1235",
        "alloc": "12-100",
      }
    }
  ]
}

```

The ``now`` field define the current simulation time. In a request coming
from Batsim to the scheduler it means that the scheduler cannot take a
decision before this time. In the reply from the scheduler it inform Batsim
the time for the scheduler to take the decision.


## Constraints

Constraints on the message format are define here:

- the message timestamp now MUST be equal or greater than each event timestamp
- events timestamps MUST be ordered: event[i].timestamp <= event[i+1].timestamp
- mandatory fields:
    - now (type: float)
    - events: (type array (can be empty))
        - timestamp (type: float)
        - type (type: string as defined below)
        - data (type: dict (can be empty))

## Event type

Event types that can be send in both ways:
```
BATSIM <---> DECISION
```

### NOP

The simplest message is just for doing nothing.

- **data**: empty
- **example**:
```json
{}
```

Message from Batsim to the decision process (a.k.a. the scheduler):
```
BATSIM ---> DECISION
```

### JOB_SUBMITTED

Some jobs was submitted to Batsim.

- **data**: list of job id
- **example**:
```json
{
  "timestamp: 10.0,
  "type": "JOB_SUBMITTED",
  "data": {
    "job_ids": ["w0!1", "w0!2"]
  }
}
```

### JOB_COMPLETED

A job has completed its execution.

- **data**: a job id string with a status string (TIMEOUT, SUCCESS)
- **example**:
```json
{
  "timestamp: 10.0,
  "type": "JOB_COMPLETED",
  "data": {"job_id": "w0!1", "status": "SUCCESS"}
}
```

### RESOURCE_STATE_CHANGED

The state of some resources has changed.

- **data**: an interval set of resource id and the new state
- **example**:
```json
{
  "timestamp: 10.0,
  "type": "RESOURCE_STATE_CHANGED",
  "data": {"resources": "1 2 3-5", "state": "42"}
}
```

### QUERY_REPLY

This is a reply to a ``QUERY_REQUEST`` message.

- **data**: can be anything
- **example**:
```json
{
  "timestamp: 10.0,
  "type": "QUERY_REPLY",
  "data": {"redis_keys": "/my/key/path0" }
}
```


Message from the decision process to Batsim:
```
BATSIM <--- DECISION
```

### QUERY_REQUEST

This is a query sent to Batsim to get information about the simulation
state (or whatever you want to know...).

- **data**: This will defined elsewhere...
- **example**:
```json
{
  "timestamp: 10.0,
  "type": "QUERY_REQUEST",
  "data": {
    "reply_type": "redis",
    "requests": {"energy_consumed": {}}
  }
}
```

### REJECT_JOB

Reject a job that was submitted before.

- **data**: A job id
- **example**:
```json
{
  "timestamp: 10.0,
  "type": "REJECT_JOB",
  "data": { "job_id": "w12!45" }
}
```

### EXECUTE_JOB

Execute a job on a given set of resources. An optional mapping can be
added to tell Batsim how to map executors to resources: where the
executors will be placed inside my allocation (resource numbers are shifted
to 0). It only works for SMPI for now.

The following example overrides the default round robin mapping to put the
first two rank (0,1) on the first allocated machine (0 which stands for
resource id 2), and the two last rank (2,3) on the second machine (1 which
stands for resource id 3).

- **data**: A job id, an allocation and a mapping (optional)
- **example**:
```json
{
  "timestamp: 10.0,
  "type": "EXECUTE_JOB",
  "data": {
    "job_id": "w12!45",
    "alloc": "2-3",
    mapping: {"0": "0", "1": "0", "2": "1", "3": "1"}
  }
}
```

### CALL_ME_LATER

Ask for Batsim to wake the scheduler up later on time.

- **data**: future timestamp float
- **example**:
```json
{
  "timestamp: 10.0,
  "type": "CALL_ME_LATER",
  "data": {"timestamp": 25.5}
}
```


### KILL_JOB

### SUBMIT_JOB

### SET_RESOURCE_STATE
