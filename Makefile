MAKE ?= make # if you are using mingw-make or smh
PKG_CONFIG = pkg-config

ARGV ?= ./res/vids/vid.mp4 1 # TODO: clean this up
SRCDIR ?= src
RELEASE ?= 0
VERBOSE ?= 0

PREPROCESSORS =

ifeq ($(RELEASE),1)
	# Release

	PREPROCESSORS += -DNDEBUG
	BIN = bin/Release
	BIN_INT = bin-int/Release
	BUILD_MODE_CFLAGS += -O3
	# don't need to set verobse to 0, because its already set to zero, and if it is 1,
	# than the user explicitly wants xab to be verbose
else
	# Debug
	BIN = bin/Debug
	BIN_INT = bin-int/Debug
	BUILD_MODE_CFLAGS += -O0 -g
endif

ifeq ($(VERBOSE), 0)
	PREPROCESSORS += -DNVERBOSE
endif

PROG = xab
SRC = xab.c context.c atom.c video_renderer.c video_reader.c egl_stuff.c utils.c framebuffer.c
OBJ = $(addprefix ${BIN_INT}/, $(SRC:.c=.o))

CC = gcc

PKG_CONFIG_LIBS = epoxy xcb xcb-atom xcb-aux xproto xcb-randr xcb-util egl libavcodec libavformat libavfilter libavutil libswresample libswscale

INCS = -I$(SRCDIR) -Ivendor $(shell $(PKG_CONFIG) --cflags $(PKG_CONFIG_LIBS))

LIBS = -lm $(shell $(PKG_CONFIG) --libs $(PKG_CONFIG_LIBS))

LDFLAGS = ${LIBS}
CFLAGS = -Wall -Wextra ${BUILD_MODE_CFLAGS} ${INCS} -DSHM

# Pretty print
Q := @
ifeq ($(V), 1)
	Q :=
endif

####################################
# commands

default: compile
.PHONY: default

all: compile_commands.json compile
.PHONY: all

compile: ${BIN}/${PROG}
	@echo "Task '$@' - DONE"
.PHONY: compile

run: compile
	./${BIN}/${PROG} ${ARGV}
.PHONY: run

clean:
	@# I do need to change it to the variables but im too lazy
	-$(RM) -rf bin bin-int compile_commands.json .cache
	@echo "Task '$@' - DONE"
.PHONY: clean

install: compile
	@echo "COPY ${BIN}/${PROG} /usr/local/bin/"
	$(Q)sudo cp ${BIN}/${PROG} /usr/local/bin/
	@echo "Task '$@' - DONE"
.PHONY: install

uninstall:
	@echo "REMOVE /usr/local/bin/${PROG}"
	$(Q)-$(RM) -f /usr/local/bin/${PROG}
	@echo "Task '$@' - DONE"
.PHONY: uninstall

compile_commands.json:
	@echo "BTW this compiles the project so have fun lol"

	@echo "GEN compile_commands.json"
	@bear -- make -n ${BIN}/${PROG} > /dev/null
	@echo "Task '$@' - DONE"


####################################
# building

# xab executable
$(BIN)/$(PROG): $(OBJ) | $(BIN)
	@echo "LD $<"
	$(Q)$(CC) $(PREPROCESSORS) -o $@ $(OBJ) $(LDFLAGS)

# objs
${BIN_INT}/%.o: $(SRCDIR)/%.c | ${BIN_INT}
	@echo "CC $<"
	$(Q)$(CC) $(PREPROCESSORS) -c $< -o $@ $(CFLAGS)

####################################
# directories

${BIN}:
	@echo "DIR $@"
	$(Q)mkdir -p $@

${BIN_INT}:
	@echo "DIR $@"
	$(Q)mkdir -p $@
