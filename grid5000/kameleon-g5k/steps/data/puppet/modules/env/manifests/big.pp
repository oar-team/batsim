# This file contains the 'big' class used to configure improved environment to be executed in grid'5000.

class env::big ( $parent_parameters = {} ){

  $big_parameters = {
    mic_enable => false
  }
  $parameters = merge( $big_parameters, $parent_parameters )

  # Include nfs class
  class {
    'env::nfs':
      parent_parameters => $parameters
  }
  # Users packages
  class { 'env::big::packages': }
  # gem
  if $env::target_g5k {
    class { 'env::big::gem':
      stage  => 'g5k_adjustment';
    }
  }
  # mail
  class { 'env::big::mail': }
  # kvm
  class { 'env::big::kvm': }
  # nvidia
  class { 'env::big::nvidia': }
  # xeon phi
  class {
    'env::big::mic':
      enable  => $parameters['mic_enable']
  }
}
