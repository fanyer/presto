"""Checking of the unicode source files

Still decidedly experimental !

Note that the fact that this module warns about stuff doesn't
necessarily mean there's anything wrong; equally, just because this
module *doesn't* warn you isn't evidence that all's well.  Several
tables' parsers don't even *try* to contribute descriptions of
characters to the checker, even though their data files do include
such descriptions.  This is just a rough stab at making it a bit
easier to spot any really bad mistakes in the sources/ directory; and
it's not much use to you unless you can check whether the names `ssang
bieub sun gyeong eum' and `kapyeounssangpieup' are meant to refer to
the same hangul character (and so on).

The names used for tables in the checker's warnings are mildly random;
if several tables are generated from one data file, whichever of those
tables comes up first in the Platform's table.keys() is the one whose
name shows up here, since it's the one passed to the Table that
actually parsed the data file. """

# Copyright Opera software ASA, 2003.  Written by Eddy.
import string

class Checker:
    instance = None
    def __init__(self):
        assert not self.instance, 'Never instanciate Checker more than once'
        self.__class__.instance = self
        self.seen = {} # { codepoint: { name: [table ...] } }

    def notice(self, codepoint, tablename, *texts):

        try: bok = self.seen[codepoint]
        except KeyError: bok = self.seen[codepoint] = {}

        for text in texts:
            text = string.lower(string.strip(text))
            if text:
                try: row = bok[text]
                except KeyError: bok[text] = [ tablename ]
                else: row.append(tablename)

    # check() finds lots of synonyms; it'd be nice to notice their
    # equivalence and not report them; e.g. "opening" == "left",
    # "closing" == "right"

    def check(self, err):
        ans = 0
        for cp, bok in self.seen.items():
            if len(bok) > 1:
                # *may* be OK, but only if there's some name all tables offered
                uniq, all = {}, {} # { name: Text(name) } { table: None }
                for k, v in bok.items():
                    for it in v: all[it] = None
                    uniq[k] = Text(k)

                # Conflate synonyms (but only here - leave unmangled in bok)
                tidied = {}
                for k, v in bok.items():
                    for x in tidied.keys():
                        if uniq[x] == uniq[k]: break
                    else: x = k
                    tidied[x] = tidied.get(x, []) + v

                # check each name to see if it's seen in all tables:
                for v in filter(lambda x, n=len(all): len(x) >= n, tidied.values()):
                    for it in all.keys(): # i.e. for all tables
                        if it not in v: break
                    else: # all tables agree on one of the names
                        all.clear()
                        break

                if not all: continue
                ans = ans + 1

                # emit a warning about this code-point
                err.write('Warning: codepoint %X has %d descriptions:\n'
                          % (cp, len(bok)))
                for key, row in bok.items():
                    assert len(row) > 0
                    if len(row) < 2: tables = row[0]
                    else: tables = '%s and %s' % (string.join(row[:-1], ', '), row[-1])
                    err.write('  in %s it is called:\n\t%s\n' % (tables, key))

        return ans # number of suspect code-points

    def __nonzero__(self): return 1 # be a true boolean, unlike None

class Text:
    """Synonym recognition.

    Used by the Checker to thin the false positives.  Not intended to
    be complete, just to keep the noise down.  Could probably be
    thinned a bit more.

    Note that the fact that this class deems two names equivalent
    doesn't mean they are; it just means they matched some simple rule
    that is matched by the alternate names for several characters.  I
    just added plausible ad-hoc rules until the number of warnings got
    down to a modest level (below 100). -- Eddy/2003/Aug. """

    def __init__(self, txt):
        self.words = reduce(lambda x, y: x+y,
                            map(string.split, string.split(txt, '-')), [])
        try: # discard some flapdoodle
            if self.words[0][-5:] == 'wards' and self.words[0][:-5] in (
                'left', 'right', 'up', 'down'): self.words[0] = self.words[0][:-5]
            elif self.words[0] == 'music' and self.words[-1] == 'sign':
                self.words = self.words[1:-1]
            elif self.words[0] == 'vulgar' and self.words[1] == 'fraction':
                self.words = self.words[1:]
        except IndexError: pass

    # There follow several ad hoc *functions* (not methods) used by __eq__ and
    # subsequently deleted (so they don't show up as methods).

    def synonym(this, that, known={'opening': 'left', 'closing': 'right',
                                   'centerline': 'centreline', 'center': 'centre',
                                   'beamed': 'barred', 'graphic': 'symbol',
                                   'slash': 'solidus', 'unit': 'sign',
                                   'pilcrow': 'paragraph',
                                   'degrees': 'degree', 'celsius': 'centigrade',
                                   'square': 'squared', 'quarter': 'quarters',
                                   'line': 'bar', 'overline': 'overscore' }):
        return known.get(this, this) == known.get(that, that)

    def verbose(this, them, siz,
                # NB known is over-ridden by, e.g., hangul re-using this ...
                known={ 'underscore': [ 'low', 'line' ],
                        'trademark': [ 'trade', 'mark' ],
                        'forms': [ 'box', 'drawings' ],
                        'bar': [ 'with', 'stroke' ],
                        'slash': [ 'with', 'stroke' ],
                        'hacek': [ 'with', 'caron' ],
                        'dot': [ 'with', 'dot', 'above' ],
                        'ring': [ 'with', 'ring', 'above' ],
                        'combining': [ 'non', 'spacing' ],
                        'guillemet': [ 'double', 'angle', 'quotation', 'mark' ],
                        'backslash': [ 'reverse', 'solidus' ],
                        'glyph': [ 'presentation', 'form' ],
                        'period': [ 'full', 'stop' ] }):
        try: row = known[this]
        except KeyError: return None

        if them[:len(row)] == row:
            siz[:] = [ len(row) ]
            return 1

        return None

    def concat(word, those, *prefixes):
        i = 0
        for pre in prefixes:
            if those[i:i+len(pre)] == pre:
                i = i + len(pre)

        while word:
            here = those[i]
            if word[:len(here)] == here: i, word = i+1, word[len(here):]
            else: break
        if word: raise IndexError
        return i

    def hangul(these, those, siz, templ=verbose, chomp=concat,
               known={ 'tieut': [ 'thieuth' ],
                       'lieul': [ 'rieul' ],
                       'digeud': [ 'tikeut' ],
                       'ha': [ 'hieuh', 'a' ],
                       'pa': ['phieuph', 'a' ],
                       'ta': [ 'thieuth', 'a' ],
                       'ka': [ 'khieukh', 'a' ],
                       'ca': [ 'chieuch', 'a' ],
                       'ja': [ 'cieuc', 'a' ],
                       'ju': [ 'cieuc', 'u' ],
                       'sa': [ 'sios', 'a' ],
                       'ba': [ 'pieup', 'a' ],
                       'ma': [ 'mieum', 'a' ],
                       'la': [ 'rieul', 'a' ],
                       'da': [ 'tikeut', 'a' ],
                       'na': [ 'nieun', 'a' ],
                       'ga': [ 'kiyeok', 'a' ],
                       'a': [ 'ieung', 'a' ],
                       # NB three-way aliases:
                       'khieukh': [ 'kiyeok' ], 'giyeog': [ 'kiyeok' ],
                       'phieuph': [ 'pieup' ], 'bieub': [ 'pieup' ],
                       'chieuch': [ 'cieuc' ], 'jieuj': [ 'cieuc' ] }):

        if templ(these[0], those, siz, known):
            siz.insert(0, 1)
            return 1

        if templ(those[0], these, siz, known):
            siz.append(1)
            return 1

        if len(these) == 1:
            try: i = chomp(these[0], those)
            except IndexError: pass
            else:
                siz[:] = [ 1, i ]
                return 1

        if len(those) == 1:
            try: i = chomp(those[0], these)
            except IndexError: pass
            else:
                siz[:] = [ i, 1 ]
                return 1

        return None

    def thai(these, those, siz, chomp=concat):
        if these[0] == 'character':
            try: i = chomp(these[1], those,
                           [ 'vowel', 'sign' ], [ 'tone' ], [ 'letter' ])
            except IndexError: pass
            else:
                siz[:] = [ 2, i ]
                return 1

        if those[0] == 'character':
            try: i = chomp(those[1], these,
                           [ 'vowel', 'sign' ], [ 'tone' ], [ 'letter' ])
            except IndexError: pass
            else:
                siz[:] = [ i, 2 ]
                return 1

        return None

    del concat

    def synarab(this, that,
                syn={'yeh': 'ya', 'heh': 'ha', 'feh': 'fa', 'reh': 'ra',
                     'teh': 'taa', 'hah': 'haa', 'beh': 'baa',
                     'theh': 'thaa', 'khah': 'khaa',
                     'kaf': 'caf', 'dhah': 'zah',
                     'maksura': 'maqsurah' }):

        if (syn.get(this, this) == syn.get(that, that) or
            (this[:len(that)] == that and this[len(that)-1:] == 'ah') or
            (that[:len(this)] == this and that[len(this)-1:] == 'ah')):
            return 1

        return None

    def arabic(these, those, siz, syn=synarab,
               shuffle={ 'on': 'above', 'under': 'below' }):

        if syn(these[0], those[0]):
            siz[:] = [ 1, 1 ]
            return 1

        try:
            # deal with "alef with hamza above" == "hamza on alef" and similar
            if syn(these[0], those[2]) and syn(these[2], those[0]):
                if these[1] == 'with':
                    try: chk = shuffle[those[1]]
                    except KeyError: pass
                    else:
                        if these[3] == chk:
                            siz[:] = [ 4, 3 ]
                            return 1

                elif those[1] == 'with':
                    try: chk = shuffle[these[1]]
                    except KeyError: pass
                    else:
                        if those[3] == chk:
                            siz[:] = [ 3, 4 ]
                            return 1

        except IndexError: pass

        return None

    del synarab

    def cyrillic(these, those, siz,
                 syn={ 'ghe': 'ge', 'kha': 'ha', 'ya': 'ia', 'yu': 'iu', 'ii': 'i',
                       'yeri': 'yeru' },
                 skippy=[ 'byelorussian', 'ukrainian' ]):

        if syn.get(these[0], these[0]) == syn.get(those[0], those[0]):
            siz[:] = [ 1, 1 ]
            return 1

        if these[:2] == skippy:
            siz[:] = [ 2, 0 ]
            return 1
        elif these[0] == skippy[1]:
            if these[1] == 'ie' and those[0] == 'e': siz[:] = [ 2, 1 ]
            else: siz[:] = [ 1, 0 ]
            return 1

        if those[:2] == skippy:
            siz[:] = [ 0, 2 ]
            return 1
        elif those[0] == skippy[1]:
            if those[1] == 'ie' and these[0] == 'e': siz[:] = [ 1, 2 ]
            else: siz[:] = [ 0, 1 ]
            return 1

        return None

    def hebrew(these, those, siz, syn={'punctuation': 'point'}):
        if these[:2] == [ 'ligature', 'yiddish' ]:
            siz[:] = [ 2, 0 ]
            if those[0] == 'letter': siz[1] = 1
            return 1

        if those[:2] == [ 'ligature', 'yiddish' ]:
            siz[:] = [ 0, 2 ]
            if these[0] == 'letter': siz[0] = 1
            return 1

        if syn.get(these[0], these[0]) == syn.get(those[0], those[0]):
            siz[:] = [ 1, 1 ]
            return 1

        return None

    def greek(these, those, siz, syn={'dialytika': 'diaeresis', 'lamda': 'lambda'}):
        """idiomatic special handling (greek here, but doc for all)

        First two arguments are lists of words; third is an initially empty
        list.  Returns a true value if it can see a way to eat some words off
        the front of at least one of the word-lists without changing whether the
        lists encode equivalent descriptions of a character.  In that event, it
        stores two integers in the third argument; how many words to eat from
        each of the two word lists (in the order in which they're supplied).

        It is safe to assume each of the first arguments is non-empty; but not
        to assume any elements beyond [0].

        Note that modifying the third argument has to be done by an operation
        which changes the mutable object to which it refers - it's no use
        assigning a new value to the variable holding the third argument ! """

        if syn.get(these[0], these[0]) == syn.get(those[0], those[0]):
            siz[:] = [ 1, 1 ]
            return 1

        return None

    def latin(these, those, siz, syn={ 'ezh': 'yogh' },
              char=( 'letter', 'ligature' )):

        if these[0] in char and those[0] in char:
            # needs to be first so "letter ae" == "letter a e"
            if string.join(these[1:], '') == string.join(those[1:], ''):
                siz[:] = [ len(these), len(those) ]
                return 1

        if syn.get(these[0], these[0]) == syn.get(those[0], those[0]):
            siz[:] = [ 1, 1 ]
            return 1

        return None

    # plus similar for cyrrilic, greek ... see idiom={...} below.
    # probably need an interface more like that of verbose rather than synonym.

    def enclosed(word, encs={'<': '>', '(': ')'}):
        try: end = encs[word[0]]
        except KeyError: return None
        return word[-1] == end

    def __eq__(self, other, syn=synonym, ver=verbose, wrapped=enclosed,
               idiom={ 'hangul': hangul, 'thai': thai,
                       'hebrew': hebrew, 'arabic': arabic,
                       'cyrillic': cyrillic, 'greek': greek,
                       'latin': latin },
               floaty=('single', 'double'),
               skippy=( 'with', 'and',
                        'sign', 'mark', 'digit', 'letter', 'character',
                        # skipping the following is a bit drastic !
                        'spacing', 'accent', 'modifier', 'combining' )):
        i = j = 0
        stray, step, dialect = {}, [], None
        try:
            while 1:
                if ((self.words[i:i+2] == [ 'two', 'ideographs' ]
                     and j+2 < len(other.words))
                    or self.words[i:i+2] == [ 'unified', 'ideograph' ]): i = i+2

                if ((other.words[j:j+2] == [ 'two', 'ideographs' ]
                     and i+2 < len(self.words)) or
                    other.words[j:j+2] == [ 'unified', 'ideograph' ]): j = j+2

                this, that = self.words[i], other.words[j]
                if dialect and dialect(self.words[i:], other.words[j:], step):
                    i, j = i+step[0], j+step[1]
                    step = []
                elif syn(this, that):
                    i, j = i+1, j+1
                    if this == that:
                        try: dialect = idiom[this]
                        except KeyError: pass
                elif ver(this, other.words[j:], step):
                    i, j = i+1, j+step[0]
                    step = []
                elif ver(that, self.words[i:], step):
                    i, j = i+step[0], j+1
                    step = []
                elif this in floaty:
                    stray[this] = stray.get(this, 0) | 1
                    i = 1 + i
                elif that in floaty:
                    stray[that] = stray.get(that, 0) | 2
                    j = j + 1
                elif this in skippy: i = i+1
                elif that in skippy: j = j+1
                elif wrapped(this) or wrapped(that):
                    if wrapped(this) or that[1:-1] == this: i = i+1
                    if wrapped(that) or that == this[1:-1]: j = j+1
                else: return None

        except IndexError: pass
        if stray.get('double', 0) not in (0, 3): return None
        if stray.get('single', 0) not in (0, 3): return None

        while i < len(self.words) and (
            self.words[i] in skippy or wrapped(self.words[i])): i = i+1
        while j < len(other.words) and (
            other.words[j] in skippy or wrapped(other.words[j])): j = j+1

        if i == len(self.words):
            if j == len(other.words): return 1
            if other.words[j] == 'or': return 1

        elif j == len(other.words):
            if self.words[i] == 'or': return 1

        return None

    del synonym, verbose, hangul, thai, hebrew, enclosed
