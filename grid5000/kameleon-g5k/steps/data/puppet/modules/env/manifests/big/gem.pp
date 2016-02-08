class env::big::gem () {

  file {
    '/etc/gemrc':
      ensure     => present,
      owner      => root,
      group      => root,
      mode       => '644',
      source     => 'puppet:///modules/env/big/gem/gemrc';
  }
}
