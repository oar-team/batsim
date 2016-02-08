class env::min::apt::grid5000 () {

  augeas {
    'apt_g5k_repository':
       context  => "/files/etc/apt/sources.list",
       onlyif   => "match #comment[. = 'G5K sources packages'] size < 1",
       changes  => [
                    "set #comment[last()+1] '#'",
                    "set #comment[last()+2] 'G5K sources packages'",
                    "set #comment[last()+3] '#'",
                    "set #comment[last()+4] 'Some debianized packages are used on squeeze reference environments :'",
                    "set #comment[last()+5] '* tgz-g5k : a tool to create neutral deployment tgz image'",
                    "set #comment[last()+6] '* mx : driver for Myrinet'",
                    "set #comment[last()+7] '#'",
                    "set #comment[last()+8] 'Uncomment the following lines to get the binaries/sources of these packages'",
                    "set #comment[last()+9] 'deb http://apt.grid5000.fr/debian sid main'",
                    "set #comment[last()+10] 'deb-src http://apt.grid5000.fr/debian sid main'"
                   ];
  }
}
