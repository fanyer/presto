
fout = open("expressions.comparison.number.ot", "w")

print >>fout, '/* -*- mode: c++ -*- */'
print >>fout
print >>fout, 'group "XPath.expressions.comparison.number";'
print >>fout
print >>fout, 'require init;'
print >>fout, 'require XPATH_SUPPORT;'
print >>fout
print >>fout, 'include "modules/xpath/xpath.h";'
print >>fout, 'include "modules/xpath/src/xpsthelpers.h";'
print >>fout
print >>fout, 'xml'
print >>fout, '{'
print >>fout, '  "<?xml version=\'1.0\'?>"'
print >>fout, '  "<!DOCTYPE root>"'
print >>fout, '  "<root>"'
print >>fout, '    "<boolean>"'
print >>fout, '      "<one><yes/></one>"'
print >>fout, '      "<zero/>"'
print >>fout, '    "</boolean>"'
print >>fout, '    "<string>"'
print >>fout, '      "<minus-one> -1 </minus-one>"'
print >>fout, '      "<zero> 0 </zero>"'
print >>fout, '      "<one> 1 </one>"'
print >>fout, '    "</string>"'
print >>fout, '    "<nodeset>"'
print >>fout, '      "<number less-than-minus-one=\'\' less-than-zero=\'\' less-than-one=\'\'> -2 </number>"'
print >>fout, '      "<number equal-to-minus-one=\'\' less-than-zero=\'\' less-than-one=\'\'> -1 </number>"'
print >>fout, '      "<number greater-than-minus-one=\'\' equal-to-zero=\'\' less-than-one=\'\'> 0 </number>"'
print >>fout, '      "<number greater-than-minus-one=\'\' greater-than-zero=\'\' equal-to-one=\'\'> 1 </number>"'
print >>fout, '      "<number greater-than-minus-one=\'\' greater-than-zero=\'\' greater-than-one=\'\'> 2 </number>"'
print >>fout, '    "</nodeset>"'
print >>fout, '  "</root>"'
print >>fout, '}'
print >>fout

numeric_constants = [("-1", -1), ("0", 0), ("1", 1)]
string_constants = [("'%s'" % numeric_constant, numeric_value) for numeric_constant, numeric_value in numeric_constants]
boolean_constants = [("false()", 0), ("true()", 1)]

nodeset_expressions = [("//string/minus-one/text()", -1), ("//string/zero/text()", 0), ("//string/one/text()", 1)]
number_expressions = [("number(%s)" % nodeset_expression, numeric_value) for nodeset_expression, numeric_value in nodeset_expressions]
string_expressions = [("string(%s)" % nodeset_expression, numeric_value) for nodeset_expression, numeric_value in nodeset_expressions]
boolean_expressions = [("boolean(//boolean/zero/node())", 0), ("boolean(//boolean/one/node())", 1)]

class LessThan:
    def __str__(self): return "<"
    def compare(self, left, right): return left < right
    def predicate(self, number): return "@less-than-%s" % number

class LessThanOrEqual:
    def __str__(self): return "<="
    def compare(self, left, right): return left <= right
    def predicate(self, number): return "@less-than-%s or @equal-to-%s" % (number, number)

class GreaterThan:
    def __str__(self): return ">"
    def compare(self, left, right): return left > right
    def predicate(self, number): return "@greater-than-%s" % number

class GreaterThanOrEqual:
    def __str__(self): return ">="
    def compare(self, left, right): return left >= right
    def predicate(self, number): return "@greater-than-%s or @equal-to-%s" % (number, number)

class Equal:
    def __str__(self): return "="
    def compare(self, left, right): return left == right
    def predicate(self, number): return "@equal-to-%s" % number

class NotEqual:
    def __str__(self): return "!="
    def compare(self, left, right): return left != right
    def predicate(self, number): return "not(@equal-to-%s)" % number

rel_operators = (LessThan(), LessThanOrEqual(), GreaterThan(), GreaterThanOrEqual())
eq_operators = (Equal(), NotEqual())
all_operators = (LessThan(), LessThanOrEqual(), GreaterThan(), GreaterThanOrEqual(), Equal(), NotEqual())

def compare(operator, lhs, rhs):
    if operator.compare(lhs, rhs): return "TRUE"
    else: return "FALSE"

print >>fout, 'test ("relational vs. constants")'
print >>fout, '{'

for operator in rel_operators:
    for lhs_list in (numeric_constants, string_constants, boolean_constants):
        for lhs in lhs_list:
            for rhs_list in (numeric_constants, string_constants, boolean_constants):
                for rhs in rhs_list:
                    print >>fout, '  verify (op_strcmp (XPath_TestBoolean (state.doc, "%s %s %s", %s), "ok") == 0);' % (lhs[0], operator, rhs[0], compare(operator, lhs[1], rhs[1]))

print >>fout, '}'
print >>fout
print >>fout, 'test ("relational vs. non-constants")'
print >>fout, '{'

for operator in rel_operators:
    for lhs_list in (nodeset_expressions, number_expressions, string_expressions, boolean_expressions):
        for lhs in lhs_list:
            for rhs_list in (nodeset_expressions, number_expressions, string_expressions, boolean_expressions):
                if lhs_list is nodeset_expressions and rhs_list is boolean_expressions or lhs_list is boolean_expressions and rhs_list is nodeset_expressions:
                    continue
                for rhs in rhs_list:
                    print >>fout, '  verify (op_strcmp (XPath_TestBoolean (state.doc, "%s %s %s", %s), "ok") == 0);' % (lhs[0], operator, rhs[0], compare(operator, lhs[1], rhs[1]))

print >>fout, '}'
print >>fout
print >>fout, 'test ("equality vs. constants")'
print >>fout, '{'

for operator in eq_operators:
    for lhs in numeric_constants:
        for rhs_list in (numeric_constants, string_constants):
            for rhs in rhs_list:
                print >>fout, '  verify (op_strcmp (XPath_TestBoolean (state.doc, "%s %s %s", %s), "ok") == 0);' % (lhs[0], operator, rhs[0], compare(operator, lhs[1], rhs[1]))

print >>fout, '}'
print >>fout
print >>fout, 'test ("equality vs. non-constants")'
print >>fout, '{'

for operator in eq_operators:
    for lhs_list in (nodeset_expressions, number_expressions, string_expressions):
        for lhs in lhs_list:
            for rhs in numeric_constants:
                print >>fout, '  verify (op_strcmp (XPath_TestBoolean (state.doc, "%s %s %s", %s), "ok") == 0);' % (lhs[0], operator, rhs[0], compare(operator, lhs[1], rhs[1]))

print >>fout, '}'
print >>fout
print >>fout, 'test ("comparison nodeset vs. number")'
print >>fout, '{'

for operator in all_operators:
    for use_operator in all_operators:
        for number, lhs in [("minus-one", -1), ("zero", 0), ("one", 1)]:
            for rhs in [-1, 0, 1]:
                if len([x for x in [-2, -1, 0, 1, 2] if operator.compare(x, lhs) and use_operator.compare(x, rhs)]) > 0: expected = "TRUE"
                else: expected = "FALSE"

                print >>fout, '  verify (op_strcmp (XPath_TestBoolean (state.doc, "//nodeset/number[%s] %s %d", %s), "ok") == 0);' % (operator.predicate(number), use_operator, rhs, expected)

print >>fout, '}'
print >>fout
print >>fout, 'test ("comparison number vs. nodeset")'
print >>fout, '{'

for operator in all_operators:
    for use_operator in all_operators:
        for lhs in [-1, 0, 1]:
            for number, rhs in [("minus-one", -1), ("zero", 0), ("one", 1)]:
                if len([x for x in [-2, -1, 0, 1, 2] if operator.compare(x, rhs) and use_operator.compare(lhs, x)]) > 0: expected = "TRUE"
                else: expected = "FALSE"

                print >>fout, '  verify (op_strcmp (XPath_TestBoolean (state.doc, "%d %s //nodeset/number[%s]", %s), "ok") == 0);' % (lhs, use_operator, operator.predicate(number), expected)

print >>fout, '}'
print >>fout
print >>fout, 'test ("comparison nodeset vs. nodeset")'
print >>fout, '{'

for use_operator in rel_operators:
    for lhs_operator in all_operators:
        for rhs_operator in all_operators:
            for lhs_number, lhs in [("minus-one", -1), ("zero", 0), ("one", 1)]:
                lhs_numbers = [x for x in [-2, -1, 0, 1, 2] if lhs_operator.compare(x, lhs)]

                for rhs_number, rhs in [("minus-one", -1), ("zero", 0), ("one", 1)]:
                    rhs_numbers = [x for x in [-2, -1, 0, 1, 2] if rhs_operator.compare(x, rhs)]

                    expected = "FALSE"

                    for x in lhs_numbers:
                        for y in rhs_numbers:
                            if use_operator.compare(x, y):
                                expected = "TRUE"

                    print >>fout, '  verify (op_strcmp (XPath_TestBoolean (state.doc, "//nodeset/number[%s] %s //nodeset/number[%s]", %s), "ok") == 0);' % (lhs_operator.predicate(lhs_number), use_operator, rhs_operator.predicate(rhs_number), expected)

print >>fout, '}'
