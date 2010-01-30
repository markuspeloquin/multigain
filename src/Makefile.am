INCLUDES = -I$(top_srcdir)/include
METASOURCES = AUTO
bin_PROGRAMS = multigain
lib_LTLIBRARIES = libmultigain.la
multigain_LDFLAGS = -lmultigain
multigain_SOURCES = \
	multigain.cpp
libmultigain_la_LDFLAGS = -no-undefined -version-info 1:0:0 -lmpg123
libmultigain_la_SOURCES = \
	decode.cpp \
	errors.cpp \
	gain_analysis.c \
	tag_locate.cpp
#AM_CFLAGS = -fpic -std=c99 -pedantic -Wall
AM_CFLAGS = -std=c99 -pedantic -Wall -g -O0
AM_CPPFLAGS = -D_FILE_OFFSET_BITS=64 -g -O0
AM_CXXFLAGS = -std=c++98 -Wall
