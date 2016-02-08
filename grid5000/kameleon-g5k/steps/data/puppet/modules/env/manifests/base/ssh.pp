# This class add configurations to sshd. It consider ssh server / service are already declared (in 'min' variant).
class env::base::ssh (){

  augeas {
    'sshd_config_base':
      changes => [
        'set /files/etc/ssh/sshd_config/MaxStartups 500'
      ],
      require  => Package['ssh server'];
  }

  Augeas['sshd_config_base'] ~> Service['ssh']

}

