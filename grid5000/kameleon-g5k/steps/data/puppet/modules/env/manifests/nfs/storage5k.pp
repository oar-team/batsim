class env::nfs::storage5k {

  #Package required by storage5k
  package {
    "libdbd-pg-perl":
      ensure  => installed;
  }
}
