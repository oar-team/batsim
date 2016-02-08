# Big environement creation recipe (base plus multiple packages)
# All recipes are stored in 'big' module. Here called with 'min' variant parameter.

class { 'env':
  given_variant    => 'big';
}
