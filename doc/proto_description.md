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

## Table of Events

- Bidirectional
  - [NOP](#nop)
  - [QUERY](#query)
  - [ANSWER](#answer)
  - [NOTIFY](#notify)
- Batsim to Scheduler
  - [SIMULATION_BEGINS](#simulation_begins)
  - [SIMULATION_ENDS](#simulation_ends)
  - [JOB_SUBMITTED](#job_submitted)
  - [JOB_COMPLETED](#job_completed)
  - [JOB_KILLED](#job_killed)
  - [RESOURCE_STATE_CHANGED](#resource_state_changed)
  - [REQUESTED_CALL](#requested_call)
- Scheduler to Batsim
  - [REJECT_JOB](#reject_job)
  - [EXECUTE_JOB](#execute_job)
  - [CALL_ME_LATER](#call_me_later)
  - [KILL_JOB](#kill_job)
  - [SUBMIT_JOB](#submit_job)
  - [SUBMIT_PROFILE](#submit_profile)
  - [SET_RESOURCE_STATE](#set_resource_state)
  - [SET_JOB_METADATA](#set_job_metadata)
  - [CHANGE_JOB_STATE](#change_job_state)

## Bidirectional events

These events can be sent from Batsim to the scheduler, or in the opposite
direction.
```
BATSIM <---> SCHEDULER
```

### NOP

The simplest message, stands either for: "nothing happened" if sent by
Batsim, or "do nothing" if sent by the scheduler. It means that the
events list is empty: ``"events": []``

- **data**: N/A
- **full message example**:
```json
{
  "now": 1024.42,
  "events":[]
}
```

### QUERY

This message allows a peer to ask specific information to its counterpart:
- Batsim can ask information to the scheduler.
- The scheduler can ask information to Batsim.

The other peer should answer such QUERY via an [ANSWER](#answer).

For now, Batsim **answers** to the following requests:
- ``consumed_energy``: the scheduler queries Batsim about the total consumed
   energy (from time 0 to now) in Joules.
  Only works if the energy mode is enabled.
  This query has no argument.
- ``air_temperature_all``: the scheduler queries Batsim about the ambient air temperature of
   all hosts.
  Only works if the temperature mode is enabled.
- ``processor_temperature_all``: the scheduler queries Batsim about the processor
   temperature of all hosts.
  Only works if the temperature mode is enabled.

For now, Batsim **queries** the following requests:
- ``estimate_waiting_time``: Batsim asks the scheduler what would be the waiting
  time of a potential job.  
  Arguments: a job description, similar to those sent in
  [JOB_SUBMITTED](#job_submitted) messages when redis is disabled.

- **data**: a dictionnary of requests.
- **example**:
```json
{
  "timestamp": 10.0,
  "type": "QUERY",
  "data": {
    "requests": {"consumed_energy": {}}
  }
}
```
or
```json
{
  "timestamp": 10.0,
  "type": "QUERY",
  "data": {
    "requests": {"processor_temperature_all": {}}
  }
}
```
or
```json
{
  "timestamp": 10.0,
  "type": "QUERY",
  "data": {
    "requests": {
      "estimate_waiting_time": {
        "job_id": "workflow_submitter0!potential_job17",
        "job": {
          "res": 1,
          "walltime": 12.0
        }
      }
    }
  }
}
```

### ANSWER

This is a reply to a [QUERY](#query) message.

- **data**: See [QUERY](#query) documentation
- **example**:
```json
{
  "timestamp": 10.0,
  "type": "ANSWER",
  "data": {"consumed_energy": 12500.0}
}
```
or
```json
{
  "timestamp": 10.0,
  "type": "ANSWER",
  "data": {"processor_temperature_all": {"0":51.21, "1":25.74, "2":21.53}}
}
```
or
```json
{
  "timestamp": 10.0,
  "type": "ANSWER",
  "data": {
    "estimate_waiting_time": {
      "job_id": "workflow_submitter0!potential_job17",
      "estimated_waiting_time": 56
    }
  }
}
```

### NOTIFY

This message allows a peer to notify something to its counterpart.
There is no expected acknowledgement when sending such message.

For now, Batsim can **notify** the scheduler of the following:
- **no_more_static_job_to_submit**: Batsim tells the scheduler that it has no more jobs to submit from the static submitters. This means that all jobs in the workloads have already been submitted to the scheduler and the scheduler cannot expect more jobs to arrive (except the potential ones through dynamic submission).

For now, the scheduler can **notify** Batsim of the following:
- **submission_finished**: The scheduler tells Batsim that dynamic job submissions are over, which allows Batsim to stop the simulation. This message **MUST** be sent if ``"scheduler_submission": {"enabled": true}`` is configures, in order to finish the simulation. See [Configuration documentation](./configuration.md) for more details.
- **continue_submission**: The scheduler tells Batsim that it have sent a ``submission_finished`` NOTIFY prematurely and that Batsim should re-enable dynamic submissions of jobs.


- **data**: the type of notification
- **example**:
```json
{
  "timestamp": 23.50,
  "type": "NOTIFY",
  "data": { "type": "no_more_static_job_to_submit" }
}
```
or
```json
{
  "timestamp": 42.0,
  "type": "NOTIFY",
  "data": { "type": "submission_finished" }
}
```


---

## Batsim to Scheduler events

These events are sent by Batsim to the scheduler.
```
BATSIM ---> SCHEDULER
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
  - **nb_compute_resources**: the number of compute resources
  - **nb_storage_resources**: the number of storage resources
  - **allow_time_sharing**: whether time sharing is enabled or not
  - **config**: the Batsim configuration
  - **compute_resources**: informations about the compute resources
    - **id**: unique resource number
    - **name**: resource name
    - **state**: resource state in {sleeping, idle, computing, switching_on, switching_off}
    - **properties**: the properties specified in the SimGrid platform for the corresponding host
  - **storage_resources**: informations about the storage resources
  - **workloads**: the map of workloads given to Batsim. The key is the unique id of the workload
    and the value is the absolute path of the workload.
   Note that this unique id prefixes each job (before the ``!``).
  - **profiles**: the map of profiles given to Batsim. The key is the unique id of a workload and
    the value is the list of profiles of that workload.
- **example**:
```json
{
  "now": 0,
  "events": [
    {
      "timestamp": 0,
      "type": "SIMULATION_BEGINS",
      "data": {
        "nb_resources": 4,
        "nb_compute_resources": 4,
        "nb_storage_resources": 0,
        "allow_time_sharng": false,
        "config": {
          "redis": {
            "enabled": false,
            "hostname": "127.0.0.1",
            "port": 6379,
            "prefix": "default"
          },
          "job_submission": {
            "forward_profiles": false,
            "from_scheduler": {
              "enabled": false,
              "acknowledge": true
            }
          }
        },
        "compute_resources": [
          {
            "id": 0,
            "name": "Bourassa",
            "state": "idle",
            "properties": {}
          },
          {
            "id": 1,
            "name": "Fafard",
            "state": "idle",
            "properties": {}
          },
          {
            "id": 2,
            "name": "Ginette",
            "state": "idle",
            "properties": {}
          },
          {
            "id": 3,
            "name": "Jupiter",
            "state": "idle",
            "properties": {}
          }
        ],
        "storage_resources": [],
        "workloads": {
          "26dceb": "/home/mmercier/Projects/batsim/workloads/test_workload_profile.json"
        },
        "profiles": {
          "26dceb":{
            "simple": {
              "type": "msg_par",
              "cpu": [5e6,  0,  0,  0],
              "com": [5e6,  0,  0,  0,
                      5e6,5e6,  0,  0,
                      5e6,5e6,  0,  0,
                      5e6,5e6,5e6,  0]
            },
            "homogeneous": {
              "type": "msg_par_hg",
              "cpu": 10e6,
              "com": 1e6
            },
            "homogeneous_no_cpu": {
              "type": "msg_par_hg",
              "cpu": 0,
              "com": 1e6
            },
            "homogeneous_no_com": {
              "type": "msg_par_hg",
              "cpu": 2e5,
              "com": 0
            },
            "sequence": {
              "type": "composed",
              "repeat" : 4,
              "seq": ["simple","homogeneous","simple"]
            },
            "delay": {
              "type": "delay",
              "delay": 20.20
            },
            "homogeneous_total": {
              "type": "msg_par_hg_tot",
              "cpu": 10e6,
              "com": 1e6
            }
          }
        }
      }
    }
  ]
}
```

### SIMULATION_ENDS

Sent when Batsim thinks that the simulation is over. It means that all the jobs
(either coming from Batsim workloads/workflows inputs, or dynamically submitted) 
have been submitted and executed (or rejected). The scheduler should
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
``{"job_submission": { "from_scheduler": {"enabled": true}}}``), this message
is sent as a reply to a [SUBMIT_JOB](#submit_job) message if and only if
dynamic job submissions acknowledgements are enabled
(``{"job_submission": {"from_scheduler": {"acknowledge": true}}}``).

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
      "id": "dyn!my_new_job",
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
      "id": "dyn!my_new_job",
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
  - **job_state**: the job state. Possible values: "NOT_SUBMITTED", "SUBMITTED", "RUNNING", "COMPLETED_SUCCESSFULLY", "COMPLETED_FAILED", "COMPLETED_WALLTIME_REACHED", "COMPLETED_KILLED", "REJECTED"
  - **return_code**: the return code of the job process (equals to 0 by default)
  - **kill_reason**: the kill reason (if any)
  - **alloc**: the allocation that this job has, same as in [EXECUTE_JOB](#execute_job) message
    message)
- **example**:
```json
{
  "timestamp": 80.087881,
  "type": "JOB_COMPLETED",
  "data": {
    "job_id": "26dceb!4",
    "job_state": "COMPLETED_SUCCESSFULLY",
    "return_code": 0,
    "kill_reason": "",
    "alloc": "0-3"
  }
}
```

### JOB_KILLED

Some jobs have been killed.


This message acknowledges that the actions coming from a previous
[KILL_JOB](#kill_job) message have been done.
The ``job_ids`` jobs correspond to those requested in the previous
[KILL_JOB](#kill_job) message)

The ``job_progress`` map is also given for all the jobs (and for the tasks
inside the jobs) that have been killed. Key is the ``job_id`` and the value
contains a progress value in \]0, 1\[, where 0 means not started and 1 means
completed.
The profile name is also given for convenience.
For sequential jobs, the progress map contains the 0-based index of the inner
task that was running at the time it was killed, and the details of this
progress are in the ``current_task`` field.
Please note that sequential jobs can be nested.

Please remark that this message does not necessarily mean that all the jobs
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

- **data**: an interval set of resource ids and the new state
- **example**:
```json
{
  "timestamp": 10.0,
  "type": "RESOURCE_STATE_CHANGED",
  "data": {"resources": "1 2 3-5", "state": "42"}
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
BATSIM <--- SCHEDULER
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

Execute a job on a given set of resources.

An optional ``mapping`` field can be added to tell Batsim how to map executors
to resources: where the executors will be placed inside the allocation (resource
numbers are shifted to 0). It can be seen as MPI rank to host mapping. It only
works for the ``smpi`` job profile type.
The following example overrides the default round robin mapping to put the first
two ranks (0 and 1) on the first allocated machine (0, which stands for resource
id 2), and the last two ranks (2 and 3) on the second machine (1, which stands
for resource id 3).

For certain job profiles that involve storage you may need to define a
``storage_mapping`` between the storage label defined in the job profile
definition and the storage resource id on the platform. For example, the job
profile of type ``msg_par_hg_pfs_tiers`` contains this field ``"storage": "pfs"``.
In order to select what is the resource that corresponds to the
``"pfs"`` storage, you should provide a mapping for this label:
``"storage_mapping": { "pfs": 2 }``. If no mapping is provided, Batsim will guess
the storage mapping only if one storage resource is provided on the platform.

Another optional field is ``additional_io_job`` that permits the scheduler 
to add a job, that represents the IO traffic, dynamically at execution
time. This dynamicity is necessary when the IO traffic depends on the job
allocation. It only works for MSG based job profile types for the additional IO
job and the job itself. The given IO job will be merged to the actual job before
its execution. The additional job allocation may be different from the job
allocation itself, for example when some IO nodes are involved.

- **data**: A job id, an allocation, a mapping (optional), an additional IO job
  (optional)
- **example**:
```json
{
  "timestamp": 10.0,
  "type": "EXECUTE_JOB",
  "data": {
    "job_id": "w12!45",
    "alloc": "2-3",
    "mapping": {"0": "0", "1": "0", "2": "1", "3": "1"}
  },
  "storage_mapping": {
    "pfs": 2
  },
  "additional_io_job": {
    "alloc": "2-3 5-6",
    "profile_name": "my_io_job",
    "profile": {
      "type": "msg_par",
      "cpu": 0,
      "com": [0  ,5e6,5e6,5e6,
              5e6,0  ,5e6,0  ,
              0  ,5e6,4e6,0  ,
              0  ,0  ,0  ,0  ]
    }
  }
}
```

### CALL_ME_LATER

Asks Batsim to call the scheduler later on, at a given timestamp.

- **data**: future timestamp (float)
- **example**:
```json
{
  "timestamp": 10.0,
  "type": "CALL_ME_LATER",
  "data": {"timestamp": 25.5}
}
```


### KILL_JOB
Kill some jobs (almost instantaneously).

As soon as all the jobs defined in the ``job_ids`` field have completed
(most probably killed, but they may also have finished *ordinarily* before
the kill),
Batsim acknowledges it with one [JOB_KILLED](#job_killed) event.

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
The submission is acknowledged by default, but acknowledgments can be disabled
in the configuration
(``{"job_submission": {"from_scheduler": {"acknowledge": false}}}``).

**Note:** The workload name SHOULD be present in the job description id field
with the notation ``WORKLOAD!JOB_NAME``. If it is not present it will be added
to the job description provided in the acknowledgment message
[JOB_SUBMITTED](#job_submitted)

- **data**: A job id (job id duplication is forbidden), classical job and
  profile information (optional).

- **example with redis** : the job description, and the profile description if
  unknown to Batsim yet, must have been pushed into redis by the
  scheduler before sending this message.
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
      "id": "dyn!my_new_job",
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

As soon as all the resources have been set into the given state,
Batsim acknowledges it by sending one
[RESOURCE_STATE_CHANGED](#resource_state_changed) event.

- **data**: an interval set of resource id, and the new state
- **example**:
```json
{
  "timestamp": 10.0,
  "type": "SET_RESOURCE_STATE",
  "data": {"resources": "1 2 3-5", "state": "42"}
}
```

### SET_JOB_METADATA

This message is a convenient way to attach metadata to a job during
simulation runtime that will appear in the final result file.  A column
named ``metadata`` will be present in the output file ``PREFIX_job.csv``
with the string provided by the scheduler, or an empty string if not set.

**Note**: If you need to add **static** metadata to a job you can simply
add one or more fields in the job profile.

**Warning**: This not a way to delegate to batsim the storage of metadata.
That should be done though Redis, (when you have to share information
between different process for example), or using the scheduler's internal
data structures.

- **data**: a job id and its metadata
- **example**:
```json
{
  "timestamp": 13.0,
  "type": "SET_JOB_METADATA",
  "data": {
    "job_id": "wload!42",
    "metadata": "scheduler-defined string"
  }
}
```

### CHANGE_JOB_STATE

Changes the state of a job, which may be helpful to implement schedulers with
complex dynamic jobs.

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
Depending on the [configuration](./configuration.md), jobs information might
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
(depending whether Redis is enabled or not).

### Without Redis
![dynamic_submission](protocol_img/dynamic_job_submission.png)

### With Redis
![dynamic_submission_redis](protocol_img/dynamic_job_submission_redis.png)
