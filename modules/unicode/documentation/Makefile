all: wordbreak.svg sentencebreak.svg

%.svg: %.dot
	dot -o $@ -Tsvg $<
