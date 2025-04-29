CXXFLAGS = -std=c++17 -O2 -g -Wall -pthread
LDLIBS = -lpthread -lCAEN_FELib -lz

CXXFLAGS  += $(shell root-config --cflags)
LDLIBS    += $(shell root-config --libs)

.PHONY: default all clean

default: psd2-test #scope2-test
all: default

psd2-test: PSD.cpp PSD2.o RawToPSD2.o
	$(CXX) $(CXXFLAGS) PSD.cpp PSD2.o RawToPSD2.o $(LDLIBS) -o psd2-test

PSD2.o: PSD2.cpp PSD2.hpp
	$(CXX) $(CXXFLAGS) $(LDLIBS) -c PSD2.cpp

RawToPSD2.o: RawToPSD2.cpp RawToPSD2.hpp
	$(CXX) $(CXXFLAGS) $(LDLIBS) -c RawToPSD2.cpp
	
# scope2-test: scope.cpp Scope2.o
# 	$(CXX) $(CXXFLAGS) scope.cpp Scope2.o $(LDLIBS) -o scope2-test

# Scope2.o: Scope2.cpp Scope2.hpp
# 	$(CXX) $(CXXFLAGS) $(LDLIBS) -c Scope2.cpp

clean:
	-rm -f *.o *~
	-rm -f psd2-test scope2-test
