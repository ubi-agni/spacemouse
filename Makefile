PREFIX ?= /vol/nirobots
CFLAGS += -fPIC

.PHONY: all
all: spacenavi lib

spacenavi.c: spacenavi.h

spacenavi: CFLAGS += -DTEST
spacenavi: spacenavi.c
	$(CC) $(CFLAGS) -o $@ $^

lib: libspacenavi.a
libspacenavi.a: spacenavi.o
	ar r libspacenavi.a spacenavi.o

install: libspacenavi.a
	cp libspacenavi.a $(PREFIX)/lib
	cp spacenavi.h $(PREFIX)/include

clean:
	rm -f *.a *.o spacenavi
