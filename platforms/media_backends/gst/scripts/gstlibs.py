#!/usr/bin/env python

import glob
import os.path
import sys
import string

from pyparsing import *

header_templ = string.Template('''
class ${className}
{
public:
	BOOL Load(OpDLL* dll);

${typedefs}

${defSymbols}
};

extern ${className} g_${className};

''')

body_templ = string.Template('''
BOOL ${className}::Load(OpDLL* dll)
{
${loadSymbols}

	BOOL loaded =${loadedExpr};

	return loaded;
}

${className} g_${className};

''');

cIdentifier = Word(alphas+"_",alphanums+"_")
cValue = Word(alphanums+"_")
cType = OneOrMore(cIdentifier | "*" | "[]")
cDeclType = cType.setResultsName("typeAndSymbol")
cParameters = Group(Optional(delimitedList(Group(cType | "...")))).setResultsName("params")
cMacro = cIdentifier + Optional("(" + Optional(delimitedList(cValue)) + ")")
cFuncAttrs = ZeroOrMore(cMacro).setResultsName("attrs")
cDeclaration = lineStart + cDeclType + Optional("(" + cParameters + ")" + cFuncAttrs).setResultsName("isfunc") + ";"
cDeclaration.ignore(cStyleComment)

def gencode(className, symbols_, headers):
    for h in headers:
        hText = file(h).read()
        for m in cDeclaration.searchString(hText):
            #print m
            retTypeList = m.typeAndSymbol[:-1]
            stripType = ["GLIB_VAR", "GST_EXPORT"]
            retType = " ".join([t for t in retTypeList if t not in stripType])
            sym = m.typeAndSymbol[-1]
            for symdict in symbols_:
                if symdict['symbol'] != sym:
                    continue
                if len(m.isfunc) > 0:
                    typedef = "typedef %s (*%s_t) (%s);" % \
                        (retType, sym, ", ".join([" ".join(x) for x in m.params]))
                else:
                    typedef = "typedef %s *%s_t;" % (retType, sym)
                symdict['typedef'] = typedef

    # list of lines
    typedefs = []
    defSymbols = []
    loadSymbols = []
    loadedExpr = ['']
    defines = []

    for symdict in symbols_:
        s = symdict['symbol']
        if not symdict.has_key('typedef'):
            print "not found: %s" % s
            continue

        if symdict.has_key('if'):
            ifdef = symdict['if']
            typedefs.append(ifdef)
            defSymbols.append(ifdef)
            loadSymbols.append(ifdef)
            loadedExpr.append(ifdef)
            defines.append(ifdef)

        typedefs.append("\t%s" % symdict['typedef'])
        defSymbols.append("\t%s_t sym_%s;" % (s, s))
        loadSymbols.append("\tsym_%s = (%s_t)dll->GetSymbolAddress(\"%s\");" % (s,s,s))
        loadedExpr.append("\t\tsym_%s != NULL &&" % s);
        defines.append("#define %s (*g_%s.sym_%s)" % (s, className, s))

        if symdict.has_key('if'):
            endif = '#endif'
            typedefs.append(endif)
            defSymbols.append(endif)
            loadSymbols.append(endif)
            loadedExpr.append(endif)
            defines.append(endif)

    loadedExpr.append("\t\tTRUE")

    header = header_templ.substitute(className=className,
                                     typedefs="\n".join(typedefs),
                                     defSymbols="\n".join(defSymbols))
    header += "\n"
    header += "\n".join(defines)
    header += "\n"

    body = body_templ.substitute(className=className,
                                 loadSymbols="\n".join(loadSymbols),
                                 loadedExpr="\n".join(loadedExpr))

    return (header, body)

def splitcode(fname):
    head = ""
    tail = ""

    begun = False
    ended = False
    for line in file(fname):
        if not begun:
            head += line
            if "BEGIN GENERATED CODE" in line:
                begun = True
        else:
            if "END GENERATED CODE" in line:
                ended = True
            if ended:
                tail += line

    return (head, tail)

def parsesymbols(fname):
    symbols = []
    for line in file(fname):
        parts = line.strip().split(' ', 1)
        symdict = {'symbol': parts[0]}
        if len(parts) > 1 and parts[1].startswith('#if'):
            symdict['if'] = parts[1]
        symbols.append(symdict)
    return symbols

def main():
    scriptdir = os.path.dirname(sys.argv[0])
    gstdir = os.path.join(scriptdir, "..")
    incdir = os.path.join(gstdir, "include")

    # 3-tuples of class name, symbol file, header file glob
    libs = [("LibGLib", "libglib.symbols", os.path.join("glib", "*.h")),
            ("LibGObject", "libgobject.symbols", os.path.join("gobject", "*.h")),
            ("LibGStreamer", "libgstreamer.symbols", os.path.join("gst", "*.h")),
            ("LibGstBase", "libgstbase.symbols", os.path.join("gst", "base", "*.h")),
            ("LibGstVideo", "libgstvideo.symbols", os.path.join("gst", "video", "*.h")),
            ("LibGstRiff", "libgstriff.symbols", os.path.join("gst", "riff", "*.h")),
            ("LibGModule", "libgmodule.symbols", os.path.join("gmodule.h"))]

    gen_h = []
    gen_cpp = []

    for (className, symbolfile, headerglob) in libs:
        symbols = parsesymbols(os.path.join(scriptdir, symbolfile))
        headerfiles = glob.glob(os.path.join(incdir, headerglob))
        header, body = gencode(className, symbols, headerfiles)
        gen_h.append(header)
        gen_cpp.append(body)

    gstlibs_h = os.path.join(gstdir, "gstlibs.h")
    (gstlibs_h_head, gstlibs_h_tail) = splitcode(gstlibs_h)
    file(gstlibs_h, "w+").write("\n".join([gstlibs_h_head]+gen_h+[gstlibs_h_tail]))

    gstlibs_cpp = os.path.join(gstdir, "gstlibs.cpp")
    (gstlibs_cpp_head, gstlibs_cpp_tail) = splitcode(gstlibs_cpp)
    file(gstlibs_cpp, "w+").write("\n".join([gstlibs_cpp_head]+gen_cpp+[gstlibs_cpp_tail]))

    print "OK"

if __name__ == '__main__':
    main()
