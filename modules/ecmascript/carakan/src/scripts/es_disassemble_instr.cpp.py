import instructions

print "/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: \"stroustrup\" -*-"
print " *"
print " * Copyright (C) Opera Software ASA  2002 - 2006"
print " *"
print " * Disassembler: generated per-instruction code."
print " *"
print " * @author modules/ecmascript/carakan/src/scripts/es_disassembler_instr.cpp.py"
print " */"
print
print "#include \"core/pch.h\""
print
print "#ifdef ES_DISASSEMBLER_SUPPORT"
print
print "#include \"modules/ecmascript/carakan/src/es_pch.h\""
print "#include \"modules/ecmascript/carakan/src/compiler/es_disassembler.h\""
print
print "void"
print "ES_Disassembler::DisassembleInstruction(ES_Code *code, ES_CodeWord *&word, ES_CodeWord *codewords)"
print "{"
print "    switch (word++->instruction)"
print "    {"

current_condition = None

for instruction in instructions.instructions:
    if current_condition != instruction.condition:
        if current_condition: print "#endif"
        current_condition = instruction.condition
        if instruction.code != 0: print
        if current_condition: print "#%s" % current_condition
    elif instruction.code != 0: print

    if instruction.name == "ESI_LAST_INSTRUCTION": break

    print "    case %s:" % instruction.name

    if instruction.name == "ESI_CALL" or instruction.name == "ESI_EVAL":
        # special handling of most significant bit in 'argc' operand
        print "        OutputRegister(word++->index, \"frame\");"
        print "        OutputImmediate(static_cast<int>(word->index & 0x7fffffffu), UNI_L(\"argc\"));"
        print "        if ((word->immediate & 0x80000000u) != 0)"
        print "            output.Append(\" [this = global object]\");"
        print "        ++word;"
        if instruction.name == "ESI_EVAL":
            print "        OutputImmediate(word++->immediate, UNI_L(\"%s\"));" % instruction.operands[2].name
            print "        OutputImmediate(word++->immediate, UNI_L(\"%s\"));" % instruction.operands[2].name
    elif instruction.name == "ESI_ADDN":
        print "        {"
        print "            OutputRegister(word++->index, \"dst\");"
        print "            unsigned count = word++->index;"
        print "            OutputImmediate(count, UNI_L(\"count\"));"
        print "            for (unsigned index = 0; index < count; ++index)"
        print "                OutputRegister(word++->index);"
        print "        }"
    elif instruction.name == "ESI_CONSTRUCT_OBJECT":
        print "        {"
        print "            OutputRegister(word++->index, \"dst\");"
        print "            unsigned count = code->data->object_literal_classes[word++->index].properties_count;"
        print "            OutputImmediate(count, UNI_L(\"class\"));"
        print "            for (unsigned index = 0; index < count; ++index)"
        print "                OutputRegister(word++->index);"
        print "        }"
    else:
        for operand in instruction.operands:
            if operand.operandtype.startswith("reg:"):
                print "        OutputRegister(word++->index & 0x7fffffffu, \"%s\");" % operand.name
            elif operand.operandtype == "imm:offset":
                print "        OutputOffset(word->offset, word - codewords);"
                print "        ++word;"
            elif operand.operandtype == "imm:cache:property":
                print "        OutputPropertyCache(code, word++->immediate);"
            elif operand.operandtype == "imm:cache:global":
                print "        OutputGlobalCache(code, word++->immediate);"
            elif operand.operandtype.startswith("imm:"):
                print "        OutputImmediate(word++->immediate, UNI_L(\"%s\"));" % operand.name
            elif operand.operandtype == "string":
                print "        OutputString(context, code, word++->index);"
            elif operand.operandtype == "double":
                print "        OutputDouble(context, code, word++->index);"
            elif operand.operandtype == "identifier":
                print "        OutputIdentifier(code, *word++);"
            elif operand.operandtype == "classid":
                print "        OutputClassID(*word++);"
            elif operand.operandtype in ("function", "regexp"):
                print "        OutputImmediate(word++->index, UNI_L(\"%s\"));" % operand.name
            else:
                # unused operand
                print "        ++word;"

    print "        break;"

print "    default:"
print "        output.AppendL(\"BROKEN PROGRAM!\\n\");"
print "    }"
print "}"
print
print "#endif // ES_DISASSEMBLER_SUPPORT"
