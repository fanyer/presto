#!/usr/bin/env python

from re import compile
from sys import argv, stderr

class Function:
    def __init__(self, impl, data=None):
        self.impl = impl
        self.data = data

    def __str__(self):
        if self.data is None: return "{ (void (*)()) %s, INT_MAX }" % self.impl
        else: return "{ (void (*)()) %s, %d }" % (self.impl, self.data)

    def isWithData(self): return data is not None

functions = { "Node.prototype.insertBefore":           Function("DOM_Node::insertBefore"),
              "Node.prototype.removeChild":            Function("DOM_Node::removeChild"),
              "CharacterData.prototype.appendData":    Function("DOM_CharacterData::appendData"),
              "Document.prototype.createNodeIterator": Function("DOM_Document::createTraversalObject", 0),
              "Document.prototype.createTreeWalker":   Function("DOM_Document::createTraversalObject", 1),
              "NodeIterator.prototype.nextNode":       Function("DOM_NodeIterator::move", 1),
              "NodeIterator.prototype.previousNode":   Function("DOM_NodeIterator::move", 0) }

constants = { "Node.ELEMENT_NODE"                : 1,
              "Node.ATTRIBUTE_NODE"              : 2,
              "Node.TEXT_NODE"                   : 3,
              "Node.CDATA_SECTION_NODE"          : 4,
              "Node.ENTITY_REFERENCE_NODE"       : 5,
              "Node.ENTITY_NODE"                 : 6,
              "Node.PROCESSING_INSTRUCTION_NODE" : 7,
              "Node.COMMENT_NODE"                : 8,
              "Node.DOCUMENT_NODE"               : 9,
              "Node.DOCUMENT_TYPE_NODE"          : 10,
              "Node.DOCUMENT_FRAGMENT_NODE"      : 11,
              "Node.NOTATION_NODE"               : 12,

              "NodeFilter.SHOW_ALL"                    : 0xffffffff,
              "NodeFilter.SHOW_ELEMENT"                : 0x00000001,
              "NodeFilter.SHOW_ATTRIBUTE"              : 0x00000002,
              "NodeFilter.SHOW_TEXT"                   : 0x00000004,
              "NodeFilter.SHOW_CDATA_SECTION"          : 0x00000008,
              "NodeFilter.SHOW_ENTITY_REFERENCE"       : 0x00000010,
              "NodeFilter.SHOW_ENTITY"                 : 0x00000020,
              "NodeFilter.SHOW_PROCESSING_INSTRUCTION" : 0x00000040,
              "NodeFilter.SHOW_COMMENT"                : 0x00000080,
              "NodeFilter.SHOW_DOCUMENT"               : 0x00000100,
              "NodeFilter.SHOW_DOCUMENT_TYPE"          : 0x00000200,
              "NodeFilter.SHOW_DOCUMENT_FRAGMENT"      : 0x00000400,
              "NodeFilter.SHOW_NOTATION"               : 0x00000800 }

def processComment(text, bindings):
    words = text.split()

    if words[0] == "Bindings:":
        index = 1

        while index < len(words):
            word = words[index]
            index += 1

            if word != "*":
                if word.endswith(":"):
                    currentIdentifier = word[:-1]
                elif word in functions:
                    bindings[currentIdentifier] = (bindings[None], functions[word])
                    bindings[None] += 1
                elif word in constants:
                    bindings[currentIdentifier] = (-1, constants[word])
                else:
                    print >>stderr, "WARNING: unknown binding: '%s'" % word

def preprocess(source):
    re_comment = compile(r"^/\*(\*[^/]|[^*])*\*/")
    re_word = compile(r"^\w+")
    re_whitespace = compile(r"^\s+")

    bindings = { None: 0 }
    bindingsUsed = { None: 0 }
    result = ""
    lastWasIdent = False
    inFunctionName = 0
    depth = 0

    while source != "":
        match = re_comment.search(source)
        if match:
            processComment(match.group()[2:-2], bindings)
            source = source[match.end():]
            continue

        match = re_word.search(source)
        if match:
            if (inFunctionName < 1 or inFunctionName > 3) and lastWasIdent: result += " "
            if inFunctionName == 0 and match.group() == "function":
                inFunctionName = 1
            elif inFunctionName < 1 or inFunctionName > 3:
                binding = bindings.get(match.group())
                if binding:
                    if binding[0] != -1:
                        result += "$%d" % binding[0]
                    else:
                        result += str(binding[1])
                    bindingsUsed[match.group()] = True
                else:
                    result += match.group()
                lastWasIdent = True
            source = source[match.end():]
            continue

        match = re_whitespace.search(source)
        if match:
            source = source[match.end():]
            continue

        if inFunctionName == 1 and source[0] == "(": inFunctionName = 2
        elif inFunctionName == 2 and source[0] == ")": inFunctionName = 3
        elif inFunctionName == 3 and source[0] == "{": inFunctionName = 4
        elif inFunctionName < 1 or inFunctionName > 3:
            if source[0] == "{": depth += 1
            elif source[0] == "}": depth -= 1
            if depth >= 0:
                lastWasIdent = False
                if source[0] == '"': result += "\\"
                result += source[0]
        source = source[1:]

    if len(bindings) != len(bindingsUsed):
        for key in bindings.keys():
            if key not in bindingsUsed:
                print >>stderr, "WARNING: binding '%s' not used." % key

    bindingsList = []

    for index in range(bindings[None]):
        for key, value in bindings.items():
            if key is not None:
                if value[0] == index:
                    function = value[1]
                    break
        bindingsList.append(str(function))

    return (result, bindingsList)

if __name__ == "__main__":
    result, bindings = preprocess(open(argv[1], "r").read())

    for binding in bindings: print "\t%s," % binding

    while result != "":
        print "\t\"%s\"," % result[:72]
        result = result[72:]

