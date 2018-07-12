# Setup a Development Environment

## Using Nix (**Recommended**)

See the [install and run](run_batsim.md) page to setup Nix with our
repository.

You can simply enter a shell that comes with all you need to build and
test Batsim with this command:
```sh
nix-shell /path/to/datamovepkgs -A batsim_dev
```

## Using Docker

If you need to change the code of Batsim you can use the docker environment ``oarteam/batsim_ci``
and use the docker volumes to make your Batsim version of the code inside the container.
```bash
# launch a batsim container
docker run -ti -v /home/myuser/mybatrepo:/root/batsim --name batsim_dev oarteam/batsim_ci bash
```
Then, inside the container run the instructions provided in the following part.

With this setting you can use your own development tools outside the
container to hack the batsim code and use the container only to build
and test your your code.

## Manual installation (not recommended)

Batsim uses [Kameleon](http://kameleon.imag.fr/index.html) to build controlled
environments. These environments allow us to generate Docker containers, which
are used by [our CI][batsim ci] to test
whether Batsim can be built correctly and whether some integration tests pass.

Thus, the most up-to-date information about how to build Batsim dependencies
and Batsim itself can be found in our Kameleon recipes:
  - [batsim_ci.yaml](../environments/batsim_ci.yaml), for the dependencies (Debian)
  - [batsim.yaml](../environments/batsim.yaml), for Batsim itself (Debian)
  - Please note that [the steps directory](../environments/steps/) contain
    subcommands that can be used by the recipes.

However, some information is also written below for the sake of simplicity, but
please note it might be outdated.

### Dependencies

Batsim dependencies are listed below:
-   SimGrid. dev version is recommended (203ec9f99 for example).
    To use SMPI jobs, use commit 587483ebe of
    [mpoquet's fork](https://github.com/mpoquet/simgrid/).
    To use energy, please consider using the Batsim upstream_sg branch and
    SimGrid commit e96681fb8.
-   RapidJSON (1.02 or greater)
-   Boost 1.62 or greater (system, filesystem, regex, locale)
-   C++11 compiler
-   Redox (and its dependencies: hiredis and libev)


# Compile and Test

When you have setup your environment (see previous section), you can
go to the already cloned Batsim repository (or clone this repository)
and configure the build.

```sh
cd batsim_repo
rm -rf build
mkdir build
cd build
cmake ..
```

Now you can code your stuff, (**note**: It is recommended to do it in a branch)
and add some tests. Then build and run the tests with:

```sh
make -j $(nproc)
make install
make test
```
