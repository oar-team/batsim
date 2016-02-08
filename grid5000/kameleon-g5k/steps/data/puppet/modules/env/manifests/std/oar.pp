class env::std::oar {


  $oar_packages = ['oar-common', 'oar-node']
  # Installed oar packages from g5k 'mirror'.
  exec {
    "retrieve_oar-common":
      command  => "/usr/bin/wget --no-check-certificate -q https://www.grid5000.fr/packages/debian/oar-common_2.5.6~rc3-1~bpo70+1_amd64.deb -O /tmp/oar-common_2.5.6~rc3-1~bpo70+1_amd64.deb",
      creates  => "/tmp/oar-common_2.5.6~rc3-1~bpo70+1_amd64.deb";
    "retrieve_oar-node":
      command  => "/usr/bin/wget --no-check-certificate -q https://www.grid5000.fr/packages/debian/oar-node_2.5.6~rc3-1~bpo70+1_amd64.deb -O /tmp/oar-node_2.5.6~rc3-1~bpo70+1_amd64.deb",
      creates  => "/tmp/oar-node_2.5.6~rc3-1~bpo70+1_amd64.deb";
  }
  package {
    "oar-common":
      ensure   => installed,
      provider => dpkg,
      source   => "/tmp/oar-common_2.5.6~rc3-1~bpo70+1_amd64.deb",
      require  => Exec["retrieve_oar-common"];
    "oar-node":
      ensure   => installed,
      provider => dpkg,
      source   => "/tmp/oar-node_2.5.6~rc3-1~bpo70+1_amd64.deb",
      require  => Exec["retrieve_oar-node"];
  }


  file {
    '/var/lib/oar/checklogs/':
      ensure   => directory,
      owner    => root,
      group    => root,
      mode     => '0755',
      require  => Package[$oar_packages];
    '/var/lib/oar/.ssh':
      ensure   => directory,
      owner    => root,
      group    => root,
      mode     => '0755',
      require  => Package[$oar_packages];
    '/var/lib/oar/.ssh/config':
      ensure   => present,
      owner    => oar,
      group    => oar,
      mode     => '0644',
      source   => 'puppet:///modules/env/std/oar/oar_sshclient_config',
      require  => [ File['/var/lib/oar/.ssh'], Package[$oar_packages] ];
    '/etc/oar/oar_ssh_host_dsa_key':
      ensure   => present,
      owner    => root,
      group    => root,
      mode     => '0600',
      source   => 'puppet:///modules/env/std/oar/oar_ssh_host_dsa_key',
      require  => Package[$oar_packages];
    '/etc/oar/oar_ssh_host_rsa_key':
      ensure   => present,
      owner    => root,
      group    => root,
      mode     => '0600',
      source   => 'puppet:///modules/env/std/oar/oar_ssh_host_rsa_key',
      require  => Package[$oar_packages];
    '/etc/oar/oar_ssh_host_dsa_key.pub':
      ensure   => present,
      owner    => root,
      group    => root,
      mode     => '0600',
      source   => 'puppet:///modules/env/std/oar/oar_ssh_host_dsa_key.pub',
      require  => Package[$oar_packages];
    '/etc/oar/oar_ssh_host_rsa_key.pub':
      ensure   => present,
      owner    => root,
      group    => root,
      mode     => '0600',
      source   => 'puppet:///modules/env/std/oar/oar_ssh_host_rsa_key.pub',
      require  => Package[$oar_packages];
    '/var/lib/oar/.batch_job_bashrc':
      ensure   => present,
      owner    => oar,
      group    => oar,
      mode     => '0755',
      source   => 'puppet:///modules/env/std/oar/batch_job_bashrc',
      require  => Package[$oar_packages];
    '/etc/security/access.conf':
      ensure   => present,
      owner    => root,
      group    => root,
      mode     => '0644',
      source   => 'puppet:///modules/env/std/oar/access.conf',
      require  => Package[$oar_packages];
    '/etc/oar/sshd_config':
      ensure   => present,
      owner    => root,
      group    => root,
      mode     => '0644',
      source   => 'puppet:///modules/env/std/oar/sshd_config',
      require  => Package[$oar_packages];
    '/var/lib/oar/.ssh/authorized_keys':
      ensure   => present,
      owner    => oar,
      group    => oar,
      mode     => '0644',
      source   => 'puppet:///modules/env/std/oar/oar_authorized_keys',
      require  => Package[$oar_packages];
    '/etc/default/oar-node':
      ensure   => present,
      owner    => root,
      group    => root,
      mode     => '0644',
      source   => 'puppet:///modules/env/std/oar/default_oar-node';
  }

  if $env::target_g5k {
    $key_values   = hiera("env::std::oar::ssh")

    file {
      "/var/lib/oar/.ssh/oarnodesetting_ssh.key":
        ensure   => file,
        owner    => oar,
        group    => oar,
        mode     => '0600',
        content  => $key_values['oarnodesetting_ssh_key'];
      "/var/lib/oar/.ssh/oarnodesetting_ssh.key.pub":
        ensure   => file,
        owner    => oar,
        group    => oar,
        mode     => '0644',
        content  => $key_values['oarnodesetting_ssh_key_pub'];
      "/var/lib/oar/.ssh/id_rsa":
        ensure   => file,
        owner    => oar,
        group    => oar,
        mode     => '0600',
        content  => $key_values['id_rsa'];
      "/var/lib/oar/.ssh/id_rsa.pub":
        ensure   => file,
        owner    => oar,
        group    => oar,
        mode     => '0644',
        content  => $key_values['id_rsa_pub'];
    }
  }
}
