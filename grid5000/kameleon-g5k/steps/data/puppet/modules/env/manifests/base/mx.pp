class env::base::mx ($architecture = 'amd64') {

  case $operatingsystem {
    'Debian','Ubuntu': {
      exec{'retrieve_mx_deb':
        command    => "/usr/bin/wget --no-check-certificate -q https://www.grid5000.fr/packages/debian/mx_1.2.16-g5k_${architecture}.deb -O /tmp/mx_1.2.16-g5k_${architecture}.deb",
        creates    => "/tmp/mx_1.2.16-g5k_${architecture}.deb",
      }
      package {
        'mx':
          ensure     => installed,
          provider   => dpkg,
          source     => "/tmp/mx_1.2.16-g5k_${architecture}.deb",
          require    =>  [
              Exec['retrieve_mx_deb'],
              File['/usr/lib64'],
              Package['syslinux']  # already defined in env::base::infiniband
            ];
      }
    }
    default: {
      err "${operatingsystem} not suported."
    }
  }

  exec{
    'mx_local_install':
      command    => '/usr/sbin/mx_local_install',
      user       => root,
      require    => Package['mx'];
  }

  service {
    'mx':
      enable     => false,
      require    => Exec['mx_local_install'];
  }

  file {
    '/etc/network/if-up.d/ip_over_mx':
      ensure     => file,
      owner      => root,
      group      => root,
      mode       => '0755',
      source     => 'puppet:///modules/env/base/mx/ip_over_mx',
      require    => Exec['mx_local_install'];
    '/usr/lib64':
      ensure  => link,
      owner   => root,
      group   => root,
      target  => '/usr/lib';
  }

  # Add mx support for openmpi
  augeas {
    'openmpi_mx_support':
      changes    => [
          "set /files/etc/ld.so.conf.d/mx.conf /usr/lib64/"
        ],
      require    => Exec['mx_local_install'];
  }

  exec{
    'update ldconfig':
      command    => "/sbin/ldconfig",
      user       => root,
      require    => Augeas['openmpi_mx_support'];
  }
}
