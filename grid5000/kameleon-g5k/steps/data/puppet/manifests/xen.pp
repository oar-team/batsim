# Xen environement creation recipe
# All recipes are stored in 'env' module. Here called with 'min' variant parameter.

class { 'env':
  given_variant    => 'xen';
}
