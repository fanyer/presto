These files are downloaded from the OpenGL registry
(http://www.opengl.org/registry/).  Any use of these files in opera
should be protected by the FEATURE_3P_OPENGL_REGISTRY feature.

The gl.spec file explicitly says it is covered by the "SGI Free
Software B License Version 2.0", which can be found at
http://oss.sgi.com/projects/FreeB/.  The short overview states:

       "Other than the added option to refer to the License's website,
       the terms of the version 2.0 License are identical to the terms
       of the X11 license."

The other three files are (as far as I can tell) lacking any statement
about what license they are covered by.

http://www.sgi.com/products/software/opengl/license.html says:

       "Application developers do not need to license the OpenGL
       API. Generally, hardware vendors that are creating binaries to
       ship with their hardware are the only developers that need to
       have a license."

It is unclear whether this covers our use, though I doubt it is a
legally binding text anyway.

(I believe several other projects use some of these files in similar
ways that we do.  E.g. glew and Xorg.  Investigating how they have
dealt with the licensing issue may provide a shortcut for our
investigations.)

eirik 2011-10-26
