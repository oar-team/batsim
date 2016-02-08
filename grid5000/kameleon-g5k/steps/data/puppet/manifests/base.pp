# Base environement creation recipe
# All recipes are stored in 'env' module. Here called with 'base' variant parameter.

class { 'env':
  given_variant    => 'base';
}
