class env::base::sshfs {

  file {
    # for sshfs
    "/usr/bin/fusermount":
      mode    => '4755';
    "/etc/udev/rules.d/40-fuse.rules":
      mode    => '0644',
      owner   => root,
      group   => root,
      source  => "puppet:///modules/env/base/sshfs/40-fuse.rules";
  }


  package {
    'sshfs':
      ensure    => installed;
  }
}
