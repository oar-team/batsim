class env::base::packages () {

  # Removed : findutils, grep, gzip, man-db, sed, tar, wget, diffutils, multiarch-support
  $utils = [ 'bzip2', 'curl', 'dnsutils', 'dtach', 'host', 'ldap-utils', 'lshw', 'lsof', 'bsd-mailx', 'm4', 'netcat-openbsd', 'rsync', 'screen', 'strace', 'taktuk', 'telnet', 'time', 'xstow', 'sudo' ]
  $languages = [ 'perl', 'python', 'ipython', 'ruby' ]

  $installed = [ $utils, $languages ]

  package {
    $installed:
      ensure => installed;
  }
}
