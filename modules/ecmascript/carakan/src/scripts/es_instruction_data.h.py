import instructions

print "/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: \"stroustrup\" -*-"
print " *"
print " * Copyright (C) Opera Software ASA  2002 - 2006"
print " *"
print " * Table of instruction data."
print " *"
print " * @author modules/ecmascript/carakan/src/scripts/es_instruction_data.h.py"
print " */"
print
print "#ifndef ES_INSTRUCTION_DATA_H"
print "#define ES_INSTRUCTION_DATA_H"
print
print "const unsigned g_instruction_operand_count[] ="
print "{"

current_condition = None

for instruction in instructions.instructions:
    if current_condition != instruction.condition:
        if current_condition: print "#endif"
        current_condition = instruction.condition
        if current_condition: print "#%s" % current_condition

    if instruction.name == "ESI_LAST_INSTRUCTION":
        print "    0"
    else:
        if instruction.name in ("ESI_COPYN", "ESI_ADDN", "ESI_CONSTRUCT_OBJECT"): noperands = "UINT_MAX"
        else: noperands = "%d" % len(instruction.operands)
        print "    %s,%s// %s" % (noperands, " " * (9 - len(noperands)), instruction.name)

print "};"
print
print "const unsigned short g_instruction_operand_register_io[] ="
print "{"

current_condition = None
instruction_index = 0
instruction_index_table = {}

for instruction in instructions.instructions:
    if current_condition != instruction.condition:
        if current_condition: print "#endif"
        current_condition = instruction.condition
        if current_condition: print "#%s" % current_condition

    if instruction.name == "ESI_LAST_INSTRUCTION":
        print "    0"
    else:
        data_in = 0
        data_out = 0
        if instruction.name not in ("ESI_COPYN", "ESI_ADDN", "ESI_CONSTRUCT_OBJECT", "ESI_EVAL", "ESI_CALL", "ESI_CONSTRUCT"):
            for operand_index, operand in enumerate(instruction.operands):
                if operand.operandtype.startswith("reg:"):
                    if "in" in operand.operandtype: data_in = data_in | (1 << operand_index)
                    if "out" in operand.operandtype: data_out = data_out | (1 << operand_index)
        print "    0x%02x%02x, // %s" % (data_in, data_out, instruction.name)

print "};"
print
print "const BOOL g_instruction_is_trivial[] ="
print "{"

current_condition = None
instruction_index = 0
instruction_index_table = {}

for instruction in instructions.instructions:
    if current_condition != instruction.condition:
        if current_condition: print "#endif"
        current_condition = instruction.condition
        if current_condition: print "#%s" % current_condition

    if instruction.name == "ESI_LAST_INSTRUCTION":
        print "    FALSE"
    else:
        print "    %s, // %s" % (instruction.trivial and "TRUE" or "FALSE", instruction.name)

print "};"
print
print "#endif // ES_INSTRUCTION_DATA_H"
