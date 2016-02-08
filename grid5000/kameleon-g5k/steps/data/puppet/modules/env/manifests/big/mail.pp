class env::big::mail () {

  $mail_packages = [ postfix ]

 package {
   $mail_packages:
     ensure    => installed,
     require   => Exec['fix_resolv_conf'];
 }
#   exec {
#     'install_postfix_manual':
#       command       => "/bin/echo 'postfix   postfix/main_mailer_type    select  No configuration' | /usr/bin/debconf-set-selections",
#   }
  # This is a damn dirty patch due to a bug in debian package for postfix. In the package, the, postinst create a /etc/postfix/main.cfg with a myhostname attribute
  # myhostname attribute is retrieved by concatening hostname found in /etc/hostname and domain extract from /etc/resolv.conf. This domain MAY have a trailing dot (.grid5000.fr.)
  # Sadly, after this, newaliases is called on this /etc/postfix/main.cfg and doesn't digest any trailing dot for attribute myhostname.
  exec {
    'fix_resolv_conf':
      #command   => "/bin/sed 's/\\(\\s*domain\\s*.*\\)\\./\\1/' -i /etc/resolv.conf"
      command   => "/bin/sed 's/\\([^\\s]*\\)\\.\\(\\s\\|$\\)/\\1\\2/g' -i /etc/resolv.conf"
  }
  file {
    '/etc/postfix/':
      ensure    => directory,
      owner     => root,
      group     => root,
      mode      => '644';
    '/etc/postfix/main.cfg':
      ensure    => present,
      owner     => root,
      group     => root,
      mode      => '644',
      source    => 'puppet:///modules/env/big/mail/postfix.cfg',
      require   => File['/etc/postfix'];
  }
}
