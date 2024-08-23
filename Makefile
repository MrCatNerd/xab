MAKE ?= make # if you are using mingw-make or smh
PKG_CONFIG = pkg-config

ARGV ?= ./res/vids/test_vid.3840x2160.mp4 1 # TODO: clean this when im done
SRCDIR ?= src
RELEASE ?= 0
VERBOSE ?= 0

ifeq ($(RELEASE),1)
	# Release

	PREPROCESSORS = -DNDEBUG -DNVERBOSE
	BIN = bin/Release
	BIN_INT = bin-int/Release
	BUILD_MODE_CFLAGS = -O3
else
	# Debug

ifeq ($(VERBOSE), 0)
    PREPROCESSORS = -DNVERBOSE
else
	PREPROCESSORS =
endif
	BIN = bin/Debug
	BIN_INT = bin-int/Debug
	BUILD_MODE_CFLAGS = -O0 -g
endif

PROG = xab
SRC = xab.c video_renderer.c video_reader.c egl_stuff.c utils.c egl_test.c
OBJ = $(addprefix ${BIN_INT}/, $(SRC:.c=.o))

CC = gcc

#todo: cleanup
PKG_CONFIG_LIBS = epoxy xcb xcb-atom xcb-aux xproto egl xcb-util libavcodec libavformat libavfilter libavutil libswresample libswscale

INCS = -I$(SRCDIR) -Ivendor $(shell $(PKG_CONFIG) --cflags $(PKG_CONFIG_LIBS))

LIBS = -lm $(shell $(PKG_CONFIG) --libs $(PKG_CONFIG_LIBS))

LDFLAGS = ${LIBS}
CFLAGS = -Wall -Wextra ${BUILD_MODE_CFLAGS} ${INCS} -DSHM

####################################
# commands

default: run
.PHONY: default

all: compile_commands.json compile
.PHONY: all

compile: ${BIN}/${PROG}
.PHONY: compile

run: compile
	./${BIN}/${PROG} ${ARGV}
.PHONY: run

clean:
	@# I do need to change it to the variables but im too lazy
	-rm -rf bin bin-int compile_commands.json .cache
.PHONY: clean

install: compile
	sudo cp ${BIN}/${PROG} /usr/local/bin
.PHONY: install


# TODO: find out if there is a way to not compile
compile_commands.json:
	# btw this compiles the program too so deal with it
	bear -- $(MAKE) ${BIN}/${PROG}

####################################
# building
$(BIN)/$(PROG): $(OBJ) | $(BIN)
	$(CC) $(PREPROCESSORS) -o $@ $(OBJ) $(LDFLAGS)

${BIN_INT}/%.o: $(SRCDIR)/%.c | ${BIN_INT}
	$(CC) $(PREPROCESSORS) -c $< -o $@ $(CFLAGS)

####################################
# directories
${BIN}:
	mkdir -p $@

${BIN_INT}:
	mkdir -p $@
