class env::base::openmpi () {

  $openmpi_packages = [ 'librdmacm1', 'libgfortran3', 'libnuma1', 'blcr-util', 'libibverbs1-dbg', 'libopenmpi1.6', 'openmpi-common', 'openmpi-bin', 'libopenmpi-dev', 'openmpi-checkpoint' ]

  package{
    $openmpi_packages:
      ensure     => installed,
      require    => Package['libibverbs-dev']
  }
}

