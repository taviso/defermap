CFLAGS  = -fPIC -O2 -march=native -Wall -W
LDFLAGS = -shared
LDLIBS  = -lX11 -ldl
PREFIX  = /usr

.PHONY: clean install

all: defermap.so

defermap.so: defermap.o
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $^ $(LDLIBS)

install: defermap.so
	install --strip $^ $(PREFIX)/lib/

clean:
	rm -f *.so *.o
