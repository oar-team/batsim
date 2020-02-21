#!/usr/bin/env Rscript
library(battools)
library(dplyr)

# Read a SWF workload.
kth_sp2 = read_swf("http://www.cs.huji.ac.il/labs/parallel/workload/l_kth_sp2/KTH-SP2-1996-2.1-cln.swf.gz")

# Only work on a short period (a month) from an arbitrary time point.
date_begin = mean(kth_sp2$submit_time)
date_end = date_begin + 60*60*24*30*3
month = kth_sp2 %>% filter(submit_time >= date_begin) %>% filter(submit_time < date_end)

# Generate a Batsim workload.
workload = swf_to_batworkload_delay(month, 100, subtime_strat = "translate_to_zero")
write_batworkload(workload, "./kth_month.json")
