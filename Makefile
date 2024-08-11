MAKE ?= make # if you are using mingw-make or smh

ARGV ?=
SRCDIR ?= src
RELEASE ?= 0

ifeq ($(RELEASE),1)
	PREPROCESSORS = -DNDEBUG
	BIN = bin/Release
	BIN_INT = bin-int/Release
else
	PREPROCESSORS =
	BIN = bin/Debug
	BIN_INT = bin-int/Debug
endif

PROG = main
SRC = main.c egl_stuff.c utils.c
OBJ = $(addprefix ${BIN_INT}/, $(SRC:.c=.o))

CC = gcc
INCS = -I$(SRCDIR) -Ivendor -I/usr/include/X11 -I/usr/include/xcb -I/usr/include/GL -I/usr/include/cairo -I/usr/include/freetype2 -I/usr/include/EGL $(shell pkg-config --cflags xft) $(shell pkg-config --cflags --libs xcb xcb-xtest xcb-util)

LIBS = -lm -lX11 -lxcb -lGL -lXft -lX11-xcb -lgif -lxcb-image -Lvendor $(shell pkg-config --libs xcb-atom) $(shell pkg-config --libs egl)

LDFLAGS = ${LIBS}
CFLAGS = -Wall -Wextra -O0 ${INCS} -DSHM

default: run
.PHONY: default

run: ${BIN}/${PROG}
	./${BIN}/${PROG} ${ARGV}
.PHONY: run

all: compile_commands.json ${BIN}/${PROG}
.PHONY: all

compile_commands.json:
	bear -- $(MAKE) ${BIN}/${PROG}

$(BIN)/$(PROG): $(OBJ) | $(BIN)
	$(CC) $(PREPROCESSORS) -o $@ $(OBJ) $(LDFLAGS)

${BIN_INT}/%.o: $(SRCDIR)/%.c | ${BIN_INT}
	$(CC) $(PREPROCESSORS) -c $< -o $@ $(CFLAGS)

clean:
	@# I do need to change it to the variables but im too lazy
	-rm -rf bin bin-int compile_commands.json .cache
.PHONY: clean

${BIN}:
	mkdir -p $@

${BIN_INT}:
	mkdir -p $@

