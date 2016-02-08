class env::nfs::nfs () {

  package {
    'nfs-common':
      ensure   => installed;
  }
}
