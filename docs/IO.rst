File System model
-----------------

Storage
~~~~~~~

The model chosen for the storage is to use a simple SimGrid host, linked
to a compute node for local disk, or anywhere in the topology for
centralized storage. The storage access latency and read/write bandwidth
is modeled by two SimGrid links that attached the storage node to the
rest of the platform. This model allows to simulate any kind of file
system topology, from classical DFS, by adding a storage to every node
of the platform, to centralized storage with a single large storage
node, and including any mix of the two.

Here is an example of storage node added to the Taurus platform that represents
an HDD:

.. code:: xml

    <host id="taurus-16.lyon.grid5000.fr_disk" speed="0">
      <prop id="role" value="storage"/>
    </host>
    <link bandwidth="150MBps" id="taurus-16.lyon.grid5000.fr_IO_read" latency="0.11ms"/>
    <link bandwidth="80MBps" id="taurus-16.lyon.grid5000.fr_IO_write" latency="0.11ms"/>

I/O transfer
~~~~~~~~~~~~

Now that the platform contains storage node, we need to model the I/O
transfer between them. Batsim implementation of I/O transfer are based
on the parallel task model described above. Every storage get a resource
ID that identify them in the platform with a unique number, just like
the computation resources. Using the storage ID and the normal resource
ID, data movement is simply modeled by a matrix of communication
involving computation node and storage nodes.

The genericity of the model is ensured by keeping it outside of Batsim.
Thus, the burden of maintaining the file system model is delegated to
the user’s defined process that is connected to Batsim called the
decision process, also called the scheduler.

The scheduler uses dynamic job to model file system internal moves like
DFS load balance, or data staging from PFS to Burst Buffers, but most of
I/O transfer are triggered by applications. But the application model
also need to be independent from the file system. So the application
model that is defined by the Batsim job profiles, provided to Batsim
with the workload file, needs to contain enough information for the
scheduler to generate actual I/O transfer at execution time. Thus, the
application model can be statically defined in the Batsim workload file,
but the I/O that is triggered by this application is dynamically
generated depending on the context by the scheduler.

Concretely, when a job is submitted in the workload, Batsim informs the
scheduler of this event with a job profile attached. This job profile
contains information about the quantity of I/O that need to be done by
the job, or any arbitrary information required by the file system model.
Then, the scheduler can choose where the application will be allocated,
and also, what are the I/O transfers that will occur during the
execution of this application. Once the decision is taken, the scheduler
send the job allocation to Batsim inside the job execution order as
usual, but includes also an additional I/O job. This I/O job has its own
allocation and the associated communication matrix that represent the
transfer between the compute nodes of the jobs and the storages.
Finally, Batsim will merge this I/O job profile with the normal job
profile before to delegate the actual simulation to SimGrid.

Full example
~~~~~~~~~~~~

.. todo::
   Add more detailed to the example:

   - the platform file
   - the full workload file
   - the launch command


Here is an example that illustrate this mechanism. Let’s assume that the
``job1`` was submitted to the scheduler with a request of 2 computing
resources. The profile of the job called is defined by a Json object,
``job1_profile``.

#. The scheduler receive the submission job from Batsim with this
   forwarded profile, where the quantity of I/O is just an information
   given to the scheduler (not read by Batsim):

   .. code:: json

      "job1_profile": {
          "type": "parallel_homogeneous_total",
          "cpu": 1e10,
          "com": 1e7,
          "io_reads":  8e8,
          "io_writes": 2e8
      }

#. The scheduler takes the decision to allocate this job on the
   resources 0 and 1. Then it decides, regarding its file system model,
   that each node will read half of the data from only one centralized
   storage (located at the resource id 10), but the nodes of the
   application will write on local their local disks (id 2 and 3). A new
   profile of type ``parallel`` is generated an submitted to Batsim with
   the name ``io_for_this_alloc_on_job1``. The kind job profile is
   composed of a computation matrix that may reflect I/O related
   computation (here set to 0), and a communication matrix that
   represents the aforementioned assumptions. Note that the direction of
   the transfer is from row to column, and that the indices of the
   matrix are mapped to the IO allocation, e.g. for the allocation of
   :math:`(0,1,2,3,10)` a value of :math:`4e8` on the 1st column and at
   the 5th row means that 400MB will be transferred from the resource 10
   to the resource 0.

   .. code:: json

      "io_for_this_alloc_on_job1": {
          "type": "parallel",
          "cpu": [0  ,0  ,0  ,0  ,0]
          "com": [0  ,0  ,1e8,0  ,0
          0  ,0  ,0  ,1e8,0
          0  ,0  ,0  ,0  ,0
          0  ,0  ,0  ,0  ,0
          4e8,4e8,0  ,0  ,0]
      }

#. The scheduler ask Batsim to execute the job with the given job
   allocation, and the additional I/O job.

   .. code:: json

      {
          "job_id":"08a582!1",
          "alloc":"0-1",
          "additional_io_job": {
              "alloc":"0-3,10",
              "profile": "io_for_this_alloc_on_job1"
          }
      }

#. Batsim merges the 2 profiles, and generates a new job with IO matrix
   that is then sent to SimGrid in order to be simulated:

   .. code:: json

      {
          "alloc": "0-3,10",
          "cpu": [5e9,5e9,0  ,0  ,0],
          "com": [0  ,5e6,1e8,0  ,0
                  5e6,0  ,0  ,1e8,0
                  0  ,0  ,0  ,0  ,0
                  0  ,0  ,0  ,0  ,0
                  4e8,4e8,0  ,0  ,0]
      }

   Note the difference of allocation between the job itself and the IO
   that it generates. Batsim is capable to merge any interval set of
   resource allocation, even if only part of the job’s nodes are taking
   part in the IO transfer.

This simple file system model is generic enough to simulate any
centralized and decentralized file system, because it not assume any
kind of I/O behavior. For example, it is possible to simulated
hierarchical file system like a PFS with I/O nodes, or a multi-tiers
storage setup with two different centralized file system, e.g. NFS and
Lustre, or even a mix of DFS and PFS.

It is fully dynamic: the I/O transfer inside the application are
generated online by the decision process, which allows to take the I/O
into account for any decision, from job allocation, to I/O gateway
selection. Also, dynamic job can be created to model internal file
system I/O transfer at any time during the simulation.

Limitations and Evolutions
~~~~~~~~~~~~~~~~~~~~~~~~~~

The file system model described above has some limitations that are discuss
here.

First, the storage model is very simple and do not reflect the fine
grain behavior of different kind of storage like HDD, SSD, of NVM. Also,
The storage model not include disk size limitation enforcement, and even
if this can be done the decision process, the contention behavior that
it implies is not modeled. To overcome the storage model limitation, it
would be possible to add a more realistic model into SimGrid, but it may
induce large changes in the underneath contention model.

The fact that the file system model is hold by the decision process
increase flexibility and permits to the Batsim users to model any kind
of behavior, but it has the drawback to lead to multiple implementation
of the same model (one for each scheduler). This can be overcome by
putting the file system model into an external process, which even more
realistic, but it increase the maintenance cost and hinder the model
genericity.

Another limitation is the absence of cache effects in the model, either
from the storage itself or from a second level like burst buffers. Also,
any cluster file system have metadata servers are also not taken into
account in our model. To allows to model fine grain behavior like the
metadata server, or the cache effects, it requires to add new features
inside Batsim, built on top of SimGrid directly, and thus putting the
file system model inside Batsim.
