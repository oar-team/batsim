class env::min::packages () {

  $editors = [ 'vim', 'nano' ]
  $utils = [ 'less' ]

  $installed = [ $editors, $utils ]

  package {
    $installed:
      ensure => installed;
  }
}

