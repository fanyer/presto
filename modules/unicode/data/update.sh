#!/bin/sh
#
# Update the data tables imported from Unicode
# Peter Krefting <peter@opera.com>

# General data tables ---
wget -O LineBreak.txt http://www.unicode.org/Public/UNIDATA/LineBreak.txt
wget -O UnicodeData.txt http://www.unicode.org/Public/UNIDATA/UnicodeData.txt
wget -O BidiMirroring.txt http://www.unicode.org/Public/UNIDATA/BidiMirroring.txt
wget -O ../testsuite/BidiTest.txt http://www.unicode.org/Public/UNIDATA/BidiTest.txt
wget -O SentenceBreakProperty.txt http://www.unicode.org/Public/UNIDATA/auxiliary/SentenceBreakProperty.txt
wget -O WordBreakProperty.txt http://www.unicode.org/Public/UNIDATA/auxiliary/WordBreakProperty.txt
wget -O CompositionExclusions.txt http://www.unicode.org/Public/UNIDATA/CompositionExclusions.txt
wget -O Scripts.txt http://www.unicode.org/Public/UNIDATA/Scripts.txt
wget -O DerivedJoiningType.txt http://www.unicode.org/Public/UNIDATA/extracted/DerivedJoiningType.txt
wget -O NamesList.txt http://www.unicode.org/Public/UNIDATA/NamesList.txt
wget -O ../testsuite/NormalizationTest.txt http://www.unicode.org/Public/UNIDATA/NormalizationTest.txt
wget -O CaseFolding.txt http://www.unicode.org/Public/UNIDATA/CaseFolding.txt
wget -O PropList.txt http://www.unicode.org/Public/UNIDATA/PropList.txt
wget -O ArabicShaping.txt http://www.unicode.org/Public/UNIDATA/ArabicShaping.txt
wget -O Blocks.txt http://www.unicode.org/Public/UNIDATA/Blocks.txt

# Thai wrap UserJS ---
wget -O thai-wrap.js http://userjs.org/scripts/download/browser/enhancements/thai-wrap.js

# Bidi reference algorithm ---
wget -O ../testsuite/bidi_reference.cpp.dist http://www.unicode.org/Public/PROGRAMS/BidiReferenceCpp/bidi.cpp
