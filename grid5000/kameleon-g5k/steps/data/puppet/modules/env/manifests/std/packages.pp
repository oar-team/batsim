class env::std::packages () {

  # bootlogd: debug purpose
  $utils = [ 'bootlogd' ]

  $installed = [ $utils ]

  package {
    $installed:
      ensure => installed;
  }
}

