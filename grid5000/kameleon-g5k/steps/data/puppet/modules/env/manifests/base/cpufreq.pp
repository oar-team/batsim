class env::base::cpufreq (){

  package {
    'cpufrequtils':
      ensure   => installed;
  }

  file {
    '/etc/default/cpufrequtils':
      ensure   => file,
      owner    => root,
      group    => root,
      mode     => '0644',
      source   => 'puppet:///modules/env/base/cpufreq/cpufrequtils'
  }
}
