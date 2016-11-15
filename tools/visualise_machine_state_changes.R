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

# http://stackoverflow.com/questions/21887088/generate-a-filled-geom-step
binded <- rbind(
  melted,
  transform(melted[order(melted$time),],
    time=time - 1e-9,  # required to avoid crazy steps
    value=ave(value, variable, FUN=function(z) c(z[[1]], head(z, -1L)))
) )

palette = c("#000000",
            "#c552bf",
            "#b68f00",
            "#ff987b",
            "#97e78b")

w=16
h=10

pdf(args[2], width=w, height=h)
ggplot(binded) + geom_area(aes(x=time,y=value,fill=variable)) + scale_fill_manual(values = palette)
dev.off()
