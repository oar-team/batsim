Batsim environment recipes
==========================

This is a Kameleon recipe for Batsim experiments. It provides several
recipes to build Docker images (can also be configured for other format
like VDI or QCOW2) to build, test and run Batsim.

- [batsim_ci](batsim_ci.yaml) contains all the dependencies to build
  batsim install it and run tests. This recipe is updated each time
  dependencies changes. As it is used by the Continuous integration system,
  it is always up-to-date. It is accessible on Docker Hub at
  [oarteam/batsimi_ci](https://hub.docker.com/r/oarteam/batsim_ci/).

- [batsim](batsim.yaml) contains a complete appliance with batsim installed
  and tested. It is accessible on Docker Hub at
  [oarteam/batsim](https://hub.docker.com/r/oarteam/batsim/).

Build images
------------

It is recommended to run these images directly from docker, but you can build
them from scratch if necessary

First, you need to install Kameleon:
[http://kameleon.imag.fr/installation.html](http://kameleon.imag.fr/installation.html)

You can now run Kameleon, for example:
```bash
  kameleon build batsim.yaml
```
You can find the generated images in ``build/batsim/``.

Run the image with docker
-------------------------

To run the image with Docker, you have to import and run the image using these
commands:
```bash
  docker import build/batsim/batsim.tar.gz my_batsim
  docker run -ti -p 8888:8888 my_batsim bash
```

Now you should be in the container.

Development
-----------

When you are building your experiment you use some development tools. The
last step of the ``setup`` section is made to install your favorite tools
in order to work the way you like. You should use the provided step, as a
template to build your own. You can find it in ``step/my_dev_tools.yaml``.

All the experiment tools (simulators, schedulers, input data, ...) are
versioned using Git but, in order to import it without credential, the
remote repositories are read-only. Thus, you have to set your personal
remote clones for any modified tools of the chain to save your work. In
order to push your work on a remote repository, SSH credentials need to be
imported using SSH forward Agent or Key import from your home. For docker
container the ssh-agent socket (SSH_AUTH_SOCK) can be mounted as volume and
exported in the environment.


References
----------

Batsim: https://gforge.inria.fr/projects/batsim/
Simgrid: http://simgrid.gforge.inria.fr/
Perl Scheduler: https://github.com/wagnerf42/batch-simulator
OAR: http://oar.imag.fr/
