#!/bin/sh

# css_grammar.cpp and css_grammar.h are generated from css_grammar.y using bison
# below are the commands used to generate them


# version check

bison=${BISON-'bison'}
set `$bison -V | head -n1 | sed 's/^[^0-9]*\([0-9]\+\)\.\([0-9]\+\).*$/\1 \2/g'`
if [ "$1" -lt "1" -o "$1" -eq "1" -a "$2" -lt "24" ]; then echo "The version of bison you are using has licensing issues. It cannot be used."; exit; fi

# generate the grammar state machine

$bison --defines --no-lines -p css css_grammar.y

# add third-party ifdefs to header file.

echo "#ifndef CSS_GRAMMAR_H" > css_grammar.h
echo "#define CSS_GRAMMAR_H" >> css_grammar.h
echo "" >> css_grammar.h
echo "#ifdef BISON_PARSER" >> css_grammar.h
echo "" >> css_grammar.h
cat css_grammar.tab.h | sed -e 's/[\t ]\+$//' >> css_grammar.h
echo "#endif // BISON_PARSER" >> css_grammar.h
echo "#endif // CSS_GRAMMAR_H" >> css_grammar.h

# move precompiled header file to top,
# use op_malloc/op_free instead of malloc/free,
# and add third-party ifdefs to generated source file.

echo "#include \"core/pch.h\"" > css_grammar.cpp
echo "" >> css_grammar.cpp
echo "#ifdef BISON_PARSER" >> css_grammar.cpp
echo "" >> css_grammar.cpp
grep -v "\"core/pch.h\"\|include <" css_grammar.tab.c | sed -e 's/[\t ]\+$//
s/\(^YYSTYPE yylval;$\)/\1\nyylval.integer.integer = 0;/
/^[\t ]*switch\ (yytype)[^$]*$/{
N
N
N
N
N
}
s/^[\t ]*switch\ (yytype)[^}]*}[ \t]*/  YYUSE(yytype);/
s/\(^.*yymsgbuf\[.*$\)/\1 \/\* ARRAY OK 2009-02-12 rune \*\//' | awk '
# skip the YYERROR/yyerrorlab block to silence dead code warnings
BEGIN { skip = 0 }
/goto yyerrlab1/ { skip = 1 }
skip == 0 { print }
/^yyerrlab1:/ { skip = 0 }' >> css_grammar.cpp
echo "#endif // BISON_PARSER" >> css_grammar.cpp

# remove temporary files
rm css_grammar.tab.c
rm css_grammar.tab.h

echo "Generated css_grammar.cpp"
echo "Generated css_grammar.h"
