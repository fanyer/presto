# usage: python es_instruction_string.h.py >../util/es_instruction_string.h
import instructions

print "/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: \"stroustrup\" -*-"
print " *"
print " * Copyright (C) Opera Software ASA  2002 - 2011"
print " *"
print " * Table of instruction names."
print " *"
print " * @author modules/ecmascript/carakan/src/scripts/es_instruction_string.h.py"
print " */"
print
print "#ifndef ES_INSTRUCTION_STRING_H"
print "#define ES_INSTRUCTION_STRING_H"
print "# if defined(ES_DISASSEMBLER_SUPPORT) || defined(ES_BYTECODE_LOGGER) || defined(ES_SLOW_CASE_PROFILING)"
print
print "const char *const g_instruction_strings[] ="
print "{"

current_condition = None

for instruction in instructions.instructions:
    if current_condition != instruction.condition:
        if current_condition: print "#endif"
        current_condition = instruction.condition
        if current_condition: print "#%s" % current_condition

    if instruction.name == "ESI_LAST_INSTRUCTION":
        print "    \"LAST_INSTRUCTION\""
    else:
        print "    \"%s\"," % instruction.name[4:]

print "};"
print
print "# endif // ES_DISASSEMBLER_SUPPORT || ES_BYTECODE_LOGGER || ES_SLOW_CASE_PROFILING"
print "#endif // ES_INSTRUCTION_STRING_H"
