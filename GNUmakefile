SOURCES=EXIFfromMP4.cpp
CFLAGS=-Wno-multichar -D_FILE_OFFSET_BITS=64 -fno-threadsafe-statics
CFLAGS2=-O3
LDFLAGS=

EXIFfromMP4 : $(SOURCES) GNUmakefile
	$(CXX) -o $@ $(CFLAGS) $(CFLAGS2) $(SOURCES) $(LDFLAGS)

clean :
	rm -f EXIFfromMP4
