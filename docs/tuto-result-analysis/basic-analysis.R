#!/usr/bin/env Rscript
library('tidyverse') # Use the tidyverse library.
theme_set(theme_bw()) # Cosmetics.

jobs = read_csv('out_jobs.csv') # Read the jobs file.

# Manually compute some metrics on each job.
jobs = jobs %>% mutate(slowdown = (finish_time - starting_time) /
                                  (finish_time - submission_time),
                       longer_than_one_minute = execution_time > 60)

# Manually compute aggregated metrics.
# Here, the mean waiting time/slowdown for jobs with small execution time.
metrics = jobs %>% filter(longer_than_one_minute == FALSE) %>%
    summarize(mean_waiting_time = mean(waiting_time),
              mean_slowdown = mean(slowdown))

print(metrics) # Print aggregated metrics.

# Visualize what you want...
# Is there a link between jobs' waiting time and size?
ggplot(jobs) +
    geom_point(aes(y=waiting_time, x=requested_number_of_processors)) +
    ggsave('plot_wt_size.pdf')

# Is this still true depending on job execution time?
ggplot(jobs) +
    geom_point(aes(y=waiting_time, x=requested_number_of_processors)) +
    facet_wrap(~longer_than_one_minute) +
    ggsave('plot_wt_size_exectime.pdf')

# Is there a link with job size and execution time?
ggplot(jobs) +
    geom_violin(aes(factor(requested_number_of_processors), execution_time)) +
    ggsave('plot_exectime_size.pdf')
