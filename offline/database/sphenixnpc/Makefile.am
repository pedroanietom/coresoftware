AUTOMAKE_OPTIONS = foreign

# List of shared libraries to produce
lib_LTLIBRARIES = \
  libsphenixnpc.la

AM_CPPFLAGS = \
  -I$(includedir) \
  -isystem$(OFFLINE_MAIN)/include \
  -isystem$(ROOTSYS)/include

AM_LDFLAGS = \
  -L$(libdir) \
  -L$(OFFLINE_MAIN)/lib \
  -L$(OFFLINE_MAIN)/lib64

libsphenixnpc_la_SOURCES = \
  CDBUtils.cc \
  SphenixClient.cc


libsphenixnpc_la_LIBADD = \
  -lnopayloadclient

##############################################
# please add new classes in alphabetical order

pkginclude_HEADERS = \
  CDBUtils.h \
  SphenixClient.h

################################################
# linking tests

BUILT_SOURCES = testexternals.cc

noinst_PROGRAMS = \
  testexternals

testexternals_SOURCES = testexternals.cc
testexternals_LDADD = libsphenixnpc.la

testexternals.cc:
	echo "//*** this is a generated file. Do not commit, do not edit" > $@
	echo "int main()" >> $@
	echo "{" >> $@
	echo "  return 0;" >> $@
	echo "}" >> $@

##############################################
# please add new classes in alphabetical order

clean-local:
	rm -f $(BUILT_SOURCES)
