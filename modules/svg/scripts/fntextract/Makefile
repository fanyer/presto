all: fntextract

fonts: fntextract
	./fntextract VeraMono.ttf svg-mo.dat
	./fntextract Vera.ttf svg-sa.dat
	./fntextract VeraSe.ttf svg-se.dat
	./fntextract VeraMoBd.ttf svg-mobd.dat
	./fntextract VeraSeBd.ttf svg-sebd.dat
	./fntextract VeraBd.ttf svg-sabd.dat

install:
	cp svg-mo.dat svg-sa.dat svg-se.dat svg-mobd.dat svg-sebd.dat svg-sabd.dat /usr/share/opera

fntextract: fntextract.cpp fntextract.h
	g++ -g -o fntextract fntextract.cpp `freetype-config --cflags --libs`
