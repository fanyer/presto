#!/usr/bin/python
# -*- coding: utf-8 -*-

import argparse

import polib


def color(this_color, string):
    """
    ANSI escape sequences produce color output in the terminal
    16 colors:   red = '31'      green = '32'      yellow = '33'
    256 colors   red = '38;05;1' green = '38;05;2' yellow = '38;05;3'
    """
    return "\x1b[" + this_color + "m" + string + "\x1b[0m"


def remove_fuzzy(entry):
    """
    Post-translation clean-up
    """
    if 'fuzzy' in entry.flags:
        print entry.msgctxt + color('33', ' fuzzy')
        index = entry.flags.index('fuzzy')
        del entry.flags[index]
    if entry.previous_msgctxt:
        entry.previous_msgctxt = None
    if entry.previous_msgid:
        entry.previous_msgid = None
    return entry


def insert_translations(args):
    """
    Insert strings from a source PO into the target PO file
    """
    source = polib.pofile(args.source_po)
    target = polib.pofile(args.target_po, wrapwidth=0)

    for line in open(args.strings).readlines():
        string = line.strip(' \n')
        source_entry = source.find(string, by='msgctxt')
        target_entry = target.find(string, by='msgctxt')
        if source_entry and target_entry:
            if not target_entry.msgstr:
                print string + color('31', ' untranslated')
            target_entry.msgstr = source_entry.msgstr
            target_entry = remove_fuzzy(target_entry)
            continue
        source_entry = source.find(string, by='msgid')
        target_entry = target.find(string, by='msgid')
        if source_entry and target_entry:
            if not target_entry.msgstr:
                print string + color('31', ' untranslated')
            target_entry.msgstr = source_entry.msgstr
            target_entry = remove_fuzzy(target_entry)
            continue

    target.save()

    return None


def main():

    parser = argparse.ArgumentParser(description=''
            'Insert strings from a source PO into the target PO file')

    parser.add_argument('strings', help='newline-separated strings or string IDs')
    parser.add_argument('source_po', help='file that contains the source strings')
    parser.add_argument('target_po', help='file that receives the source strings')

    args = parser.parse_args()

    insert_translations(args)

    return None


if __name__ == "__main__":
    main()
