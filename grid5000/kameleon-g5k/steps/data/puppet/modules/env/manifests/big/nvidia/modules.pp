class env::big::nvidia::modules () {

  augeas {
    'blacklist_vga16fb':
      context   => "/files/etc/modprobe.d/blacklist.conf",
      tag       => "modules",
      changes   =>["set blacklist[last()+1] vga16fb",],
      onlyif    =>"match blacklist[.='vga16fb'] size == 0 ";
    'blacklist_rivafb':
      context   => "/files/etc/modprobe.d/blacklist.conf",
      tag       => "modules",
      changes   =>["set blacklist[last()+1] rivafb",],
      onlyif    =>"match blacklist[.='rivafb'] size == 0 ";
    'blacklist_rivatv':
      context   => "/files/etc/modprobe.d/blacklist.conf",
      tag       => "modules",
      changes   =>["set blacklist[last()+1] rivatv",],
      onlyif    =>"match blacklist[.='rivatv'] size == 0 ";
    'blacklist_nvidiafb':
      context   => "/files/etc/modprobe.d/blacklist.conf",
      tag       => "modules",
      changes   =>["set blacklist[last()+1] nvidiafb",],
      onlyif    =>"match blacklist[.='nvidiafb'] size == 0 ";
  }
}
