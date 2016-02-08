class env::base::tuning (){

# *** Setting up max open file descriptors
#
# See: https://www.grid5000.fr/mediawiki/index.php/Max_open_file_descriptors_tuning
#

  file {
    '/etc/security/limits.conf':
      ensure   => file,
      owner    => root,
      group    => root,
      mode     => '0644',
      source   => 'puppet:///modules/env/base/tuning/limits.conf';
  }
  augeas {
    'pam_limits_modification':
      context => "/files/etc/pam.d/common-session",
      incl    => "/etc/pam.d/common-session",
      lens    => "Pam.lns",
      onlyif  => "match *[type='session' and control='required' and module='pam_limits.so'] size < 1",
      changes => [
                  "ins 0 after *[type='session'][last()]",
                  "set 0/type session",
                  "set 0/control required",
                  "set 0/module pam_limits.so",
      ];
  }

# *** Setting up TCP buffers
#
# See: https://www.grid5000.fr/mediawiki/index.php/TCP_bandwidth_tuning
#

  file {
    '/etc/sysctl.conf':
      ensure   => file,
      owner    => root,
      group    => root,
      mode     => '0644',
      source   => 'puppet:///modules/env/base/tuning/sysctl.conf';
  }
}

# C-state are DISABLED on node for jessie (and later?) environments, variant base and above (xen, big, std, nfs).
# This is done in Kadeploy description file. Parameter intel_idle.max_cstate=0 and idle=poll are used (cf image dsc file, or kaenv -p jessie-x64-{base,xen,nfs,big,std})
