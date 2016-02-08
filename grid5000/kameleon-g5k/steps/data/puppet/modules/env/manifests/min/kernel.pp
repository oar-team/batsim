class env::min::kernel {

  # Install kernel : not required here. Kameleon set-up the kernel because it is required to have SSH access on the build VM (only access way for virtualbox backend)

  # Setup links: creates symlink /vmlinuz and /initrd pointing to real files in /boot
  include env::min::kernel::setup_links

  # blacklist undesired module and regenerate initramfs
  include env::min::kernel::modules

  # initramfs regeneration declaration
  include env::min::kernel::initramfs

}
