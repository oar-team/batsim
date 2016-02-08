class env::std::g5ksubnets {

  exec {
    "retrieve_g5ksubnets":
      command => "/usr/bin/wget --no-check-certificate -q https://www.grid5000.fr/packages/debian/g5k-subnets.gem -O /tmp/g5k-subnets.gem",
      creates => "/tmp/g5k-subnets.gem";
  }
  package {
    "g5k-subnets":
      ensure  => installed,
      provider => gem,
      source   => "/tmp/g5k-subnets.gem",
      require  =>  Exec["retrieve_g5ksubnets"];
  }
}
