CC = g++
INCLUDES = -I. -I../../../../../
WARNINGS = -Wall -Wno-parentheses -Wno-switch
DEFINES = -DUNIX -DHTML5_STANDALONE -DDEBUG_ENABLE_PRINTF -DDEBUG_ENABLE_SYSTEM_OUTPUT -DHTML5_PARSER
COVERAGE = -fprofile-arcs -ftest-coverage
CFLAGS = $(WARNINGS) $(INCLUDES) $(DEFINES)
CCFLAGS_DEBUG = -D_DEBUG -DDEBUG_ENABLE_OPASSERT -ggdb

ifeq ($(shell uname -m), ppc)
ARCHITECTURE=POWERPC
endif

ifeq ($(ARCHITECTURE), POWERPC)
CFLAGS += -DARCHITECTURE_POWERPC -DIEEE_MC68k
ARCHITECTURE_NAME = powerpc
endif

CCFLAGS = $(CCFLAGS_DEBUG)
COMPILE = $(CC) $(CFLAGS) -c

HTML5_SRC = $(wildcard ../*.cpp) ../../elementref.cpp
RAGNAROK_STANDALONE_SRC = $(wildcard modules/util/*.cpp) \
     $(wildcard modules/util/adt/*.cpp)
RAGNAROK_STANDALONE2_SRC = modules/logdoc/src/logdoc.cpp
STANDALONE_SRC = \
    debug/src/debug_printf.cpp \
    unicode/unicode_bidi.cpp \
    unicode/unicode_bidiutils.cpp \
    unicode/unicode_case.cpp \
    unicode/unicode_linebreak.cpp \
    unicode/unicode_module.cpp \
    unicode/unicode_normalize.cpp \
    unicode/unicode_properties.cpp \
    unicode/unicode_segmenter.cpp \
    stdlib/util/opbitvector.cpp \
    stdlib/src/stdlib_array.cpp \
    stdlib/src/stdlib_float.cpp \
    stdlib/src/stdlib_integer.cpp \
    stdlib/src/stdlib_memory.cpp \
    stdlib/src/stdlib_module.cpp \
    stdlib/src/stdlib_printf.cpp \
    stdlib/src/stdlib_string.cpp \
    stdlib/src/stdlib_strtod.cpp \
    stdlib/src/stdlib_unicode.cpp \
    stdlib/src/thirdparty_dtoa_floitsch/stdlib_dtoa.cpp \
    stdlib/src/stdlib_double_format.cpp \
    stdlib/src/thirdparty_printf/uni_printf.cpp \
    stdlib/src/thirdparty_printf/uni_scanf.cpp \
    stdlib/src/thirdparty_rng/stdlib_mt19937ar.cpp \
    memory/src/memory_fundamentals.cpp \
    memory/src/memory_segment.cpp \
    memory/src/memory_pooling.cpp \
    memory/src/memory_debug.cpp \
    memory/src/memory_memguard.cpp \
    memory/src/memory_virtualalloc.cpp \
    memory/src/memory_smo.cpp \
    memory/src/memory_module.cpp \
    memory/src/memory_opsrcsite.cpp \
    memory/src/memory_opallocinfo.cpp \
    memory/src/memory_events.cpp \
    memory/src/memory_reporting.cpp \
    memory/src/memory_upperten.cpp \
    memory/src/memory_state.cpp \
    memory/src/memory_group.cpp \
    memory/src/memory_executable.cpp

OBJ_HTML5 = $(patsubst ../%.cpp, obj/%.o, $(HTML5_SRC))
OBJ_RAGNAROK = $(patsubst modules/util/%.cpp, obj/%.o, $(RAGNAROK_STANDALONE_SRC))
OBJ_RAGNAROK2 = obj/logdoc.o
OBJ_STANDALONE = $(patsubst %.cpp,obj/%.o,$(STANDALONE_SRC))
OBJFILES = $(OBJ_HTML5) $(OBJ_RAGNAROK) $(OBJ_RAGNAROK2) $(OBJ_STANDALONE)

all: ragnarok

test:

coverage: CFLAGS += $(COVERAGE)
coverage: ragnarok

ragnarok: $(OBJFILES)
	$(CC) $(CFLAGS) -o ragnarok $(OBJFILES) && echo "Finished building ragnarok"

$(OBJ_HTML5): obj/%.o: ../%.cpp
	@mkdir -p $(dir $@)
	$(COMPILE) $(CCFLAGS) -o $@ $<

$(OBJ_RAGNAROK): obj/%.o: modules/util/%.cpp
	@mkdir -p $(dir $@)
	$(COMPILE) $(CCFLAGS) -o $@ $<

$(OBJ_RAGNAROK2): obj/%.o: modules/logdoc/src/%.cpp
	@mkdir -p $(dir $@)
	$(COMPILE) $(CCFLAGS) -o $@ $<

$(OBJ_STANDALONE): obj/%.o: ../../../../%.cpp
	@mkdir -p $(dir $@)
	$(COMPILE) $(CCFLAGS) -o $@ $<

rebuild: clean ragnarok


clean:
	rm -f $(OBJFILES)
