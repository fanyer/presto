import instructions

def outputFlowed(prefix, continuationPrefix, paragraphs, infix, suffix):
    lastWord = None
    line = prefix

    for index, paragraph in enumerate(paragraphs):
        if index != 0:
            print line
            print infix
            line = continuationPrefix

        if paragraph[0] == "!":
            paragraph = paragraph[1:]
            cp = continuationPrefix + " " * (1 + len(paragraph.split()[0]))
        else:
            cp = continuationPrefix

        for word in paragraph.split():
            if len(line) + 1 + len(word) > 80:
                print line
                line = cp

            if line.endswith("."): line += "  "
            else: line += " "

            line += word
            lastWord = word

    if suffix:
        if len(line) + len(suffix) > 80:
            print line[:-len(lastWord)].rstrip()
            print cp + " " + lastWord + suffix
        else:
            print line + suffix
    else:
        print line

print "/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: \"stroustrup\" -*-"
print " *"
print " * Copyright (C) Opera Software ASA  2002 - 2006"
print " *"
print " * Table of instruction names."
print " *"
print " * @author modules/ecmascript/carakan/src/scripts/es_instruction.h.py"
print " */"
print
print "#ifndef ES_INSTRUCTION_H"
print "#define ES_INSTRUCTION_H"
print
print "/**"
print " * Instruction set definition"
print " * =========================="
print " *"

outputFlowed(" *", " *", instructions.documentation, " *", None)

print " */"
print
print "enum ES_Instruction"
print "{"

current_condition = None

for instruction in instructions.instructions:
    if current_condition != instruction.condition:
        if current_condition: print "#endif"
        current_condition = instruction.condition
        if instruction.code != 0: print
        if current_condition: print "#%s" % current_condition
    elif instruction.code != 0: print

    if instruction.name == "ESI_LAST_INSTRUCTION":
        print "    ESI_LAST_INSTRUCTION"
    else:
        print "    %s," % instruction.name

    if instruction.operands:
        documentation = instruction.documentation + ["!Operands: %s" % ", ".join(map(str, instruction.operands))]
    else:
        documentation = instruction.documentation

    outputFlowed("    /**<", "        ", documentation, "", " */")

print "};"
print
print "#endif // ES_INSTRUCTION_H"
