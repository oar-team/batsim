# Base environement creation recipe with NFS enabled
# All recipes are stored in 'env' module. Here called with 'nfs' variant parameter.

class { 'env':
  given_variant    => 'nfs';
}
