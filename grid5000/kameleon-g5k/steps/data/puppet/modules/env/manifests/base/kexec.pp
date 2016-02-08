class env::base::kexec {

  file {
    "/etc/default/kexec":
      mode    => '0755',
      owner   => root,
      group   => root,
      source  => "puppet:///modules/env/base/kexec/kexec";
  }

  package {
    'kexec-tools':
      ensure  => installed;
  }

}
