class env::min::locales {

  file {
    "/etc/locale.gen":
      mode    => '0644',
      owner   => root,
      group   => root,
      source  => "puppet:///modules/env/min/locales/locale.gen",
      notify  => Exec['generate-locales'];
    "/etc/default/locale":
      mode    => '0644',
      owner   => root,
      group   => root,
      source  => "puppet:///modules/env/min/locales/locale";
  }
  package {
    'locales':
      ensure  => installed;
  }
  exec {
    'generate-locales':
      command  => '/usr/sbin/locale-gen',
      user     => root,
      require  => Package['locales'];
  }
}
