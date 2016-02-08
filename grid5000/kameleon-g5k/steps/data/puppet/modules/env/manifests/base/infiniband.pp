class env::base::infiniband (){

  # Removed compare to older recipe : rds-tools and sdpnetstat (not present in debian repo anymore)
  $infiniband_packages = [ 'ibverbs-utils', 'libibverbs-dev', 'libcxgb3-dev', 'libipathverbs-dev', 'libmlx4-dev', 'libmthca-dev', 'librdmacm-dev', 'rdmacm-utils', 'ibutils', 'infiniband-diags', 'perftest', 'qlvnictools', 'srptools' ]
  $installed = [ $infiniband_packages, 'syslinux' ]

  package {
    $installed:
      ensure  => installed;
  }

  file {
    '/etc/infiniband/openib.conf':
      ensure   => file,
      owner    => root,
      group    => root,
      mode     => '0644',
      source   => 'puppet:///modules/env/base/infiniband/openib.conf',
      require  => Package[$infiniband_packages];
    '/etc/init.d/openibd':
      ensure   => file,
      owner    => root,
      group    => root,
      mode     => '0755',
      source   => 'puppet:///modules/env/base/infiniband/openibd',
      require  => Package[$infiniband_packages];
    '/etc/infiniband/ifcfg-ib0':
      ensure   => file,
      owner    => root,
      group    => root,
      mode     => '0644',
      source   => 'puppet:///modules/env/base/infiniband/ifcfg-ib0',
      require  => Package[$infiniband_packages];
   '/lib/udev/rules.d/90-ib.rules':
      ensure  => present,
      owner   => root,
      group   => root,
      mode    => '0644',
      source  => 'puppet:///modules/env/base/infiniband/90-ib.rules',
      require => Package['udev'];
  }

  service {
    'openibd':
      enable   => true,
      require  => [
               Package[$infiniband_packages],
               File['/etc/init.d/openibd']
      ];
  }
}
