AUTOMAKE_OPTIONS = subdir-objects
AM_CPPFLAGS = -I../include

if DEBUGMODE
AM_CFLAGS = -g3 -O0 -DDEBUG
AM_CXXFLAGS = -g3 -O0 -DDEBUG
else
AM_CFLAGS = -O3 -DNDEBUG
AM_CXXFLAGS = -O3 -DNDEBUG
endif
AM_CXXFLAGS += -std=c++11 -fpermissive

bin_PROGRAMS = programs/tpprenum programs/tppmktop
MODULES = strutil.cpp structio.cpp tppnames.cpp exceptions.cpp logger.cpp bond_matrix.cpp core.cpp process_options.cpp
if GAMESSFEATURES
bin_PROGRAMS += programs/tppreadbonds
programs_tppreadbonds_SOURCES = programs/tppreadbonds.cpp structio.cpp topreader.cpp topwriter.cpp \
				hessian_accept.cpp calc.cpp logger.cpp strutil.cpp db_base.cpp \
				atom_definer.cpp bond_definer.cpp bond_matrix.cpp core.cpp
programs_tppreadbonds_LDADD = -llapack -lmysqlpp
endif
if SELFTESTFEATURES
bin_PROGRAMS += programs/tppmktest
programs_tppmktest_SOURCES = programs/tppmktest.cpp testcase.cpp structio.cpp topreader.cpp topwriter.cpp \
			     strutil.cpp tppnames.cpp logger.cpp db_base.cpp atom_definer.cpp bond_definer.cpp \
			     bond_matrix.cpp core.cpp
programs_tppmktest_LDADD = -lmysqlpp
endif
programs_tpprenum_SOURCES = programs/tpprenum.cpp pdbutils.cpp $(MODULES)
programs_tppmktop_SOURCES = programs/tppmktop.cpp db_base.cpp atom_definer.cpp bond_definer.cpp topwriter.cpp $(MODULES)
programs_tppmktop_LDADD = -lmysqlpp
