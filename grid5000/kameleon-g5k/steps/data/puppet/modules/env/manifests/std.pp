# This file contains the 'std' class used to configure the standard environment to be executed in grid'5000.

class env::std ( $parent_parameters = {} ){

  if $env::target_g5k {
    $root_pwd = hiera("env::std::misc::rootpwd")
  }
  else {
    $root_pwd = '$1$qzZwnZXQ$Ak1xs7Oma6HUHw/xDJ8q91' # grid5000
  }

  $std_parameters = {
    ganglia_enable => true,
    ntp_drift_file => true,
    misc_keep_tmp  => false,
    misc_root_pwd  => $root_pwd,
    mic_enable     => true,
  }

  $parameters = merge( $std_parameters, $parent_parameters )

  # Include big class
  class {
    'env::big':
      parent_parameters => $parameters
  }
  # OAR
  class { 'env::std::oar': }
  # g5kchecks
  class { 'env::std::g5kchecks': }
  # g5kcode
  class { 'env::std::g5kcode': }
  # g5k-subnets
  class { 'env::std::g5ksubnets': }
  # Log net access
  class { 'env::std::net_access': }
  # Packages
  class { 'env::std::packages': }

}
