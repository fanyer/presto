import re
import os
import textwrap

__all__ = ("strquote", "nojust", "expand", "tablify", "dump_docblock",
           "split_camelcase", "join_camelcase", "join_camelcase2",
           "join_lower", "join_underscore", "join_dashed", "join_plain",
           "dashed_named",
           "_",
           "tempdir",
           )

DIST_ROOT = os.path.split(os.path.dirname(os.path.abspath(__file__)))[0]

def strquote(string):
    return '"' + string.replace("\\", "\\\\").replace('"', '\\"') + '"'

def nojust(str, num, fillchar=None):
    """
    nojust(str, width[, fillchar]) -> string

    Helper function for doing no string justification, it uses the same
    function signature as str.ljust and str.rjust and can be used in the
    `tablify` function.
    Return str as it is, no justification is made.
    """
    return str

def expand(format, lst, columns=None):
    """
    >>> expand("%s", ["foo", "bar"])
    ['foo', 'bar']
    >>> expand("%s - %s", [("foo", 5), ("bar", 10)])
    ['foo - 5', 'bar - 10']
    >>> expand("%s - %s", [(5, "foo"), (10, "bar")])
    ['5  - foo', '10 - bar']
    >>> expand([" %s", ",%s"], ["foo", "bar"])
    [' foo', ',bar']
    """
    if not lst:
        return []
    if type(format) in (list, tuple):
        first, format = format
    else:
        first = format
    if type(lst[0]) not in (list, tuple):
        lst = [[i] for i in lst]
    colcount = len(lst[0])
    if columns == None:
        columns = [str.ljust] * (colcount - 1) + [nojust]
    else:
        columns = [c or nojust for c in columns]
    paddings = [0] * colcount
    for i in range(colcount):
        paddings[i] = max((len(str(row[i])) for row in lst))
    out = []
    out.append(first % tuple(func(str(col), size) for col, func, size in zip(lst[0], columns, paddings)))
    for row in lst[1:]:
        out.append(format % tuple(func(str(col), size) for col, func, size in zip(row, columns, paddings)))
    return out

def tablify(format, lst, columns=None):
    """
    >>> tablify("%s\\n", ["foo", "bar"])
    'foo\\nbar\\n'
    >>> tablify("%s - %s\\n", [("foo", 5), ("bar", 10)])
    'foo - 5\\nbar - 10\\n'
    >>> tablify("%s - %s\\n", [(5, "foo"), (10, "bar")])
    '5  - foo\\n10 - bar\\n'
    >>> tablify([" %s\\n", ",%s\\n"], ["foo", "bar"])
    ' foo\\n,bar\\n'
    """
    return "".join(expand(format, lst, columns=columns))

def dump_docblock(text, is_block=True, indent=0):
    """
    >>> dump_docblock("A single line")
    '/**\\n * A single line\\n */'
    >>> dump_docblock("First line\\nsecond line")
    '/**\\n * First line\\n * second line\\n */'
    >>> dump_docblock(["First line", "second line"])
    '/**\\n * First line\\n * second line\\n */'
    """
    if type(text) in [list, tuple]:
        lines = text
    else:
        lines = text.splitlines()
    indent = " " * indent
    if len(lines) == 1 and not is_block:
        return "/** " + lines[0] + "*/"
    else:
        return indent + "/**\n" + ("".join([indent + " * " + line + "\n" for line in lines])) + indent + " */"

def parse_docblock(cmt):
    """
    >>> parse_docblock("/// one liner")
    'one liner'
    >>> parse_docblock("///no space")
    'no space'
    >>> parse_docblock("/**\\n    * Several\\n     * lines\\n */")
    'Several\\nlines\\n'
    """
    if re.match("///", cmt):
        cmt = cmt[3:]
    elif cmt.startswith("/**") and cmt.endswith("*/"):
        m = re.match(r"/\*\*(?:\w*[\n])?(.+)/$", cmt, re.DOTALL | re.MULTILINE)
        cmt = m.group(1)
        cmt = "".join([re.sub("^[ \t]*[*]", "", line) for line in cmt.splitlines(True)])
    return textwrap.dedent(cmt)

words_re = re.compile("([A-Z][A-Z0-9]*(?=[A-Z][a-z0-9]))|([A-Z][A-Z0-9]*$)|([A-Z][a-z0-9]*)|([a-z][a-z0-9]*)")

def split_camelcase(text):
    """
    >>> split_camelcase("foo")
    ['foo']
    >>> split_camelcase("fooBar")
    ['foo', 'Bar']
    >>> split_camelcase("FooBar")
    ['Foo', 'Bar']
    >>> split_camelcase("fooBAR")
    ['foo', 'BAR']
    >>> split_camelcase("fooBARBaz")
    ['foo', 'BAR', 'Baz']
    >>> split_camelcase("x")
    ['x']
    >>> split_camelcase("aB")
    ['a', 'B']
    """
    return [word.group(0) for word in words_re.finditer(text)]

def join_camelcase(words, first_lower=False):
    """
    >>> join_camelcase([])
    ''
    >>> join_camelcase(['foo'])
    'Foo'
    >>> join_camelcase(['foo', 'Bar'])
    'FooBar'
    >>> join_camelcase(['Foo', 'Bar'])
    'FooBar'
    >>> join_camelcase(['foo', 'BAR'])
    'FooBAR'
    >>> join_camelcase(['FOO', 'Bar'])
    'FOOBar'
    >>> join_camelcase(['foo', 'BAR', 'Baz'])
    'FooBARBaz'
    >>> join_camelcase(['x'])
    'X'
    >>> join_camelcase(['set', 'X'])
    'SetX'
    """
    if not words:
        return ""
    word = words[0]
    if first_lower:
        word = word.lower()
    else:
        if len(word) == 1:
            word = word.upper()
        elif word.upper() != word:
            word = word[0].upper() + word[1:].lower()
    out = word
    for word in words[1:]:
        if word.upper() == word:
            out += word
        else:
            out += word[0].upper() + word[1:].lower()
    return out

def join_camelcase2(words):
    """
    >>> join_camelcase2([])
    ''
    >>> join_camelcase2(['foo'])
    'foo'
    >>> join_camelcase2(['foo', 'Bar'])
    'fooBar'
    >>> join_camelcase2(['Foo', 'Bar'])
    'fooBar'
    >>> join_camelcase2(['FOO', 'BAR'])
    'fooBAR'
    >>> join_camelcase2(['FOO', 'Bar'])
    'fooBar'
    >>> join_camelcase2(['foo', 'BAR', 'Baz'])
    'fooBARBaz'
    >>> join_camelcase2(['x'])
    'x'
    >>> join_camelcase2(['Set', 'X'])
    'setX'
    """
    return join_camelcase(words, True)

def join_lower(words, sep):
    return sep.join([word.lower() for word in words])

def join_underscore(words):
    """
    >>> join_underscore([])
    ''
    >>> join_underscore(['foo'])
    'foo'
    >>> join_underscore(['foo', 'Bar'])
    'foo_bar'
    >>> join_underscore(['foo', 'BAR', 'Baz'])
    'foo_bar_baz'
    >>> join_underscore(['a'])
    'a'
    >>> join_underscore(['a', 'B'])
    'a_b'
    """
    return join_lower(words, "_")

def join_dashed(words):
    """
    >>> join_dashed([])
    ''
    >>> join_dashed(['foo'])
    'foo'
    >>> join_dashed(['foo', 'Bar'])
    'foo-bar'
    >>> join_dashed(['foo', 'BAR', 'Baz'])
    'foo-bar-baz'
    >>> join_dashed(['a'])
    'a'
    >>> join_dashed(['a', 'B'])
    'a-b'
    """
    return join_lower(words, "-")

def join_plain(words):
    """
    >>> join_plain([])
    ''
    >>> join_plain(['foo'])
    'foo'
    >>> join_plain(['foo', 'Bar'])
    'fooBar'
    >>> join_plain(['foo', 'BAR', 'Baz'])
    'fooBARBaz'
    >>> join_plain(['a'])
    'a'
    >>> join_plain(['a', 'B'])
    'aB'
    """
    return "".join(words)

def dashed_name(name, dash="-"):
    """
    >>> dashed_name("bar")
    'bar'
    >>> dashed_name("Foo")
    'foo'
    >>> dashed_name("FooBar")
    'foo-bar'
    >>> dashed_name("HTTPVersion")
    'http-version'
    >>> dashed_name("serviceID")
    'service-id'
    >>> dashed_name("ID")
    'id'
    >>> dashed_name("CamelCaseIsHere")
    'camel-case-is-here'
    """
    return join_lower(split_camelcase(name), dash)


#class Join(object):
#    def __init__(self, join, prefix=None, suffix=None):
#        self.join = join
#        self.prefix = prefix
#        self.suffix = suffix
#
#    def __call__(self, words):
#        out = self.join(words)
#        if self.prefix:
#            out = self.prefix + out
#        if self.suffix:
#            out = out + self.suffix
#        return out
#
#class NameConfig(object):
#    join_func = None
#    def __init__(self, func):
#        self.join_func = func
#
#    def setJoinFunc(self, func):
#        self.join_func = func
#
#default_name = NameConfig(join_plain)
#
#class Name(object):
#    """
#
#    >>> cc = NameConfig(join_camelcase)
#    >>> '%s' % Name(['set', 'X'], conf=cc)
#    u'SetX'
#    >>> u'%s' % Name(['set', 'X'], conf=cc)
#    u'SetX'
#    >>> Name(['set', 'X'], conf=cc) + 'Size'
#    u'SetXSize'
#    >>> 'set' + Name(['X', 'Size'], conf=cc)
#    u'setXSize'
#    >>> Name(['set', 'X'], conf=cc) + Name('Size', conf=cc)
#    Name([u'set', u'X', u'Size'])
#    >>> unicode(Name(['set', 'X'], conf=cc) + Name('Size', conf=cc))
#    u'SetXSize'
#    >>> unicode(Name("someName", conf=NameConfig(Join(join_camelcase2, prefix="_"))))
#    u'_someName'
#    >>> unicode(Name("someName", conf=NameConfig(Join(join_underscore, prefix="_"))))
#    u'_some_name'
#    """
#    def __init__(self, name, conf=None):
#        if isinstance(name, Name):
#            name = name.words
#            if not conf:
#                conf = name.conf
#        elif not isinstance(name, (list, tuple)):
#            name = split_camelcase(unicode(name))
#        else:
#            name = map(unicode, name)
#        self.words = name
#        if not conf:
#            conf = default_name
#        self.conf = conf
#
#    def __repr__(self):
#        return "Name(%r)" % self.words
#
#    def __add__(self, n):
#        if not isinstance(n, Name):
#            return unicode(self) + n
#        words = self.words + n.words
#        return Name(words, conf=self.conf)
#
#    def __radd__(self, n):
#        if not isinstance(n, Name):
#            return n + unicode(self)
#        words = n.words + self.words
#        return Name(words, conf=self.conf)
#
#    def __eq__(self, n):
#        if not isinstance(n, Name):
#            return unicode(self) == n
#        return self.words == n.words
#
#    def __str__(self):
#        if self.conf:
#            return self.conf.join_func(self.words)
##        elif nameconf.join_func:
##            return nameconf.join_func(self.words)
#        else:
#            return "".join(self.words)
##            raise ProtoError("No join function configured, cannot create string representation of name")
#
#    __unicode__ = __str__

# TODO: Add i18n support to _
def _(message):
    return message

def tempdir(subdirs=None):
    from hashlib import md5
    path = os.path.abspath(os.path.join(__file__, "..", ".."))
    return os.path.join(os.path.expanduser("~/"), ".hob", md5(path).hexdigest(), *(subdirs or []))
