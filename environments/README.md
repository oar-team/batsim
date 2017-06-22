Batsim environment recipes
==========================

This directory contains several recipes that uses Batsim:

- [batsim_ci](batsim_ci.yaml) contains all the dependencies to build
  Batsim, to run tests and to build the documentation.
  This recipe is updated each time dependencies changes.
  As it is used by the Continuous integration system,
  it is always up-to-date. It is accessible on Docker Hub at
  [oarteam/batsimi_ci](https://hub.docker.com/r/oarteam/batsim_ci/).

- [batsim_g5k](batsim_g5k.yaml) builds an appliance that can be deployed on
  Grid'5000 thanks to kadeploy. This recipe can be used as a base to setup
  reproducible experiments with Batsim.

Build images
------------

If you only want to execute an existing image, executing it directly from
docker is recommended.
All the images can be built from scratch if necessary

First, you need to install Kameleon:
[http://kameleon.imag.fr/installation.html](http://kameleon.imag.fr/installation.html)

You can now run Kameleon, for example:
```bash
  kameleon build batsim_ci.yaml
```
You can find the generated images in ``build/batsim/``.

If you want to use the [batsim_g5k](batsim_g5k.yaml) image,
some configuration is needed:
- select the base Grid'5000 debian image you want to use
  (images can be found in ``/grid5000/images`` on Grid'5000 nodes),
  move the base image if needed then modify the ``rootfs_archive_url``
  variable accordingly in [batsim_g5k.yaml](batsim_g5k.yaml)
- specify your Grid'5000 user name (``g5k_user``) and user id
  (``g5k_user_uid``) in [batsim_g5k.yaml](batsim_g5k.yaml)
- once a batsim_g5K image has been generated, you may need to modify the
  yaml description file so that the paths are fine on Grid'5000

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
