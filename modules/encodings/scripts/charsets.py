#!/usr/bin/env python
"""Generate the alias and MIB tables.

This script generates two tables from the IANA list of MIME character sets.

The alias table contains all the IANA listed aliases for the various
encodings, mapping them to the canonical MIME name. This is used to
normalize the encoding tag. After processing the alias list in the IANA
list, we add some additional aliases that are commonly found "in the wild"
on the web, see the list opera_16bit_extras below.

The MIB table contains mappings from the MIB enums, as described by RFC
3808. This is used in some binary markup representations to avoid
transmitting the full IANA tag and to allow for a fast lookup.

When generating the list, we also merge some encodings that we consider
to be identical, but that are listed separatenly by IANA. This list
contains some subset encodings, and also cases where the DBCS part of a
MBCS encoding has been listed separately by IANA. The reason for the
merges should be documented in the list.

Run getupdate.sh (in this directory) to get the latest IANA table and
update the source tree.
"""

import string

# --- Utilities

def canonize(name):
    return name.strip().lower()

# --- Opera-specific

# contains the preferred names of the charsets Opera supports

opera_supports = {"us-ascii" : 1,
                  "invariant" : 1,
                  "iso_646.basic:1983" : 1,
                  "iso-8859-1" : 1,
                  "iso-8859-2" : 1,
                  "iso-8859-3" : 1,
                  "iso-8859-4" : 1,
                  "iso-8859-5" : 1,
                  "iso-8859-6-e" : 1,
                  "iso-8859-6-i" : 1,
                  "iso-8859-6" : 1,
                  "iso-8859-7" : 1,
                  "iso-8859-8" : 1,
                  "iso-8859-8-e" : 1,
                  "iso-8859-8-i" : 1,
                  "iso-8859-9" : 1,
                  "iso-8859-10" : 1,
                  "iso-8859-11" : 1,
                  "iso-8859-13" : 1,
                  "iso-8859-14" : 1,
                  "iso-8859-15" : 1,
                  "iso-8859-16" : 1,
                  "koi8-r" : 1,
                  "utf-7" : 1,
                  "utf-8" : 1,
                  "utf-16" : 1,
                  "utf-16le" : 1,
                  "utf-16be" : 1,
                  "windows-1250" : 1,
                  "windows-1251" : 1,
                  "windows-1252" : 1,
                  "windows-1253" : 1,
                  "windows-1254" : 1,
                  "windows-1255" : 1,
                  "windows-1256" : 1,
                  "windows-1257" : 1,
                  "windows-1258" : 1,
                  "ibm866" : 1,
                  "shift_jis" : 1,
                  "iso-2022-jp" : 1,
                  "iso-2022-jp-1" : 1,
                  "iso-2022-cn" : 1,
                  "iso-2022-kr" : 1,
                  "big5" : 1,
                  "big5-hkscs" : 1,
                  "euc-jp" : 1,
                  "gb2312" : 1,
                  "gbk" : 1,
                  "gb18030" : 1,
                  "viscii" : 1,
                  "euc-kr" : 1,
                  "koi8-u" : 1,
                  "ks_c_5601-1987" : 1,
                  "hz-gb-2312" : 1,
                  "euc-tw" : 1,
                  "gb_2312-80" : 1,
                  "tis-620" : 1,
                  "windows-874" : 1,
                  "windows-31j" : 1,
                  "unicode-1-1-utf-7" : 1,
                  "unicode-1-1" : 1,
                  "iso-10646-ucs-basic" : 1,
                  "iso-10646-unicode-latin1" : 1,
                  "iso-10646-j-1"  : 1,
                  "iso-10646-ucs-2": 1,
                  "macintosh": 1,
                  "cp51932" : 1,
                  "cp50220" : 1,
                 }

# --- extra mappings in addition to IANA-specified (16-bit builds)

# contains charset aliases supported in the 16-bit build

opera_16bit_extras = [("cp1250",            "windows-1250"),
                      ("cp1251",            "windows-1251"),
                      ("cp1252",            "windows-1252"),
                      ("cp1253",            "windows-1253"),
                      ("cp1254",            "windows-1254"),
                      ("cp1255",            "windows-1255"),
                      ("cp1256",            "windows-1256"),
                      ("cp1257",            "windows-1257"),
                      ("cp1258",            "windows-1258"),
                      ("microsoft-cp1250",  "windows-1250"),
                      ("microsoft-cp1251",  "windows-1251"),
                      ("microsoft-cp1252",  "windows-1252"),
                      ("microsoft-cp1253",  "windows-1253"),
                      ("microsoft-cp1254",  "windows-1254"),
                      ("microsoft-cp1255",  "windows-1255"),
                      ("microsoft-cp1256",  "windows-1256"),
                      ("microsoft-cp1257",  "windows-1257"),
                      ("microsoft-cp1258",  "windows-1258"),
                      ("x-cp1250",          "windows-1250"),
                      ("x-cp1251",          "windows-1251"),
                      ("x-cp1252",          "windows-1252"),
                      ("x-cp1253",          "windows-1253"),
                      ("x-cp1254",          "windows-1254"),
                      ("x-cp1255",          "windows-1255"),
                      ("x-cp1256",          "windows-1256"),
                      ("x-cp1257",          "windows-1257"),
                      ("x-cp1258",          "windows-1258"),
                      ("iso8859-1",         "iso-8859-1"),
                      ("iso8859-2",         "iso-8859-2"),
                      ("iso8859-3",         "iso-8859-3"),
                      ("iso8859-4",         "iso-8859-4"),
                      ("iso8859-5",         "iso-8859-5"),
                      ("iso8859-6",         "iso-8859-6"),
                      ("iso8859-7",         "iso-8859-7"),
                      ("iso8859-8",         "iso-8859-8"),
                      ("iso8859-9",         "iso-8859-9"),
                      ("iso8859-10",        "iso-8859-10"),
                      ("iso8859-11",        "iso-8859-11"),
                      ("iso8859-12",        "iso-8859-12"),
                      ("iso8859-13",        "iso-8859-13"),
                      ("iso8859-14",        "iso-8859-14"),
                      ("iso8859-15",        "iso-8859-15"),
                      ("iso8859-16",        "iso-8859-16"),
                      ("iso88591",          "iso-8859-1"),  # from FF and old Opera
                      ("iso88592",          "iso-8859-2"),  # from FF and old Opera
                      ("iso88593",          "iso-8859-3"),  # from FF and old Opera
                      ("iso88594",          "iso-8859-4"),  # from FF and old Opera
                      ("iso88595",          "iso-8859-5"),  # from FF and old Opera
                      ("iso88596",          "iso-8859-6"),  # from FF and old Opera
                      ("iso88597",          "iso-8859-7"),  # from FF and old Opera
                      ("iso88598",          "iso-8859-8"),  # from FF and old Opera
                      ("iso88599",          "iso-8859-9"),  # from FF and old Opera
                      ("iso885910",         "iso-8859-10"), # from FF and old Opera
                      ("iso885911",         "iso-8859-11"), # from FF and old Opera
                      ("iso885912",         "iso-8859-12"), # from FF and old Opera
                      ("iso885913",         "iso-8859-13"), # from FF and old Opera
                      ("iso885914",         "iso-8859-14"), # from FF and old Opera
                      ("iso885915",         "iso-8859-15"), # from FF and old Opera
                      ("iso885916",         "iso-8859-16"), # from FF and old Opera
                      ("cp932",             "shift_jis"),
                      ("ms932",             "shift_jis"),
                      ("cn-big5",           "big5"),
                      ("x-x-big5",          "big5"),
                      ("cn-gb",             "gb2312"),
                      ("euc-cn",            "gb2312"),
                      ("sjis",              "shift_jis"),
                      ("shift-jis",         "shift_jis"),
                      ("x-sjis",            "shift_jis"),
                      ("iso-2022-jp-1",     "iso-2022-jp-1"),
                      ("x-euc-jp",          "euc-jp"),
                      ("cseucjpkdfmtjapanese", "euc-jp"), # CORE-48024; from FF
                      ("euc-tw",            "euc-tw"),
                      ("windows-949",       "euc-kr"),
                      ("ksc5601",           "euc-kr"),
                      ("x-gbk",             "gbk"),
                      ("tis-620-2533",      "iso-8859-11"), # Really: TIS-620
                      ("visual",            "iso-8859-8"),
                      ("utf8",              "utf-8"),
                      ("x-mac-roman",       "macintosh"),
                      ("x-mac-ce",          "x-mac-ce"),
                      ("x-mac-cyrillic",    "x-mac-cyrillic"),
                      ("x-mac-ukrainian",	"x-mac-cyrillic"),
                      ("x-mac-greek",       "x-mac-greek"),
                      ("x-mac-turkish",     "x-mac-turkish"),
                      ("x-user-defined",	"x-user-defined")]

# charsets considered distinct by IANA are mapped onto other charsets
# that they are considered to be the same as by Opera

iana_aliases = {"ks_c_5601-1987" : "euc-kr", # DBCS <-> MBCS
                "gb_2312-80"     : "gbk", # Most GB2312 content is the superset GBK
                "tis-620"        : "iso-8859-11", # Join subsets/supersets 
                "windows-874"    : "iso-8859-11", # Join subsets/supersets
                "iso-8859-6-e"   : "iso-8859-6", # Explicit direction => visual
                "iso-8859-8-e"   : "iso-8859-8", # Explicit direction => visual
                "windows-31j"    : "shift_jis", # Old subset
                "iso-8859-9"     : "windows-1254", # ISO 8859-9 uses the windows-1254 table
                "cp51932"        : "euc-jp", # EUC-JP minus (the uncommon) JIS X 0212
                "cp50220"        : "iso-2022-jp", # Compatibility name for iso-2022-jp implementations

                # Following mappings are lies, but will work fine for
                # *input* only.
                "unicode-1-1-utf-7" : "utf-7", # Not *completely* true
                "unicode-1-1"    : "utf-16", # Not *completely* true either
                "iso-10646-ucs-basic" : "utf-16", # ASCII subset only
                "iso-10646-unicode-latin1" : "utf-16", # Latin subset only
                "iso-10646-j-1"  : "utf-16", # Japanese subset only
                "iso-10646-ucs-2": "utf-16", # UTF-16 minus surrogates
                "invariant" : "us-ascii", # Subset of ASCII
                "iso_646.basic:1983" : "us-ascii", # Subset of ASCII
               }

# remapping of canonical names

canonical_remapping = {"gb2312" : "gbk"} # GB2312 is a subset of GBK

def make_opera_table(filename = "../utility/aliases.inl"):
    pairs = []
    for (name, aliases) in ids.items():
        if opera_supports.has_key(name):
            # merge charsets split by IANA
            if iana_aliases.has_key(name):
                pairs.append((name, iana_aliases[name]))
                name = iana_aliases[name]

            for alias in aliases:
                pairs.append((string.lower(alias), name))
            pairs.append((name, name))

    pairs += opera_16bit_extras;

    newpairs = []
    for (alias, name) in pairs:
        newpairs.append((canonize(alias), name))

    newpairs.sort()
    pairs = newpairs

    # First count the number of unique entries
    entries = 0
    seen = {}
    seen["unicode11utf7"] = 1 # Hack to not write this one explicitly
    seen["unicode11"] = 1 # Removed due to bug#157809

    for (alias, name) in pairs:
        if not seen.has_key(alias):
            seen[alias] = 1
            entries += 1

    # Start writing the file
    outf = open(filename, "w")
    outf.write("/** @file aliases.inl\n")
    outf.write("  * This file is auto-generated by modules/encodings/scripts/charsets.py\n")
    outf.write("  * DO NOT EDIT THIS FILE MANUALLY.\n")
    outf.write("  */\n\n")
    outf.write("#ifdef IANA_DATA_TABLES\n")
    outf.write("#define CHARSET_ALIASES_COUNT %d\n\n" % entries)
    outf.write("CHARSET_ALIAS_DECLARE\n")
    outf.write("{\n")
    outf.write("\tCHARSET_ALIAS_INIT_START\n")

    # Now print them
    entries = 0
    seen = {}
    seen["unicode11utf7"] = 1 # Hack to not write this one explicitly
    seen["unicode11"] = 1 # Removed due to bug#157809

    for (alias, name) in pairs:
        if not seen.has_key(alias):
            if entries:
                outf.write(",\n")
            outf.write('\tCHARSET_ALIAS("%s", "%s")' % (alias,
                                              canonical_remapping.get(name, name)))
            seen[alias] = 1
            entries += 1

    outf.write("\n")
    outf.write("\tCHARSET_ALIAS_INIT_END\n")
    outf.write("};\n")
    outf.write("#endif // IANA_DATA_TABLES\n")
    outf.close()

def make_mib_table(filename = "../utility/mib.inl"):
    pairs = []
    for (name, mib) in mibs.items():
        if opera_supports.has_key(name):
            pairs.append((mib, name))
    pairs.sort()

    outf = open(filename, "w")
    outf.write("/** @file mib.inl\n")
    outf.write("  * This file is auto-generated by modules/encodings/scripts/charsets.py\n")
    outf.write("  * DO NOT EDIT THIS FILE MANUALLY.\n")
    outf.write("  */\n\n")
    outf.write("#ifdef IANA_DATA_TABLES\n")
    outf.write("#define MIB_ENTRIES_COUNT %d\n\n" % len(pairs))
    outf.write("MIB_CHARSET_DECLARE\n")
    outf.write("{\n")
    outf.write("\tMIB_CHARSET_INIT_START\n")
    entries = 0
    for (mib, name) in pairs:
        if entries:
            outf.write(",\n")
        entries += 1
        if iana_aliases.has_key(name):
            outf.write('\tMIB_CHARSET( %4hd, "%s" ) /* %s */' %
                       (mib, iana_aliases[name], name))
        else:
            outf.write('\tMIB_CHARSET( %4hd, "%s" )' % (mib, name))

    outf.write("\n")
    outf.write("\tMIB_CHARSET_INIT_END\n")
    outf.write("};\n")
    outf.write("#endif // IANA_DATA_TABLES\n")
    outf.close

# --- Generic

def parse_charsets(file = "data/character-sets.txt"):
    identifiers = {}
    mibenums = {}
    inf = open(file)

    line = inf.readline()
    while line[ : 5] != "Name:": # Skip header
        line = inf.readline()

    while line != "\n":
        name = string.lower(string.split(line)[1])
        aliases = []
        mib = None

        while line != "" and line != "\n":
            if line[ : 7] == "Alias: " and line[7 : 11] != "None":
                if string.find(line, "(preferred MIME name)") != -1:
                    aliases.append(name)
                    name = string.lower(string.split(line)[1])
                elif line[7 : -1] != "None" and line[7 : -1] != "":
                    aliases.append(line[7 : -1])
            elif line[ : 9] == "MIBenum: ":
                mib = int(line[9:])

            line = inf.readline()

        identifiers[name] = aliases
        if mib is not None:
            mibenums[name] = mib
        line = inf.readline()

    inf.close()

    return identifiers, mibenums

# ---

(ids, mibs) = parse_charsets()
make_opera_table()
make_mib_table()
