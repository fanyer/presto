#!/usr/bin/env python
# -*- coding: utf-8 -*-

import argparse
import collections
import re
import sys

import polib


Check = collections.namedtuple('Check', ['name', 'message', 'pattern', 'ordering'])

# See http://docs.python.org/release/2.2.1/lib/typesseq-strings.html
python = Check(name='python',
               message='Mismatched python gettext variables',
               pattern=re.compile(r'(?sum)(%\(.*?\)[diouxXeEfFgGcrs%])'),
               ordering='set')

urls = Check(name='urls',
             message='Mismatched URLs',
             pattern=re.compile(r'''(?isum)
                        ( <a\ href=.*?> | <img\ src=.*?>
                        )''', re.X),    # ignore white space unless backslashed
             ordering='sorted')

html = Check(name='html',
             message='Mismatched HTML tags',
             pattern=re.compile(r'''(?isum)
                        ( <a\ href=.*?> | </a>
                        | <h\d>         | </h\d>    | <h\d\ .+?>
                        | <p>           | </p>      | <p\ .+?>
                        | <span>        | </span>   | <span\ .+?>
                        | <kbd>         | </kbd>    | <kbd\ .+?>
                        | <pre>         | </pre>
                        )''', re.X),    # ignore white space unless backslashed
             ordering='sorted')

extensions = Check(name='extensions',
                   message='Mismatched filename extensions',
                   pattern=re.compile(r'(?sum)\W(\.[a-z0-9]+)'),
                   ordering='sorted')

# In Chinese files, there are no spaces, so do not look for them
filenames = Check(name='filenames',
                  message='Mismatched filenames',
                  pattern=re.compile(r'''(?isum)
                            ( [a-z0-9]+\.css
                            | [a-z0-9]+\.dll
                            | [a-z0-9]+\.exe
                            | [a-z0-9]+\.html
                            | [a-z0-9]+\.ini
                            | [a-z0-9]+\.jar
                            | [a-z0-9]+\.js
                            | [a-z0-9]+\.msi
                            | [a-z0-9]+\.so
                            | [a-z0-9]+\.sqlite
                            | [a-z0-9]+\.txt
                            | [a-z0-9]+\.xml
                            )''', re.X),
                  ordering='sorted')

products = Check(name='products',
                 message='Mismatched product names',
                 pattern=re.compile(r'''(?sum)
                           ( My\ Opera\ Mail
                           | Opera\ Mail
                           | My\ Opera(?!\ Link)   # ignore "My Opera Link"
                           | Opera\ Link
                           | Opera\ Devices\ SDK
                           | Opera\ Dragonfly
                           | Opera\ Mobile
                           | Opera\ Mini
                           | Opera\ Portal
                           | Opera\ Software\ ASA
                           | Opera\ Turbo
                           | Opera\ TV\ Browser   # Is it a trademark name?
                           | Opera\ TV\ Store
                           | Small-Screen\ Rendering\ \(SSR\)
                           )''', re.X),   # ignore white space unless backslashed
                 ordering='set')

plural_form = {
    # http://www.gnu.org/software/gettext/manual/html_node/Plural-forms.html
    # http://www.gnu.org/software/gettext/manual/html_node/Translating-plural-forms.html
    # http://translate.sourceforge.net/wiki/l10n/pluralforms
    # Case 1: no plural form: nplurals=1; plural=0
    'bn'    : (1, ''),   # we requested and got confirmation
    'fa'    : (1, ''),
    'ja'    : (1, ''),
    'ko'    : (1, ''),
    'lo'    : (1, ''),
    'th'    : (1, ''),
    'vi'    : (1, ''),
    'zh-cn' : (1, ''),
    'zh-tw' : (1, ''),
    # Case 2: English and others: nplurals=2; plural=(n != 1)
    # Hungarian and Turkish have a plural only when the number is not explicit
    'en'    : (2, ''),
    # Case 2a: nplurals=2; plural=(n > 1)
    'fr'    : (2, 'a'),
    'fr-CA' : (2, 'a'),
    'pt-BR' : (2, 'a?'),   # (2, '') or (2, 'a'), it's complicated!
    # Case 3a: nplurals=3;
    # plural=(n%10==1 && n%100!=11 ? 0 :
    #         n != 0 ? 1 : 2)
    'lv'    : (3, 'a'),
    # Case 3b: nplurals=3;
    # plural=(n==1 ? 0 :
    #        (n==0 || (n%100 > 0 && n%100 < 20)) ? 1 : 2)
    'ro'    : (3, 'b'),
    # Case 3c: nplurals=3;
    # plural=(n%10==1 && n%100!=11 ? 0 :
    #         n%10>=2 && (n%100<10 or n%100>=20) ? 1 : 2)
    'lt'    : (3, 'c'),
    # Case 3d: nplurals=3;
    # plural=(n%10==1 && n%100!=11 ? 0 :
    #         n%10>=2 && n%10<=4 && (n%100<10 || n%100>=20) ? 1 : 2)
    'bs'    : (3, 'd'),
    'hr'    : (3, 'd'),
    'me'    : (3, 'd'),
    'ru'    : (3, 'd'),
    'sr'    : (3, 'd'),
    'uk'    : (3, 'd'),
    # Case 3e: nplurals=3;
    # plural=(n==1) ? 0 :
    #        (n>=2 && n<=4) ? 1 : 2
    'be'    : (3, 'e'),
    'cs'    : (3, 'e'),
    'sk'    : (3, 'e'),
    # Case 3f: nplurals=3;
    # plural=(n==1 ? 0 :
    #         n%10>=2 && n%10<=4 && (n%100<10 || n%100>=20) ? 1 : 2)
    'pl'    : (3, 'f'),
    # Case 4a: Slovenian: nplurals=4;
    # plural=(n%100==1 ? 1 :
    #         n%100==2 ? 2 :
    #         n%100==3 || n%100==4 ? 3 : 0)
    'sl'    : (4, 'a'),
    # Case 4b: Scottish Gaelic: nplurals=4;
    # plural=(n==1 || n==11) ? 0 :
    #        (n==2 || n==12) ? 1 :
    #        (n > 2 && n < 20) ? 2 : 3
    'gd'    : (4, 'b'),
    # Case 5: Irish Gaelic: nplurals=5;
    # plural=n==1 ? 0 :
    #        n==2 ? 1 :
    #        n<7 ? 2 :
    #        n<11 ? 3 : 4
    'ga'    : (5, ''),
    # Case 6: Arabic: nplurals=6;
    # plural= n==0 ? 0 :
    #         n==1 ? 1 :
    #         n==2 ? 2 :
    #         n%100>=3 && n%100<=10 ? 3 :
    #         n%100>=11 ? 4 : 5
    'ar'    : (6, '')
}


def momail_pattern(language):
    """
    Function that returns a momail pattern for each language
    """
    if language in plural_form:
        nplurals = plural_form[language][0]
        variant  = plural_form[language][1]
    else:
        nplurals = plural_form['en'][0]
        variant  = plural_form['en'][1]

    repeat_pattern = ',.*?' * nplurals
    zero_pattern = ',.*?' * (nplurals + 1)

    return re.compile(r'''(?sum)   # flags
                          ( \[_\d\]
                          | \[numf,_\d\]
                          | \[ \* %(nplurals)d %(variant)s , _\d %(repeat_pattern)s \]
                          | \[ \* %(nplurals)d %(variant)s , _\d %(zero_pattern)s \]
                          )''' % {'nplurals': nplurals, 'variant': variant,
                                  'repeat_pattern': repeat_pattern,
                                  'zero_pattern': zero_pattern}, re.X)


def color(this_color, string):
    """
    ANSI escape sequences produce color output in the terminal
    16 colors:   red = '31'      green = '32'      yellow = '33'
    256 colors   red = '38;05;1' green = '38;05;2' yellow = '38;05;3'
    """
    return "\x1b[" + this_color + "m" + string + "\x1b[0m"


def find_mismatched(pofile, args):
    """
    Check for correspondence of msgid and msgstr:
        * File name extensions: .html, .js
        * File names: operaprefs.ini
        * HTML tags
        * python format strings
        * My Opera Mail strings
    """
    language = None
    language_entry = pofile.find('<LanguageCode>')
    if not language_entry:
        language_entry = pofile.find('en')
    if language_entry:
        language = language_entry.msgstr

    if not args.extensions and not args.filenames and not args.html and \
        not args.momail and not args.products and not args.python and \
        not args.urls and not args.all:
        args.html = args.python = True

    if args.all:
        args.extensions = args.filenames = args.html = args.momail = args.products = args.python = True

    if args.msgctxt:
        string = args.msgctxt
        entry = pofile.find(string, by='msgctxt')
        if entry:
            if entry.flags and 'fuzzy' in entry.flags:
                print color('33', '%s: %s %s' % (pofile.fpath, string, 'fuzzy'))
            elif not entry.translated():
                print color('33', '%s: %s %s' % (pofile.fpath, string, 'untranslated'))
            elif entry.msgid == entry.msgstr and language not in ['en', 'en-GB']:
                print color('33', '%s: %s %s' % (pofile.fpath, string, 'msgid = msgstr'))
        else:
            print color('31', '%s: %s %s' % (pofile.fpath, string, 'missing'))

    for check in [extensions, filenames, html, products, python, urls]:
        if getattr(args, check.name):
            mismatched = []
            if args.msgctxt:
                string = args.msgctxt
                entry = pofile.find(string, by='msgctxt')
                if entry and entry.mismatched(check.pattern, check.ordering):
                    mismatched.append(entry)
            elif args.strings:
                try:
                    for line in open(args.strings).readlines():
                        string = line.strip(' \n')
                        entry = pofile.find(string, by='msgctxt')
                        if entry and entry.mismatched(check.pattern, check.ordering):
                            mismatched.append(entry)
                except IOError:
                    sys.exit('File %s is not a list of strings' % args.strings)
            else:
                mismatched = pofile.mismatched_entries(check.pattern, check.ordering)
            for entry in mismatched:
                print color('33', '%s: %s' % (pofile.fpath, check.message))
                print color('32', 'msgid:  ' + str(re.findall(check.pattern, entry.msgid)))
                print color('31', 'msgstr: ' + str(re.findall(check.pattern, entry.msgstr)))
                print entry

    if args.momail:
        momail_general_pattern = re.compile(r'(?sum)\[.*?_\d.*?\]')
        if language:
            momail_specific_pattern = momail_pattern(language)
        else:
            momail_specific_pattern = momail_general_pattern
        for entry in pofile.findall(momail_general_pattern, scope='momail'):
            if not entry.translated():
                continue
            print_entry = False
            #momail_variables = r'(_\d|%n|n%)'
            momail_variables = re.compile(r'''(?sum) (\[\*) \d\w?, (_\d) .*? (%n|n%) .*? (\])''', re.X)
            if entry.mismatched(momail_variables, ordering='set'):
                print_entry = True
                print color('33', '%s: Mismatched My Opera Mail variables' % pofile.fpath)
                print color('32', 'msgid:  ' + str(sorted(set(re.findall(momail_variables, entry.msgid)))))
                print color('31', 'msgstr: ' + str(sorted(set(re.findall(momail_variables, entry.msgstr)))))
            general_set = set(re.findall(momail_general_pattern, entry.msgstr))
            specific_set = set(re.findall(momail_specific_pattern, entry.msgstr))
            if general_set != specific_set:
                print_entry = True
                # https://volunteers.opera.com/node/4794
                if language in plural_form:
                    print color('33', '%s: My Opera Mail plural format type "%s"' % (pofile.fpath,
                          '*' + str(plural_form[language][0]) + plural_form[language][1]))
                else:
                    print color('33', '%s: My Opera Mail plural format type "%s"' % (pofile.fpath,
                          '*' + str(plural_form['en'][0]) + plural_form['en'][1]))
                for element in sorted(general_set - specific_set):
                    print color('31', 'FIX: %s' % element)
            if print_entry:
                print entry
        return None


def main():

    parser = argparse.ArgumentParser(description='Check for mismatches in msgid and msgstr with respect to numerous formats: including file names, HTML tags, python gettext variables, and My Opera Mail variables. When no optional arguments are given, the default behavior is to check for HTML and python gettext errors.')

    parser.add_argument('po_file', nargs='+', help='PO file whose msgstr should be checked, or a newline-separated list of PO files')

    parser.add_argument("--all", help="severe checking, with all arguments", action="store_true")
    parser.add_argument("--extensions", help="check file name extensions in strings", action="store_true")
    parser.add_argument("--filenames", help="check file names in strings", action="store_true")
    parser.add_argument("--html", help="check HTML tags in strings", action="store_true")
    parser.add_argument("--urls", help="check URLs in strings", action="store_true")
    parser.add_argument("--momail", help="check My Opera Mail variables in strings", action="store_true")
    parser.add_argument("--products", help="check product names in strings", action="store_true")
    parser.add_argument("--python", help="check python gettext variables in strings", action="store_true")

    parser.add_argument("--msgctxt", help="check a single string ID, not the whole PO file", action="store")
    parser.add_argument("--strings", help="check a list of string IDs, not the whole PO file", action="store")

    args = parser.parse_args()

    for name in args.po_file:
        try:
            find_mismatched(polib.pofile(name), args)
        except:
            for line in open(name).readlines():
                try:
                    path = line.strip(' \n')
                    find_mismatched(polib.pofile(path), args)
                except IOError:
                    sys.exit('File %s is not a PO file or a list of PO files' % name)

    return None


if __name__ == "__main__":
    main()
