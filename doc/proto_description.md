# Introduction
A Batsim simulation consists in two processes:
- Batsim itself, in charge of simulating what happens on the platform
- A *Decision Process* (or more simply scheduler), in charge of making decisions

The two processes communicate via a socket with the protocol explained in
the present document.  The protocol is synchronous and follows a simple
request-reply pattern.  Whenever an event which may require making decision
occurs in Batsim in the simulation, the following steps occur:

1. Batsim suspends the simulation
2. Batsim sends a request to the scheduler (telling it what happened on the
   platform)
3. Batsim waits for a reply from the scheduler
4. Batsim receives the reply
5. Batsim resumes the simulation, applying the decision which have been
   made

![protocol_overview_figure](protocol_img/request_reply.png)

ZeroMQ is used in both processes (Batsim uses a ZMQ REQ socket, the
scheduler a ZMQ REP one).

The behavior of this protocol depends on the
[configuration](./configuration.md):
- If Redis is enabled, job metadata is stored into a Redis server and not sent
  through the protocol. In this case, the protocol is only used for
  synchronization purposes. More information about Redis conventions are
  described [there](data_storage_description.md).
- Batsim may or may not forward job profile information to the scheduler when
  jobs are submitted (see [JOB_SUBMITTED](#job_submitted) documentation)
- Dynamic jobs submissions can be enabled or disabled.
  Many parameters of job submissions can be adjusted, please refer to the
  [Dynamic submission of jobs](#dynamic-submission-of-jobs) documentation for
  more details.

# Message Composition

It is a JSON object that looks like this:

```json
{
  "now": 1024.24,
  "events": [
    {
      "timestamp": 1000,
      "type": "EXECUTE_JOB",
      "data": {
        "job_id": "workload!job_1234",
        "alloc": "1 2 4-8",
      }
    },
    {
      "timestamp": 1012,
      "type": "EXECUTE_JOB",
      "data": {
        "job_id": "workload!job_1235",
        "alloc": "12-100",
      }
    }
  ]
}

```

The ``now`` field defines the current simulation time.
- If the message comes from Batsim, it means that the scheduler cannot make
  decisions before ``now`` (time travel simulation is not supported at the
  moment)
- If the message comes from the scheduler, it tells Batsim that the
  scheduler finished making its decisions at timestamp ```now```. It is
  used by Batsim to know when the scheduler will be available for making
  new decisions.


## Constraints

Constraints on the message format are defined here:

- the message timestamp ``now`` MUST be greater than or equal to every
  event ``timestamp``
- events timestamps MUST be in ascending order: event[i].timestamp <=
  event[i+1].timestamp
- mandatory fields:
    - ``now`` (type: float)
    - ``events``: (type array (can be empty))
        - ``timestamp`` (type: float)
        - ``type`` (type: string as defined below)
        - ``data`` (type: dict (can be empty))

---

## Bidirectional events

These events can be sent from Batsim to the scheduler, or in the opposite
direction.
```
BATSIM <---> DECISION
```

### NOP

The simplest message, stands either for: "nothing happened" if sent by
Batsim, or "do nothing" if sent by the scheduler. It means that the
events list is empty: ``"events": []``

- **data**: N/A
- **full message example**:
```json
{
  "now": 1024.24,
  "events": []
}
```

---

## Batsim to Scheduler events

These events are sent by Batsim to the scheduler.
```
BATSIM ---> DECISION
```

### SIMULATION_BEGINS
Sent at the beginning of the simulation. Once it has been sent,
and if redis is enabled, meta-information can be read from Redis.

Batsim configuration is sent through the ``config`` object (in ``data``).
Any custom information can be added into the
[Batsim configuration](./configuration.md), which gives a generic way to give
metainformation from Batsim to any scheduler at runtime.

- **data**:
  - **nb_resources**: the number of resources
  - **allow_time_sharing**: whether time sharing is enabled or not
  - **config**: the Batsim configuration
  - **resources_data**: information about the resources
    - **id**: unique resource number
    - **name**: resource name
    - **state**: resource state in {sleeping, idle, computing, switching_on, switching_off}
    - **properties**: the properties specified in the SimGrid platform for the corresponding host
- **example**:
```json
{
  "timestamp": 0.0,
  "type": "SIMULATION_BEGINS",
  "data": {
    "allow_time_sharing": false,
    "nb_resources": 1,
    "config": {},
    "resources_data": [
      {
        "id": 0,
        "name": "host0",
        "state": "idle",
        "properties": {}
      }
    ]
  }
}
```

### SIMULATION_ENDS
Sent when Batsim thinks that the simulation is over. It means that all the jobs
(either coming from Batsim workloads/workflows inputs, or dynamically submitted
ones) have been submitted and executed (or rejected). The scheduler should
answer a [NOP](#nop) to this message then close its socket and terminate.

- **data**: empty
- **example**:
```json
{
  "timestamp": 100.0,
  "type": "SIMULATION_ENDS",
  "data": {}
}
```

### JOB_SUBMITTED

The content of this message depends on the
[Batsim configuration](./configuration.md).

This event means that one job has been submitted within Batsim.
It is sent whenever a job coming from Batsim inputs (workloads and workflows)
has been submitted.
If dynamic job submissions are enabled (the configuration contains
``{"job_submission": { "from_scheduler": {"enabled": true}}}``), this message is
is sent as a reply to a [SUBMIT_JOB](#submit_job) message if and only if
dynamic job submissions acknowledgements are enabled
(``{"job_submission": {"from_scheduler": {"acknowledge": true}}}``)

The ``job_id`` field is always sent and contains a unique job identifier.
If redis is enabled (``{"redis": {"enabled": true}}``),
``job_id`` is the only forwarded field.
Otherwise (if redis is disabled), a JSON description of the job is forwarded
in the ``job`` field.
A JSON description of the job profile is sent if and only if
profiles forwarding is enabled
(``{"job_submission": {"forward_profiles": true}}``).

- **data**: a job id and optional information depending on the configuration
- **example without redis and without forwarded profiles**:
```json
{
  "timestamp": 10.0,
  "type": "JOB_SUBMITTED",
  "data": {
    "job_id": "dyn!my_new_job",
    "job": {
      "profile": "delay_10s",
      "res": 1,
      "id": "my_new_job",
      "walltime": 12.0
    }
  }
}
```
- **example without redis and with forwarded profiles**:
```json
{
  "timestamp": 10.0,
  "type": "JOB_SUBMITTED",
  "data": {
    "job_id": "dyn!my_new_job",
    "job": {
      "profile": "delay_10s",
      "res": 1,
      "id": "my_new_job",
      "walltime": 12.0
    },
    "profile":{
      "type": "delay",
      "delay": 10
    }
  }
}
```
- **example with redis**:
```json
{
  "timestamp": 10.0,
  "type": "JOB_SUBMITTED",
  "data": {"job_id": "w0!1"}
}
```

### JOB_COMPLETED

A job has completed its execution. It acknowledges that the actions coming
from a previous [EXECUTE_JOB](#execute_job) message have been done (successfully
or not, depending on whether the job completed without reaching timeout).

- **data**:
  - **job_id**: the job unique identifier
  - **status**: whether SUCCESS or TIMEOUT (**DEPRECATED**)
  - **job_state**: the job state. Possible values: "NOT_SUBMITTED", "SUBMITTED", "RUNNING", "COMPLETED_SUCCESSFULLY", "COMPLETED_FAILED", "COMPLETED_WALLTIME_REACHED", "COMPLETED_KILLED", "REJECTED"
  - **kill_reason**: the kill reason (if any)
- **example**:
```json
{
  "timestamp": 10.000000,
  "type": "JOB_COMPLETED",
  "data": {
    "job_id": "2cf8ca!10",
    "status": "TIMEOUT",
    "job_state": "COMPLETED_KILLED",
    "kill_reason": "Walltime reached"
  }
}
```

### JOB_KILLED

Some jobs have been killed.
It acknowledges that the actions coming from a previous [KILL_JOB](#kill_job)
message have been done.
The ``job_ids`` jobs correspond to those requested in the previous
[KILL_JOB](#kill_job) message)

The ``job_progress`` map is also given for the all the jobs and tasks
inside the job that have been killed. Key is the ``job_id`` and the value
contains a progress value that in \]0, 1\[ with 0 for not started and 1 for
complete task and the profile name is also given for convenience. For
sequential job the progress map contains the 0-based index of the inner
task that was running at the time it was killed and the details of this
progress in the ``current_task`` field. Note that sequential jobs can be
nested.

Please remark that this message does not necessarily means that all the jobs
have been killed. It means that all the jobs have completed. Some of the jobs
might have completed *ordinarily* before the kill.
In this case, [JOB_COMPLETED](#job_completed) events corresponding to the
aforementioned jobs should be received before the [JOB_KILLED](#job_killed)
event.

- **data**: A list of job ids
- **example**:
```json
{
  "timestamp": 10.0,
  "type": "JOB_KILLED",
  "data": {
    "job_ids": [
      "w0!1",
      "w0!2"
    ]
  }
}
```

- **data**: A list of job ids + progress
- **example**:
```json
{
  "timestamp": 10.0,
  "type": "JOB_KILLED",
  "data": {
    "job_ids": [
      "w0!1",
      "w0!2"
    ],
    "job_progress": {
      "w0!1": {
        "profile": "my_simple_profile",
        "progress": 0.52
      },
      "w0!2": {
        "profile": "my_sequential_profile",
        "current_task_index": 3,
        "current_task": {
          "profile": "my_simple_profile",
          "progress": 0.52
        }
      },
      "w0!3": {
        "profile": "my_composed_profile",
        "current_task_index": 2,
        "current_task": {
          "profile": "my_sequential_profile",
          "current_task_index": 3,
          "current_task": {
            "profile": "my_simple_profile",
            "progress": 0.52
          }
        }
      }
    }
  }
}
```

### RESOURCE_STATE_CHANGED

The state of some resources has changed. It acknowledges that the actions
coming from a previous [SET_RESOURCE_STATE](#set_resource_state) message have
been done.

- **data**: an interval set of resource id and the new state
- **example**:
```json
{
  "timestamp": 10.0,
  "type": "RESOURCE_STATE_CHANGED",
  "data": {"resources": "1 2 3-5", "state": "42"}
}
```

### QUERY_REPLY

This is a reply to a [QUERY_REQUEST](#query_request) message. It depends on the
The message content depends on whether redis is enabled in the
[Batsim configuration](./configuration.md).
If ``{"redis": { "enabled": true }}``, the reply will
go in redis and only the key will be given. Otherwise, the response will be
put directly in the message.

- **data**: See [QUERY_REQUEST](#query_request) documentation
- **example**:
```json
{
  "timestamp": 10.0,
  "type": "QUERY_REPLY",
  "data": {"redis_keys": "/my/key/path0" }
}
```
or
```json
{
  "timestamp": 10.0,
  "type": "QUERY_REPLY",
  "data": {"consumed_energy": "12500" }
}
```

### REQUESTED_CALL
This message is a response to the [CALL_ME_LATER](#call_me_later) message.

- **data**: empty
- **example**:
```json
{
  "timestamp": 25.5,
  "type": "REQUESTED_CALL",
  "data": {}
}
```

---

## Scheduler to Batsim events
These events are sent by the scheduler to Batsim.
```
BATSIM <--- DECISION
```

### QUERY_REQUEST

This is a query sent to Batsim to get information about the simulation
state (or whatever you want to know...). The supported requests are:
- "consumed_energy" with no argument that asks Batsim about the total
  consumed energy (from time 0 to now) in Joules. Works only in energy
  mode.

- **data**: a dictionnary of requests.
- **example**:
```json
{
  "timestamp": 10.0,
  "type": "QUERY_REQUEST",
  "data": {
    "requests": {"consumed_energy": {}}
  }
}
```

### REJECT_JOB

Rejects a job that has already been submitted.
The rejected job will not appear into the final jobs trace.

- **data**: A job id
- **example**:
```json
{
  "timestamp": 10.0,
  "type": "REJECT_JOB",
  "data": { "job_id": "w12!45" }
}
```

### EXECUTE_JOB

Execute a job on a given set of resources. An optional mapping can be
added to tell Batsim how to map executors to resources: where the
executors will be placed inside the allocation (resource numbers are shifted
to 0). It only works for SMPI for now.

The following example overrides the default round robin mapping to put the
first two ranks (0 and 1) on the first allocated machine (0, which stands for
resource id 2), and the last two ranks (2 and 3) on the second machine (1, which
stands for resource id 3).

- **data**: A job id, an allocation and a mapping (optional)
- **example**:
```json
{
  "timestamp": 10.0,
  "type": "EXECUTE_JOB",
  "data": {
    "job_id": "w12!45",
    "alloc": "2-3",
    "mapping": {"0": "0", "1": "0", "2": "1", "3": "1"}
  }
}
```

### CALL_ME_LATER

Asks Batsim to call the scheduler later on, at a given timestamp.

- **data**: future timestamp float
- **example**:
```json
{
  "timestamp": 10.0,
  "type": "CALL_ME_LATER",
  "data": {"timestamp": 25.5}
}
```


### KILL_JOB
Kills some jobs (almost instantaneously).

- **data**: A list of job ids
- **example**:
```json
{
  "timestamp": 10.0,
  "type": "KILL_JOB",
  "data": {"job_ids": ["w0!1", "w0!2"]}
}
```

### SUBMIT_JOB

Submits a job (from the scheduler). Job submissions from the scheduler must
be enabled in the [configuration](./configuration.md)
(``{"job_submission": {"from_scheduler": {"enabled": true}}``).
The submission is acknowledged by default, but acknowledgements can be disabled
in the configuration
(``{"job_submission": {"from_scheduler": {"acknowledge": false}}}``).

- **data**: A job id (job id duplication is forbidden), classical job and
  profile information (optional).

- **example with redis** : the job description, and the profile description if
  it unknown to Batsim yet, must have been pushed into redis by the
  scheduler before sending this message
```json
{
  "timestamp": 10.0,
  "type": "SUBMIT_JOB",
  "data": {
    "job_id": "w12!45",
  }
}
```
- **example without redis** : the whole job description goes through the protocol.
```json
{
  "timestamp": 10.0,
  "type": "SUBMIT_JOB",
  "data": {
    "job_id": "dyn!my_new_job",
    "job":{
      "profile": "delay_10s",
      "res": 1,
      "id": "my_new_job",
      "walltime": 12.0
    },
    "profile":{
      "type": "delay",
      "delay": 10
    }
  }
}
```

### SUBMIT_PROFILE

Submits a profile (from the scheduler). Job submissions from the scheduler must
be enabled in the [configuration](./configuration.md)
(``{"job_submission": {"from_scheduler": {"enabled": true}}``).

- **data**: A workload name, profile name, and the data of the profile.

- **with redis** : Instead of using this message, the profiles should be pushed
  to redis directly by the scheduler.

- **example without redis** : the whole profile description goes through the protocol.
```json
{
  "timestamp": 10.0,
  "type": "SUBMIT_PROFILE",
  "data": {
    "workload_name": "dyn_wl1",
    "profile_name":  "delay_10s",
    "profile": {
      "type": "delay",
      "delay": 10
    }
  }
}
```

### SET_RESOURCE_STATE

Sets some resources into a state.

- **data**: an interval set of resource id, and the new state
- **example**:
```json
{
  "timestamp": 10.0,
  "type": "SET_RESOURCE_STATE",
  "data": {"resources": "1 2 3-5", "state": "42"}
}
```

### NOTIFY
The scheduler notifies Batsim of something.

For example, the ``submission_finished`` notifies that job submissions
from the scheduler are over, which allows Batsim to stop the simulation.
This message **must** be sent if ``"scheduler_submission": {"enabled": false}``
is configured. See [Configuration documentation](./configuration.md) for more
details.
If the scheduler realizes that it commited the mistake of notifying
``submission_finished`` prematurely, the ``continue_submission`` notification
can be sent to make the scheduler able to submit dynamic jobs again.


- **data**: empty
- **example**:
```json
{
  "timestamp": 42.0,
  "type": "NOTIFY",
  "data": { "type": "submission_finished" }
}
```

### CHANGE_JOB_STATE

Changes the state of a job, which may be helpful to implement schedulers with
dynamic complex jobs.

``` json
{
  "timestamp": 42.0,
  "type": "CHANGE_JOB_STATE",
  "data": {
    "job_id": "w12!45",
    "job_state": "COMPLETED_KILLED",
    "kill_reason": "Sub-jobs were killed."
  }
}
```

# Use cases
The way to do some operations with the protocol is shown in this section.

## Executing jobs
Depending on the [configuration](./configuration.md), job information might
either be transmitted through the protocol or Redis.
![executing_jobs_figure](protocol_img/job_submission_and_execution.png)

## Dynamic submission of jobs
Jobs are in most cases given as Batsim inputs, which are submitted within
Batsim (the scheduler knows about them via [JOB_SUBMITTED](#job_submitted) events).

However, jobs can also be submitted from the scheduler throughout the
simulation. For this purpose:
- dynamic job submissions **must** be enabled in the
  [Batsim configuration](./configuration.md)
- the scheduler **must** tell Batsim when it has finished submitting dynamic
  jobs (via a [NOTIFY](#notify) event).
  Otherwise, Batsim will wait for new simulation events forever,
  causing either a SimGrid deadlock or an infinite loop at the end of the
  simulation.
- the scheduler **must** make sure that Batsim has enough information to avoid
  SimGrid deadlocks during the simulation. If at some simulation time all
  Batsim workloads/workflows inputs have been executed and nothing is happening
  on the platform, this might lead to a SimGrid deadlock. If the scheduler
  knows that it will submit a dynamic job in the future, it should ask Batsim
  to call it at this timestamp via a [CALL_ME_LATER](#call_me_later) event.

The protocol behavior of dynamic submissions is customizable in the
  [Batsim configuration](./configuration.md):
- Batsim might or might not send acknowledgements when jobs have been submitted.
- Metainformation are sent via Redis if Redis is enabled, or directly via the
  protocol otherwise.

A simple scheduling algorithm using dynamic job submissions can be found in
[Batsched](https://gitlab.inria.fr/batsim/batsched/blob/master/src/algo/submitter.cpp).
This implementation should work whether Redis is enabled and whether dynamic
job submissions are acknowledged.

The following two figures outline how submissions should be done
(depending whether Redis is enabled).

### Without Redis
![dynamic_submission](protocol_img/dynamic_job_submission.png)

### With Redis
![dynamic_submission_redis](protocol_img/dynamic_job_submission_redis.png)
