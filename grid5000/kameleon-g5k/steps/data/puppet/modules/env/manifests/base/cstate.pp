# Force cstate to be disabled (cstate=0)
class env::base::cstate {

  file {
    "/usr/local/sbin/force_cstate_0.rb":
      mode    => '0744',
      owner   => root,
      group   => root,
      source  => "puppet:///modules/env/base/cstate/force_cstate_0.rb";
    "/etc/systemd/system/force_cstate_0.service":
      mode    => '0744',
      owner   => root,
      group   => root,
      source  => "puppet:///modules/env/base/cstate/force_cstate_0.service";
  }

  service {
    "force_cstate_0":
      ensure  => running,
      enable  => true,
      require => File["/usr/local/sbin/force_cstate_0.rb", "/etc/systemd/system/force_cstate_0.service"];
  }
}
