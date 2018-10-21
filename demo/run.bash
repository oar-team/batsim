#!/usr/bin/env bash

# Install Nix package manager
if [[ ! -d "/nix" ]] || [[ ! -d "$HOME/.nix-profile" ]];
then
  if [[ -x "$(command -v curl)" ]];
  then
    curl https://nixos.org/nix/install | sh
  else
    echo "Need 'curl' to install Nix"
    exit 1
  fi
fi
echo "OK: Nix installed"

# Activate Nix environment
if [[ ! -x "$(command -v nix-env)" ]] && [[ ! -x "$(command -v nix-shell)" ]];
then
  . ~/.nix-profile/etc/profile.d/nix.sh
fi
echo "OK: Nix environment activated"

# Install Batsim, Batshed and Batexpe
if ! nix-env -q 'batsim' &> /dev/null;
then
  nix-env -f https://github.com/oar-team/kapack/archive/master.tar.gz -iA batsim
fi
if ! nix-env -q 'batsched' &> /dev/null;
then
  nix-env -f https://github.com/oar-team/kapack/archive/master.tar.gz -iA batsched
fi
if ! nix-env -q 'batexpe' &> /dev/null;
then
  nix-env -f https://github.com/oar-team/kapack/archive/master.tar.gz -iA batexpe
fi
echo "OK: Batsim, Batsched and Batexpe installed"

# Install the Jupyter Notebook
if [[ ! -x "$(command -v jupyter)" ]];
then
  nix-env -i python3.6-jupyter-1.0.0
fi
echo "OK: Jupyter Notebook installed"

# Launch the notebook
nix-shell https://github.com/oar-team/kapack/archive/master.tar.gz \
  -A evalysNotebookEnv \
  --run 'jupyter notebook BatsimDemo.ipynb'

exit 0