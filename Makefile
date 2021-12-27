TARGET=glib_copy
all: $(TARGET)

SRC=
SRC+=main.c
SRC+=file_util.c

GIO_FLAGS_L=$(shell pkg-config --libs gio-2.0 gio-2.0)
GIO_FLAGS_I=$(shell pkg-config --cflags-only-I gio-2.0)

OBJ=$(subst .c,.o,$(SRC))

$(TARGET): $(OBJ)
	gcc -ggdb3 -O0 $(OBJ) $(GIO_FLAGS_L) -o $@

clean:
	rm -f $(TARGET)
	rm -f *.o
	rm -f *.d

DEPFLAGS = -MT $@ -MMD -MP -MF $*.d

%.o:%.c
	gcc -ggdb3 -O0 -c -std=gnu99 $(DEPFLAGS) $(GIO_FLAGS_I) $< -o $@

$(SRC): Makefile
	touch $@

-include $(wildcard $(SRC:.c=.d))
