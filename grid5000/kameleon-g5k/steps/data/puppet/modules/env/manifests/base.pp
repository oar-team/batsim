# This file contains the 'base' class used to configure a basic environment to be executed in grid'5000.

class env::base ( $parent_parameters = {} ){

  $base_parameters = {
    misc_keep_tmp => true,
    ganglia_enable => false
  }

  $parameters = merge ( $base_parameters, $parent_parameters )
  # Include min class
  class {
    'env::min':
      parent_parameters => $parameters;
  }

  # User packages
  class { 'env::base::packages': }
  # Include kexec-tools
  class { 'env::base::kexec': }
  # SSh modification
  class { 'env::base::ssh': }
  # Sshfs
  class { 'env::base::sshfs': }
  # Specific tuning
  class { 'env::base::tuning': }
  # Cpufreq
  class { 'env::base::cpufreq': }
  # Ganglia
  class {
    'env::base::ganglia':
      enable => $parameters['ganglia_enable']
  }
  #TODO: merge ib and mx as 'high perf network' (or equivalent)?
  # Infiniband
  class { 'env::base::infiniband': }
  # MX ?
  class { 'env::base::mx': }
  # Openmpi
  class { 'env::base::openmpi': }
  # disable cstates
  class { 'env::base::cstate': }

}
