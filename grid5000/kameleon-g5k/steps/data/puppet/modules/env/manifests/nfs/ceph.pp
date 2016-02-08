class env::nfs::ceph (
  $version = 'hammer'
) {

  $ceph_packages = [ 'ceph', 'ceph-fs-common', 'ceph-fuse', 'ceph-mds' ]
  $ceph_packages_g5k_repository = [ 'ceph-deploy' ] # Ceph deploy is not distributed on ceph repo for jessie. So we picked wheezy package that works on jessie and distribute it.
  $ceph_packages_g5k_repository_dep = [ 'python-setuptools' ]
  case $operatingsystem {
    'Debian': {
      # Add ceph repositories.
      class {
        'env::nfs::ceph::apt':
          version => $version;
      }

      # Install ceph and deps
      package {
        $ceph_packages :
          ensure   => installed,
          require  => [Class['env::nfs::ceph::apt'], Exec['/usr/bin/apt-get update']];
      }

      # Ceph-deploy is used by dfsg5k to setup easily a ceph fs on g5k nodes.
      # Retrieve ceph-deploy: not availaible on ceph repositories atm.
      exec {
        "retrieve_ceph-deploy":
          command  => "/usr/bin/wget --no-check-certificate -q https://www.grid5000.fr/packages/debian/ceph-deploy_all.deb -O /tmp/ceph-deploy_all.deb",
          creates  => "/tmp/g5kchecks_all.deb";
      }
      # Install ceph-deploy from deb retrieved on g5k.
      package {
        "ceph-deploy":
          ensure   => installed,
          provider => dpkg,
          source   => "/tmp/ceph-deploy_all.deb",
          require  => [Exec["retrieve_ceph-deploy"], Package[$ceph_packages_g5k_repository_dep] ] ;
        $ceph_packages_g5k_repository_dep:
          ensure   => installed;
      }


      # Ensure service does not start at boot
      service {
        'ceph':
          enable  => false,
          require => Package['ceph'];
      }
    }
    default: {
      err "${operatingsystem} not suported."
    }
  }
}
