:date: 2015-12-01
:author: Michael Mercier <michael_mercier@inria.fr>

Batsim environment recipes
==========================

This is a Kameleon recipe for Batsim experiments. It provides a complete
appliance with Batsim and Simgrid as simulator and with several scheduler
that plugs with it:

* the Perl scheduler from F.Wagner and the OAR scheduler
* the OAR python scheduler

It also provide a script to run the experiment and reproduce the results.

Usage
-----

First, you need to install Kameleon:
http://kameleon.imag.fr/installation.html

Then, you have to open the recipe ``debian8_batsim.yaml`` with your favorite
text editor and chose the isolation mechanism that suits your environment.
To do so, just comment and uncomment the ``extend`` lines. You can also
look at the other global variables if you want to customize your appliance.

.. note:: You will need to install some specific tools depending on the
   method you chose.

You can now run Kameleon::

  kameleon build debian8_batsim.yaml

You can find the generated images in ``build/debian8_batsim/``.

Run the image
-------------

You can use either VirtualBox or KVM or just a chroot to use this image
directly from the exported images that you can find in
``build/debian8_batsim/``.

With Docker
~~~~~~~~~~~

If you want to use Docker, you can import and run the image using these
commands::

  docker import build/debian8_batsim/debian8_batsim.tar.gz simctn
  docker run -ti \
    --env LANG=en_US.UTF-8 \
    --volume $SSH_AUTH_SOCK:/ssh-agent --env SSH_AUTH_SOCK=/ssh-agent \
    -p 8888:8888 \
    simctn bash

Now you should be in the container.

With Qemu/KVM
~~~~~~~~~~~~~

You can use virt-manager to have a nice GUI like VirtualBox or launch it by
and with this command::

  sudo qemu-system-x86_64 -m 1024 --enable-kvm \
    -redir tcp:2222::22 \
    -redir tcp:8888::8888 \
    build/debian8_batsim/debian8_batsim.qcow2

Then ssh into it::

  ssh -A root@localhost -p2222
  # password: root

Run the experiment
------------------

Login into the image. Default credential is **login: root** and **password:
root**. Now you can run the experiment like this::

  cd /root/expe-batsim
  ./experiment.py

You can modify the experimentation parameters directly inside the
``experiment.py`` script.

Results
-------

To check the results of the experiment you can use ipython notebook (now
jupyther). To do so, run the server and connect to it via your local
browser::

  ipython3 notebook --no-browser --ip 0.0.0.0

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
exported in the environment.  Note that the examples below already do this
for you.


References
----------

Batsim: https://gforge.inria.fr/projects/batsim/
Simgrid: http://simgrid.gforge.inria.fr/
Perl Scheduler: https://github.com/wagnerf42/batch-simulator
OAR: http://oar.imag.fr/
