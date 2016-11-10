#!/usr/bin/env Rscript
library(ggplot2)
library(reshape)

args = commandArgs(trailingOnly=TRUE)

# Reading arguments
if (length(args)!=2)
{
  stop("Two arguments (and only two) must be supplied: the input file and the output file.n", call.=FALSE)
}

changes = read.csv(args[1])
changes_without_duplicated_times = changes[!duplicated(changes$time),]
melted = melt(changes_without_duplicated_times, id="time")

palette = c("#000000",
            "#c552bf",
            "#b68f00",
            "#ff987b",
            "#97e78b")

pdf(args[2])
ggplot(melted) + geom_area(aes(x=time,y=value,fill=variable)) + scale_fill_manual(values = palette)
dev.off()
