# Makefile to create and run all the configuration tests

all:	common
	@rm -f cfg[0-9] cfg[0-9][0-9]
	@rm -f cfg[0-9].cpp cfg[0-9][0-9].cpp
	@rm -f cfg[0-9].i cfg[0-9][0-9].i

common:
	./mkcfgs.pl
	$(MAKE) -f Makefile.auto

verbose:	common
	@rm -f cfg*.cpp cfg*.o

compile:	common
