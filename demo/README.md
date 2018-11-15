# Batsim demo

Simple notebook experiment running Batsim and analysing the results visually with Evalys.

## Dependencies

-   *curl* for installing Nix package manager.

## Automatic setup

The file *run.bash* allows for setting up and launching the notebook automatically.

### Requirements

-   Run without privileges from a sudo-capable user.

### Running

```shell
./run.bash
```

## Manual setup

Reproduce the following steps for installing tools and launching the notebook manually.

### Step 1: Install the Nix package manager

```shell
curl https://nixos.org/nix/install | sh
```

### Step 2: Activate Nix environment

```shell
. ~/.nix-profile/etc/profile.d/nix.sh
```

### Step 3: Install Batsim tools

```shell
nix-env -f https://github.com/oar-team/kapack/archive/master.tar.gz -iA batsim_dev
nix-env -f https://github.com/oar-team/kapack/archive/master.tar.gz -iA batsched_dev
nix-env -f https://github.com/oar-team/kapack/archive/master.tar.gz -iA batexpe
```

### Step 4: Launch the notebook

```shell
nix-shell https://github.com/oar-team/kapack/archive/master.tar.gz \
  -A evalysNotebookEnv \
  --run 'jupyter notebook BatsimDemo.ipynb'
```
