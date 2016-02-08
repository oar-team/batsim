class env::nfs::ceph::apt (
  $version = 'hammer'
) {

  $key = '17ED316D'

  # TODO: move apt-refresh dans min/apt
  exec {
    'import_ceph_apt_key':
      command     => "/usr/bin/wget -q 'http://ceph.com/git/?p=ceph.git;a=blob_plain;f=keys/release.asc' -O- | apt-key add -",
      unless      => "/usr/bin/apt-key list | /bin/grep '${key}'";
    '/usr/bin/apt-get update':
      refreshonly => true;
  }

  file {
    '/etc/apt/sources.list.d/ceph.list':
      ensure  => file,
      mode    => '0644',
      owner   => root,
      group   => root,
      content => "# ceph
deb http://ceph.com/debian-${version}/ ${::lsbdistcodename} main
deb-src http://ceph.com/debian-${version}/ ${::lsbdistcodename} main";
  }

  Exec['import_ceph_apt_key'] -> File['/etc/apt/sources.list.d/ceph.list'] ~> Exec['/usr/bin/apt-get update']

}

