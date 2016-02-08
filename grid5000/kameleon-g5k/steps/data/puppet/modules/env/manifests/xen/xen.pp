class env::xen::xen () {

  $xen_packages = [ 'xen-utils', 'debootstrap', 'xen-tools', 'sysfsutils', 'lvm2', 'xen-linux-system-amd64' ]
  package {
    $xen_packages :
      ensure   => installed;
      #notify   => Exec['update-grub'];
  }
  file {
    '/hypervisor':  # Given in dsc file to kadeploy to configure /boot/grub/grub.cfg correctly.
      ensure   => link,
      target   => '/boot/xen-4.4-amd64.gz';
    '/etc/xen/xend-config.sxp':
      ensure   => file,
      owner    => root,
      group    => root,
      mode     => '0644',
      source   => 'puppet:///modules/env/xen/xen/xend-config.sxp',
      require  => Package['xen-utils'];
    '/root/.ssh/id_rsa':
      ensure   => file,
      owner    => root,
      group    => root,
      mode     => '0600',
      source   => 'puppet:///modules/env/xen/xen/id_rsa';
    '/root/.ssh/id_rsa.pub':
      ensure   => file,
      owner    => root,
      group    => root,
      mode     => '0600',
      source   => 'puppet:///modules/env/xen/xen/id_rsa.pub';
    '/etc/xen-tools/skel/root':
      ensure   => directory,
      owner    => root,
      group    => root,
      mode     => '0700',
      require  => Package['xen-tools'];
    '/etc/xen-tools/skel/root/.ssh':
      ensure   => directory,
      owner    => root,
      group    => root,
      mode     => '0700',
      require  => File['/etc/xen-tools/skel/root'];
    '/etc/xen-tools/skel/root/.ssh/authorized_keys': # Line content defined below
      ensure   => file,
      owner    => root,
      group    => root,
      mode     => '0600',
      require  => File['/etc/xen-tools/skel/root/.ssh'];
    '/etc/xen-tools/xen-tools.conf':
      ensure   => file,
      owner    => root,
      group    => root,
      mode     => '0644',
      content  => template('env/xen/xen/xen-tools.erb'),
      require  => Package['xen-tools'];
    '/usr/local/bin/random_mac':
      ensure   => file,
      owner    => root,
      group    => root,
      mode     => '0755',
      source   => 'puppet:///modules/env/xen/xen/random_mac';
    '/etc/init.d/xen-g5k':
      ensure   => file,
      owner    => root,
      group    => root,
      mode     => '0755',
      source   => 'puppet:///modules/env/xen/xen/xen-g5k';
  }
  if $env::target_g5k {
    file {
      '/etc/xen-tools/skel/etc':
        ensure   => directory,
        owner    => root,
        group    => root,
        mode     => '0644',
        require  => Package['xen-tools'];
      '/etc/xen-tools/skel/etc/apt':
        ensure   => directory,
        owner    => root,
        group    => root,
        mode     => '0644',
        require  => File['/etc/xen-tools/skel/etc'];
      '/etc/xen-tools/skel/etc/apt/apt.conf.d':
        ensure   => directory,
        owner    => root,
        group    => root,
        mode     => '0644',
        require  => File['/etc/xen-tools/skel/etc/apt'];
      '/etc/xen-tools/skel/etc/apt/apt.conf.d/proxy':
        ensure   => file,
        owner    => root,
        group    => root,
        mode     => '0644',
        source   => 'puppet:///modules/env/min/apt/g5k-proxy',
        require  => File['/etc/xen-tools/skel/etc/apt/apt.conf.d'];
      '/etc/xen-tools/skel/etc/dhcp':
        ensure   => directory,
        owner    => root,
        group    => root,
        mode     => '0644',
        require  => File['/etc/xen-tools/skel/etc'];
      '/etc/xen-tools/skel/etc/dhcp/dhclient-exit-hooks.d':
        ensure   => directory,
        owner    => root,
        group    => root,
        mode     => '0644',
        require  => File['/etc/xen-tools/skel/etc/dhcp'];
      '/etc/xen-tools/skel/etc/dhcp/dhclient-exit-hooks.d/g5k-update-host-name':
        ensure   => file,
        owner    => root,
        group    => root,
        mode     => '0644',
        source   => 'puppet:///modules/env/min/network/g5k-update-host-name',
        require  => File['/etc/xen-tools/skel/etc/dhcp/dhclient-exit-hooks.d'];
    }
  }

  file_line {
    '/etc/xen-tools/skel/root/.ssh/authorized_keys dom0_key':
      line     => file('env/xen/xen/id_rsa.pub'),
      path     => '/etc/xen-tools/skel/root/.ssh/authorized_keys',
      require  => File['/etc/xen-tools/skel/root/.ssh/authorized_keys'];
  }

  $proxy_environment = $env::from_g5k ? {
    true     => ['http_proxy="http://proxy:3128"',  'https_proxy="http://proxy:3128"'],
    false    => [],
  }
  $required_resource_to_create_vm = $env::target_g5k ? {
    true     => File['/etc/xen-tools/skel/etc/apt/apt.conf.d/proxy'],
    false    => nil,
  }
  exec {
    'create_example_domU':
      command  => '/usr/bin/xen-create-image --hostname=domU --role=udev --genpass=0 --password=grid5000 --dhcp --mac=$(random_mac) --bridge=br0 --size=1G',
      environment => $proxy_environment,
      creates  => '/etc/xen/domU.cfg',
      timeout  => 600,
      require => [
        Package['xen-tools', 'xen-utils'],
        File_line['/etc/xen-tools/skel/root/.ssh/authorized_keys dom0_key'],
        File['/etc/xen-tools/xen-tools.conf', '/usr/local/bin/random_mac'],
        $required_resource_to_create_vm,
      ];
  }
}
