#!/usr/bin/env python
# -*- coding: iso-8859-1 -*-

import re
import sys
import os
import time
import util
from cStringIO import StringIO

def generate(sourceRoot, outputRoot):
    class File:
        def __init__(self, path, root=outputRoot):
            self.__path = os.path.join(root, *path.split("/"))
            self.__file = StringIO()
            self.__updated = False

        def __getattr__(self, name):
            return getattr(self.__file, name)

        def close(self):
            written = self.__file.getvalue()

            try: existing = open(self.__path).read()
            except: existing = None

            if written != existing:
                self.__updated = True
                util.makedirs(os.path.dirname(self.__path))
                open(self.__path, "w").write(written)

        def updated(self):
            return self.__updated

        def __str__(self):
            return self.__path

    atoms_txt_path = os.path.join(sourceRoot, "modules/dom/src/atoms.txt")

    opatom_h_path = "modules/dom/src/opatom.h"
    opatom_cpp_path = "modules/dom/src/opatom.cpp.inc"
    opatom_ot_path = "modules/dom/selftest/opatom.ot"

    errors = []
    warnings = []
    informational = []

    util.fileTracker.addInput(atoms_txt_path)
    atoms_txt = open(atoms_txt_path, "r")
    atoms = {}

    def addAtom(name, css_property, html_attribute, svg_attribute, condition):
        atom = atoms.get(name)
        if atom:
            errors.append("modules/dom/src/atoms.txt:%d: atom defined twice, previously defined at line %d" % (line_nr, atom['line_nr']))
        else:
            atoms[name] = dict(line_nr=line_nr, name=name, css_property=css_property, html_attribute=html_attribute, svg_attribute=svg_attribute, condition=condition)

    re_comment = re.compile(r"^\s*#.*$")
    re_condition = r"(?:{(.*)})?"
    re_simple_atom = re.compile(r"^\s*([a-zA-Z0-9_]+)\s*" + re_condition + "\s*$")
    re_complex_atom = re.compile(r"^\s*([a-zA-Z0-9_]+)\s*=\s\[(.*)\]\s*" + re_condition + "\s*$")

    line_nr = 0

    in_header = True
    atoms_txt_header = []

    for line in atoms_txt:
        line_nr += 1

        line = line.strip()

        if in_header:
            if line.startswith("#"):
                atoms_txt_header.append(line)
            else:
                in_header = False

        if not line: continue

        match = re_comment.match(line)
        if match:
            continue

        match = re_simple_atom.match(line)
        if match:
            condition = match.group(2)
            if condition:
                condition = condition.strip();
            addAtom(match.group(1), None, None, None, condition)
            continue

        def is_html(d):
            return d.startswith("ATTR_") or d.startswith("Markup::HA_")

        def is_svg(d):
            return d.startswith("SVG_SA_") or d.startswith("Markup::SVGA_")

        match = re_complex_atom.match(line)
        if match:
            data = match.group(2).split(",")
            css_property = [datum.strip() for datum in data if datum.strip().startswith("CSS_PROPERTY_")]
            html_attribute = [datum.strip() for datum in data if is_html(datum.strip())]
            svg_attribute = [datum.strip() for datum in data if is_svg(datum.strip())]
            unknown = [datum.strip() for datum in data if not datum.strip().startswith("CSS_PROPERTY_") and not is_html(datum.strip()) and not is_svg(datum.strip())]
            if css_property: css_property = css_property[0]
            else: css_property = None
            if html_attribute: html_attribute = html_attribute[0]
            else: html_attribute = None
            if svg_attribute: svg_attribute = svg_attribute[0]
            else: svg_attribute = None
            if unknown: errors.append("modules/dom/src/atoms.txt:%d: unknown token(s): %s" % (line_nr, ", ".join(unknown)))
            condition = match.group(3);
            if condition:
                condition = condition.strip();
            addAtom(match.group(1), css_property, html_attribute, svg_attribute, condition)
            continue

        errors.append("modules/dom/src/atoms.txt:%d: parse error" % line_nr)

    atoms_txt.close()

    atom_names = atoms.keys()
    atom_names.sort()

    if errors:
        print >>sys.stderr, "Errors:"
        for error in errors: print >>sys.stderr, error
        return 1

    if "--grep-for-uses" in sys.argv:
        for atom_name in atom_names:
            atom = atoms[atom_name]
            if not atom["css_property"]:
                result = os.system("grep -R OP_ATOM_%s `find modules/dom/src -name '*.cpp' -print` >/dev/null" % atom_name)
                if os.WIFEXITED(result) and os.WEXITSTATUS(result) == 1:
                    warnings.append("%s:%d: OP_ATOM_%s appears to be unused." % (atoms_txt_path, atom["line_nr"], atom_name))

    if warnings:
        print >>sys.stderr, "Warnings:"
        for warning in warnings: print >>sys.stderr, warning

    last_atom_name = None
    last_line_nr = 0
    reorder = False

    for atom_name in atom_names:
        if atoms[atom_name]["line_nr"] < last_line_nr:
            reorder = True
            break
        last_atom_name = atom_name
        last_line_nr = atoms[atom_name]["line_nr"]

    if reorder:
        print >>sys.stderr, """
WARNING: Reordering atoms in modules/dom/src/atoms.txt.

This file is tracked by Git!  You should commit the updated (reordered)
version of the file, or everyone will get this message when building.
"""

        atoms_txt = open(atoms_txt_path, "w")

        for line in atoms_txt_header:
            print >>atoms_txt, line

        print >>atoms_txt

        for atom_name in atom_names:
            atom = atoms[atom_name]
            data = []
            condition = ""

            if atom["svg_attribute"]: data.append(atom["svg_attribute"])
            if atom["html_attribute"]: data.append(atom["html_attribute"])
            if atom["css_property"]: data.append(atom["css_property"])
            if atom["condition"]: condition = " {%s}" % atom["condition"]

            if data: print >>atoms_txt, "%s = [%s]%s" % (atom_name, ", ".join(data), condition)
            else: print >>atoms_txt, "%s%s" % (atom_name, condition)

        atoms_txt.close()

    opatom_h = File(opatom_h_path)

    print >>opatom_h, "/* -*- mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */"
    print >>opatom_h
    print >>opatom_h, "#ifndef DOM_OPATOM_H"
    print >>opatom_h, "#define DOM_OPATOM_H"
    print >>opatom_h
    print >>opatom_h, "#include \"modules/dom/src/domdefines.h\""
    print >>opatom_h
    print >>opatom_h, "enum OpAtom"
    print >>opatom_h, "{"
    print >>opatom_h, "\tOP_ATOM_UNASSIGNED = -1,"
    print >>opatom_h

    re_exclude_from_format = re.compile(r"ifdef|ifndef|if|\(|\)|defined")
    re_whitespace = re.compile(r"\s+")

    def formatCondition(condition):
        condition = re_exclude_from_format.sub("", condition)
        condition = re_whitespace.sub(" ", condition)
        return condition.strip()

    last_condition = None

    for index in range(len(atom_names)):
        atom_name = atom_names[index]
        atom = atoms[atom_name]

        if atom["condition"]:
            if atom["condition"] != last_condition:
                print >>opatom_h, "#%s" % atom["condition"]
                last_condition = atom["condition"]

        print >>opatom_h, "\tOP_ATOM_%s," % atom_name

        if atom["condition"]:
            if index < len(atom_names) - 1:
                next_condition = atoms[atom_names[index + 1]]["condition"]
            else:
                next_condition = None
            if next_condition != atom["condition"]:
                print >>opatom_h, "#endif // %s" % formatCondition(atom["condition"])
                last_condition = None

    print >>opatom_h
    print >>opatom_h, "\tOP_ATOM_ABSOLUTELY_LAST_ENUM"
    print >>opatom_h, "};"
    print >>opatom_h
    print >>opatom_h, "extern const unsigned g_DOM_atomData[OP_ATOM_ABSOLUTELY_LAST_ENUM];"
    print >>opatom_h, "extern const unsigned short g_DOM_SVG_atomData[OP_ATOM_ABSOLUTELY_LAST_ENUM];"
    print >>opatom_h
    print >>opatom_h, "OpAtom DOM_StringToAtom(const uni_char *string);"
    print >>opatom_h, "const char *DOM_AtomToString(OpAtom atom);"
    print >>opatom_h
    print >>opatom_h, "Markup::AttrType DOM_AtomToHtmlAttribute(OpAtom atom);"
    print >>opatom_h, "int DOM_AtomToCssProperty(OpAtom atom);"
    print >>opatom_h, "Markup::AttrType DOM_AtomToSvgAttribute(OpAtom atom);"
    print >>opatom_h
    print >>opatom_h, "#endif // DOM_OPATOM_H"

    opatom_h.close()

    opatom_cpp = File(opatom_cpp_path)

    print >>opatom_cpp, "/* -*- mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */"
    print >>opatom_cpp
    print >>opatom_cpp, "#ifdef DOM_NO_COMPLEX_GLOBALS"
    print >>opatom_cpp, "# define DOM_ATOM_NAMES_START() void DOM_atomNames_Init(DOM_GlobalData *global_data) { const char **names = global_data->atomNames;"
    print >>opatom_cpp, "# define DOM_ATOM_NAMES_ITEM(name) *names = name; ++names;"
    print >>opatom_cpp, "# define DOM_ATOM_NAMES_ITEM_LAST(name) *names = name;"
    print >>opatom_cpp, "# define DOM_ATOM_NAMES_END() }"
    print >>opatom_cpp, "#else // DOM_NO_COMPLEX_GLOBALS"
    print >>opatom_cpp, "# define DOM_ATOM_NAMES_START() const char *const g_DOM_atomNames[] = {"
    print >>opatom_cpp, "# define DOM_ATOM_NAMES_ITEM(name) name,"
    print >>opatom_cpp, "# define DOM_ATOM_NAMES_ITEM_LAST(name) name"
    print >>opatom_cpp, "# define DOM_ATOM_NAMES_END() };"
    print >>opatom_cpp, "#endif // DOM_NO_COMPLEX_GLOBALS"
    print >>opatom_cpp
    print >>opatom_cpp, "DOM_ATOM_NAMES_START()"

    last_atom_name = atom_names[-1]
    comma = ""
    last_condition = None

    for index in range(len(atom_names)):
        atom_name = atom_names[index]
        atom = atoms[atom_name]

        if atom_name == last_atom_name: comma = "_LAST"

        if atom["condition"]:
            if atom["condition"] != last_condition:
                print >>opatom_cpp, "#%s" % atom["condition"]
                last_condition = atom["condition"]

        print >>opatom_cpp, "\tDOM_ATOM_NAMES_ITEM%s(\"%s\")" % (comma, atom_name)

        if atom["condition"]:
            if index < len(atom_names) - 1:
                next_condition = atoms[atom_names[index + 1]]["condition"]
            else:
                next_condition = None
            if next_condition != atom["condition"]:
                print >>opatom_cpp, "#endif // %s" % formatCondition(atom["condition"])
                last_condition = None

    print >>opatom_cpp, "DOM_ATOM_NAMES_END()"
    print >>opatom_cpp
    print >>opatom_cpp, "const unsigned g_DOM_atomData[] ="
    print >>opatom_cpp, "{"

    comma = ","
    last_condition = None

    for index in range(len(atom_names)):
        atom_name = atom_names[index]
        atom = atoms[atom_name]

        if atom_name == last_atom_name: comma = ""

        if atom["condition"]:
            if atom["condition"] != last_condition:
                print >>opatom_cpp, "#%s" % atom["condition"]
                last_condition = atom["condition"]

        html_attribute = atom["html_attribute"]
        css_property = atom["css_property"]
        if html_attribute is None and css_property is None:
            print >>opatom_cpp, "\tUINT_MAX%s // %s" % (comma, atom_name)
        else:
            if html_attribute is None: html_attribute = "USHRT_MAX"
            if css_property is None: css_property = "USHRT_MAX"
            print >>opatom_cpp, "\tstatic_cast<unsigned>(%s << 16 | %s)%s // %s" % (html_attribute, css_property, comma, atom_name)

        if atom["condition"]:
            if index < len(atom_names) - 1:
                next_condition = atoms[atom_names[index + 1]]["condition"]
            else:
                next_condition = None
            if next_condition != atom["condition"]:
                print >>opatom_cpp, "#endif // %s" % formatCondition(atom["condition"])
                last_condition = None

    print >>opatom_cpp, "};"
    print >>opatom_cpp
    print >>opatom_cpp, "#ifdef SVG_DOM"
    print >>opatom_cpp, "const unsigned short g_DOM_SVG_atomData[] ="
    print >>opatom_cpp, "{"

    comma = ","
    last_condition = None

    for index in range(len(atom_names)):
        atom_name = atom_names[index]
        atom = atoms[atom_name]

        if atom_name == last_atom_name: comma = ""

        if atom["condition"]:
            if atom["condition"] != last_condition:
                print >>opatom_cpp, "#%s" % atom["condition"]
                last_condition = atom["condition"]

        svg_attribute = atom["svg_attribute"]
        if svg_attribute is None:
            print >>opatom_cpp, "\tUSHRT_MAX%s // %s" % (comma, atom_name)
        else:
            print >>opatom_cpp, "\t%s%s // %s" % (svg_attribute, comma, atom_name)

        if atom["condition"]:
            if index < len(atom_names) - 1:
                next_condition = atoms[atom_names[index + 1]]["condition"]
            else:
                next_condition = None
            if next_condition != atom["condition"]:
                print >>opatom_cpp, "#endif // %s" % formatCondition(atom["condition"])
                last_condition = None

    print >>opatom_cpp, "};"
    print >>opatom_cpp
    print >>opatom_cpp, "#endif // SVG_DOM"

    opatom_cpp.close()

    opatom_ot = File(opatom_ot_path)

    print >>opatom_ot, "/* -*- mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */"
    print >>opatom_ot
    print >>opatom_ot, "group \"DOM.OpAtom.simple\";"
    print >>opatom_ot
    print >>opatom_ot, "include \"modules/dom/src/opatomdata.h\";"
    print >>opatom_ot, "include \"modules/util/tempbuf.h\";"
    print >>opatom_ot, "include \"modules/doc/frm_doc.h\";"
    print >>opatom_ot
    print >>opatom_ot, "test(\"Sequenciality\")"
    print >>opatom_ot, "{"
    print >>opatom_ot, "  /* If this test fails, chances are someone has modified the OpAtom"
    print >>opatom_ot, "     declaration manually and not updated this file correctly. */"
    print >>opatom_ot
    print >>opatom_ot, "  int index = -1;"
    print >>opatom_ot
    print >>opatom_ot, "  verify(OP_ATOM_UNASSIGNED == index++);"

    last_condition = None

    for index in range(len(atom_names)):
        atom_name = atom_names[index]
        atom = atoms[atom_name]

        if atom["condition"]:
            if atom["condition"] != last_condition:
                print >>opatom_ot, "#%s" % atom["condition"]
                last_condition = atom["condition"]

        print >>opatom_ot, "  verify(OP_ATOM_%s == index++);" % atom_names[index]

        if atom["condition"]:
            if index < len(atom_names) - 1:
                next_condition = atoms[atom_names[index + 1]]["condition"]
            else:
                next_condition = None
            if next_condition != atom["condition"]:
                print >>opatom_ot, "#endif // %s" % formatCondition(atom["condition"])
                last_condition = None

    print >>opatom_ot, "  verify(OP_ATOM_ABSOLUTELY_LAST_ENUM == index);"
    print >>opatom_ot, "}"
    print >>opatom_ot
    print >>opatom_ot, "test(\"Atom to string conversion\")"
    print >>opatom_ot, "{"

    last_condition = None

    for index in range(len(atom_names)):
        atom_name = atom_names[index]
        atom = atoms[atom_name]

        if atom["condition"]:
            if atom["condition"] != last_condition:
                print >>opatom_ot, "#%s" % atom["condition"]
                last_condition = atom["condition"]

        print >>opatom_ot, "  verify(op_strcmp(DOM_AtomToString(OP_ATOM_%s), \"%s\") == 0);" % (atom_name, atom_name)

        if atom["condition"] or last_condition:
            if index < len(atom_names) - 1:
                next_condition = atoms[atom_names[index + 1]]["condition"]
            else:
                next_condition = None
            if next_condition != atom["condition"]:
                print >>opatom_ot, "#endif // %s" % formatCondition(atom["condition"])
                last_condition = None

    print >>opatom_ot, "}"
    print >>opatom_ot
    print >>opatom_ot, "test(\"String to atom conversion\")"
    print >>opatom_ot, "{"
    print >>opatom_ot, "  verify(DOM_StringToAtom(UNI_L(\"aaaaaa\")) == OP_ATOM_UNASSIGNED);"

    last_condition = None

    for index in range(len(atom_names)):
        atom_name = atom_names[index]
        atom = atoms[atom_name]

        if atom["condition"]:
            if atom["condition"] != last_condition:
                print >>opatom_ot, "#%s" % atom["condition"]
                last_condition = atom["condition"]

        print >>opatom_ot, "  verify(DOM_StringToAtom(UNI_L(\"%s\")) == OP_ATOM_%s);" % (atom_name, atom_name)
        print >>opatom_ot, "  verify(DOM_StringToAtom(UNI_L(\"%sA\")) == OP_ATOM_UNASSIGNED);" % atom_name

        if atom["condition"] or last_condition:
            if index < len(atom_names) - 1:
                next_condition = atoms[atom_names[index + 1]]["condition"]
            else:
                next_condition = None
            if next_condition != atom["condition"]:
                print >>opatom_ot, "#endif // %s" % formatCondition(atom["condition"])
                last_condition = None

    print >>opatom_ot, "  verify(DOM_StringToAtom(UNI_L(\"zzzzzz\")) == OP_ATOM_UNASSIGNED);"
    print >>opatom_ot, "}"
    print >>opatom_ot
    print >>opatom_ot, "test(\"Atom to HTML attribute conversion\")"
    print >>opatom_ot, "{"

    last_condition = None

    html_atom_names = [atom_name for atom_name in atom_names if atoms[atom_name]["html_attribute"]]

    for index in range(len(html_atom_names)):
        atom_name = html_atom_names[index]
        atom = atoms[atom_name]

        html_attribute = atoms[atom_name]["html_attribute"]

        if atom["condition"]:
            if atom["condition"] != last_condition:
                print >>opatom_ot, "#%s" % atom["condition"]
                last_condition = atom["condition"]

        print >>opatom_ot, "  verify(DOM_AtomToHtmlAttribute(OP_ATOM_%s) == %s || %s == USHRT_MAX);" % (atom_name, html_attribute, html_attribute)

        if atom["condition"] or last_condition:
            if index < len(html_atom_names) - 1:
                next_condition = atoms[html_atom_names[index + 1]]["condition"]
            else:
                next_condition = None
            if next_condition != atom["condition"]:
                print >>opatom_ot, "#endif // %s" % formatCondition(atom["condition"])
                last_condition = None

    print >>opatom_ot, "}"
    print >>opatom_ot
    print >>opatom_ot, "test(\"Atom to SVG attribute conversion\")"
    print >>opatom_ot, "\trequire SVG_SUPPORT;"
    print >>opatom_ot, "\trequire SVG_DOM;"
    print >>opatom_ot, "{"

    last_condition = None

    svg_atom_names = [atom_name for atom_name in atom_names if atoms[atom_name]["svg_attribute"]]

    for index in range(len(svg_atom_names)):
        atom_name = svg_atom_names[index]
        atom = atoms[atom_name]

        svg_attribute = atoms[atom_name]["svg_attribute"]

        if atom["condition"]:
            if atom["condition"] != last_condition:
                print >>opatom_ot, "#%s" % atom["condition"]
                last_condition = atom["condition"]

        print >>opatom_ot, "  verify(DOM_AtomToSvgAttribute(OP_ATOM_%s) == %s || %s == USHRT_MAX);" % (atom_name, svg_attribute, svg_attribute)

        if atom["condition"] or last_condition:
            if index < len(svg_atom_names) - 1:
                next_condition = atoms[svg_atom_names[index + 1]]["condition"]
            else:
                next_condition = None
            if next_condition != atom["condition"]:
                print >>opatom_ot, "#endif // %s" % formatCondition(atom["condition"])
                last_condition = None

    print >>opatom_ot, "}"
    print >>opatom_ot
    print >>opatom_ot, "test(\"Atom to CSS property conversion\")"
    print >>opatom_ot, "{"

    last_condition = None

    css_atom_names = [atom_name for atom_name in atom_names if atoms[atom_name]["css_property"]]

    for index in range(len(css_atom_names)):
        atom_name = css_atom_names[index]
        atom = atoms[atom_name]

        css_property = atoms[atom_name]["css_property"]
        if css_property is not None:
            if atom["condition"]:
                if atom["condition"] != last_condition:
                    print >>opatom_ot, "#%s" % atom["condition"]
                    last_condition = atom["condition"]

            print >>opatom_ot, "  verify(DOM_AtomToCssProperty(OP_ATOM_%s) == %s);" % (atom_name, css_property)

            if atom["condition"] or last_condition:
                if index < len(css_atom_names) - 1:
                    next_condition = atoms[css_atom_names[index + 1]]["condition"]
                else:
                    next_condition = None
                if next_condition != atom["condition"]:
                    print >>opatom_ot, "#endif // %s" % formatCondition(atom["condition"])
                    last_condition = None

    print >>opatom_ot, "}"
    print >>opatom_ot
    print >>opatom_ot, "group \"DOM.OpAtom.complicated\";"
    print >>opatom_ot, "require init;"
    print >>opatom_ot
    print >>opatom_ot, "include \"modules/dom/src/domenvironmentimpl.h\";"
    print >>opatom_ot, "include \"modules/dom/src/domobj.h\";"
    print >>opatom_ot
    print >>opatom_ot, "global"
    print >>opatom_ot, "{"
    print >>opatom_ot, "  class AtomsTester : public DOM_Object"
    print >>opatom_ot, "  {"
    print >>opatom_ot, "  private:"
    print >>opatom_ot, "    OpAtom expected, got;"
    print >>opatom_ot, "    BOOL result, called;"
    print >>opatom_ot
    print >>opatom_ot, "  public:"
    print >>opatom_ot, "    AtomsTester()"
    print >>opatom_ot, "      : expected(OP_ATOM_UNASSIGNED), got(OP_ATOM_UNASSIGNED), result(FALSE), called(FALSE)"
    print >>opatom_ot, "    {"
    print >>opatom_ot, "    }"
    print >>opatom_ot
    print >>opatom_ot, "    virtual ES_GetState GetIndex(int property_index, ES_Value *value, ES_Runtime *)"
    print >>opatom_ot, "    {"
    print >>opatom_ot, "      expected = (OpAtom) property_index;"
    print >>opatom_ot, "      DOMSetUndefined(value);"
    print >>opatom_ot, "      return GET_SUCCESS;"
    print >>opatom_ot, "    }"
    print >>opatom_ot
    print >>opatom_ot, "    virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime *)"
    print >>opatom_ot, "    {"
    print >>opatom_ot, "      result = (got = property_name) == expected;"
    print >>opatom_ot, "      called = TRUE;"
    print >>opatom_ot, "      DOMSetUndefined(value);"
    print >>opatom_ot, "      return GET_SUCCESS;"
    print >>opatom_ot, "    }"
    print >>opatom_ot
    print >>opatom_ot, "    virtual ES_PutState PutName(OpAtom property_name, ES_Value *, ES_Runtime *)"
    print >>opatom_ot, "    {"
    print >>opatom_ot, "      result = property_name == expected;"
    print >>opatom_ot, "      called = TRUE;"
    print >>opatom_ot, "      return PUT_SUCCESS;"
    print >>opatom_ot, "    }"
    print >>opatom_ot
    print >>opatom_ot, "    virtual int Call(ES_Object *, ES_Value *, int, ES_Value *return_value, ES_Runtime *)"
    print >>opatom_ot, "    {"
    print >>opatom_ot, "      if (result)"
    print >>opatom_ot, "        DOMSetBoolean(return_value, TRUE);"
    print >>opatom_ot, "      else"
    print >>opatom_ot, "      {"
    print >>opatom_ot, "        TempBuffer *buffer = GetEmptyTempBuf();"
    print >>opatom_ot, "        char expected_int[14]; /* ARRAY OK 2009-04-24 jl */"
    print >>opatom_ot, "        char got_int[14]; /* ARRAY OK 2009-04-24 jl */"
    print >>opatom_ot, "        const char *expected_string, *got_string;"
    print >>opatom_ot
    print >>opatom_ot, "        op_sprintf(expected_int, \"%d\", (int) expected);"
    print >>opatom_ot, "        op_sprintf(got_int, \"%d\", (int) got);"
    print >>opatom_ot
    print >>opatom_ot, "        expected_string = (expected > OP_ATOM_UNASSIGNED && expected < OP_ATOM_ABSOLUTELY_LAST_ENUM) ? DOM_AtomToString(expected) : \"<invalid>\";"
    print >>opatom_ot, "        got_string = (got > OP_ATOM_UNASSIGNED && got < OP_ATOM_ABSOLUTELY_LAST_ENUM) ? DOM_AtomToString(got) : \"<invalid>\";"
    print >>opatom_ot
    print >>opatom_ot, "        CALL_FAILED_IF_ERROR(buffer->Append(\"Expected \"));"
    print >>opatom_ot, "        CALL_FAILED_IF_ERROR(buffer->Append(expected_int));"
    print >>opatom_ot, "        CALL_FAILED_IF_ERROR(buffer->Append(\" (\"));"
    print >>opatom_ot, "        CALL_FAILED_IF_ERROR(buffer->Append(expected_string));"
    print >>opatom_ot, "        if (called)"
    print >>opatom_ot, "        {"
    print >>opatom_ot, "          CALL_FAILED_IF_ERROR(buffer->Append(\"), got \"));"
    print >>opatom_ot, "          CALL_FAILED_IF_ERROR(buffer->Append(got_int));"
    print >>opatom_ot, "          CALL_FAILED_IF_ERROR(buffer->Append(\" (\"));"
    print >>opatom_ot, "          CALL_FAILED_IF_ERROR(buffer->Append(got_string));"
    print >>opatom_ot, "          CALL_FAILED_IF_ERROR(buffer->Append(\")\"));"
    print >>opatom_ot, "        }"
    print >>opatom_ot, "        else"
    print >>opatom_ot, "          CALL_FAILED_IF_ERROR(buffer->Append(\"), got nothing\"));"
    print >>opatom_ot
    print >>opatom_ot, "        DOMSetString(return_value, buffer->GetStorage());"
    print >>opatom_ot, "      }"
    print >>opatom_ot
    print >>opatom_ot, "      result = FALSE;"
    print >>opatom_ot, "      called = FALSE;"
    print >>opatom_ot
    print >>opatom_ot, "      return ES_VALUE;"
    print >>opatom_ot, "    }"
    print >>opatom_ot, "  };"
    print >>opatom_ot, "}"
    print >>opatom_ot
    print >>opatom_ot, "html \"\";"
    print >>opatom_ot
    print >>opatom_ot, "test(\"Setup (c++)\")"
    print >>opatom_ot, "{"
    print >>opatom_ot, "  verify(state.doc);"
    print >>opatom_ot, "  verify(state.doc->ConstructDOMEnvironment() == OpStatus::OK);"
    print >>opatom_ot, "  verify(state.doc->GetDOMEnvironment());"
    print >>opatom_ot, "  verify(state.doc->GetDOMEnvironment()->IsEnabled());"
    print >>opatom_ot
    print >>opatom_ot, "  DOM_Environment *env = state.doc->GetDOMEnvironment();"
    print >>opatom_ot
    print >>opatom_ot, "  DOM_Object *atomstester = OP_NEW(AtomsTester, ());"
    print >>opatom_ot, "  verify(atomstester != NULL);"
    print >>opatom_ot, "  verify(atomstester->SetFunctionRuntime(env->GetRuntime(), UNI_L(\"atomsTester\"), NULL, NULL) == OpStatus::OK);"
    print >>opatom_ot, "  verify(env->GetWindow()->Put(UNI_L(\"atomsTester\"), *atomstester) == OpStatus::OK);"
    print >>opatom_ot, "}"
    print >>opatom_ot
    print >>opatom_ot, "language ecmascript;"
    print >>opatom_ot
    print >>opatom_ot, "test(\"Setup (ecmascript)\")"
    print >>opatom_ot, "  disabled;"
    print >>opatom_ot, "{"
    print >>opatom_ot, "  atoms = ["

    for atom_name in atom_names:
        print >>opatom_ot, "    \"%s\"," % atom_name

    print >>opatom_ot, "    null"
    print >>opatom_ot, "  ];"
    print >>opatom_ot, "}"
    print >>opatom_ot
    print >>opatom_ot, "test(\"GetName conversion\")"
    print >>opatom_ot, "  disabled;"
    print >>opatom_ot, "{"
    print >>opatom_ot, "  var index = 0;"
    print >>opatom_ot, "  while (atoms[index])"
    print >>opatom_ot, "  {"
    print >>opatom_ot, "    var x = atomsTester[index];"
    print >>opatom_ot, "    var y = atomsTester[atoms[index]];"
    print >>opatom_ot, "    var result = atomsTester();"
    print >>opatom_ot, "    if (result != true)"
    print >>opatom_ot, "      throw result;"
    print >>opatom_ot, "    ++index;"
    print >>opatom_ot, "  }"
    print >>opatom_ot, "}"
    print >>opatom_ot
    print >>opatom_ot, "test(\"PutName conversion\")"
    print >>opatom_ot, "  disabled;"
    print >>opatom_ot, "{"
    print >>opatom_ot, "  var index = 0;"
    print >>opatom_ot, "  while (atoms[index])"
    print >>opatom_ot, "  {"
    print >>opatom_ot, "    var x = atomsTester[index];"
    print >>opatom_ot, "    atomsTester[atoms[index]] = true;"
    print >>opatom_ot, "    var result = atomsTester();"
    print >>opatom_ot, "    if (result != true)"
    print >>opatom_ot, "      throw result;"
    print >>opatom_ot, "    ++index;"
    print >>opatom_ot, "  }"
    print >>opatom_ot, "}"

    opatom_ot.close()

    if outputRoot == sourceRoot:
        util.updateModuleGenerated(os.path.join(sourceRoot, "modules", "dom"),
                                   ["src/opatom.h",
                                    "src/opatom.cpp.inc",
                                    "selftest/opatom.ot"])

    if reorder:
        return 1
    elif opatom_h.updated() or opatom_cpp.updated() or opatom_ot.updated():
        return 2
    else:
        return 0
