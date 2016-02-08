class env::xen::grub () {

  package {
    'grub2':
      ensure      => installed;
  }

  exec {
    'update-grub':
      command     => "/usr/sbin/update-grub2",
      refreshonly => true,
      require     => Package['grub2'];
  }
}
