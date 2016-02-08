class env::min::misc ($root_pwd = '$1$qzZwnZXQ$Ak1xs7Oma6HUHw/xDJ8q91', $keep_tmp = false) {

  # Set root password
  user {
    'root':
      ensure   => 'present',
      password => $root_pwd;
  }

  # Set timezone
  file {
    '/etc/localtime':
      ensure   => present,
      source   => '/usr/share/zoneinfo/Europe/Paris';
  }

  # Use bash as default shell
  file {
    '/bin/sh':
      ensure => 'link',
      target => '/bin/bash',
  }

  if $keep_tmp {
    # Don't delete /tmp on reboot
    file {
      '/etc/tmpfiles.d/tmp.conf':
        ensure => 'link',
        target => '/dev/null';
    }
  }

}
