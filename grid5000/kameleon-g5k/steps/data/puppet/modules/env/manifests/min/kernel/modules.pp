class env::min::kernel::modules {

  # Blacklist modules
  file {
    '/etc/modprobe.d/blacklist.conf':
      ensure => 'file',
  }
  augeas {
    'blacklist_nouveau':
      context   => "/files/etc/modprobe.d/blacklist.conf",
      tag       => "modules",
      changes   =>["set blacklist[last()+1] nouveau",],
      onlyif    =>"match blacklist[.='nouveau'] size == 0 ";
    'blacklist_myri10ge':
      context   => "/files/etc/modprobe.d/blacklist.conf",
      tag       => "modules",
      changes   =>["set blacklist[last()+1] myri10ge",],
      onlyif    =>"match blacklist[.='myri10ge'] size == 0 ";
    'blacklist_usb_storage':
      context   => "/files/etc/modprobe.d/blacklist.conf",
      tag       => "modules",
      changes   =>["set blacklist[last()+1] usb_storage",],
      onlyif    =>"match blacklist[.='usb_storage'] size == 0 ";
    'blacklist_usbhid':
      context   => "/files/etc/modprobe.d/blacklist.conf",
      tag       => "modules",
      changes   =>["set blacklist[last()+1] usbhid",],
      onlyif    =>"match blacklist[.='usbhid'] size == 0 ";
    'blacklist_ohci_hcd':
      context   => "/files/etc/modprobe.d/blacklist.conf",
      tag       => "modules",
      changes   =>["set blacklist[last()+1] ohci_hcd",],
      onlyif    =>"match blacklist[.='ohci_hcd'] size == 0 ";
    'blacklist_ehci_hcd':
      context   => "/files/etc/modprobe.d/blacklist.conf",
      tag       => "modules",
      changes   =>["set blacklist[last()+1] ehci_hcd",],
      onlyif    =>"match blacklist[.='ehci_hcd'] size == 0 ";
    'blacklist_usbcore':
      context   => "/files/etc/modprobe.d/blacklist.conf",
      tag       => "modules",
      changes   =>["set blacklist[last()+1] usbcore",],
      onlyif    =>"match blacklist[.='usbcore'] size == 0 ";

  }

  # Retrieve all modules tag and regenerate initramfs
  # This allow another manifest to modify blacklist.conf
  # or another blacklist file and benefit from this refresh.
  # It only needs to tag Augeas with 'modules' tag.
  Augeas <| tag == "modules" |> ~> Exec['generate_initramfs']
}
