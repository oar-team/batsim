class env::std::net_access {

  package {
    "rsyslog":
      ensure    => installed;
  }

  file {
    "/etc/rsyslog.conf":
      mode    => '0600',
      owner   => root,
      group   => root,
      source  => "puppet:///modules/env/std/net_access/rsyslog.conf";
    # iptables installed by kameleon.
    "/etc/network/if-pre-up.d/iptables":
      mode    => '0755',
      owner   => root,
      group   => root,
      source  => "puppet:///modules/env/std/net_access/iptables"
  }
}

