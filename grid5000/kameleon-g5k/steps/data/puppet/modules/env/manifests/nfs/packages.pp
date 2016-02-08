class env::nfs::packages {

  # Shells are installed in NFS variant
  # because nfs add ldap and ldap is where users choose their shells
  $shells = [ 'bash', 'zsh', 'tcsh', 'ash' ]

  $installed = [ $shells ]

  package {
    $installed:
      ensure => installed;
  }
}
