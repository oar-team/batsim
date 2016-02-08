class env::min::kernel::setup_links {

  # Setup symlink for initrd and vmlinuz
  file {
    '/initrd.img':
      ensure    => link,
      target    => "/boot/initrd.img-${kernelrelease}";
    '/vmlinuz':
      ensure    => link,
      target    => "/boot/vmlinuz-${kernelrelease}";
  }
}
