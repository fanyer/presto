import os
import os.path
import sys
import re

class Instruction:
    def __init__(self, code, name, trivial, condition, documentation, operands, implicit_boolean, alias):
        self.code = code
        self.name = name
        self.trivial = trivial
        self.condition = condition
        self.documentation = documentation
        self.operands = operands[:]
        self.implicit_boolean = implicit_boolean
        self.alias = alias

class Operand:
    def __init__(self, repeated, name, operandtype, valuetype=None):
        self.repeated = repeated
        self.name = name
        self.operandtype = operandtype
        self.valuetype = valuetype

    def __str__(self):
        if self.operandtype.startswith("reg:"):
            return "reg(%s)" % self.name
        elif self.operandtype.startswith("imm:"):
            return "imm(%s)" % self.name
        elif self.operandtype == "constant":
            return "const(%s)" % self.name
        elif self.operandtype == "identifier":
            return "identifier(%s)" % self.name
        elif self.operandtype == "classid":
            return self.name
        elif self.operandtype in ("function", "regexp"):
            return "imm(%s)" % self.name
        else:
            return "unusued"

instructions = []
documentation = []

def parseInstructions():
    if os.path.isdir(os.path.join("..", "..", "..", "..", "modules", "ecmascript")):
        path = os.path.join("..", "..", "..", "..", "modules", "ecmascript", "carakan", "src", "compiler", "es_instructions.txt")
    elif os.path.isfile("module.about"): path = os.path.join("carakan", "src", "compiler", "es_instructions.txt")
    elif os.path.isdir("src"): path = os.path.join("src", "compiler", "es_instructions.txt")
    elif os.path.isdir("compiler"): path = os.path.join("compiler", "es_instructions.txt")
    elif os.path.isfile("es_instructions.txt"): path = "es_instructions.txt"
    elif os.path.isdir(os.path.join("..", "compiler")): path = os.path.join("..", "compiler", "es_instructions.txt")
    elif os.path.isdir(os.path.join("modules", "ecmascript")):
        path = os.path.join("modules", "ecmascript", "carakan", "src", "compiler", "es_instructions.txt")
    else:
        print >>sys.stderr, "don't know where to find es_instructions.txt! (current path: " , os.getcwd() , ")"
        sys.exit(1)

    lines = open(path)
    name = None

    for line in lines:
        # comments
        if line.lstrip().startswith("#"): continue

        # instruction name
        match = re.match("^(ESI_[A-Z0-9_]+)\\s*(<trivial>\\s*)?(?:\\[(alias|ifdef)\\s+(.+)\\])?\\s*$", line.strip())
        if match:
            if name is not None:
                instructions.append(Instruction(len(instructions), name, trivial, condition, map(" ".join, filter(None, doc)), operands, implicit_boolean, alias))

            name = match.group(1)
            trivial = match.group(2) is not None
            if match.group(3) == "ifdef":
                condition = "ifdef " + match.group(4)
            else:
                condition = None
            doc = [[]]
            operands = []
            implicit_boolean = False
            if match.group(3) == "alias":
                alias = match.group(4)
            else:
                alias = None

            continue

        if name is None:
            if not line.strip(): continue
            else:
                print >>sys.stderr, "parse error!"
                sys.exit(1)

        if not line.strip():
            if doc[-1]: doc.append([])
            continue

        # documentation
        if line.lstrip() != line:
            doc[-1].append(line.strip())
            continue

        if line.startswith("*"):
            line = line.lstrip("*")
            repeated = True
        else:
            repeated = False

        # register operand
        match = re.match("^([A-Za-z]+):\\s+(reg:(?:in|out|in:out|out:optional|-))(?:\\s+<(null|undefined|boolean|number|int32|uint32|double|string|object|property|primitive)>)?$", line.strip())
        if match:
            operands.append(Operand(repeated, match.group(1), match.group(2), match.group(3)))
            continue

        # immediate operand
        match = re.match("^([A-Za-z]+):\\s+(imm:(?:signed|unsigned|offset|cache:property|cache:global))$", line.strip())
        if match:
            operands.append(Operand(repeated, match.group(1), match.group(2)))
            continue

        # others
        match = re.match("^([A-Za-z]+):\\s+(identifier|string|double|regexp|function|classid)$", line.strip())
        if match:
            operands.append(Operand(repeated, match.group(1), match.group(2)))
            continue

        # unused
        if re.match("^unused:\\s+unused$", line.strip()):
            operands.append(Operand(repeated, "unused", "unused"))
            continue

        if line.strip() == "=> implicit boolean register":
            implicit_boolean = True
            continue

        print >>sys.stderr, "unrecognized line: %r" % line
        sys.exit(1)
    else:
        if name is not None:
            instructions.append(Instruction(len(instructions), name, trivial, condition, map(" ".join, filter(None, doc)), operands, implicit_boolean, alias))

    global documentation

    lines = open(path.replace("s.txt", "_set.txt"))
    documentation = [[]]

    for line in lines:
        if line.strip():
            documentation[-1].append(line.strip())
        elif documentation[-1]:
            documentation.append([])

    documentation = map(" ".join, filter(None, documentation))

parseInstructions()

if __name__ == "__main__":
    print "parsed %d instructions:" % len(instructions)

    for instruction in instructions:
        if instruction.condition: condition = " [%s]" % instruction.condition
        else: condition = ""
        print "%s%s" % (instruction.name, condition)
        print "  %r" % instruction.documentation

        for operand in instruction.operands:
            if operand.valuetype:
                extra = " <%s>" % operand.valuetype
            else: extra = ""
            print "  %s: %s%s" % (operand.name, operand.operandtype, extra)
