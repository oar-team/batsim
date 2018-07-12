# Overview
Batsim uses different git repositories.
The main repositories and their role are listed below:

- [github](https://github.com/oar-team/batsim)
  - the [issues](https://github.com/oar-team/batsim/issues) stores known bugs
    and enhancement proposals
  - [pull requests](https://github.com/oar-team/batsim/pulls) can be done to
    share your modification with us
- [gitlab](https://gitlab.inria.fr/batsim/batsim):
  - the [continuous integration](https://gitlab.inria.fr/batsim/batsim/pipelines)
    is built on Gitlab CI
  - the [dev board](https://gitlab.inria.fr/batsim/batsim/boards) (kanban)
    stores tasks *to do* and taks that are being done.

Furthermore, these repositories are in the Batsim ecosystem:
- [evalys](https://github.com/oar-team/evalys) includes visualisation tools of
  most Batsim outputs
- Different schedulers can be plugged to Batsim:
  - the schedulers from [OAR 3](https://github.com/oar-team/oar3), thanks to
    the [bataar](https://github.com/oar-team/oar3/blob/master/oar/kao/bataar.py)
    script.
  - [pybatsim](https://gitlab.inria.fr/batsim/pybatsim) contains resource
    management algorithms in Python 3.
  - [batsched](https://gitlab.inria.fr/batsim/batsched) contains general purpose
    and energy oriented resource management algorithms. It is implemented in C++.
  - [datsched](https://gitlab.inria.fr/batsim/datsched) contains some algorithms
    implemented in D.
  - Resource management algorithms in Rust can be found
    [there](https://gitlab.inria.fr/users/adfaure/projects)

# How to contribute?
If you encounter any bug in Batsim, please open an issue
[on github](https://github.com/oar-team/batsim/issues).
It will allow us to know that the bug exists, and will help a lot resolving the
problem ;).

If you want to request a new feature, you may contact us by email and/or
open a thread about it on
[the github issues page](https://github.com/oar-team/batsim/issues).

If you want to share any improvement on the Batsim code, you can use the
[github pull request](https://github.com/oar-team/batsim/pulls) mechanism so
we can include your modifications.

# Hacking Batsim
This little section explains a few choices that have been made when implementing
Batsim. Coding conventions are first given to maximize the project code
homogeneity. The different subsections then explain some code aspects.

## Coding conventions
The existing code base tries to follow the following conventions.

- Header files should have the ``.hpp`` extension.
  Source files should have the ``.cpp`` extension.
- Variables, functions and methods should be lowercase,
  and underscore ``_`` should be used as the word separator.
  Example: ``int my_fancy_integer = my_wonderful_function(42);``
- ``using`` should NOT be present in header files.
- Classes should be in UpperCamelCase. Example: ``MyBeautifulClass instance;``
- The code should be indented respecting the
  [Allman style](https://en.wikipedia.org/wiki/Indent_style#Allman_style).
  - Curly brackets ``{}`` should be present even for one-statement blocks.

## SimGrid process spawning management
Batsim is composed of multiple SimGrid processes. Most spawned processes have
parameters. A ``struct`` should be used to store the process arguments (even
for simple parameters, as more arguments may arise in the long run).

An instance of this parameters ``struct`` should be allocated dynamically
before spawning the process, and the process should deallocate the memory of
this instance.

The following example shows the code implied in spawning the ``execute_job`` process.

``` C++
// Arguments of the execute_job process
struct ExecuteJobProcessArguments
{
    BatsimContext * context;
    SchedulingAllocation * allocation;
};

// Calling function
int server_process(int argc, char *argv[])
{
    // ...

    // Allocates memory for the execute_job process parameters
    ExecuteJobProcessArguments * exec_args = new ExecuteJobProcessArguments;
    exec_args->context = data->context;
    exec_args->allocation = allocation;

    // Spawns the execute_job process
    MSG_process_create("job_executor", execute_job_process, (void*) exec_args, host);

    // ...
}

// Process function
int execute_job_process(int argc, char *argv[])
{
    // Get input parameters
    ExecuteJobProcessArguments * args = (ExecuteJobProcessArguments *) MSG_process_get_data(MSG_process_self());

    // ...

    // Cleans memory
    delete args;

    return 0;
}
```

In brief, if one wants to add a new process in Batsim, it should be done as follows
- create a new function named ``int something_process(int argc, char *argv[])``
  where ``something`` should be replaced by the process name. This function
  should return 0 and deallocate the memory of its arguments.
- create a ``struct SomethingProcessArguments`` to store the arguments of
  the process

## Communication between SimGrid processes
Processes may communicate with other processes via messages. The messaging
system uses SimGrid mailboxes, but the ``send_message`` function should be
used for the sake of code homogeneity.

Files ``ipp.hpp`` and ``ipp.cpp`` define functions and structures related to
inter process communications. All messages must be typed. The IPMessageType
enumeration stores the possible values:
``` C++
enum class IPMessageType
{
    JOB_SUBMITTED   //!< Submitter -> Server. The submitter tells the server that a new job has been submitted.
    ,KILLING_DONE   //!< Killer -> Server. The killer tells the server that all the jobs have been killed.
    // ...
};
```

If a message has associated data, a specific ``struct`` should be defined for it
(even for simple parameters, as more parameters may arise in the long run).

In the following example, a job executor tells the server that the job has
finished:
``` C++
int execute_job_process(int argc, char *argv[])
{
    // ...

    // Allocate memory for the message
    JobCompletedMessage * message = new JobCompletedMessage;
    message->job_id = args->allocation->job_id;

    // Send the message to the server
    send_message("server", IPMessageType::JOB_COMPLETED, (void*)message);

    // ...
}

int server_process(int argc, char *argv[])
{
    // The server waits and receives the message
    msg_task_t task_received = NULL;
    IPMessage * task_data;
    MSG_task_receive(&(task_received), "server");

    // Data associated with the SimGrid message
    task_data = (IPMessage *) MSG_task_get_data(task_received);

    if (task_data->type == IPMessageType::JOB_COMPLETED)
    {
        // Gets the message content
        JobCompletedMessage * message = (JobCompletedMessage*) task_data->data;

        // ...
    }
    // ...

    // Cleans the task_data AND the underlying task_data->data
    delete task_data;
}
```

In brief, if one wants to add an inter process message type in Batsim,
it should be done as follows:
- add a new ``SOMETHING`` enumerated value in the ``IPMessageType`` enumeration
- if needed, create a new ``struct SomethingMessage`` if data is associated with
  the message.
- make sure the new ``SOMETHING`` enumerated value is handled correctly in the
  ``std::string ip_message_type_to_string(IPMessageType type);`` function.
- make sure the new message type is destroyed correctly in the ``IPMessage``
  destructor.
