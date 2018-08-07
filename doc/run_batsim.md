# Install and Run batsim

**Important note**: It is highly recommended to install Batsim with the
provided methods because we are using specific version of SimGrid and
up-to-date packages (like boost) that may not be easily available in your
distribution yet.

If you are looking for development setup to be able to compile batsim and
run the tests, see the [Setup a Development Environment](dev_batsim.md)
documentation.

## Install batsim with Nix (**Recommended**)

First you need to install Nix but don't worry it is pretty straightforward:
```sh
curl https://nixos.org/nix/install | sh
```

Follow the instructions provided at the end of the script: You need to
source a file to access to the Nix commands:
```sh
~/nix-profiles/etc/profile.d/nix.sh
```

Then, get our Nix repository that contains the batsim package:
```sh
git clone https://gitlab.inria.fr/vreis/datamove-nix.git datamovepkgs
nix-env --file ./datamovepkgs --install batsim
```

Batsim is now available directly:
```sh
batsim --help
```

You can also install Batsched, the scheduler used for the tests and the
examples, with the same mechanism:
```sh
nix-env --file ./datamovepkgs -iA batsched
```

## Run batsim directly with docker (Deprecated)

A simple way to run batsim is to run it directly with docker because you
have nothing to install and/or configure (except Docker itself...).

You can run batsim directly using this image without any installation. For
example:
```sh
docker run --net host -u $(id -u):$(id -g) -v $PWD:/data oarteam/batsim  -p ./platforms/energy_platform_homogeneous_no_net_32.xml -w ./workload_seed20_200jobs.json -e seed20
```

To make it more understandable, here is the command decomposition:

- ``--net host`` to access external redis server (optional)
- ``--user $(id -u):$(id -g)`` to generate outputs with your own user permission instead of root permission
- ``--volume $PWD:/data`` to share your local folder with batsim so it can
  find the platform file and so on: Batsim is running inside docker in the
  ``/data`` folder.
- ``oarteam/batsim`` image name (you can add a tag to get a specific version like ``oarteam/batsim:1.2.0``
- ``--platform plt.xml --workload wl.json ...`` add batsim parameters

Then you can run your own scheduler to make the simulation begins.

# Bonus :)

## Create the docker image with Nix

We use the [Nix package manager](https://nixos.org/nix/) to build a minimal
docker image for batsim.

Get the Nix repository that contains the batsim package [here](https://gitlab.inria.fr/vreis/datamove-nix):

```sh
git clone https://gitlab.inria.fr/vreis/datamove-nix.git datamovepkgs
cd datamovepkgs

# For stable version
nix-build . -A batsimDocker
# For latest version from master head
nix-build . -A batsimDocker_git
```

Then you need docker to load the image:
```sh
cat result | docker load

# see it in docker
docker images

# add some tag
docker tag oarteam/batsim:1.2.0 oarteam/batsim:latest

# login to docker (needed only once)
docker login

# push it to the docker hub
docker push oarteam/batsim:1.2.0
docker push oarteam/batsim:latest

```

