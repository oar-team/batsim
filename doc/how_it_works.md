# Case study 1: j1 completion -> execute j2 and j3
In a real system, the scheduling algorithm is called from time to time to
make some decisions.
This case study consists in making the decision to execute two jobs (j2 and j3)
on job j1 completion. However, the decision-making procedure takes some time and
makes the decision in an online fashion: the decision to execute j1 is made
before the decision to execute j2.

## Protocol
In a Batsim simulation, most decisions are taken in another (Linux) process.
This decision-making process will simply be referred to as the Scheduler from now on.

The two processes communicate via a protocol. In this protocol, the Scheduler
must answer one message to each Batsim message. The messages may contain
multiple events. In this case, the messages would be:

![case1_overview_figure](protocol_img/case1_overview.png)

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
- did something until time 13
- first chose, at time 13, to execute job 2 on machines 0 and 1.
- did something until time 14
- then chose, at time 14, to execute job 3 on machines 2 and 3.
- did something until time 15 (``"now": 15.0``) and finished making its decisions.

