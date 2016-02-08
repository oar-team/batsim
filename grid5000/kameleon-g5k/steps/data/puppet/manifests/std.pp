# Standard environement creation recipe
# All recipes are stored in 'env' module. Here called with 'std' variant parameter.

class { 'env':
  given_variant    => 'std';
}
