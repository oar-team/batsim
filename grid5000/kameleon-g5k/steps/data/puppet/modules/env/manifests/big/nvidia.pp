class env::big::nvidia () {

  #packages = [ 'g++', 'gfortran', 'freeglut3-dev', 'libxmu-dev', 'libxi-dev' ]

  # Blacklist nvidia modules
  include 'env::big::nvidia::modules'
  # Install nvidia drivers
  include 'env::big::nvidia::drivers'
  # Install cuda
  include 'env::big::nvidia::cuda'

}
