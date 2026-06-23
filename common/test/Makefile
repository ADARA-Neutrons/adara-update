#
# *** ADARA Parser Unit Tests (GoogleTest) ***
#
# Standalone Makefile (mirrors ../../parser/Makefile) that builds the ported
# Mantid ADARA packet tests against the ADARA parser sources.
#
# GoogleTest is provided by the pixi/conda environment; CONDA_PREFIX must be set
# (it is when running inside `pixi run` / `pixi shell`).
#
#   make            # build the test binary
#   make check      # build and run the tests
#   make clean
#

PROG = adara-packet-test

CXX ?= g++

# GoogleTest's headers require C++17 or newer.
CXXFLAGS = -Wall -Wextra -std=c++17 -g

# Parser headers live one directory up (common/).
CPPFLAGS = -I..
LDFLAGS =

ifneq ($(CONDA_PREFIX),)
CPPFLAGS += -I$(CONDA_PREFIX)/include
LDFLAGS += -L$(CONDA_PREFIX)/lib -Wl,-rpath,$(CONDA_PREFIX)/lib
endif

# GoogleTest (gtest_main provides main()) plus pthread.
LDADD = -lgtest -lgtest_main -lpthread

# Parser objects under test. POSIXParser is not needed: the test fixture derives
# directly from ADARA::Parser (implemented in ADARAParser.cc).
PARSER_OBJS = ADARAPackets.o ADARAParser.o

OBJS = ADARAPacketTest.o $(PARSER_OBJS)

all: $(PROG)

ADARAPackets.o: ../ADARAPackets.cc ../ADARAPackets.h ../ADARA.h ../ADARAUtils.h
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c -o $@ ../ADARAPackets.cc

ADARAParser.o: ../ADARAParser.cc ../ADARAParser.h ../ADARAPackets.h ../ADARA.h
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c -o $@ ../ADARAParser.cc

ADARAPacketTest.o: ADARAPacketTest.cc ADARAPacketsTestData.h ../ADARAParser.h ../ADARAPackets.h
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c -o $@ ADARAPacketTest.cc

$(PROG): $(OBJS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $(OBJS) $(LDADD)

check: $(PROG)
	./$(PROG)

clean:
	rm -f $(PROG) $(OBJS)

.PHONY: all check clean
