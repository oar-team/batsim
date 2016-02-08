class env::big::kvm () {

  # If sudo is used somewhere else, he shoud be placed in 'packages' instead of 'kvm' class.
  $packages = [ 'kvm', 'uml-utilities', 'virtinst',  'genisoimage', 'libvirt-bin', 'python-libvirt', 'libguestfs-tools' ]
  # WARNING! Due to bug #5257, this should NOT work on wheezy environments. Cf old chef recipe setup/recipes/kvm to see a workaround

  package {
    $packages:
      ensure    => installed;
  }

  file {
    '/etc/sudoers':
      ensure    => present,
      owner     => root,
      group     => root,
      mode      => '0440',
      source    => 'puppet:///modules/env/big/kvm/sudoers',
      require   => Package['sudo'];
    '/etc/udev/rules.d/60-qemu-system.rules':
      ensure    => present,
      owner     => root,
      group     => root,
      mode      => '0644',
      source    => 'puppet:///modules/env/big/kvm/60-qemu-system.rules';
    '/usr/local/bin/create_tap':
      ensure    => present,
      owner     => root,
      group     => root,
      mode      => '0755',
      source    => 'puppet:///modules/env/big/kvm/create_tap';
    '/usr/local/bin/random_mac':
      ensure    => present,
      owner     => root,
      group     => root,
      mode      => '0755',
      source    => 'puppet:///modules/env/big/kvm/random_mac';
  }

  service {
    'uml-utilities':
      provider  => systemd,
      enable    => false;
  }
  # Not sure this is required anymore. Try without, uncomment if needed
  # augeas {
  #   'set_XDG_RUNTIME_DIR':
  #     context   => "/files/etc/profile",
  #     tag       => "modules",
  #     changes   =>["set export[last()+1] XDG_RUNTIME_DIR=/tmp/$USER-runtime-dir",];
  # }
}
