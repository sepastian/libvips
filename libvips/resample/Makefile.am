# only build the C++ stuff if ENABLE_CXX
# you'd think we could just define a couple of variables, but that seems to
# confuse libtool and make it link the library with g++
# instead, have two completely different paths
if ENABLE_CXX

libresample_la_SOURCES = \
	quadratic.c \
	resample.c \
	resample.h \
	shrink.c \
	im_affine.c \
	interpolate.c \
	transform.c \
	resample_dispatch.c \
	bicubic.cpp \
	lbb.cpp \
	nohalo.cpp \
	vsqbs.cpp \
	templates.h 

else

libresample_la_SOURCES = \
	resample.c \
	resample.h \
	shrink.c \
	im_affine.c \
	interpolate.c \
	transform.c \
	resample_dispatch.c 

EXTRA_DIST = \
	bicubic.cpp \
	lbb.cpp \
	nohalo.cpp \
	vsqbs.cpp \
	templates.h 

endif

noinst_LTLIBRARIES = libresample.la

INCLUDES = -I${top_srcdir}/libvips/include @VIPS_CFLAGS@ @VIPS_INCLUDES@ 
