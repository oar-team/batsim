# Batsim Job Profiles

In order to know what has to be simulated for each job of a workload, Batsim is
using the notion of job profile. Each job is associated to a profile, but a
profile can be associated to multiple jobs.

Each profile is defined in the workload JSON file in the ``profiles`` section.
The only common field on the profile is the ``type``. Here is a list of all the
profile types supported by Batsim, with an explaination on how they work and how
to use it.

**Note**: You can use scientific notation to represent big numbers, e.g.
``8e3`` for ``8x10^3``.

## Delay

This is the simplest profile. In fact there is no job execution, only a certain
amount of time. It does **NOT** take into account the platform at all.

### Example

Waiting for 20.20 seconds.

```json
{
    "type": "delay",
    "delay": 20.20
}
```

## MSG parallel task

This profile correspond to a parallel task executed simultaneously on each node
allocated to the job.

### Parameters

- ``cpu``: a vector containing the amount of flops to be compute on
  each nodes.
- ``com``: a vector containing the amount of bytes to be transfered between
  nodes. You can see this vector as matrix where host in row is sending to the
  host in column. When row equals column it is intranode communication using
  local loopback interface.

### Example

```json
{
    "type": "msg_par",
    "cpu": [5e6,  0,  0,  0],
    "com": [5e6,  0,  0,  0,
            5e6,5e6,  0,  0,
            5e6,5e6,  0,  0,
            5e6,5e6,5e6,  0]
}
```

## MSG parallel homogeneous task

This model is a convenient way to generate homogeneous task computation and
communication. The loopback communication is set to 0.

### Parameters
- ``cpu``: the amount of flops to be compute by each nodes.
- ``com``: the amount of bytes to be send and receive by each nodes.

### Example

```json
{
    "type": "msg_par_hg",
    "cpu": 10e6,
    "com": 1e6
}
```

## MSG parallel homogeneous task with total amount

This model is a convenient way to generate homogeneous task computation and
communication by giving the total amount work to be done. The loopback
communication is set to 0. It give to this job the ability to be allocated on
any number of resources while conserving the same amount of work to do.

### Parameters
- ``cpu``: the total amount of flops to be compute spread over all nodes: each
  node will have ``cpu / number of nodes`` amount of flops to compute.
- ``com``: the amount of bytes to be send and receive by each nodes: each
  node will have ``com / number of nodes`` amount of bytes to send and the same
  amount to receive.

### Example

```json
{
    "type": "msg_par_hg_tot",
    "cpu": 10e6,
    "com": 1e6
}
```


## Composed

This job profile is a list of profiles to be executed in a sequence.

### Parameters
- ``seq``: the list of profiles by name.
- ``repeat`` (optional): the number of times the sequence will be repeated (none by default).


### Example

```json
{
    "type": "composed",
    "repeat" : 4,
    "seq": ["prof1","prof2","prof1"]
}
```


## MSG homogeneous IO to/from a storage tier

Represents an IO transfer between all the nodes of a job's allocation and a
centralized storage tier. The storage tier is represented by one node.

### Parameters
- ``size``: the amount of bytes to be transfered.
- ``direction``: the direction of the transfer. Posible values are
  ``from_storage`` and ``to_storage``.
- ``storage``: The name of the storage. It will be map to a specific node at the job
  execution time. Default value is ``pfs``.

### Example

```json
{
    "type": "msg_par_hg_pfs_tiers"
    "size": 10e5,
    "direction": "to_storage",
    "storage": "nfs"
}
```

## MSG IO staging between two storage tiers

This profile represents an IO transfer between two storage tiers.

### Parameters
- ``size``: the amount of bytes to be transfered.
- ``from``: The name of the storage that send. It will be map to a specific node at the job execution time.
- ``to``: The name of the storage that receive. It will be map to a specific node at the job execution time.

### Example

```json
{
    "type": "data_staging",
    "size": 10e5,
    "from": "pfs",
    "to": "nfs"
}
```


