# This file contains the 'nfs' class used to configure a basic environment with nfs support to be executed in grid'5000.

class env::nfs ( $parent_parameters = {} ){
  $nfs_parameters = {
    ntp_drift_file => false
  }
  $parameters = merge( $nfs_parameters, $parent_parameters )
  # Include base class
  class {
    'env::base':
      parent_parameters => $parameters
  }
  # Openiscsi (storage5k)
  class { 'env::nfs::openiscsi': }
  # Ceph
  class { 'env::nfs::ceph': }
  # ntp (required by nfs)
  class {
    'env::nfs::ntp':
      drift_file => $parameters['ntp_drift_file']
  }
  # package (shells)
  class { 'env::nfs::packages': }
  # ldap
  class { 'env::nfs::ldap': }
  # nfs
  class { 'env::nfs::nfs': }
  # storage5k required
  class { 'env::nfs::storage5k': }
}
