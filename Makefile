MAKE ?= make # if you are using mingw-make or smh
PKG_CONFIG ?= pkg-config

# ARGV ?= ./res/vids/vid.mp4
# ARGV ?= ./res/vids/vid.mp4 --monitor=0
ARGV ?= ~/Videos/Hidamari/cat-in-rain.mp4 --monitor=0 ~/Videos/Hidamari/pixel-mario.mp4 --monitor=1 --pixelated=1 # TODO: clean this up
RELEASE ?= 0
VERBOSE ?= 0

PREPROCESSORS =

ifeq (${xrandr},1)
	PREPROCESSORS += -DHAVE_LIBXRANDR
endif
ifeq (${cglm},1)
	PREPROCESSORS += -DHAVE_LIBCGLM
endif

# too lazy to check how to do else ifs in make
ifeq (${noglcalldebug},1)
	noglcalldebug := 1
endif
ifeq (${noglcalldebug},0)
	noglcalldebug := 0
endif

ifeq ($(RELEASE),1)
	# Release
	PREPROCESSORS += -DNDEBUG
	BIN = bin/Release
	BIN_INT = bin-int/Release
	BUILD_MODE_CFLAGS += -O3
	noglcalldebug := 1
else
	# Debug
	BIN = bin/Debug
	BIN_INT = bin-int/Debug
	BUILD_MODE_CFLAGS += -O0 -g
	noglcalldebug := 0
endif

ifeq ($(VERBOSE), 0)
	PREPROCESSORS += -DNVERBOSE
endif

ifeq ($(noglcalldebug), 1)
	PREPROCESSORS += -DNGLCALLDEBUG
endif

CC := gcc

PROG := xab

SRCDIR := src
SRC :=  $(wildcard ${SRCDIR}/*.c)
OBJ := $(patsubst ${SRCDIR}/%.c, ${BIN_INT}/%.o, ${SRC})

PKG_CONFIG_LIBS = epoxy xcb xcb-atom xcb-aux xproto xcb-util egl libavcodec libavformat libavfilter libavutil libswresample libswscale

ifeq (${xrandr},1)
	PKG_CONFIG_LIBS += xcb-randr
endif
ifeq (${cglm},1)
	PKG_CONFIG_LIBS +=cglm
endif

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

all: compile compile_commands.json
.PHONY: all

compile: ${BIN}/${PROG}
	@echo "Task '$@' - DONE"
.PHONY: compile

run: compile
	./${BIN}/${PROG} ${ARGV}
.PHONY: run

clean:
	@# I do need to change it to the variables but im too lazy
	-$(RM) -rf bin bin-int compile_commands.json
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
	@echo "GEN compile_commands.json"
	$(Q)compiledb make compile xrandr=1 cglm=1
	@echo "Task '$@' - DONE"


####################################
# building

# xab executable
$(BIN)/$(PROG): $(OBJ) | $(BIN)
	@echo "LD $(BIN)/$(PROG)"
	$(Q)$(CC) $(PREPROCESSORS) -o $@ $(OBJ) $(LDFLAGS)

# objs
${BIN_INT}/%.o: $(SRCDIR)/%.c | ${BIN_INT}
	@echo "CC $<"
	$(Q)$(CC) $(PREPROCESSORS) -c $< -o $@ $(CFLAGS)

####################################
# directories

${BIN}:
	@echo "MKDIR $@"
	$(Q)mkdir -p $@

${BIN_INT}:
	@echo "MKDIR $@"
	$(Q)mkdir -p $@
