DIA= $(wildcard *.dia)
SVG= $(DIA:.dia=.svg)
PDF= $(SVG:.svg=.pdf)
PNG= $(SVG:.svg=.png)

all: pdf png
pdf: ${PDF}
png: ${PNG}

%.pdf: %.svg
	inkscape -A $@ $^ -b "#ffffff"

%.png: %.svg
	inkscape -e $@ $^ -b "#ffffff"

%.svg: %.dia
	dia -e $@ -t svg $^

clean:
	rm -f *.pdf *.png
