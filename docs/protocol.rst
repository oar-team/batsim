.. _protocol:

Protocol
========

A Batsim simulation consists in two main parts:

- Batsim itself, in charge of simulating what happens on the platform.
- An *External Decision Component* (EDC), in charge of making decisions.

The two parts communicate via a protocol, called the *batprotocol*.
The protocol is synchronous and follows a simple request-reply pattern.
Whenever an event which may require making decision occurs in Batsim during the simulation, the following steps occur:

1. Batsim suspends the simulation
2. Batsim sends a request to the EDC, telling it what happened on the platform
3. Batsim waits and receives the reply from the EDC (blocking call)
4. Batsim resumes the simulation, applying the decisions which have been made

.. image:: img/proto/request_reply.png
    :width: 100%
    :alt: Protocol overview


The communication between Batsim and the EDC depends on whether the EDC is connected as a library or as a process (see also :ref:`EDC implementation<tuto_sched_implem>` for more details).

Communication with an EDC process is performed by exchanging messages using the `ZeroMQ request-reply pattern`_.
Batsim uses a ZMQ REQ socket to send its messages to the EDC, while the EDC uses a ZMQ REP socket to send its replies.

Communication with an EDC library is performed by calling specific functions that must be implemented by the library.


Initialization phase
--------------------

During the initialization phase of Batsim, and before the simulation can start, there is a handshake between Batsim and the EDC.

First, Batsim sends a message containing the string of initialization data of the EDC, as provided by the :ref:`cli` arguments.
The message is binary-formatted and contains the size of the initialization string buffer and the initialization string itself.


Then the EDC answers with a message containing two parts.
The first part contains initialization flags.
There is currently one flag expected by Batsim: the serialization format that will be used to exchange messages via the batprotocol.
The flag value ``0x1`` should be selected to use Flatbuffer's binary format, and flag value ``0x2`` to use the JSON format.
The second part contains a protocol message which must be formatted using the format defined in the flags.
This protocol message must contain only one event: ``EDCHelloEvent``.

The ``EDCHelloEvent`` contains multiple informations on the EDC, such as its name, version and the version of the batprotocol it uses, and a set of requested simulation features.
See ``EDCHelloEvent`` and `Simulation features`_ for more details.

.. note::
    When using EDC as a library the messages exchanged with Batsim are not sent/received, but transmitted via the library API.
    During the initialization phase, Batsim calls the EDC library's ``batsim_edc_init`` function.
    The string of initialization data can be retrieved from the parameters ``init_data`` and ``init_size``.
    The EDC must fill the parameters ``flags`` with the flags, and must fill the ``reply_data`` and ``reply_size`` parameters with the protocol message containing the ``EDCHelloEvent`` correctly formatted.

.. _simulation_features:

Simulation features
-------------------

The behavior of specific events of this protocol depends on the simulation features that are requested in the ``EDCHelloEvent``.

Here is the list of the simulation features that may be requested:

- ``dynamic_registration``: If set to ``true`` the dynamic registration of jobs and profiles will be enabled for this EDC. This means the EDC must send a ``FinishRegistrationEvent`` so the simulation can end.
- ``profile_reuse``: If set to ``true`` any existing profile can be used to register dynamic jobs. Otherwise the EDC must make sure that the desired profile has not been garbage collected from Batsim's memory (which happens when the last job that references a profile completes) when registering a job.
- ``acknowledge_dynamic_jobs``: If set to ``true`` Batsim will emit a ``JobSubmittedEvent`` to this EDC when the EDC registers a job.
- ``forward_profiles_on_job_submission``: If set to ``true`` Batsim will include profiles information in ``JobSubmittedEvent``.
- ``forward_profiles_on_jobs_killed``: If set to ``true`` Batsim will include profile information in ``JobsKilledEvent``.
- ``forward_profiles_on_simulation_begins``: If set to ``true`` Batsim will include profile information in ``SimulationBeginsEvent``.


Message composition
-------------------

The communication uses the *Batprotocol* to ease the reading and writing of messages and simulation events, which internally relies on `Flatbuffer`_.
The protocol supports serialization of messages in binary or as a JSON object.


A message contains two fields: the ``now`` field and the ``events`` field.
The ``now`` field (float value) is a timestamp that defines the current simulation time.

- If the message comes from Batsim, it means that the EDC cannot make decisions before ``now`` as it would change the past.
- If the message comes from the EDC, it tells Batsim that the EDC finished making its decisions at timestamp ``now``.
  This is used by Batsim to know when the EDC will be available for making new decisions.

The ``events`` field defines a sequence of events.
Each event must contain a field ``timestamp`` and there are constraints on their values:

- The message timestamp ``now`` **must** be greater or equal to every event ``timestamp``.
- In the sequence of events (field ``events`` of a message), the event timestamps **must** be in (non-strictly) ascending order.


The various event types are defined in details in the `Batprotocol API`_.
See `Table of Events`_ below for a quick list.
The behavior of specific events of this protocol depends on the requested `Simulation features`_ asked by the EDC during the initialization phase of the simulation.


.. note::
    When using EDC as a library the messages exchanged with Batsim are not sent/received, but transmitted via the library API.
    During the simulation loop, Batsim calls the EDC library's ``batsim_edc_take_decisions`` function.
    The content of the message from Batsim can be retrieved from the parameters ``what_happened`` and ``what_happened_size``.
    The answer message of the EDC must be put in the ``decisions`` and ``decisions_size`` parameters.


Table of Events
---------------

Here is the list of all events described in the batprotocol.
More details on each event can be found in the `Batprotocol API`_, in particular the list of attributes of each event.

From Batsim to the External Decision Component:

- `SimulationBeginsEvent <SimulationBeginsEvent_>`_
- `SimulationEndsEvent <SimulationEndsEvent_>`_
..
- `JobSubmittedEvent <JobSubmittedEvent_>`_
- `JobCompletedEvent <JobCompletedEvent_>`_
- `JobsKilledEvent <JobsKilledEvent_>`_
..
- `RequestedCallEvent <RequestedCallEvent_>`_
- `AllStaticJobsHaveBeenSubmittedEvent <AllStaticJobsHaveBeenSubmittedEvent_>`_
- `AllStaticExternalEventsHaveBeenInjectedEvent <AllStaticExternalEventsHaveBeenInjectedEvent_>`_
- `ExternalEventOccurredEvent <ExternalEventOccurredEvent_>`_
..
- `HostsPStateChangedEvent <HostsPStateChangedEvent_>`_
- `HostsTurnedOnOffEvent <HostsTurnedOnOffEvent_>`_

From the External Decision Component to Batsim:

- `EDCHelloEvent <EDCHelloEvent_>`_
- `ForceSimulationStopEvent <ForceSimulationStopEvent_>`_
..
- `ExecuteJobEvent <ExecuteJobEvent_>`_
- `RejectJobEvent <RejectJobEvent_>`_
- `KillJobsEvent <KillJobsEvent_>`_
..
- `RegisterProfileEvent <RegisterProfileEvent_>`_
- `RegisterJobEvent <RegisterJobEvent_>`_
- `FinishRegistrationEvent <FinishRegistrationEvent_>`_
..
- `CallMeLaterEvent <CallMeLaterEvent_>`_
- `StopCallMeLaterEvent <StopCallMeLaterEvent_>`_
..
- `CreateProbeEvent <CreateProbeEvent_>`_
- `StopProbeEvent <StopProbeEvent_>`_
- `TriggerProbeEvent <TriggerProbeEvent_>`_
- `ResetProbeEvent <ResetProbeEvent_>`_
- `ProbeDataEmittedEvent <ProbeDataEmittedEvent_>`_
..
- `ChangeHostsPStateEvent <ChangeHostsPStateEvent_>`_
- `TurnOnOffHostsEvent <TurnOnOffHostsEvent_>`_


Events from Batsim to the External Decision Component
-----------------------------------------------------

`SimulationBeginsEvent <link_SimuBegins_>`_
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Sent at the beginning of the simulation, this event contains various information about Batsim's command-line options, the simulation context, the platform composition and the workloads used.



`SimulationEndsEvent <link_SimuEnds_>`_
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Sent when Batsim thinks that the simulation is over. It means that all the jobs (either coming from Batsim workloads inputs, or dynamically submitted) have been submitted and executed (or rejected).

When receiving a `SimulationEndsEvent`, the EDC should answer a message without events, close its socket if it is an EDC as a process, and then terminate.


`JobSubmittedEvent <link_JobSub_>`_
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The content of this event depends on how Batsim has been called (see :ref:`cli` and `Simulation features`_).

This event means that one job has been submitted within Batsim. It is
sent whenever a job coming from Batsim workload inputs has been submitted. If dynamic jobs registration is enabled, this
event is sent as a reply to a `RegisterJobEvent <RegisterJobEvent_>`_ if and only if dynamic jobs registration acknowledgments are also enabled.
More information can be found in `Dynamic registration of jobs and profiles`_.

A JSON description of the job profile is sent if and only if profiles forwarding is enabled (see `Simulation features`_).


`JobCompletedEvent <link_JobComp_>`_
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This event means that a job has completed its execution.
It acknowledges that the actions coming from a previous `ExecuteJobEvent <ExecuteJobEvent_>`_ have been done (successfully or not, depending on whether the job completed without reaching timeout).


`JobsKilledEvent <link_JobsKilled_>`_
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This event means that some jobs have been killed.
It acknowledges that the actions coming from a previous `KillJobsEvent <KillJobsEvent_>`_ have been done.
The ``job_ids`` correspond to those requested in the previous `KillJobsEvent <KillJobsEvent_>`_.

The ``progresses`` object is also given for all the jobs (and for the
tasks inside the jobs) that have been killed. The ``progress`` value contains a progress value in ]0, 1[, where 0 means not started
and 1 means completed. The profile name is also given for convenience.
For jobs with a composed profile the progress representation or more complex, see the `batprotocol API <link_JobsKilled_>`_ for more details.

Please remark that this event does not necessarily mean that all the
jobs have been killed. It means that all the jobs have completed. Some
of the jobs might have completed *ordinarily* before the kill. In this
case, several `JobCompletedEvent <JobCompletedEvent_>`_ corresponding to the aforementioned jobs should be received before the ``JobsKilledEvent``.



`RequestedCallEvent <link_ReqCall_>`_
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This event is a response to a `CallMeLaterEvent <CallMeLaterEvent_>`_.


`AllStaticJobsHaveBeenSubmittedEvent <link_AllJobs_>`_
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Batsim tells the EDC that it has no more jobs to submit from the static workload submitters. This means that all jobs in the workloads have already been submitted to the EDC and the EDC cannot expect more jobs to arrive (except the potential ones through dynamic submission).


`AllStaticExternalEventsHaveBeenInjectedEvent <link_AllEvents_>`_
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Only applicable if a list of external events are given as input to Batsim via the ``--external-events`` :ref:`cli` option. Batsim tells the EDC that there is no more external event to occur from the external event submitters. That means that all external events have occurred and the EDC cannot expect a new external event to occur.

`ExternalEventOccurredEvent <link_ExtEvent_>`_
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Batsim tells the EDC that an external event has occurred.


`HostsPStateChangedEvent <link_HostPS_>`_
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This event means that the PState of some resources has changed.
It acknowledges that the actions coming from a previous `ChangeHostsPStateEvent <ChangeHostsPStateEvent_>`_ have been done.


`HostsTurnedOnOffEvent <link_HostTurned_>`_
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This event means that some resources has been turned ON or OFF.
It acknowledges that the actions coming from a previous `TurnOnOffHostsEvent <TurnOnOffHostsEvent_>`_ have been done.



Events from the External Decision Component to Batsim
-----------------------------------------------------

`EDCHelloEvent <link_EDCHello_>`_
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This event must be sent by the EDC during the `Initialization phase`_.

`ForceSimulationStopEvent <link_ForceStop_>`_
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Asks Batsim to forcefully stop the simulation, dropping all subsequent events in the same protocol message (sent by the EDC) and dropping all events that Batsim would have sent to the EDC.
Batsim should send only a `SimulationEndsEvent <SimulationEndsEvent_>`_ in its next message to the EDC.


`ExecuteJobEvent <link_ExecJob_>`_
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Execute a job on a given :ref:`interval_set` of resources.

.. todo::
    Document the optionnal attributes of the event related to ``executor_placement_type``, ``mapping``, ``profile_allocation_override``, ``storage_placement``


`RejectJobEvent <link_RejJob_>`_
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Rejects a job that has already been submitted.
The rejected job will not appear into the final jobs trace.


`KillJobsEvent <link_KillJob_>`_
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Kill some jobs (almost instantaneously).

As soon as all the jobs defined in the ``job_ids`` field have completed
(most probably killed, but they may also have finished *ordinarily*
before the kill), Batsim acknowledges it with one `JobsKilledEvent <JobsKilledEvent_>`_.


`RegisterProfileEvent <link_RegisProf_>`_
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Registers a profile (from the EDC).

Jobs registration from the EDC must be enabled (see `Simulation features`_).
More information can be found in `Dynamic registration of jobs and profiles`_.


`RegisterJobEvent <link_RegisJob_>`_
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Registers a job (from the EDC) at the current simulation time.

Jobs registration from the EDC must be enabled (see `Simulation features`_).
Acknowledgment of registrations can be enabled (see `Simulation features`_).
More information can be found in `Dynamic registration of jobs and profiles`_.

**Important note:** The workload name MUST be present in the job description id field with the notation ``WORKLOAD!JOB_NAME``.


`FinishRegistrationEvent <link_FinishReg_>`_
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The EDC tells Batsim that dynamic job registrations are over, therefore allowing Batsim to stop the simulation eventually. This event **MUST** be sent if dynamic jobs registration is enabled (see `Simulation features`_).


`CallMeLaterEvent <link_CallMe_>`_
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Asks Batsim to call the EDC later on, at a given timestamp or periodically.

Batsim will send a `RequestedCallEvent <RequestedCallEvent_>`_ when the desired timestamp is reached.


`StopCallMeLaterEvent <link_StopCall_>`_
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Asks Batsim to stop the periodic call, as asked by a previous `CallMeLaterEvent <CallMeLaterEvent_>`_.



`CreateProbeEvent <link_CreatePr_>`_
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. todo::
    Document probes management

`StopProbeEvent <link_StopPr_>`_
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

`TriggerProbeEvent <link_TriggerPr_>`_
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

`ResetProbeEvent <link_ResetPr_>`_
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

`ProbeDataEmittedEvent <linkDataPr_>`_
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~



`ChangeHostsPStateEvent <link_ChangeHost_>`_
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Asks Batsim to change the PState of some resources.
Batsim will send a `HostsPStateChangedEvent <HostsPStateChangedEvent_>`_ when the actions have been done.

`TurnOnOffHostsEvent <link_TrunHost_>`_
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Asks Batsim to turn ON or OFF some resources.
Batsim will send a `HostsTurnedOnOffEvent <HostsTurnedOnOffEvent_>`_ when the actions have been done.



.. _dynamic_job_registration:

Dynamic registration of jobs and profiles
-----------------------------------------

Jobs are in most cases given as Batsim inputs, which are submitted by Batsim (the EDC knows about them via ``JobSubmittedEvent`` events of the batprotocol).

However, jobs can also be submitted from the EDC (via registration events) throughout the simulation.
For this purpose:

- Dynamic jobs registration **must** be enabled (see `Simulation features`_).
- The EDC **must** tell Batsim when it has finished registering dynamic jobs (via a ``FinishRegistrationEvent``).
  Otherwise, Batsim will wait for new simulation events forever, causing either a SimGrid deadlock or an infinite loop at the end of the simulation.
- The EDC **must** make sure that Batsim has enough information to avoid SimGrid deadlocks during the simulation.
  If at some simulation time all Batsim workloads inputs have been executed and nothing is happening on the platform, this might lead to a SimGrid deadlock.
  If the EDC knows that it will register a dynamic job in the future, it should ask Batsim to call it at this timestamp via a ``CallMeLaterEvent``.

The protocol behavior of dynamic registrations is customizable (see `Simulation features`_).
Batsim might or might not send acknowledgments when jobs have been registered.

An example of dynamic jobs and profiles registration can be found in the ``dyn-register`` `EDC library`_ of Batsim's tests.

The following figure outlines how a dynamic job registration should be done

.. image:: img/proto/dynamic_job_submission.png
   :width: 75 %
   :alt: Dynamic job submission

.. todo::

    Update the dyn-register EDC library link to the master branch

.. _ZeroMQ request-reply pattern: https://zguide.zeromq.org/docs/chapter1/#Ask-and-Ye-Shall-Receive
.. _Flatbuffer: https://flatbuffers.dev/
.. _Batprotocol API: https://batprotocol.readthedocs.io/en/latest/api.html
.. _EDC library: https://framagit.org/batsim/batsim/-/blob/batprotocol/test/edc-lib/dyn-register.cpp

.. _link_SimuBegins: https://batprotocol.readthedocs.io/en/latest/api.html#simulation-begins
.. _link_SimuEnds: https://batprotocol.readthedocs.io/en/latest/api.html#simulation-end
.. _link_JobSub: https://batprotocol.readthedocs.io/en/latest/api.html#job-submitted
.. _link_JobComp: https://batprotocol.readthedocs.io/en/latest/api.html#job-completed
.. _link_JobsKilled: https://batprotocol.readthedocs.io/en/latest/api.html#jobs-killed
.. _link_ReqCall: https://batprotocol.readthedocs.io/en/latest/api.html#requested-call
.. _link_AllJobs: https://batprotocol.readthedocs.io/en/latest/api.html#all-static-jobs-have-been-submitted
.. _link_AllEvents: https://batprotocol.readthedocs.io/en/latest/api.html#all-static-external-events-have-been-injected
.. _link_ExtEvent: https://batprotocol.readthedocs.io/en/latest/api.html#external-event-occurred
.. _link_HostPS: https://batprotocol.readthedocs.io/en/latest/api.html#hosts-pstate-changed
.. _link_HostTurned: https://batprotocol.readthedocs.io/en/latest/api.html#hosts-turned-on-off
.. _link_EDCHello: https://batprotocol.readthedocs.io/en/latest/api.html#external-decision-component-hello
.. _link_ForceStop: https://batprotocol.readthedocs.io/en/latest/api.html#force-simulation-stop
.. _link_ExecJob: https://batprotocol.readthedocs.io/en/latest/api.html#execute-jobs
.. _link_RejJob: https://batprotocol.readthedocs.io/en/latest/api.html#reject-jobs
.. _link_KillJob: https://batprotocol.readthedocs.io/en/latest/api.html#kill-jobs
.. _link_RegisProf: https://batprotocol.readthedocs.io/en/latest/api.html#register-profile
.. _link_RegisJob: https://batprotocol.readthedocs.io/en/latest/api.html#register-job
.. _link_FinishReg: https://batprotocol.readthedocs.io/en/latest/api.html#finish-registration
.. _link_CallMe: https://batprotocol.readthedocs.io/en/latest/api.html#call-me-later
.. _link_StopCall: https://batprotocol.readthedocs.io/en/latest/api.html#stop-call-me-later
.. _link_CreatePr: https://batprotocol.readthedocs.io/en/latest/api.html#create-probe
.. _link_StopPr: https://batprotocol.readthedocs.io/en/latest/api.html#stop-probe
.. _link_TriggerPr: https://batprotocol.readthedocs.io/en/latest/api.html#trigger-probe
.. _link_ResetPr: https://batprotocol.readthedocs.io/en/latest/api.html#reset-probe
.. _linkDataPr: https://batprotocol.readthedocs.io/en/latest/api.html#probe-data-emitted
.. _link_ChangeHost: https://batprotocol.readthedocs.io/en/latest/api.html#change-hosts-pstate
.. _link_TrunHost: https://batprotocol.readthedocs.io/en/latest/api.html#turn-on-off-hosts
