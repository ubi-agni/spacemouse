spacenavi: CFLAGS += -DTEST
spacenavi: spacenavi.c
	$(CC) $(CFLAGS) -o $@ $^

lib: libspacenavi.a
libspacenavi.a: spacenavi.o
	ar r libspacenavi.a spacenavi.o

install: libspacenavi.a
	cp libspacenavi.a /vol/nirobots/lib
	cp spacenavi.h /vol/nirobots/include

clean:
	rm -f *.a *.o spacenavi
