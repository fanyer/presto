#
# Makefile for building 'webgl' standalone.
# Setup shamelessly stolen from Carakan :)
#
all: wgl

INCLUDE=
CCFLAGS=

ifneq ($(NOUSERMK), YES)
-include user.mk
endif

Cc=$(CROSS)gcc$(COMPILER_SUFFIX)
CC=$(CROSS)g++$(COMPILER_SUFFIX)

ROOT = ../../..

SRC=\
    modules/unicode/unicode_bidi.cpp \
    modules/unicode/unicode_bidiutils.cpp \
    modules/unicode/unicode_case.cpp \
    modules/unicode/unicode_linebreak.cpp \
    modules/unicode/unicode_module.cpp \
    modules/unicode/unicode_normalize.cpp \
    modules/unicode/unicode_properties.cpp \
    modules/unicode/unicode_segmenter.cpp \
    modules/webgl/src/wgl_ast.cpp \
    modules/webgl/src/wgl_builder.cpp \
    modules/webgl/src/wgl_cgbuilder.cpp \
    modules/webgl/src/wgl_context.cpp \
    modules/webgl/src/wgl_error.cpp \
    modules/webgl/src/wgl_lexer.cpp \
    modules/webgl/src/wgl_parser.cpp \
    modules/webgl/src/wgl_pretty_printer.cpp \
    modules/webgl/src/wgl_printer.cpp \
    modules/webgl/src/wgl_printer_console.cpp \
    modules/webgl/src/wgl_printer_stdio.cpp \
    modules/webgl/src/wgl_string.cpp \
    modules/webgl/src/wgl_symbol.cpp \
    modules/webgl/src/wgl_validate.cpp \
    modules/webgl/src/wgl_validator.cpp \
    modules/webgl/src/wgl_visit.cpp \
    modules/webgl/src/wgl_intrinsics.cpp \
    modules/memory/src/memory_fundamentals.cpp \
    modules/memory/src/memory_segment.cpp \
    modules/memory/src/memory_pooling.cpp \
    modules/memory/src/memory_debug.cpp \
    modules/memory/src/memory_memguard.cpp \
    modules/memory/src/memory_virtualalloc.cpp \
    modules/memory/src/memory_smo.cpp \
    modules/memory/src/memory_module.cpp \
    modules/memory/src/memory_opsrcsite.cpp \
    modules/memory/src/memory_opallocinfo.cpp \
    modules/memory/src/memory_events.cpp \
    modules/memory/src/memory_reporting.cpp \
    modules/memory/src/memory_upperten.cpp \
    modules/memory/src/memory_state.cpp \
    modules/memory/src/memory_group.cpp \
    modules/memory/src/memory_executable.cpp \
    modules/stdlib/util/opbitvector.cpp \
    modules/stdlib/util/opdate.cpp \
    modules/stdlib/src/stdlib_array.cpp \
    modules/stdlib/src/stdlib_date.cpp \
    modules/stdlib/src/stdlib_float.cpp \
    modules/stdlib/src/stdlib_integer.cpp \
    modules/stdlib/src/stdlib_memory.cpp \
    modules/stdlib/src/stdlib_module.cpp \
    modules/stdlib/src/stdlib_printf.cpp \
    modules/stdlib/src/stdlib_string.cpp \
    modules/stdlib/src/stdlib_strtod.cpp \
    modules/stdlib/src/stdlib_unicode.cpp \
    modules/stdlib/src/thirdparty_dtoa_floitsch/stdlib_dtoa_floitsch.cpp \
    modules/stdlib/src/stdlib_double_format.cpp \
    modules/stdlib/src/thirdparty_printf/uni_printf.cpp \
    modules/stdlib/src/thirdparty_printf/uni_scanf.cpp \
    modules/stdlib/src/thirdparty_rng/stdlib_mt19937ar.cpp

SRC_STANDALONE=\
    ./modules/util/adt/bytebuffer.cpp \
    ./modules/util/adt/opvector.cpp \
    ./modules/util/excepts.cpp \
    ./modules/util/md5.c \
    ./modules/util/operalib.cpp \
    ./modules/util/tempbuf.cpp \
    ./modules/util/uniprntf.cpp \
    ./modules/util/OpHashTable.cpp \
    ./modules/util/opstring.cpp \
    ./modules/util/simset.cpp \
    ./modules/pi/system/OpMemory.cpp

OBJ_CPP=$(patsubst %.cpp,obj/$(OUTPUT)/%.o,$(filter %.cpp,$(SRC)))
OBJ_C=$(patsubst %.c,obj/$(OUTPUT)/%.o,$(filter %.c,$(SRC)))
OBJ_STANDALONE_CPP=$(patsubst %.cpp,obj/$(OUTPUT)/%.o,$(filter %.cpp,$(SRC_STANDALONE)))
OBJ_STANDALONE_C=$(patsubst %.c,obj/$(OUTPUT)/%.o,$(filter %.c,$(SRC_STANDALONE)))

OBJ=$(OBJ_CPP) $(OBJ_C)
OBJ_STANDALONE=$(OBJ_STANDALONE_CPP) $(OBJ_STANDALONE_C)

INCLUDE += -I. -I$(ROOT)

ifeq (NO$(filter CYG%, $(shell uname -s)), NO)
CCFLAGS += -DUNIX -DLINGOGI
else
CCFLAGS += -DMSWIN
endif
CCFLAGS += -DWGL_STANDALONE -DCANVAS3D_SUPPORT
CCFLAGS += -DWCHAR_IS_UNICHAR -fshort-wchar -MMD -fno-exceptions
CCFLAGS += $(INCLUDE)
CCFLAGS += $(CCFLAGS_USER)
CCFLAGS += -Wall -Wno-parentheses -Wno-switch

CCFLAGS_DEBUG = -D_DEBUG -DDEBUG_ENABLE_OPASSERT -ggdb
CCFLAGS_RELEASE = -O2 -ggdb

MODE ?= DEBUG

ifeq ($(MODE), DEBUG)
CCFLAGS += $(CCFLAGS_DEBUG)
OUTPUT ?= debug
else
CCFLAGS += $(CCFLAGS_RELEASE)
OUTPUT ?= release
EXEPREFIX ?= fast-
endif

wgl : wgl.cpp $(OBJ) $(OBJ_STANDALONE)
	$(CC) $(CCFLAGS) -o $@ $^

debug:
	@echo $(OBJ)

$(OBJ_CPP): obj/$(OUTPUT)/%.o: $(ROOT)/%.cpp
	@mkdir -p $(dir $@)
	@echo Compiling $(patsubst obj/$(OUTPUT)/%,%,$@)
	@$(CC) $(CCFLAGS) $(INCLUDE) -fpermissive -c $< -o $@

$(OBJ_C): obj/$(OUTPUT)/%.o: $(ROOT)/%.c
	@mkdir -p $(dir $@)
	@echo Compiling $(patsubst obj/$(OUTPUT)/%,%,$@)
	@$(Cc) $(CCFLAGS) $(INCLUDE) -c $< -o $@

$(OBJ_STANDALONE_CPP): obj/$(OUTPUT)/%.o: %.cpp
	@mkdir -p $(dir $@)
	@echo Compiling $(patsubst obj/$(OUTPUT)/%,%,$@)
	@$(CC) $(CCFLAGS) $(INCLUDE) -fpermissive -c $< -o $@

$(OBJ_STANDALONE_C): obj/$(OUTPUT)/%.o: %.c
	@mkdir -p $(dir $@)
	@echo Compiling $(patsubst obj/$(OUTPUT)/%,%,$@)
	@$(Cc) $(CCFLAGS) $(INCLUDE) -c $< -o $@

$(patsubst %.o,%.d,$(OBJ_CPP)): obj/$(OUTPUT)/%.d: %.cpp
	@rm -f $@

$(patsubst %.o,%.d,$(OBJ_C)): obj/$(OUTPUT)/%.d: %.c
	@rm -f $@

$(patsubst %.o,%.d,$(OBJ_STANDALONE_CPP)): obj/$(OUTPUT)/%.d: %.cpp
	@rm -f $@

$(patsubst %.o,%.d,$(OBJ_STANDALONE_C)): obj/$(OUTPUT)/%.d: %.c
	@rm -f $@

clean:
	rm -rf obj

tags:
	rm -f TAGS
	find .. -name '*.h'	-print0 | xargs -0 etags -a --language=c++
	find .. -name '*.cpp'	-print0 | xargs -0 etags -a --language=c++
	find .. -name '*.c'	-print0 | xargs -0 etags -a --language=c

# Not all object files depend on all generated header files, really, but in
# practice it doesn't matter much, and it's nice if the generation happens
# before the actually dependent object files are compiled.
$(OBJECTS): Makefile $(GENERATED_HEADERS)

$(JSSHELL_OBJS): Makefile $(GENERATED_HEADERS)
$(TESTLEXER_OBJS): Makefile $(GENERATED_HEADERS)
$(OPPSEUDOTHREAD_OBJS): Makefile $(GENERATED_HEADERS)

-include $(OBJ:%.o=%.d)
-include $(OBJ_STANDALONE:%.o=%.d)
