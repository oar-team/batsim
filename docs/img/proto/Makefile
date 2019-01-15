DIA= $(wildcard *.dia)
SVG= $(DIA:.dia=.svg)
PDF= $(SVG:.svg=.pdf)
PNG= $(SVG:.svg=.png)

all: png
pdf: ${PDF}
png: ${PNG}

%.svg: %.dia
	dia -e $@ -t svg $^

%.pdf: %.svg
	inkscape -A $@ $^

%.png: %.svg
	inkscape -e $@ $^

clean:
	rm -f *.pdf *.png
