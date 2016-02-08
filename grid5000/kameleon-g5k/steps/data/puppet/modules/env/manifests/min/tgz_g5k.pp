class env::min::tgz_g5k {
  case $operatingsystem {
    'Debian','Ubuntu': {
      # This package is retrieve from www.grid5000.fr. I guess this is because it needs to be retrievable from outside of g5k.
      exec{'retrieve_tgz-g5k_deb':
        command => "/usr/bin/wget --no-check-certificate -q https://www.grid5000.fr/packages/debian/tgz-g5k_all.deb -O /tmp/tgz-g5k_all.deb",
        creates => "/tmp/tgz-g5k_all.deb",
      }
      package {
        'tgz-g5k':
          ensure   => installed,
          provider => dpkg,
          source   => '/tmp/tgz-g5k_all.deb',
          require  =>  [
              Exec['retrieve_tgz-g5k_deb'],
              Package['ssh client']
          ]
      }
    }
    default: {
      err "${operatingsystem} not suported."
    }
  }
}

