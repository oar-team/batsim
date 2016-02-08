#!/usr/bin/ruby

Signal.trap("TERM") {
  $f.close()
  exit
}

$f = File.open("/dev/cpu_dma_latency", "w")
$f.syswrite("0")
sleep

