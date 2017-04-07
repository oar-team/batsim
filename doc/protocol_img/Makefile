SVG= $(wildcard *.svg)
PDF= $(SVG:.svg=.pdf)
PNG= $(SVG:.svg=.png)

all:pdf png

pdf: request_reply.pdf

png: request_reply.png

%.pdf: %.svg
	inkscape -A $@ $^ -b "#ffffff"

%.png: %.svg
	inkscape -e $@ $^ -b "#ffffff"

clean:
	rm -f *.pdf *.png
