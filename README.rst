:date: 2015-12-01
:author: Michael Mercier <michael_mercier@inria.fr>

SimCtn
======

This is a Kameleon recipe for BatSim experiments. It provides a complete
appliance with BatSim and SimGrid as simulator and with several scheduler
that plugs with it:

* the Perl scheduler from F.Wagner and the OAR scheduler
* the OAR python scheduler

It also provide a script to run the experiment and reproduce the results.

.. note:: You can find already built images and cache here:
   http://simctn.gforge.inria.fr/

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

If some error happen during the build try to retry (using ``r``)

You can find the generated images in ``build/debian8_batsim/``.

If you want to use Docker, you can import and run the image using these
commands::

  docker import build/debian8_batsim/debian8_batsim.tar.gz simctn
  docker run -ti simctn bash

Now you should be in the container. You can run the experiment like this::

  cd /root
  ./experiment.py

You can modify the experimentation parameters directly inside the
``experiment.py`` script.

References
----------

BatSim: https://gforge.inria.fr/projects/batsim/
SimGrid: http://simgrid.gforge.inria.fr/
Perl Scheduler: https://github.com/wagnerf42/batch-simulator
OAR: http://oar.imag.fr/
