all: buildjsshell

INCLUDE=
CCFLAGS=

ifneq ($(NOUSERMK), YES)
-include user.mk
endif

Cc=$(CROSS)gcc$(COMPILER_SUFFIX)
CC=$(CROSS)g++$(COMPILER_SUFFIX)

REGRESSION = regression

ROOT = ../../../..

SRC=\
    modules/ecmascript/carakan/src/vm/es_engine.cpp \
    modules/unicode/unicode_bidi.cpp \
    modules/unicode/unicode_bidiutils.cpp \
    modules/unicode/unicode_case.cpp \
    modules/unicode/unicode_module.cpp \
    modules/unicode/unicode_normalize.cpp \
    modules/unicode/unicode_properties.cpp \
    modules/unicode/unicode_segmenter.cpp \
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
    modules/stdlib/src/stdlib_double_format.cpp \
    modules/stdlib/src/thirdparty_printf/uni_printf.cpp \
    modules/stdlib/src/thirdparty_printf/uni_scanf.cpp \
    modules/stdlib/src/thirdparty_rng/stdlib_mt19937ar.cpp \
    modules/stdlib/src/thirdparty_dtoa_floitsch/stdlib_dtoa_floitsch.cpp \
    modules/regexp/src/regexp_advanced_api.cpp \
    modules/regexp/src/regexp_api.cpp \
    modules/regexp/src/regexp_engine.cpp \
    modules/regexp/src/re_class.cpp \
    modules/regexp/src/re_compiler.cpp \
    modules/regexp/src/re_matcher.cpp \
    modules/regexp/src/re_object.cpp \
    modules/regexp/src/re_searcher.cpp \
    modules/regexp/src/re_native.cpp \
    modules/regexp/src/re_native_ia32.cpp \
    modules/regexp/src/re_native_arm.cpp \
    modules/regexp/src/re_native_mips.cpp \
    modules/ecmascript/carakan/src/util/es_native_disass.c \
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
    modules/ecmascript/carakan/src/builtins/es_array_builtins.cpp \
    modules/ecmascript/carakan/src/builtins/es_boolean_builtins.cpp \
    modules/ecmascript/carakan/src/builtins/es_date_builtins.cpp \
    modules/ecmascript/carakan/src/builtins/es_debug_builtins.cpp \
    modules/ecmascript/carakan/src/builtins/es_error_builtins.cpp \
    modules/ecmascript/carakan/src/builtins/es_function_builtins.cpp \
    modules/ecmascript/carakan/src/builtins/es_global_builtins.cpp \
    modules/ecmascript/carakan/src/builtins/es_json_export.cpp \
    modules/ecmascript/carakan/src/builtins/es_json_import.cpp \
    modules/ecmascript/carakan/src/builtins/es_math_builtins.cpp \
    modules/ecmascript/carakan/src/builtins/es_number_builtins.cpp \
    modules/ecmascript/carakan/src/builtins/es_object_builtins.cpp \
    modules/ecmascript/carakan/src/builtins/es_regexp_builtins.cpp \
    modules/ecmascript/carakan/src/builtins/es_string_builtins.cpp \
    modules/ecmascript/carakan/src/builtins/es_typedarray_builtins.cpp \
    modules/ecmascript/carakan/src/compiler/es_analyzer.cpp \
    modules/ecmascript/carakan/src/compiler/es_compiler.cpp \
    modules/ecmascript/carakan/src/compiler/es_compiler_expr.cpp \
    modules/ecmascript/carakan/src/compiler/es_compiler_stmt.cpp \
    modules/ecmascript/carakan/src/compiler/es_compiler_unroll.cpp \
    modules/ecmascript/carakan/src/compiler/es_code.cpp \
    modules/ecmascript/carakan/src/compiler/es_disassembler.cpp \
    modules/ecmascript/carakan/src/compiler/es_disassemble_instr.cpp \
    modules/ecmascript/carakan/src/compiler/es_lexer.cpp \
    modules/ecmascript/carakan/src/compiler/es_native.cpp \
    modules/ecmascript/carakan/src/compiler/es_native_codegen.cpp \
    modules/ecmascript/carakan/src/compiler/es_native_regalloc.cpp \
    modules/ecmascript/carakan/src/compiler/es_native_ia32.cpp \
    modules/ecmascript/carakan/src/compiler/es_native_arm.cpp \
    modules/ecmascript/carakan/src/compiler/es_native_mips.cpp \
    modules/ecmascript/carakan/src/compiler/es_native_stack_frame.cpp \
    modules/ecmascript/carakan/src/compiler/es_parser.cpp \
    modules/ecmascript/carakan/src/compiler/es_parser_expr.cpp \
    modules/ecmascript/carakan/src/compiler/es_parser_stmt.cpp \
    modules/ecmascript/carakan/src/compiler/es_process_instr.cpp \
    modules/ecmascript/carakan/src/compiler/es_source_location.cpp \
    modules/ecmascript/carakan/src/ecmascript_manager.cpp \
    modules/ecmascript/carakan/src/ecmascript_module.cpp \
    modules/ecmascript/carakan/src/ecmascript_object.cpp \
    modules/ecmascript/carakan/src/ecmascript_runtime.cpp \
    modules/ecmascript/carakan/src/es_program_cache.cpp \
    modules/ecmascript/carakan/src/kernel/es_string.cpp \
    modules/ecmascript/carakan/src/kernel/es_collector.cpp \
    modules/ecmascript/carakan/src/kernel/es_mark_sweep_heap.cpp \
    modules/ecmascript/carakan/src/kernel/es_page.cpp \
    modules/ecmascript/carakan/src/kernel/es_rts.cpp \
    modules/ecmascript/carakan/src/kernel/es_value.cpp \
    modules/ecmascript/carakan/src/kernel/es_box.cpp \
    modules/ecmascript/carakan/src/util/es_hash_table.cpp \
    modules/ecmascript/carakan/src/util/es_formatter.cpp \
    modules/ecmascript/carakan/src/object/es_class.cpp \
    modules/ecmascript/carakan/src/object/es_clone_object.cpp \
    modules/ecmascript/carakan/src/object/es_arguments.cpp \
    modules/ecmascript/carakan/src/object/es_array_object.cpp \
    modules/ecmascript/carakan/src/object/es_boolean_object.cpp \
    modules/ecmascript/carakan/src/object/es_date_object.cpp \
    modules/ecmascript/carakan/src/object/es_error_object.cpp \
    modules/ecmascript/carakan/src/object/es_number_object.cpp \
    modules/ecmascript/carakan/src/object/es_regexp_object.cpp \
    modules/ecmascript/carakan/src/object/es_string_object.cpp \
    modules/ecmascript/carakan/src/object/es_typedarray.cpp \
    modules/ecmascript/carakan/src/object/es_object.cpp \
    modules/ecmascript/carakan/src/object/es_host_object.cpp \
    modules/ecmascript/carakan/src/object/es_property.cpp \
    modules/ecmascript/carakan/src/object/es_property_table.cpp \
    modules/ecmascript/carakan/src/object/es_function.cpp \
    modules/ecmascript/carakan/src/object/es_global_object.cpp \
    modules/ecmascript/carakan/src/object/es_indexed.cpp \
    modules/ecmascript/carakan/src/object/es_special_property.cpp \
    modules/ecmascript/carakan/src/util/es_bcdebugger.cpp \
    modules/ecmascript/carakan/src/util/es_bclogger.cpp \
    modules/ecmascript/carakan/src/util/es_codegenerator_arm.cpp \
    modules/ecmascript/carakan/src/util/es_codegenerator_ia32.cpp \
    modules/ecmascript/carakan/src/util/es_codegenerator_mips.cpp \
    modules/ecmascript/carakan/src/util/es_strtod.cpp \
    modules/ecmascript/carakan/src/util/es_util.cpp \
    modules/ecmascript/carakan/src/util/es_tempbuf.cpp \
    modules/ecmascript/carakan/src/vm/es_allocation_context.cpp \
    modules/ecmascript/carakan/src/vm/es_context.cpp \
    modules/ecmascript/carakan/src/vm/es_execution_context.cpp \
    modules/ecmascript/carakan/src/vm/es_frame.cpp \
    modules/ecmascript/carakan/src/vm/es_instructionhandlers.cpp \
    modules/ecmascript/carakan/src/vm/es_register_blocks.cpp \
    modules/ecmascript/carakan/src/vm/es_suspended_call.cpp \
    modules/ecmascript/oppseudothread/oppseudothread.cpp \
    modules/ecmascript/oppseudothread/oppseudothread_threaded.cpp \
    modules/ecmascript/oppseudothread/oppseudothread_pthreads.cpp \
    modules/ecmascript/structured/es_persistent.cpp

SRC_CPP=$(filter %.cpp,$(SRC))
SRC_C=$(filter %.c,$(SRC))

SRC_STANDALONE=\
    modules/util/adt/bytebuffer.cpp \
    modules/util/adt/opvector.cpp \
    modules/util/excepts.cpp \
    modules/util/operalib.cpp \
    modules/util/tempbuf.cpp \
    modules/util/uniprntf.cpp \
    modules/util/OpHashTable.cpp \
    modules/util/opstring.cpp \
    modules/util/simset.cpp \
    modules/pi/system/OpMemory.cpp

SRC_JSSHELL=\
    programs/stopwatch.cpp \
    programs/jsshell_object.cpp \
    programs/jsshell.cpp

ifeq ($(JUMBO), YES)
OBJ_JUMBO_CPP=$(patsubst %.cpp,obj/$(OUTPUT)/%.o,$(shell python jumbo.py $(SRC_CPP)))
else
OBJ_CPP=$(patsubst %.cpp,obj/$(OUTPUT)/%.o,$(SRC_CPP))
endif

OBJ_C=$(patsubst %.c,obj/$(OUTPUT)/%.o,$(SRC_C))
OBJ_STANDALONE_CPP=$(patsubst %.cpp,obj/$(OUTPUT)/%.o,$(filter %.cpp,$(SRC_STANDALONE)))
OBJ_STANDALONE_C=$(patsubst %.c,obj/$(OUTPUT)/%.o,$(filter %.c,$(SRC_STANDALONE)))
OBJ_JSSHELL=$(patsubst %.cpp,obj/$(OUTPUT)/%.o,$(filter %.cpp,$(SRC_JSSHELL)))

OBJ=$(OBJ_CPP) $(OBJ_JUMBO_CPP) $(OBJ_C)
OBJ_STANDALONE=$(OBJ_STANDALONE_CPP) $(OBJ_STANDALONE_C)

INCLUDE += -Iobj/$(OUTPUT)/include -I. -I$(ROOT)

ifeq ($(shell uname -m), ppc)
ARCHITECTURE=POWERPC
DISABLE_NATIVE_SUPPORT=YES
endif

CCFLAGS += -DUNIX -DLINGOGI -DWCHAR_IS_UNICHAR -fshort-wchar -DECMASCRIPT_STANDALONE_COMPILER -MMD -fno-exceptions
CCFLAGS_CPP += -fno-rtti

ES_STANDALONE_VERSION:=$(shell git describe)
ES_STANDALONE_BUILDTIME:=$(shell date)

CCFLAGS += -DES_STANDALONE_VERSION='"$(ES_STANDALONE_VERSION)"' -DES_STANDALONE_BUILDTIME='"$(ES_STANDALONE_BUILDTIME)"'

ifneq ($(DISABLE_NATIVE_SUPPORT), YES)
CCFLAGS += -DES_NATIVE_SUPPORT -DECMASCRIPT_NATIVE_SUPPORT -DEXECUTABLE_MEMORY_MANAGER -DMEMORY_EXEC_SEGMENT_SIZE=32768
endif
ifneq ($(DISABLE_DISASSEMBLER), YES)
CCFLAGS += -DES_DISASSEMBLER_SUPPORT
endif
CCFLAGS += -DES_COMBINED_ADD_SUPPORT
CCFLAGS += -DES_CARAKAN_PARM_MAX_PARSER_STACK=900*1024
ifeq ($(STANDALONE_ES_DEBUGGER_SUPPORT), YES)
CCFLAGS += -DECMASCRIPT_DEBUGGER
endif
CCFLAGS += $(CCFLAGS_USER)
CCFLAGS += -Wall -Wno-parentheses -Wno-switch

CCFLAGS_DEBUG = -D_DEBUG -DDEBUG_ENABLE_OPASSERT -ggdb -DES_HARDCORE_GC_MODE -DES_BYTECODE_LOGGER -DES_DEBUG_COMPACT_OBJECTS
CCFLAGS_RELEASE = -O2 -ggdb -DNDEBUG

ifeq ($(STRICT_WARNINGS), YES)
CCFLAGS_OBJ += -Werror
endif

ARCHITECTURE ?= auto

ifeq ($(ARCHITECTURE), ARM)
CCFLAGS += -DARCHITECTURE_ARM -DARCHITECTURE_ARM_VFP -Wno-psabi
ARCHITECTURE_NAME = arm
endif

ifeq ($(ARCHITECTURE), THUMB)
CCFLAGS += -DARCHITECTURE_THUMB -mthumb
ARCHITECTURE_NAME = thumb
endif

ifeq ($(ARCHITECTURE), SUPERH)
CCFLAGS += -DARCHITECTURE_SUPERH
ARCHITECTURE_NAME = superh
endif

ifeq ($(ARCHITECTURE), MIPS)
CCFLAGS += -DARCHITECTURE_MIPS
ARCHITECTURE_NAME = mips
endif

ifeq ($(ARCHITECTURE), POWERPC)
CCFLAGS += -DARCHITECTURE_POWERPC -DIEEE_MC68k
ARCHITECTURE_NAME = powerpc
endif

ifneq (, $(findstring $(ARCHITECTURE), IA32 AMD64 auto))
ARCHITECTURE_NAME = ia32
CCFLAGS += -DARCHITECTURE_IA32 -DARCHITECTURE_SSE

ifeq ($(shell uname -m), x86_64)
AUTO_ARCHITECTURE = AMD64
else
AUTO_ARCHITECTURE = IA32
endif

ifeq ($(ARCHITECTURE), auto)
ARCHITECTURE = $(AUTO_ARCHITECTURE)
endif

ifeq ($(ARCHITECTURE), IA32)
ifneq ($(AUTO_ARCHITECTURE), IA32)
CCFLAGS_ARCH = -m32
endif
endif

ifeq ($(ARCHITECTURE), AMD64)
CCFLAGS_ARCH = -DSIXTY_FOUR_BIT -DARCHITECTURE_AMD64 -DARCHITECTURE_AMD64_UNIX
endif
endif

ifeq ($(shell uname -s), Darwin)
ifeq ($(ARCHITECTURE), IA32)
CCFLAGS += -DES_GCC_STACK_REALIGNMENT
endif
endif

ifeq ($(NATIVE_DISASS), YES)
CCFLAGS += -DNATIVE_DISASSEMBLER_SUPPORT
LIBRARIES += -lopcodes -lbfd -liberty
endif

ifeq ($(THREADED), YES)
CCFLAGS += -DOPPSEUDOTHREAD_THREADED
LIBRARIES += -lpthread
endif

ifeq ($(COPY_COLLECTOR), YES)
CCFLAGS += -DES_COPY_COLLECTOR
endif

MODE ?= DEBUG

ifeq ($(MODE), DEBUG)
CCFLAGS += $(CCFLAGS_DEBUG)
OUTPUT ?= debug
else
CCFLAGS += $(CCFLAGS_RELEASE)
OUTPUT ?= release
EXEPREFIX ?= fast-
endif

# JSSHELL_SRC = programs/jsshell.cpp
# TESTLEXER_SRC=programs/testlexer.cpp
# OPPSEUDOTHREAD_SRC = \
#     programs/test_oppseudothread.cpp \
#     programs/test_oppseudothread2.cpp \
#     $(ROOT)/modules/ecmascript/carakan/src/util/oppseudothread.cpp
# GENCV_SRC = \
#     $(ROOT)/modules/ecmascript/carakan/src/util/generate_codevectors_$(ARCHITECTURE_NAME).cpp \
#     $(ROOT)/modules/ecmascript/carakan/src/util/es_codegenerator_$(ARCHITECTURE_NAME).cpp

#OBJECTS = \
#    $(filter %.o,$(SRC:%.cpp=obj/$(OUTPUT)/%.o)) \
#    $(filter %.o,$(SRC:%.c=obj/$(OUTPUT)/%.o))
# JSSHELL_OBJS=$(JSSHELL_SRC:%.cpp=obj/$(OUTPUT)/%.o)
# TESTLEXER_OBJS=$(TESTLEXER_SRC:%.cpp=obj/$(OUTPUT)/%.o)
# OPPSEUDOTHREAD_OBJS=$(OPPSEUDOTHREAD_SRC:%.cpp=obj/$(OUTPUT)/%.o)
# GENCV_OBJS=$(GENCV_SRC:%.cpp=obj/$(OUTPUT)/%.o)

PYTHON ?= python

SVN ?= http://svn.oslo.osa

buildjsshell: $(EXEPREFIX)jsshell
	@echo Done building: $(ARCHITECTURE)

tools:
	@echo $(CC) $(CCFLAGS) $(INCLUDE)

fetch-moztests:
	@if [ ! -d $(REGRESSION)/mozilla-jstests ]; then \
		echo "Checking out mozilla-jstests..."; \
		mkdir -p $(REGRESSION); \
		(cd $(REGRESSION) && svn --quiet co $(SVN)/testsuites/trunk/core/standards/scripts/mozilla-jstests); \
	else \
		echo "Updating mozilla-jsstests..."; \
		(cd $(REGRESSION)/mozilla-jstests && rm -f harness/{pass,fail} && svn --quiet up); \
	fi

fetch-opjsunit:
	@if [ ! -d $(REGRESSION)/opjsunit ]; then \
		echo "Checking out opjsunit..."; \
		mkdir -p $(REGRESSION); \
		(cd $(REGRESSION) && svn --quiet co $(SVN)/testsuites/trunk/core/standards/scripts/opjsunit); \
	else \
		echo "Updating opjsunit..."; \
		(cd $(REGRESSION)/opjsunit && rm -f tests/{pass,fail} && svn --quiet up); \
	fi

fetch-sputnik:
	@if [ ! -d $(REGRESSION)/sputnik ]; then \
		echo "Checking out sputnik..."; \
		mkdir -p $(REGRESSION); \
		(cd $(REGRESSION) && svn --quiet co $(SVN)/testsuites/trunk/external/google/sputnik); \
	else \
		echo "Updating sputnik..."; \
		(cd $(REGRESSION)/sputnik && rm -f {pass,fail} && svn --quiet up); \
	fi

run-moztests:
	@(cd $(REGRESSION)/mozilla-jstests/harness && $(PYTHON) mozillatest.py --really-quiet --crash-list=../../moz-crashlist.txt --fail-list=../../moz-faillist.txt --shell=../../../$(EXEPREFIX)jsshell)

run-moztests-nc:
	@(cd $(REGRESSION)/mozilla-jstests/harness && $(PYTHON) mozillatest.py --really-quiet --crash-list=../../moz-crashlist-nc.txt --fail-list=../../moz-faillist-nc.txt --shell=../../../$(EXEPREFIX)jsshell --engine=carakan-nc)

moztests: buildjsshell fetch-moztests run-moztests
moztests-nc: buildjsshell fetch-moztests run-moztests-nc

run-opjsunit:
	@(cd $(REGRESSION)/opjsunit/tests && $(PYTHON) ../harness/opjsunit.py --shell=../../../$(EXEPREFIX)jsshell --timeout --crash-list=../../op-crashlist.txt --fail-list=../../op-faillist.txt $(ARGS))

run-opjsunit-nc:
	@(cd $(REGRESSION)/opjsunit/tests && $(PYTHON) ../harness/opjsunit.py --shell=../../../$(EXEPREFIX)jsshell --engine=carakan-nc --timeout --crash-list=../../op-crashlist-nc.txt --fail-list=../../op-faillist-nc.txt $(ARGS))

run-opjsunit-es-debug:
	@(cd $(REGRESSION)/opjsunit/harness && python opjsunit.py --shell=../../../$(EXEPREFIX)jsshell --timeout --crash-list=../../op-crashlist-es-debug.txt --fail-list=../../op-faillist-es-debug.txt --engine-arg=-es-debug)

opjsunit: buildjsshell fetch-opjsunit run-opjsunit
opjsunit-nc: buildjsshell fetch-opjsunit run-opjsunit-nc
opjsunit-es-debug: buildjsshell fetch-opjsunit run-opjsunit-es-debug

run-sputnik:
	@(cd $(REGRESSION)/sputnik && echo "Running sputnik..." && $(PYTHON) sputnik_regress.py -s ../../$(EXEPREFIX)jsshell)

run-sputnik-nc:
	@(cd $(REGRESSION)/sputnik && echo "Running sputnik..." && $(PYTHON) sputnik_regress.py -s ../../$(EXEPREFIX)jsshell -e carakan-nc)

sputnik: buildjsshell fetch-sputnik run-sputnik
sputnik-nc: buildjsshell fetch-sputnik run-sputnik-nc

test: buildjsshell fetch-moztests fetch-opjsunit fetch-sputnik run-moztests run-moztests-nc run-opjsunit run-opjsunit-nc run-sputnik run-sputnik-nc

$(EXEPREFIX)jsshell: $(OBJ) $(OBJ_STANDALONE) $(OBJ_JSSHELL)
	@mkdir -p $(dir $@)
	@echo Linking $@
	@$(CC) $(CCFLAGS) $(CCFLAGS_ARCH) $(LDFLAGS_USER) -o $@ $(OBJ) $(OBJ_STANDALONE) $(OBJ_JSSHELL) $(LIBRARIES) $(LIBRARIES_USER)

# testlexer: $(TESTLEXER_OBJS) $(OBJECTS)
# 	@mkdir -p $(dir $@)
# 	@echo Linking $@
# 	@$(CC) -o $@ $(TESTLEXER_OBJS) $(OBJECTS) $(CCFLAGS)

# oppseudothread: $(OPPSEUDOTHREAD_OBJS)
# 	@echo Building $@
# 	$(CC) -o $@ $(CCFLAGS) $(INCLUDE) $^

# gencv: $(GENCV_OBJS)
# 	@echo Building gencv_$(ARCHITECTURE_NAME)
# 	$(CC) -o gencv_$(ARCHITECTURE_NAME) $(CCFLAGS) $(INCLUDE) $^

PCH_PATH = core/pch.h
PCH = obj/$(OUTPUT)/include/$(PCH_PATH).gch

$(PCH): $(PCH_PATH)
	@mkdir -p $(dir $@)
	@echo Precompiling $(patsubst obj/$(OUTPUT)/%,%,$@)
	@$(CC) $(CCFLAGS) $(CCFLAGS_CPP) $(INCLUDE) -c $< -o $@

$(OBJ_CPP): obj/$(OUTPUT)/%.o: $(ROOT)/%.cpp $(PCH)
	@mkdir -p $(dir $@)
	@echo Compiling $(patsubst obj/$(OUTPUT)/%,%,$@)
	@$(CC) $(CCFLAGS) $(CCFLAGS_CPP) $(CCFLAGS_OBJ) $(CCFLAGS_ARCH) $(INCLUDE) -c $< -o $@

$(OBJ_JUMBO_CPP): obj/$(OUTPUT)/%.o: %.cpp $(PCH)
	@mkdir -p $(dir $@)
	@echo Compiling $(patsubst obj/$(OUTPUT)/%,%,$@)
	@$(CC) $(CCFLAGS) $(CCFLAGS_CPP) $(CCFLAGS_ARCH) $(INCLUDE) -c $< -o $@

$(OBJ_C): obj/$(OUTPUT)/%.o: $(ROOT)/%.c
	@mkdir -p $(dir $@)
	@echo Compiling $(patsubst obj/$(OUTPUT)/%,%,$@)
	@$(Cc) $(CCFLAGS) $(CCFLAGS_OBJ) $(CCFLAGS_ARCH) $(INCLUDE) -c $< -o $@

$(OBJ_STANDALONE_CPP) $(OBJ_JSSHELL): obj/$(OUTPUT)/%.o: %.cpp $(PCH)
	@mkdir -p $(dir $@)
	@echo Compiling $(patsubst obj/$(OUTPUT)/%,%,$@)
	@$(CC) $(CCFLAGS) $(CCFLAGS_CPP) $(CCFLAGS_OBJ) $(CCFLAGS_ARCH) $(INCLUDE) -c $< -o $@

$(OBJ_STANDALONE_C): obj/$(OUTPUT)/%.o: %.c
	@mkdir -p $(dir $@)
	@echo Compiling $(patsubst obj/$(OUTPUT)/%,%,$@)
	@$(Cc) $(CCFLAGS) $(CCFLAGS_OBJ) $(CCFLAGS_ARCH) $(INCLUDE) -c $< -o $@

$(patsubst %.o,%.d,$(OBJ_CPP)): obj/$(OUTPUT)/%.d: %.cpp
	@rm -f $@

$(patsubst %.o,%.d,$(OBJ_C)): obj/$(OUTPUT)/%.d: %.c
	@rm -f $@

$(patsubst %.o,%.d,$(OBJ_STANDALONE_CPP)): obj/$(OUTPUT)/%.d: %.cpp
	@rm -f $@

$(patsubst %.o,%.d,$(OBJ_STANDALONE_C)): obj/$(OUTPUT)/%.d: %.c
	@rm -f $@

$(patsubst %.o,%.d,$(OBJ_JSSHELL)): obj/$(OUTPUT)/%.d: %.cpp
	@rm -f $@

INSTRUCTIONS = $(ROOT)/modules/ecmascript/carakan/src/scripts/instructions.py $(ROOT)/modules/ecmascript/carakan/src/compiler/es_instructions.txt
GENERATED_HEADERS = $(ROOT)/modules/ecmascript/carakan/src/vm/es_instruction.h \
                    $(ROOT)/modules/ecmascript/carakan/src/compiler/es_instruction_data.h \
                    $(ROOT)/modules/ecmascript/carakan/src/util/es_instruction_string.h

$(ROOT)/modules/ecmascript/carakan/src/vm/es_instruction.h: $(ROOT)/modules/ecmascript/carakan/src/scripts/es_instruction.h.py $(INSTRUCTIONS)
	$(PYTHON) $(ROOT)/modules/ecmascript/carakan/src/scripts/es_instruction.h.py > $(ROOT)/modules/ecmascript/carakan/src/vm/es_instruction.h

$(ROOT)/modules/ecmascript/carakan/src/vm/es_instructionhandlers.cpp: $(ROOT)/modules/ecmascript/carakan/src/scripts/es_instructionhandlers.cpp.py $(INSTRUCTIONS)
	$(PYTHON) $(ROOT)/modules/ecmascript/carakan/src/scripts/es_instructionhandlers.cpp.py > $(ROOT)/modules/ecmascript/carakan/src/vm/es_instructionhandlers.cpp

$(ROOT)/modules/ecmascript/carakan/src/util/es_instruction_string.h: $(ROOT)/modules/ecmascript/carakan/src/scripts/es_instruction_string.h.py $(INSTRUCTIONS)
	$(PYTHON) $(ROOT)/modules/ecmascript/carakan/src/scripts/es_instruction_string.h.py > $(ROOT)/modules/ecmascript/carakan/src/util/es_instruction_string.h

$(ROOT)/modules/ecmascript/carakan/src/compiler/es_instruction_data.h: $(ROOT)/modules/ecmascript/carakan/src/scripts/es_instruction_data.h.py $(INSTRUCTIONS)
	$(PYTHON) $(ROOT)/modules/ecmascript/carakan/src/scripts/es_instruction_data.h.py > $(ROOT)/modules/ecmascript/carakan/src/compiler/es_instruction_data.h

$(ROOT)/modules/ecmascript/carakan/src/compiler/es_process_instr.cpp: $(ROOT)/modules/ecmascript/carakan/src/scripts/es_process_instr.cpp.py $(INSTRUCTIONS)
	$(PYTHON) $(ROOT)/modules/ecmascript/carakan/src/scripts/es_process_instr.cpp.py > $(ROOT)/modules/ecmascript/carakan/src/compiler/es_process_instr.cpp

$(ROOT)/modules/ecmascript/carakan/src/compiler/es_disassemble_instr.cpp: $(ROOT)/modules/ecmascript/carakan/src/scripts/es_disassemble_instr.cpp.py $(INSTRUCTIONS)
	$(PYTHON) $(ROOT)/modules/ecmascript/carakan/src/scripts/es_disassemble_instr.cpp.py > $(ROOT)/modules/ecmascript/carakan/src/compiler/es_disassemble_instr.cpp

clean:
	rm -rf obj

tags:
	rm -f TAGS
	find . -name '*.h'	-print0 | xargs -0 etags -a --language=c++
	find . -name '*.cpp'	-print0 | xargs -0 etags -a --language=c++
	find . -name '*.c'	-print0 | xargs -0 etags -a --language=c

# Not all object files depend on all generated header files, really, but in
# practice it doesn't matter much, and it's nice if the generation happens
# before the actually dependent object files are compiled.
$(OBJECTS): Makefile $(GENERATED_HEADERS)

$(JSSHELL_OBJS): Makefile $(GENERATED_HEADERS)
$(TESTLEXER_OBJS): Makefile $(GENERATED_HEADERS)
$(OPPSEUDOTHREAD_OBJS): Makefile $(GENERATED_HEADERS)

-include $(OBJ:%.o=%.d)
-include $(OBJ_STANDALONE:%.o=%.d)
-include $(OBJ_JSSHELL:%.o=%.d)
-include $(PCH:%.gch=%.d)

fetch-libcarakan:
	if [ ! -d programs/libcarakan ]; \
	then \
		git clone git.oslo.osa:~jl/libcarakan.git programs/libcarakan; \
	fi

-include programs/libcarakan/src/libcarakan.mk
