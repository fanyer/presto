#!/usr/bin/env python
# -*- coding: utf-8 -*-

import difflib
import re

import dict_en

vowels_ascii = u'AEIOUaeiou'
vowels_latin1 = u'AEIOUaeiouÁÂÃÄÅÆÈÉÊËÌÍÎÏÒÓÔÕÖØÙÚÛÜÝáâãäåæèéêëìíîïòóôõöøùúûüýÿ'
consonant_groups = re.compile(r'([^%s]+)' % vowels_latin1)

suffixes_sv = [set([u'', u's', u'en', u'a', u't', u'er', u'r', u'n', u'et', u'ar', u'e', u'de', u'd', u'ts', u'na', u'as', u'ning', u'des', u'es', u're', u'namn', u'ade', u'erna', u'are', u'ad', u'ns', u'era', u'fil', u'ens', u'at', u'ta', u'st', u'nde', u'för', u'rad', u'g', u'ering', u'arna', u'sta', u'släge', u'ra', u'ningar', u'met', u'hanterare', u'ets', u'erat', u'eras', u'erar', u'da', u'ats', u'sfel', u'ren', u'raden', u'program', u'ma', u'm', u'het', u'erade', u'dag', u'ande', u'-servern', u'-fil', u'versionen', u'te', u'sprogram', u'sida', u'sfil', u'nummer', u'ningen', u'net', u'lista', u'la', u'i', u'föra', u'funktioner', u'åt', u'ligt', u'ing', u'ende', u'aren', u'amn', u'varande', u'taget', u'tagen', u'liga', u'lig', u'ka', u'k'])]

suffixes_en = [ \
        set([u'', u's', u'ed', u'ing', u'able', u'ability', u'er', u'ers', u'or', u'ors', u'ive', u'ively', u'ion', u'ions', u'ly', u'y', u'ment', u'ments', u'ance', u'ation', u'ations', u'est', u'arily']),   # develop, entertain, download, depart, document, accept, patent, corrupt, deliver, correct, confirm, strong, moment, restrict
        #set([u'', u's', u'd', u'ing', u'able', u'ability', u'ment', u'ments']),   # agree (exception: agreed, not agreeed) creates confusion
        set([u'', u'es', u'ed', u'ing', u'able', u'ability', u'ible', u'ibility', u'er', u'ers', u'ive', u'ively', u'ment', u'ments']),   # access, attach, match, address, progress, compress (exception: matches, not matchs)
        set([u'e', u'es', u'ed', u'ing', u'able', u'ability', u'er', u'ers', u'or', u'ors', u'ion', u'ions', u'ity', u'ive', u'ively', u'ement', u'ements', u'ely', u'ic', u'ically', u'est']),   # us, serv, remov, manag, receiv, requir, stat, plac, validat, complet, automat, secur, separat, execut, administrat, alternat, describ, descript, driv, alternat, hug
        set([u'e', u'es', u'ed', u'ing', u'able', u'ability', u'er', u'ers', u'or', u'ors', u'ion', u'ions', u'ity', u'ive', u'ment', u'ments', u'ely', 'ically']),   # argu (exception: argument, not arguement)
        set([u'y', u'ies', u'ied', u'ying', u'iable', u'ier', u'ic', u'ically', u'icate', u'icates', u'ication', u'ications']),   # propert, identif, cop, specif, verif, appl, certif, notif
        set([u'', u'ize', u'ized', u'izing', u'izable', u'izability', u'ization', u'izations']),   # custom
]

class Word(unicode):
    Dict = dict()
    def __init__(self, word):
        unicode.__init__(self)
        self.__class__.Dict[word] = self


def _sequence(word, descending=True, with_vowels=True):
    """
    Creates a list: ['downloading', 'downloadin', 'downloadi', 'download',...]
    The idea is to find close matches that share the same stem
    A descending sequence is weighted to the front end of the word
    """
    def split(word):
        nodouble = re.sub(r'(.)\1+', r'\1', word.lower())
        return consonant_groups.findall(nodouble)

    truncation_list = []
    if with_vowels:
        leftover = word
    else:
        leftover = split(word)
    while len(leftover) > 0:
        if with_vowels:
            truncation_list.append(leftover)
        else:
            truncation_list.append('-'.join(leftover))
        if descending:
            leftover = leftover[0:-1]
        else:
            leftover = leftover[1:]
    return truncation_list


def get_close_matches(word, word_list, minimum=3, n=12, cutoff=0.45, descending=True, with_vowels=True):
    """
    Find close matches for word in word_list
    """
    matches = []
    sm = difflib.SequenceMatcher()
    sm.set_seq2(_sequence(word, descending, with_vowels))
    for item in word_list:
        if descending and not item.startswith(word[0]):
            continue
        elif not descending and not item.endswith(word[-1]):
            continue
        comparison_sequence = _sequence(item, descending, with_vowels)
        if with_vowels and len(comparison_sequence) < minimum or \
                not with_vowels and len(comparison_sequence) < 2:
            continue
        sm.set_seq1(comparison_sequence)
        ratio = sm.ratio()
        if ratio < cutoff:
            continue
        else:
            matches.append((ratio, item))

    def ordering(match):
        sm_actual = difflib.SequenceMatcher()
        sm_actual.set_seqs(match[1], word)
        return (match[0], sm_actual.ratio())

    matches.sort(reverse=True, key=ordering)
    words = [match[1] for match in matches]
    if not n:
        n = len(words)
    return words[0:n]


def best_root(word, word_list, suffix_list=suffixes_en):
    """
    Find the root of word which yields the maximum number of known suffixes
    """
    root = word
    value = dict()
    while len(root) > 2:
        matches = [item for item in word_list if re.match(root, item)]
        suffixes = [re.sub(root, '', item, count=1) for item in word_list]
        value[root] = max([len(set(suffixes) & item) for item in suffix_list])
        root = root[:-1]

    best = sorted([(value[root], root) for root in value], reverse=True)

    if not best or best[0][0] == 1:
        return word
    else:
        return best[0][1]


def popular_suffixes(word_list, max_length=8):
    """
    Find popular suffixes
    """
    counter = dict()
    for word in word_list:
        for n in range(1, max_length + 1):
            if len(word) < n + 2:
                continue
            root = word[:-n]
            suffix = word[-n:]
            if root in word_list:
                if suffix in counter:
                    counter[suffix] += 1
                else:
                    counter[suffix] = 1

    popular = sorted([(counter[suffix], suffix) for suffix in counter], reverse=True)

    return popular


