INCLUDES = -I$(top_srcdir)/include
METASOURCES = AUTO
lib_LTLIBRARIES = libmultigain.la
libmultigain_la_LDFLAGS = -no-undefined -version-info 1:0:0
libmultigain_la_SOURCES = \
	decode.cpp \
	gain_analysis.c \
	tag_locate.cpp
#AM_CFLAGS = -fpic -std=c99 -pedantic -Wall
AM_CFLAGS = -std=c99 -pedantic -Wall
AM_CXXFLAGS = -std=c++98 -Wall
