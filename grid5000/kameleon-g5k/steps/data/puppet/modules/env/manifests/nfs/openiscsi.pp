class env::nfs::openiscsi (){

  # used by storage5k (bug #4309)

  package {
    'open-iscsi':
      ensure  => installed;
  }

  file {
    '/etc/udev/rules.d/55-openiscsi.rules':
      owner   => root,
      group   => root,
      mode    => '0644',
      source  => 'puppet:///modules/env/nfs/openiscsi/55-openiscsi.rules';
    '/etc/udev/scripts':
      ensure  => "directory",
      owner   => root,
      group   => root,
      mode    => '0755';
    '/etc/udev/scripts/iscsidev.sh':
      ensure  => present,
      owner   => root,
      group   => root,
      mode    => '0755',
      source  => 'puppet:///modules/env/nfs/openiscsi/iscsidev.sh',
      require => File['/etc/udev/scripts'];
  }
}
