#!/usr/bin/env python
# -*- coding: utf-8 -*-

import copy
import difflib
import re

from lxml import etree
import polib

import dict_en


def get_words(string, fuzzy_dict=None, thesaurus=None, XML=True, PYTHON= True, MOMAIL=True):
    """
    Remove extraneous information from string: punctuation, tags, data

    XML == true: change
           '<a href="chat.html">Chat</a>' --> 'Chat'
    MOMAIL == true: change
           '[*2,_1,Contact updated,Contacts updated]' --> 'Contact updated'

    The fuzzy dictionary then converts words to their root value,
    discarding inflections such as plurals and verb forms.
    """
    if XML:
        try:
            text = ur'<text>' + string + ur'</text>'
            text_element = etree.fromstring(text)
            string = etree.tostring(text_element, encoding=unicode, method='text')
        except:
            pass

    if PYTHON:
        python_pattern = re.compile(r'(?sum)%\(.*?\)[diouxXeEfFgGcrs%]')
        string = python_pattern.sub('', string)

    if MOMAIL:
        momail_pattern = re.compile(r'(?sum)\[\*.*?,_\d,(.*?),.*?\]')
        first_group = lambda match_object: match_object.group(1)
        string = momail_pattern.sub(first_group, string)

    word_pattern = re.compile(r'(?sum)(\w+([-\.]\w+)*)')
    word_list = [group[0] for group in word_pattern.findall(string)]

    if fuzzy_dict:
        fuzzy_list = []
        for word in word_list:
            lower = word.lower()
            if lower in fuzzy_dict:
                fuzzy_list.append(fuzzy_dict[lower][0])
            else:
                fuzzy_list.append(lower)
        return fuzzy_list
    else:
        return word_list


def get_close_matches(string, pofile, by='msgid', n=None, cutoff=0.6):
    """
    Mirrors the function of the same name in difflib
    Unlike Levenshtein: matches at word level, not at character level
    Our words are not misspelled, and we mean what we write!
    Good at finding strings which are slightly wrong or uncertain
    """
    matches = []
    word_list = get_words(string, fuzzy_dict=dict_en.fuzzy, XML=True, PYTHON=True, MOMAIL=True)
    sm = difflib.SequenceMatcher()
    sm.set_seq2(word_list)
    for entry in pofile.translated_entries():
        msg = getattr(entry, by)
        entry_list = get_words(msg, fuzzy_dict=dict_en.fuzzy, XML=True, PYTHON=True, MOMAIL=True)
        sm.set_seq1(entry_list)
        ratio = sm.ratio()
        if ratio < cutoff:
            continue
        else:
            matches.append((ratio, entry))

    def ordering(match):
        msg = getattr(match[1], by)
        sm_actual = difflib.SequenceMatcher()
        sm_actual.set_seqs(msg, string)
        return (match[0], sm_actual.ratio())

    matches.sort(reverse=True, key=ordering)
    entries = [match[1] for match in matches]
    if not n:
        n = len(entries)
    return entries[0:n]


def serial_search(string, pofile, by='msgid', n=None):
    """
    Looks for keyword1...keywordN in that order, with anything in between
    Good at finding strings with known keywords
    """
    word_list = get_words(string, fuzzy_dict=dict_en.fuzzy, XML=False, PYTHON=False, MOMAIL=False)
    pattern = ur'(?isum)' + ur'\b' + ur'.*?\b'.join(word_list)
    entries = [entry for entry in pofile.findall(pattern, by) if entry.translated()]

    def ordering(match):
        msg = getattr(match, by)
        fuzzy_msg = get_words(msg, fuzzy_dict=dict_en.fuzzy, XML=False, PYTHON=False, MOMAIL=False)
        return (len(fuzzy_msg), msg)

    entries.sort(key=ordering)
    if not n:
        n = len(entries)
    return entries[0:n]


def search(string, pofile, by='msgid', n=None, cutoff=0.6):
    """
    A search engine for PO files, combining techniques
    """
    dictionary = english_suffixes(word_frequency(pofile, by))
    entries = get_close_matches(string, pofile, by, n, cutoff)
    matches = serial_search(string, pofile, by, n)
    combined = set(entries) | set(matches)

    def ordering(match):
        msg = getattr(match, by)
        fuzzy_msg = get_words(msg, fuzzy_dict=dict_en.fuzzy, XML=False, PYTHON=False, MOMAIL=False)
        return (len(fuzzy_msg), msg)

    combined = sorted(combined, key=ordering)
    if not n:
        n = len(combined)
    return combined[0:n]


def word_frequency(pofile, by='msgid', lower=True, group=1):
    """
    Create a dictionary of words in the PO file
    key=word, value=number of occurrences
    """
    if lower:
        strings = [getattr(entry, by).lower() for entry in pofile if entry.translated()]
    else:
        strings = [getattr(entry, by) for entry in pofile if entry.translated()]

    dictionary = dict()
    for string in strings:
        words = get_words(string, fuzzy_dict=None, XML=True, PYTHON=True, MOMAIL=True)
        for n in range(len(words)+1-group):
            if group == 1:
                key = words[n]
            else:
                key = tuple(words[n:n+group])
            if key in dictionary:
                dictionary[key] += 1
            else:
                dictionary[key] = 1

    return dictionary


def suffix_dictionary(dictionary, suffix, dropped=None, root_size=3):
    """
    Return the root form of a word: {'used': 'use', 'Pages': 'page'}
    """
    n = len(suffix)
    roots = dict()
    for word in dictionary:
        ending = word[-n:]
        root = word[:-n]
        if dropped:
            stem = root + dropped
        else:
            stem = root
        if stem in dictionary and ending == suffix and len(root) >= root_size:
            roots[word] = root
    return roots


def better_suffixes(dictionary):
    """
    Return the root form of a word: {'used': 'use', 'Pages': 'page'}
    """
    suffix = dict()
    unknown = dict()
    for word in dictionary:
        ending = word[-3:]
        root = word[:-3]
        if ending == 'ing':
            if root not in dictionary:
                if root[-2] == root[-1]:   # blogging
                    if root + 'ing' in dictionary:
                        suffix[root + 'ing'] = root[:-1]
                    if root + 'ed' in dictionary:
                        suffix[root + 'ed'] = root[:-1]
                    if root[:-1] + 's' in dictionary:
                        suffix[root[:-1] + 's'] = root[:-1]
                elif root + 'e' in dictionary:   # changing
                    if root + 'e' in dictionary:
                        suffix[root + 'e'] = root
                    if root + 'ing' in dictionary:
                        suffix[root + 'ing'] = root
                    if root + 'ed' in dictionary:
                        suffix[root + 'ed'] = root
                    if root + 'es' in dictionary:
                        suffix[root + 'es'] = root
                else:
                    unknown[word] = root
            elif root in dictionary:
                if root[-1] == 'y':   # applying
                    root = root[:-1]
                    if root + 'ying' in dictionary:
                        suffix[root + 'ying'] = root
                    if root + 'ied' in dictionary:
                        suffix[root + 'ied'] = root
                    if root + 'ies' in dictionary:
                        suffix[root + 'ies'] = root
                else:
                    if root + 'ing' in dictionary:
                     suffix[root + 'ing'] = root
                    if root + 'ed' in dictionary:
                        suffix[root + 'ed'] = root
                    if root + 's' in dictionary:
                        suffix[root + 's'] = root


        ending = word[-2:]
        root = word[:-2]
        if ending == 'ed':
            if root not in dictionary:
                if root[-2] == root[-1]:   # blogged
                    if root + 'ing' in dictionary:
                        suffix[root + 'ing'] = root[:-1]
                    if root + 'ed' in dictionary:
                        suffix[root + 'ed'] = root[:-1]
                    if root[:-1] + 's' in dictionary:
                        suffix[root[:-1] + 's'] = root[:-1]
                elif root + 'e' in dictionary:   # changed
                    if root + 'e' in dictionary:
                        suffix[root + 'e'] = root
                    if root + 'ing' in dictionary:
                        suffix[root + 'ing'] = root
                    if root + 'ed' in dictionary:
                        suffix[root + 'ed'] = root
                    if root + 'es' in dictionary:
                        suffix[root + 'es'] = root
                else:
                    unknown[word] = root

    return (suffix, unknown)


def english_suffixes(dictionary):
    """
    Return the root form of a word: {'used': 'use', 'Pages': 'page'}
    """
    suffix = dict()

    # adverb
    ly = suffix_dictionary(dictionary, suffix='ly', root_size=4)

    # verb
    ed = suffix_dictionary(dictionary, suffix='ed', root_size=3)
    ed_e = suffix_dictionary(dictionary, suffix='ed', dropped='e', root_size=3)
    ied_y = suffix_dictionary(dictionary, suffix='ied', dropped='y', root_size=3)

    truncated_y = set(ied_y.values())

    ing = suffix_dictionary(dictionary, suffix='ing', root_size=2)
    ing_e = suffix_dictionary(dictionary, suffix='ing', dropped='e', root_size=2)
    ying_y = suffix_dictionary(dictionary, suffix='ying', dropped='y', root_size=2)

    # Short roots: 'being', 'doing', 'going', 'using'
    exceptions_ing = ['biting', 'bring', 'using', 'willing']
    for exception in exceptions_ing:
        if exception in ing:
            del ing[exception]

    apos = suffix_dictionary(dictionary, suffix="'s", root_size=3)

    # Could be noun or verb! Need to separate them out
    s = suffix_dictionary(dictionary, suffix='s', root_size=3)

    exceptions_s = ['news']
    for exception in exceptions_s:
        if exception in s:
            del s[exception]

    # Notice that es_e is a subset of s
    # We are interested in cases where es_e key is a verb
    es = suffix_dictionary(dictionary, suffix='es', root_size=3)
    es_e = suffix_dictionary(dictionary, suffix='es', dropped='e', root_size=3)
    ies_y = suffix_dictionary(dictionary, suffix='ies', dropped='y', root_size=3)

    # Short roots: 'does', 'ones', 'uses'
    exceptions_es = ['notes', 'themes']
    for exception in exceptions_es:
        if exception in es:
            del es[exception]

    # Do reduce 'copying' to 'cop' if 'copies' or 'copied' exists
    truncated_y = set(ied_y.values()) | set(ies_y.values())
    ying_y = dict((k, v) for k,v in ying_y.items() if v in truncated_y)

    # Don't reduce 'devices' to 'devic'
    truncated_e = set(ed_e.values()) | set(ing_e.values())
    es_e = dict((k, v) for k,v in es_e.items() if v in truncated_e)

    for each in [ly, ed, ed_e, ied_y, ing, ing_e, ying_y, apos, s, es, es_e, ies_y]:
        suffix.update(each)

    for value in suffix.values():
        if value not in dictionary:
            if value + 'e' in dictionary:
                suffix[value + 'e'] = value
            if value + 'y' in dictionary:
                suffix[value + 'y'] = value

    return dict((k, v) for k,v in suffix.items())


def word_to_phoneme(word):
    """
    Parse a word into a list of vowel and consonant groups
    """
    groups = []
    vowels = 'aeiou'
    consonants = 'bcdfghjklmnpqrstvwxz'
    y = 'y'
    digits = '0123456789'

    phoneme = ''
    for char in list(word):
        if char in consonants:
            if phoneme and phoneme[0] in consonants:
                if char != phoneme[-1]:
                    phoneme += char
            elif phoneme:
                groups.append(phoneme)
                phoneme = char
            else:
                phoneme = char
        elif char in vowels:
            if phoneme and phoneme[0] in vowels:
                phoneme += char
            elif phoneme:
                groups.append(phoneme)
                phoneme = char
            else:
                phoneme = char
        elif char in y:
            if phoneme:
                groups.append(phoneme)
                phoneme = char
            else:
                phoneme = char
        elif char in digits:
            if phoneme and phoneme[0] in digits:
                phoneme += char
            elif phoneme:
                groups.append(phoneme)
                phoneme = char
            else:
                phoneme = char
        else:
            if phoneme:
                groups.append(phoneme)
                phoneme = char
            else:
                phoneme = char

    groups.append(phoneme)
    return groups


def phoneme_to_word(phoneme):
    """
    Parse a word into a list of vowel and consonant groups
    """
    return ''.join(phoneme)
