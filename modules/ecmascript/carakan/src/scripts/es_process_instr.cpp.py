import instructions

def Type(type):
    if type in ("null", "undefined", "boolean", "string", "int32", "double", "object"):
        return "ESTYPE_%s" % type.upper()
    elif type == "number":
        return "ESTYPE_INT32_OR_DOUBLE"

print "/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: \"stroustrup\" -*-"
print " *"
print " * Copyright (C) Opera Software ASA  2002 - 2006"
print " *"
print " * Optimizer: generated per-instruction code."
print " *"
print " * @author modules/ecmascript/carakan/src/scripts/es_process_instr.cpp.py"
print " */"
print
print "#include \"core/pch.h\""
print
print "#include \"modules/ecmascript/carakan/src/es_pch.h\""
print "#include \"modules/ecmascript/carakan/src/compiler/es_analyzer.h\""
print
print "#ifdef ES_NATIVE_SUPPORT"
print
print "void"
print "ES_Analyzer::ProcessInstruction(Input *&iptr, Output *&optr)"
print "{"
print "    switch (word++->instruction)"
print "    {"

current_condition = None
first = True

for instruction in instructions.instructions:
    # instruction with custom handling
    if instruction.name in ("ESI_LOAD", "ESI_LOAD_IMM", "ESI_COPY", "ESI_COPYN", "ESI_ADD", "ESI_ADDN", "ESI_CONSTRUCT_OBJECT", "ESI_EVAL", "ESI_CALL", "ESI_CONSTRUCT"):
        continue

    if current_condition != instruction.condition:
        if current_condition: print "#endif"
        current_condition = instruction.condition
        if not first: print
        if current_condition: print "#%s" % current_condition
    elif not first: print

    first = False
    
    if instruction.name == "ESI_LAST_INSTRUCTION": break

    print "    case %s:" % instruction.name
    
    for operand in instruction.operands:
        if operand.operandtype.startswith("reg:"):
            if operand.operandtype in ("reg:in", "reg:in:out"):
                if operand.operandtype == "reg:in":
                    print "        iptr->index = word++->index;"
                else:
                    print "        iptr->index = word->index;"
                if operand.valuetype in ("boolean", "string", "number", "int32", "object"):
                    print "        iptr->has_type = TRUE;"
                    print "        iptr->has_forced_type = TRUE;"
                    print "        iptr->type = %s;" % Type(operand.valuetype)
                else:
                    print "        iptr->has_type = FALSE;"
                print "        ++iptr;"

            if operand.operandtype in ("reg:out", "reg:in:out", "reg:out:optional"):
                if operand.valuetype in ("null", "undefined", "boolean", "string", "number", "int32", "double", "object"):
                    if operand.operandtype.endswith(":optional"): optional = ", FALSE, 0, TRUE"
                    else: optional = ""
                    print "        *optr++ = Output(word++->index, TRUE, %s%s);" % (Type(operand.valuetype), optional)
                else:
                    if operand.operandtype.endswith(":optional"): optional = ", TRUE"
                    else: optional = ""
                    print "        *optr++ = Output(word++->index%s);" % optional
        else:
            print "        ++word;"

    print "        break;"

print "    }"
print "}"
print
print "#endif // ES_NATIVE_SUPPORT"
