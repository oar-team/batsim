class env::min::apt::keyring {

  $operatingsystem_downcase = inline_template('<%= @operatingsystem.downcase %>')
  package {
    "${operatingsystem_downcase}-keyring":
      ensure => installed;
    "${operatingsystem_downcase}-archive-keyring":
      ensure => installed;
  }

}
