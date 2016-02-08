# This file contains the 'min' class used to configure an environment with minimal modification to be executed in grid'5000.

class env::min ( $parent_parameters = {} ) {

  $min_parameters = {
    misc_root_pwd => '$1$qzZwnZXQ$Ak1xs7Oma6HUHw/xDJ8q91',
    misc_keep_tmp => false,
  }
  $parameters = merge( $min_parameters, $parent_parameters )

  # Package manager
  case $operatingsystem {
    'Debian','Ubuntu': {
      class { 'env::min::apt': }
    }
    'Centos': {
      class { 'env::min::yum': }
    }
    default: {
      err "${operatingsystem} not suported."
    }
  }

  # User package
  class { 'env::min::packages': }
  # system
  class { 'env::min::udev': }
  # ssh
  class { 'env::min::ssh': }
  # setup
  class { 'env::min::locales': }
  # motd
  class { 'env::min::motd': }
  # tgs-g5k
  class { 'env::min::tgz_g5k': }
  # network configuration
  class { 'env::min::network': }
  # misc (root password, localtime, default shell...)
  class {
    'env::min::misc':
      root_pwd => $parameters['misc_root_pwd'],
      keep_tmp => $parameters['misc_keep_tmp'];
  }
  # kernel installation
  class { 'env::min::kernel': }
  # Tagging to recognize images
  class { 'env::min::image_versioning': }

}
