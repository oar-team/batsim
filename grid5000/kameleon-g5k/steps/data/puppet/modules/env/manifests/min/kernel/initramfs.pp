class env::min::kernel::initramfs {

  exec {
    'generate_initramfs':
      command     => "/usr/sbin/update-initramfs -u -k $kernelrelease",
      refreshonly => true;
  }
}
