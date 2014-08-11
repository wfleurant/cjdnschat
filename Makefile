CFLAGS+=-g
sources:=main.c terminal.c session.c message.c
objects:=$(patsubst %.c,build/%.o,$(sources))

cjdnschat: $(objects)
	gcc $(CFLAGS) $(LFLAGS) -o $@ $^ -luv

$(objects): | build

build:
	mkdir build

deps.mk:
	gcc -MM $(sources) > $@

clean:
	rm cjdnschat
	rm build -Rf

$(objects): Makefile deps.mk

build/%.o: %.c
	gcc $(CFLAGS) -c -o $@ $<
-include deps.mk

