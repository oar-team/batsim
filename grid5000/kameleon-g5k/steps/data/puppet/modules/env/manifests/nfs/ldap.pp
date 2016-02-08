class env::nfs::ldap () {

  # Contains configuration to have ldap authentication working (ldap, nss, pam, nscd...)

  $ldap_packages = [ libnss-ldapd, libpam-ldapd, nslcd ]

  package {
    $ldap_packages:
      ensure   => installed;
  }

  file {
    '/etc/ldap/ldap.conf':
      ensure   => file,
      owner    => root,
      group    => root,
      mode     => '0644',
      source   => 'puppet:///modules/env/nfs/ldap/ldap.conf';
    '/etc/ldap/certificates':
      ensure   => directory,
      owner    => root,
      group    => root,
      mode     => '0755';
    '/etc/ldap/certificates/ca.grid5000.fr.cert':
      ensure   => file,
      owner    => root,
      group    => root,
      mode     => '0644',
      source   => 'puppet:///modules/env/nfs/ldap/ca.grid5000.fr.cert',
      require  => File['/etc/ldap/certificates'];
    '/etc/nsswitch.conf':
      ensure   => file,
      owner    => root,
      group    => root,
      mode     => '0644',
      source   => 'puppet:///modules/env/nfs/ldap/nsswitch.conf';
    '/etc/libnss-ldap.conf':
      ensure   => file,
      owner    => root,
      group    => root,
      mode     => '0644',
      source   => 'puppet:///modules/env/nfs/ldap/libnss-ldap.conf';
    '/etc/pam_ldap.conf':
      ensure   => file,
      owner    => root,
      group    => root,
      mode     => '0644',
      source   => 'puppet:///modules/env/nfs/ldap/libnss-ldap.conf';
    '/etc/pam.d/common-account':
      ensure   => file,
      owner    => root,
      group    => root,
      mode     => '0644',
      source   => 'puppet:///modules/env/nfs/ldap/common-account';
    '/etc/pam.d/common-auth':
      ensure   => file,
      owner    => root,
      group    => root,
      mode     => '0644',
      source   => 'puppet:///modules/env/nfs/ldap/common-auth';
    '/etc/pam.d/common-password':
      ensure   => file,
      owner    => root,
      group    => root,
      mode     => '0644',
      source   => 'puppet:///modules/env/nfs/ldap/common-password';
    '/etc/nscd.conf':
      ensure   => file,
      owner    => root,
      group    => root,
      mode     => '0644',
      source   => 'puppet:///modules/env/nfs/ldap/nscd.conf',
      notify   => Service['nscd'];
    '/etc/nslcd.conf':
      ensure   => file,
      owner    => root,
      group    => root,
      mode     => '0644',
      source   => 'puppet:///modules/env/nfs/ldap/nslcd.conf',
      notify   => Service['nslcd'];
  }

  service {
    'nscd':
      ensure   => running;
    'nslcd':
      ensure   => running;
  }
}
