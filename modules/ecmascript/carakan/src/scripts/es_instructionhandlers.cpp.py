import instructions

print "/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: \"stroustrup\" -*-"
print " *"
print " * Copyright (C) Opera Software ASA  2002 - 2006"
print " *"
print " * Table of instruction names."
print " *"
print " * @author modules/ecmascript/carakan/src/scripts/es_instructionhandlers.cpp.py"
print " */"
print
print "#include \"core/pch.h\""
print
print "#include \"modules/ecmascript/carakan/src/es_pch.h\""
print
print "#ifdef ES_NATIVE_SUPPORT"
print
print "static void *"
print "ViolateFunctionPointer(IH_FUNCTION_PTR(ptr))"
print "{"
print "    union { IH_FUNCTION_PTR(fn); void *v; } u;"
print "    u.fn = ptr;"
print "    return u.v;"
print "}"
print
print "void ES_SetupFunctionHandlers()"
print "{"
print "    void **ihs = g_ecma_instruction_handlers = OP_NEWA_L(void *, %u);" % (len(instructions.instructions) - 1)
print

current_condition = None

for index, instruction in enumerate(instructions.instructions):
    if current_condition != instruction.condition:
        if current_condition: print "#endif"
        current_condition = instruction.condition
        if current_condition: print "#%s" % current_condition

    if instruction.name in ("ESI_RETURN_FROM_EVAL", "ESI_EXIT", "ESI_EXIT_TO_BUILTIN", "ESI_EXIT_TO_EVAL"):
        print "    ihs[%s] = NULL;" % instruction.name
    elif instruction.name != "ESI_LAST_INSTRUCTION":
        instr = instruction.name[4:]
        if instruction.alias is not None:
           instr = instruction.alias[4:]
        print "    ihs[%s] = ViolateFunctionPointer(&ES_Execution_Context::IH_%s);" % (instruction.name, instr)

print "}"
print
print "#endif // ES_NATIVE_SUPPORT"
