#!/bin/sh
#
# Update the data tables imported from Unicode
# Peter Krefting <peter@opera.com>

# Mapping tables ---
# ISO 8859
for iso in 1 2 3 4 5 6 7 8 9 10 11 13 14 15 16; do
  wget -O 8859-$iso.txt http://www.unicode.org/Public/MAPPINGS/ISO8859/8859-$iso.TXT
done

# MS-Windows codepages
for cp in 874 932 936 949 950 1250 1251 1252 1253 1254 1255 1256 1257 1258; do
  wget -O cp$cp.txt http://www.unicode.org/Public/MAPPINGS/VENDORS/MICSFT/WINDOWS/CP$cp.TXT
done

# MS-DOS codepages
wget -O cp866.txt http://www.unicode.org/Public/MAPPINGS/VENDORS/MICSFT/PC/CP866.TXT

# Apple encodings
wget -O mac-ce.txt http://www.unicode.org/Public/MAPPINGS/VENDORS/APPLE/CENTEURO.TXT
wget -O mac-cyrillic.txt http://www.unicode.org/Public/MAPPINGS/VENDORS/APPLE/CYRILLIC.TXT
wget -O mac-greek.txt http://www.unicode.org/Public/MAPPINGS/VENDORS/APPLE/GREEK.TXT
wget -O mac-roman.txt http://www.unicode.org/Public/MAPPINGS/VENDORS/APPLE/ROMAN.TXT
wget -O mac-turkish.txt http://www.unicode.org/Public/MAPPINGS/VENDORS/APPLE/TURKISH.TXT

# Unicode block table related to font-switching.
# Use build_uniblocks.py to convert this file into uniblocks.txt
wget -O os2.html http://www.microsoft.com/typography/otspec/os2.htm
