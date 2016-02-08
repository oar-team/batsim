# This file contains the 'xen' class used to configure xen environment to be executed in grid'5000.

class env::xen ( $parent_parameters = {} ) {

  $xen_parameters = {}
  $parameters = merge( $xen_parameters, $parent_parameters )

  # Include base
  class{ 'env::base': }

  # xen packages
  class{ 'env::xen::xen': }

}
