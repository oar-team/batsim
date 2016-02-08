class env::big::packages () {

  # editors
  $editors = [ 'jed', 'joe', 'emacs' ]

  #utils
  $utils = [ 'at', 'bash-completion', 'bc', 'connect-proxy', 'kbd', 'debhelper', 'debootstrap', 'diffstat', 'discover', 'discover-data', 'ftp', 'genders', 'gnuplot', 'hdparm', 'html2text', 'hwloc', 'inotify-tools', 'info', 'iperf', 'iputils-arping', 'iputils-tracepath', 'kanif', 'lsb-release', 'mysql-client', 'nmap', 'numactl', 'pv', 'r-base', 'stress', 'nuttcp', 'db-util', 'clustershell', 'parallel', 'cron', 'postgresql-client', 'tmux' ]

  # Dev and languages
  $general_dev = [ 'libreadline6-dev', 'autoconf', 'flex', 'bison', 'libyaml-0-2', 'cgdb', 'cmake', 'cvs', 'gdb', 'gfortran', 'git-core', 'subversion', 'libatlas-base-dev', 'libdate-calc-perl', 'libc6', 'libjson-perl', 'libjson-xs-perl', 'libssl-dev', 'make', 'patch', 'tcl', 'php5-cli', 'valgrind', 'libtool', 'libnuma-dev', 'libdb-dev', 'libatlas-dev', 'debconf-utils' ]
  $perl_dev = [ 'libwww-perl', 'libperl-dev', 'libswitch-perl' ]
  $python_dev = [ 'python-dev', 'python-imaging', 'python-matplotlib-data', 'python-matplotlib-doc', 'python-mysqldb', 'python-numpy', 'python-paramiko', 'python-scipy', 'python-sqlite', 'python-httplib2', 'python-yaml', 'python-psycopg2', 'python-pip']
  $java_dev = [ 'openjdk-7-jdk', 'openjdk-7-jre', 'ant' ]
  case $operatingsystem {
      'centos':           { $dev = [ $general_dev, $perl_dev, $python_dev, $java_dev, 'gcc', 'ruby-libs', 'ruby-devel', 'ruby-docs', 'ruby-rack', 'ruby-ri', 'ruby-irb', 'ruby-rdoc', 'ruby-mode', 'libwww-perl', 'libperl-dev' ]}
      /^(Debian|Ubuntu)$/:{ $dev = [ $general_dev, $perl_dev, $python_dev, $java_dev, 'build-essential', 'binutils-doc', 'ruby-dev', 'ruby-rack', 'ri', 'libruby', 'manpages-dev' ]  }
      default:            { $dev = [ $general_dev, $perl_dev, $python_dev, $java_dev ]}
    }
  $gems = [ 'mime-types', 'rdoc', 'rest-client', 'restfully']

  # System tools
  $system = [ 'htop', 'psmisc' ]


  file {
    '/etc/parallel/config':
      ensure  => absent,
      require => Package['parallel'];
    '/etc/at.allow':
      ensure  => present,
      owner   => root,
      group   => root,
      require => Package['at'];
    '/etc/cron.allow':
      ensure  => present,
      owner   => root,
      group   => root,
      require => Package['cron'];
  }

  package {
    [ $editors, $utils, $dev, $system ]:
      ensure   => installed;
    'rake_gem':
      name     => 'rake',
      provider => gem,
      ensure   => installed,
      require  => Package['ruby'];
    $gems:
      ensure   => installed,
      provider => gem,
      require  => [Package['rake_gem'], Package['ruby-rack']];
  }
}
