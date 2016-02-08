class env::base::ganglia ($enable = false){

  package {
    'ganglia-monitor':
      ensure  => installed;
  }

  file {
    '/etc/ganglia' :
      ensure  => directory,
      owner   => root,
      group   => root,
      mode    => '0644';
    '/etc/ganglia/gmond.conf' :
      ensure  => file,
      owner   => root,
      group   => root,
      mode    => '0644',
      source  => "puppet:///modules/env/base/ganglia/gmond.conf",
      require => File['/etc/ganglia'];
  }

  # Debian jessie suffers a bug that make puppet unable to correctly detect if some services are enabled or not (https://bugs.debian.org/cgi-bin/bugreport.cgi?bug=760616 or #bug=751638 )
  # Because of this bug, service ganglia-monitor can't be made "disabled" with the standard method.
  if "${::lsbdistcodename}" == "jessie" and "${::operatingsystem}" == "Debian" {
    unless $enable {
      exec {
        'disable ganglia-monitor':
          command => '/bin/systemctl disable ganglia-monitor',
          require => Package['ganglia-monitor'];
      }
    }
  }
  else {
    service {
      'ganglia-monitor':
        enable  => $enable,
        require => Package['ganglia-monitor'];
    }
  }
}
