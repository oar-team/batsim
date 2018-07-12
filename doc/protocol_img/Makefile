DIA= $(wildcard *.dia)
SVG= $(DIA:.dia=.svg)
PDF= $(SVG:.svg=.pdf)
PNG= $(SVG:.svg=.png)

all: pdf png
pdf: ${PDF}
png: ${PNG}

%.svg: %.dia
	dia -e $@ -t svg $^

%.pdf: %.svg
	inkscape -A $@ $^ -b "#ffffff"

%.png: %.pdf
	inkscape -e $@ $^ -b "#ffffff"

clean:
	rm -f *.pdf *.png
