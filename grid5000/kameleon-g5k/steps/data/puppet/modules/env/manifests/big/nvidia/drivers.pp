class env::big::nvidia::drivers () {

  ### This class exists for gpuclus cluster, that require a recent version of nvidia driver (346.22)

  # May be changed to a link inside g5k if required
  $driver_source = 'https://www.grid5000.fr/packages/debian/NVIDIA-Linux-x86_64-346.22.run'

  package {
    ['module-assistant', 'dkms']:
      ensure    => installed;
  }
  exec{
    'retrieve_nvidia_drivers':
      command   => "/usr/bin/wget --no-check-certificate -q $driver_source -O /tmp/NVIDIA-Linux.run; chmod u+x /tmp/NVIDIA-Linux.run",
      timeout   => 1200, # 20 min
      creates   => "/tmp/NVIDIA-Linux.run";
    'prepare_kernel_module_build':
      command   => "/usr/bin/m-a prepare -i",
      user      => root,
      require   => Package['module-assistant'];
    'install_nvidia_driver':
      command   => "/tmp/NVIDIA-Linux.run -qa --no-cc-version-check --ui=none --dkms; /bin/rm /tmp/NVIDIA-Linux.run",
      user      => root,
      require   => [Exec['prepare_kernel_module_build'], File['/tmp/NVIDIA-Linux.run'], Package['dkms']];
  }
  file{
    '/tmp/NVIDIA-Linux.run':
      ensure    => file,
      require   => Exec['retrieve_nvidia_drivers'];
  }
}
