noinst_PROGRAMS = calc
noinst_DATA = tntnet.properties

calc_SOURCES = \
	calc.ecpp \
	main.cpp

EXTRA_DIST = \
	calc.ecpp

AM_CPPFLAGS = -I$(top_srcdir)/framework/common
calc_LDADD = $(top_builddir)/framework/common/libtntnet.la -lcxxtools

CLEANFILES = calc.cpp tntnet.properties

SUFFIXES=.ecpp .cpp
.ecpp.cpp:
	$(AM_V_GEN)$(top_builddir)/sdk/tools/ecppc/ecppc -o $@ $<

tntnet.properties:
	$(AM_V_GEN)echo "rootlogger=INFO" > $@
