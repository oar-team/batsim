# Introduction #

[The Batsim protocol](proto_description.md) is used to synchronise Batsim and
the Decision process. However, to keep this protocol as simple as possible,
metadata (notably associated to jobs) are shared via the Redis data storage.

This data storage mechanism associates *values* to *keys*.
These pairs are written (**set**) by Batsim, and can be read (**get**) by the
Decision process.


# Keys Prefix #

Since several Batsim instances can be run at the same time, all the keys
explained in this document must be prefixed by some instance-specific prefix.
At the moment, this prefix is set to the absolute filename of the socket used
in [the Batsim protocol](proto_description.md), followed by a colon ':'.


# List of Used keys #

This document gathers the different keys used in the data storage.


## Platform size ##

The size of the simulation platform (the number of computing entities) is set
in the **nb_res** key. The value is a string representation of an unsigned
integer.

## Jobs' Information ##

The jobs are identified by a string.
Let's name **jID** the job identifier of job *j*.

As soon as *j* is submitted within Batsim, the corresponding job and profiles
are set, respectively into the **job_jID** and **profile_jID** keys.
The values associated with these keys are in JSON, detailed below.

### Job JSON details ###
Here is job JSON description string:
``` json
{
  "id":1,
  "subtime":10,
  "walltime": 100,
  "res": 4,
  "profile": "2"
}
```

This string is a JSON object, which must contain the following keys:
- **id**, the job number within a given workload
- **subtime**, the minimum time at which the job can have been submitted
- **walltime**, the user-given upper bound on the job execution time
- **res**, the number of required resources
- **profile**, the profile associated with the job

More information can be added into the JSON object, depending on Batsim's
input files.

### Profile JSON details ###
A profile describes how a job should be computed. This information can be
used in clairvoyant schedulers. The profiles can be quite different depending
on the computation model you wish to use. Here are some examples of profile
strings:

``` json
{
    "type": "msg_par",
    "cpu": [5e6,5e6,5e6,5e6],
    "com": [5e6,5e6,5e6,5e6,
            5e6,5e6,5e6,5e6,
            5e6,5e6,5e6,5e6,
            5e6,5e6,5e6,5e6]
}
```

``` json
{
    "type": "msg_par_hg",
    "cpu": 10e6,
    "com": 1e6
}
```

``` json
{
    "type": "composed",
    "nb" : 4,
    "seq": ["1","2","1"]
}
```

``` json
{
    "type": "delay",
    "delay": 20.20
}
```
