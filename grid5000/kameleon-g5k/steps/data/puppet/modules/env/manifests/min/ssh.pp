class env::min::ssh {

  package {
    'ssh server':
      name => $operatingsystem ? {
        'debian'           => 'openssh-server',
        'ubuntu'           => 'openssh-server',
        'centos'           => 'sshd'
      },
      ensure => installed;
    'ssh client':
      name => $operatingsystem ? {
        'debian'           => 'openssh-client',
        'ubuntu'           => 'openssh-client',
        'centos'           => 'openssh-client'
      }
  }
  service {
    'ssh':
      name => $operatingsystem ? {
        'debian'           => 'ssh',
        'ubuntu'           => 'ssh',
        'centos'           => 'sshd'
      },
      ensure => running;
  }

  augeas {
    'sshd_config_min':
      changes => [
        'set /files/etc/ssh/sshd_config/HostKey[1] /etc/ssh/ssh_host_rsa_key',
        'set /files/etc/ssh/sshd_config/HostKey[2] /etc/ssh/ssh_host_dsa_key',
        'rm /files/etc/ssh/sshd_config/HostKey[3]',
        'rm /files/etc/ssh/sshd_config/HostKey[4]',
        'set /files/etc/ssh/sshd_config/UsePrivilegeSeparation no',
        'set /files/etc/ssh/sshd_config/ServerKeyBits 768',
        'set /files/etc/ssh/sshd_config/PermitRootLogin without-password',
        'set /files/etc/ssh/sshd_config/PermitUserEnvironment yes'
      ],
      require  => Package['ssh server'];
  }
  # Todo: check that key files are overwritten by postinstall

  Augeas['sshd_config_min'] ~> Service['ssh']

}

