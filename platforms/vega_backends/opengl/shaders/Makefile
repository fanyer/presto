# Cg compiler - GLSL -> D3D ps 2.0

PROFILE=ps_2_0
CGOPTS=-oglsl -profile $(PROFILE) -strict -glslWerror -O3 -d3d #-nostdlib
CG=/usr/bin/cgc

IN_FILES=$(shell ls -1 *.shd)

%.d3d: %.shd
	$(CG) $(CGOPTS) -o $@ $<

all: $(IN_FILES:.shd=.d3d)

.PHONY: clean

clean:
	$(RM) *.d3d