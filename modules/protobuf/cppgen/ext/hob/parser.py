import re
import sys
from copy import deepcopy, copy
from hob.proto import Options, OptionValue, Message, Field, Request, Event, Service, Proto, Quantifier, Enum, EnumValue, Package, parse_option_value, DocBlock
from hob.utils import parse_docblock

class TokenError(Exception):
    def __repr__(self):
        return "TokenError(%r)" % self.message

class ParseError(Exception):
    def __init__(self, text, token):
        Exception.__init__(self, text)
        self.token = token

    def __repr__(self):
        return "ParseError(%r,%r)" % (self.message, self.token)

class Scanner(object):
    """A regular expression based scanner which mimics the API define by re.Scanner (Python 2.5+)"""

    def __init__(self, lexicon, flags=0):
        self.lexicon = lexicon
        self.groups = [0] # First index is not used
        extend = self.groups.extend
        def process(items):
            for l in items:
                r = re.compile(l[0])
                extend([l]*(r.groups+1))
                yield "(" + l[0] + ")"
        # Build the expression by placing each lexicon match in a group and ORing them together
        self.expr = "|".join(process(lexicon))
        self.reg = re.compile(self.expr, flags)

    def scan(self, string):
        result = []
        append = result.append
        match = self.reg.match
        while 1:
            m = match(string)
            if not m:
                break
            action = self.groups[m.lastindex][1]
            if callable(action):
                action = action(self, m.group())
            if action is not None:
                append(action)
            string = string[m.end():]
        return result, string

class Position(object):
    def __init__(self, line, col, offset):
        self.line = line
        self.col = col
        self.offset = offset

    def __repr__(self):
        return "Position(%r,%r,%r)" % (self.line, self.col, self.offset)

class Token(object):
    def __init__(self, name, value, pos, source, ignore=False):
        self.name = name
        self.value = value
        self.pos = pos
        self.source = source
        self.ignore = ignore

    def __repr__(self):
        extra = ""
        if self.ignore:
            extra = ",ignore=%r" % self.ignore
        return "Token(%r,%r,%r,%r%s)" % (self.name, self.value, self.pos, self.source, extra)

    def __eq__(self, o):
        if isinstance(o, Token):
            return self.name == o.name
        elif isinstance(o, (str, unicode)):
            return self.name == o
        else:
            raise TypeError("Cannot compare Token object with %s" % type(o))

    def __ne__(self, o):
        return not self == o

    def __contains__(self, names):
        if isinstance(names, Token):
            names = [t.name for t in names]
        return self.name in names

def create_lexer_rules():
    def ignore_WHITESPACE(s, t): return "ignore_WHITESPACE", t
    def DOCBLOCK(s, t): return "DOCBLOCK", t
    def COMMENT(s, t): return "COMMENT", t
    def SYNTAX(s, t): return "SYNTAX", t
    def PACKAGE(s, t): return "PACKAGE", t
    def IMPORT(s, t): return "IMPORT", t
    def MESSAGE(s, t): return "MESSAGE", t
    def SERVICE(s, t): return "SERVICE", t
    # TODO: Need to decide on service format
    def RPC(s, t): return "RPC", t
    def COMMAND(s, t): return "COMMAND", t
    def EVENT(s, t): return "EVENT", t
    def RETURNS(s, t): return "RETURNS", t
    def ENUM(s, t): return "ENUM", t
    def EXTENSIONS(s, t): return "EXTENSIONS", t
    def TO(s, t): return "TO", t
    def MAX(s, t): return "MAX", t
    def EXTEND(s, t): return "EXTEND", t
    def STRING(s, t): return "STRING", t
    def BOOL(s, t): return "BOOL", t
    def IDENTIFIER_PATH(s, t): return "IDENTIFIER_PATH", t
    def IDENTIFIER(s, t): return "IDENTIFIER", t
    def L_CURLY_BRACKET(s, t): return "L_CURLY_BRACKET", t
    def R_CURLY_BRACKET(s, t): return "R_CURLY_BRACKET", t
    def L_SQUARE_BRACKET(s, t): return "L_SQUARE_BRACKET", t
    def R_SQUARE_BRACKET(s, t): return "R_SQUARE_BRACKET", t
    def L_PAREN(s, t): return "L_PAREN", t
    def R_PAREN(s, t): return "R_PAREN", t
    def QUANTIFIER(s, t): return "QUANTIFIER", t
    def EQ(s, t): return "EQ", t
    def SEMI(s, t): return "SEMI", t
    def COMMA(s, t): return "COMMA", t
    def NUMBER(s, t): return "NUMBER", t
    def OPTION(s, t): return "OPTION", t
    def TYPE(s, t): return "TYPE", t

    rules = [(r'[ \t\r\n]+', ignore_WHITESPACE),
             (r'(?:///.*$|/\*\*(?:.|[\r\n])*?\*/)', DOCBLOCK),
             (r'(?://.*$|/\*(?:.|[\r\n])*?\*/)', COMMENT),
             (r'syntax(?!\w)', SYNTAX),
             (r'package(?!\w)', PACKAGE),
             (r'import(?!\w)', IMPORT),
             (r'message(?!\w)', MESSAGE),
             (r'service(?!\w)', SERVICE),
             (r'command(?!\w)', COMMAND),
             (r'rpc(?!\w)', RPC),
             (r'event(?!\w)', EVENT),
             (r'returns(?!\w)', RETURNS),
             (r'enum(?!\w)', ENUM),
             (r'(?:required|optional|repeated)(?!\w)', QUANTIFIER),
             (r'option(?!\w)', OPTION),
             (r'extensions(?!\w)', EXTENSIONS),
             (r'to(?!\w)', TO),
             (r'max(?!\w)', MAX),
             (r'extend(?!\w)', EXTEND),
             (r'(true|false)(?!\w)', BOOL),
             (r'(int32|uint32|sint32|int64|uint64|sint64|fixed32|sfixed32|fixed64|sfixed64|float|double|bool|string|bytes)(?!\w)', TYPE),
             (r'"[^"]*"', STRING),
             (r'[a-zA-Z][a-zA-Z0-9_]*(?:[.][a-zA-Z][a-zA-Z0-9_]*)+', IDENTIFIER_PATH),
             (r'[a-zA-Z][a-zA-Z0-9_]*(?!\w)', IDENTIFIER),
             (r'\[', L_SQUARE_BRACKET),
             (r'\]', R_SQUARE_BRACKET),
             (r'{', L_CURLY_BRACKET),
             (r'}', R_CURLY_BRACKET),
             (r'\(', L_PAREN),
             (r'\)', R_PAREN),
             (r'=', EQ),
             (r';', SEMI),
             (r',', COMMA),
             (r'(?:\.[0-9]+)|(?:[0-9]+(?:\.[0-9]+)?)', NUMBER),
             ]
    return rules

keywords = ("PACKAGE", "IMPORT", "MESSAGE", "ENUM",
            "QUANTIFIER", "OPTION", "EXTENSIONS", "TO", "MAX",
            "EXTEND", "BOOL", "TYPE", "SYNTAX",
            "SERVICE", "COMMAND", "RPC", "EVENT", "RETURNS")

class Lexer(object):
    def __init__(self, rules, tabsize=4):
        self.rules = rules
        self.pos = Position(1, 0, 0)
        self.tabsize = tabsize
        self._linereg = re.compile("\r\n?|\n|\t")

    def copy(self):
        return Lexer(self.rules, self.tabsize)

    @staticmethod
    def extractRules(o):
        for k in dir(o):
            if not k.startswith("t_"):
                continue
            v = getattr(o, k)
            name = k[len("t_"):]

            def caller(scanner, token):
                return name, token

            yield (v, caller)

    def process(self, value):
        m = None
        for m in self._linereg.finditer(value):
            if m.group() == "\t":
                self.pos.col += self.tabsize - (self.pos.col % self.tabsize)
            else:
                self.pos.col = 0
                self.pos.line += 1
        self.pos.col += len(value) - (m and m.end() or 0)
        self.pos.offset += len(value)

    def scan(self, text):
        rules = list(self.rules)
        scanner = Scanner(rules, re.MULTILINE)
        tokens, remainder = scanner.scan(text)
        for t, v in tokens:
            pos = deepcopy(self.pos)
            if t.startswith("ignore_"):
                token = Token(t[len("ignore_"):], v, pos, text, ignore=True)
            else:
                token = Token(t, v, pos, text)
            self.process(v)
            yield token
        if remainder:
            raise TokenError("Failed to lex all text: %r" % remainder)

class Matcher(object):
    def on_syntax(self, syntax):
        pass

    def on_package_name(self, name):
        pass

    def on_package_open(self):
        pass

    def on_package_close(self):
        pass

    def on_import(self, filename):
        pass

    def on_message_open(self, name):
        pass

    def on_message_close(self):
        pass

    def on_extend_open(self, name):
        pass

    def on_extend_close(self):
        pass

    def on_extensions(self, start, end):
        pass

    def on_field_open(self, quantifier, type_, name, tag):
        pass

    def on_field_close(self):
        pass

    def on_enum_open(self, name):
        pass

    def on_enum_close(self):
        pass

    def on_enum_value_open(self, name, value):
        pass

    def on_enum_value_close(self):
        pass

    def on_service_open(self, name):
        pass

    def on_service_close(self):
        pass

    def on_rpc_open(self, name, message_in, message_out):
        pass

    def on_rpc_close(self):
        pass

    def on_command_open(self, name, message_in, message_out, tag):
        pass

    def on_command_close(self):
        pass

    def on_event_open(self, name, message_out, tag):
        pass

    def on_event_close(self):
        pass

    def on_option(self, name, value, is_custom):
        pass

    def on_default(self, value):
        pass

    def on_comment(self, comment):
        pass

    def on_docblock(self, doc):
        pass

class Parser(object):
    def __init__(self, lexer, matcher, name=None, debug_logger=None, debug=False, require_syntax=False):
        self.lexer = lexer
        self.tokens = None
        self.name = name
        self.matcher = matcher
        self.require_syntax = require_syntax
        self._lookahead = []
        self._debug = debug
        if debug_logger == None:
            debug_logger = sys.stdout
        self._log = debug_logger
        self._comments = []
        self._docblocks = []
        self.syntax = "scope"

    def debug(self, *args):
        if not self._debug:
            return
        for arg in args:
            self._log.write(arg)

    def debugl(self, *args):
        args = args + ("\n",)
        self.debug(*args)
#        self.debug("\n")

    def parse(self, text):
        self.tokens = self.lexer.scan(text)
        self.matcher.on_package_open()
        try:
            self.program()
        except StopIteration, e:
            self.matcher.on_package_close()
            return

    def next(self, with_ignored=False):
        self._comments = []
        self._docblocks = []
        offset = 0
        while True:
            if offset < len(self._lookahead):
                token = self._lookahead[offset]
            else:
                token = self.tokens.next()
                self._lookahead.append(token)
            if token.ignore and not with_ignored:
                offset += 1
                continue
            elif token.name == "DOCBLOCK":
                self.matcher.on_docblock(token.value)
                self._docblocks.append(token)
                self.debugl("DOCBLOCK %s (%r) :%r:%r" % (token.name, token.value, token.pos.line, token.pos.col))
                offset += 1
                continue
            elif token.name == "COMMENT":
                self.matcher.on_comment(token.value)
                self._comments.append(token)
                self.debugl("COMMENT %s (%r) :%r:%r" % (token.name, token.value, token.pos.line, token.pos.col))
                offset += 1
                continue
            del self._lookahead[0:offset + 1]
            self.debugl("MATCH %s (%r) :%r:%r" % (token.name, token.value, token.pos.line, token.pos.col))
            return token

    def la(self, with_ignored=False):
        """Look ahead for the next token, skips comments/whitespace.
        """
        offset = 0
        while True:
            if offset < len(self._lookahead):
                token = self._lookahead[offset]
            else:
                token = self.tokens.next()
                self._lookahead.append(token)
            if token.ignore and not with_ignored:
                offset += 1
                continue
            elif token.name == "DOCBLOCK":
                offset += 1
                continue
            elif token.name == "COMMENT":
                offset += 1
                continue
            self.debugl("LA %s (%r) :%r:%r" % (token.name, token.value, token.pos.line, token.pos.col))
            return token

    def _location(self, token):
        text = " at line %d col %d" % (token.pos.line, token.pos.col)
        if self.name:
            text += " in '%s'" % self.name
        return text

    def fail(self, expected, token):
        if isinstance(token, (list, tuple)):
            raise ParseError("Expected %s, got: %r%s" % (" or ".join(expected), token.name, self._location(token)), token)
        else:
            raise ParseError("Expected %s, got: %r%s" % (expected, token.name, self._location(token)), token)

    def match(self, token, with_ignored=False):
        t = self.next(with_ignored=with_ignored)
        if isinstance(token, (list, tuple)):
            if t.name not in token:
                raise ParseError("Expected %s, got: %r ('%s')%s" % (" or ".join(token), t.name, t.value, self._location(t)), t)
            return t
        else:
            if t.name != token:
                raise ParseError("Expected %s, got: %r ('%s')%s" % (token, t.name, t.value, self._location(t)), t)
            return t

    def matchIdentifier(self):
        return self.match(("IDENTIFIER", ) + keywords)

    def matchIdentifierPath(self):
        return self.match(("IDENTIFIER", "IDENTIFIER_PATH") + keywords)

    def program(self):
        t = self.la()
        if self.require_syntax or t == "SYNTAX":
            self.match("SYNTAX")
            self.match("EQ")
            syntax = self.match(("IDENTIFIER", "STRING"))
            self.match("SEMI")
            syntax_value = syntax.value
            if syntax == "STRING":
                syntax_value = syntax_value[1:-1].replace('\\"', '"').replace('\\\\', '\\')
            if syntax_value not in ("proto2", "scope"):
                raise ParseError("Unrecognized syntax identifier %s. This parser only recognizes 'proto2' and 'scope'" % syntax.value, syntax)
            self.syntax = syntax.value
            self.matcher.on_syntax(syntax.value)
        while True:
            t = self.la()
            if t == "SEMI":  # Allow empty statements
                self.match("SEMI")
                continue

            if t == "MESSAGE":
                self.debugl("MESSAGE start")
                self.message(t)
                self.debugl("MESSAGE end")
            elif t == "EXTEND":
                self.debugl("MESSAGE (extend) start")
                self.message(t)
                self.debugl("MESSAGE (extend) end")
            elif t == "ENUM":
                self.debugl("ENUM start")
                self.enum()
                self.debugl("ENUM end")
            elif t == "SERVICE":
                self.debugl("SERVICE start")
                self.service()
                self.debugl("SERVICE end")
            elif t == "OPTION":
                self.debugl("OPTION_FILE start")
                self.line_option()
                self.debugl("OPTION_FILE end")
            elif t == "PACKAGE":
                self.debugl("PACKAGE start")
                self.package()
                self.debugl("PACKAGE end")
            elif t == "IMPORT":
                self.debugl("IMPORT start")
                self.import_()
                self.debugl("IMPORT end")
            else:
                self.fail(("MESSAGE", "EXTEND", "ENUM", "OPTION", "PACKAGE", "IMPORT", "SEMI"), t)

    def inline_program(self):
        t = self.la()
        if t == "MESSAGE":
            self.debugl("MESSAGE start")
            self.message(t)
            self.debugl("MESSAGE end")
        elif t == "EXTEND":
            self.debugl("MESSAGE (extend) start")
            self.message(t)
            self.debugl("MESSAGE (extend) end")
        elif t == "ENUM":
            self.debugl("ENUM start")
            self.enum()
            self.debugl("ENUM end")
        elif t == "OPTION":
            self.debugl("OPTION start")
            self.line_option()
            self.debugl("OPTION end")
        else:
            self.fail(("MESSAGE", "EXTEND", "ENUM", "OPTION"), t)

    def package(self):
        self.match("PACKAGE")
        t_name = self.match(("IDENTIFIER", "IDENTIFIER_PATH"))
        self.match("SEMI")
        self.matcher.on_package_name(t_name.value)

    def import_(self):
        self.match("IMPORT")
        t_file = self.match("STRING")
        t = self.la(with_ignored=True)
        if t == "WHITESPACE":
            self.match("WHITESPACE", with_ignored=True)
            t = self.la()
        if t == "SEMI":
            self.match("SEMI")
        self.matcher.on_import(t_file.value)

    def message(self, token):
        if token.name not in ("MESSAGE", "EXTEND"):
            raise ParseError("Invalid token (%s) used in message parsing" % token.name, token)

        self.match(token.name)
        if token.name == "EXTEND":
            t_name = self.matchIdentifierPath()  # Name of message or path
        else:
            t_name = self.matchIdentifier()  # Name of message
        self.match("L_CURLY_BRACKET")

        if token.name == "MESSAGE":
            self.matcher.on_message_open(t_name.value)
        elif token.name == "EXTEND":
            self.matcher.on_extend_open(t_name.value)

        while True:
            t = self.la()
            if t == "R_CURLY_BRACKET":
                self.match("R_CURLY_BRACKET")

                if token.name == "MESSAGE":
                    self.matcher.on_message_close()
                elif token.name == "EXTEND":
                    self.matcher.on_extend_close()
                break
            if t == "SEMI":  # Allow empty statements
                self.match("SEMI")
                continue

            if t in ("MESSAGE", "EXTEND", "ENUM", "OPTION"):
                self.inline_program()
            elif t == "EXTENSIONS":
                self.match("EXTENSIONS")
                t_start = self.match("NUMBER")
                self.match("TO")
                t = self.la()
                if t == "MAX":
                    t_end = self.match("MAX")
                else:
                    t_end = self.match("NUMBER")
                self.match("SEMI")
                self.matcher.on_extensions(t_start.value, t_end.value)
            elif t == "QUANTIFIER":
                self.debugl("FIELD start")
                t_q = self.match("QUANTIFIER")  # required | optional | repeated
                t_type = self.match(("TYPE", "IDENTIFIER", "IDENTIFIER_PATH"))  # Type or message-name or message-path
                t_name = self.matchIdentifier()  # Field name
                self.match("EQ")
                t_tag = self.match("NUMBER")  # Field tag
                self.matcher.on_field_open(t_q.value, t_type.value, t_name.value, t_tag.value)
                self.inline_options(allow_default=True)
                self.match("SEMI")
                self.debugl("FIELD done")
                self.matcher.on_field_close()
            else:
                self.fail(("MESSAGE", "EXTEND", "ENUM", "EXTENSIONS", "QUANTIFIER", "OPTION", "SEMI", "R_CURLY_BRACKET"), t)

    def enum(self):
        self.match("ENUM")
        t_name = self.matchIdentifier()
        self.match("L_CURLY_BRACKET")

        self.matcher.on_enum_open(t_name.value)

        while True:
            t = self.la()
            if t == "R_CURLY_BRACKET":
                self.match("R_CURLY_BRACKET")
                self.matcher.on_enum_close()
                t = self.la()
                if t == "SEMI":
                    self.match("SEMI")
                break
            if t == "SEMI":  # Allow empty statements
                self.match("SEMI")
                continue

            if t == "OPTION":
                self.debugl("OPTION start")
                self.line_option()
                self.debugl("OPTION end")
            elif t == "IDENTIFIER":
                t_id = self.matchIdentifier()  # Enum name
                self.match("EQ")
                t_num = self.match("NUMBER")
                self.matcher.on_enum_value_open(t_id.value, t_num.value)
                self.inline_options()
                self.match("SEMI")
                self.matcher.on_enum_value_close()
            else:
                self.fail(("OPTION", "IDENTIFIER", "SEMI", "R_CURLY_BRACKET"), t)

    def service(self):
        self.match("SERVICE")
        t_name = self.matchIdentifier()
        self.match("L_CURLY_BRACKET")

        self.matcher.on_service_open(t_name.value)

        while True:
            t = self.la()
            if t == "R_CURLY_BRACKET":
                self.match("R_CURLY_BRACKET")
                self.matcher.on_service_close()
                t = self.la()
                if t == "SEMI":
                    self.match("SEMI")
                break
            if t == "SEMI":  # Allow empty statements
                self.match("SEMI")
                continue

            if t == "OPTION":
                self.debugl("OPTION start")
                self.line_option()
                self.debugl("OPTION end")
            elif t == "RPC":
                self.match("RPC")
                t_rpc = self.matchIdentifier()  # RPC name

                self.match("L_PAREN")
                t_in = self.matchIdentifierPath()  # RPC input
                self.match("R_PAREN")

                self.match("RETURNS")
                self.match("L_PAREN")
                t_out = self.matchIdentifierPath()  # RPC response
                self.match("R_PAREN")

                self.matcher.on_rpc_open(t_rpc.value, t_in.value, t_out.value)
                t = self.la()
                if t == "L_CURLY_BRACKET":
                    self.match("L_CURLY_BRACKET")
                    t = self.la()
                    while True:
                        if t == "R_CURLY_BRACKET":
                            self.match("R_CURLY_BRACKET")
                            break
                        if t == "SEMI":  # Allow empty statements
                            self.match("SEMI")
                            continue
                        option = self.line_option()
                        t = self.la()
                self.match("SEMI")
                self.matcher.on_rpc_close()
            elif t == "COMMAND":
                self.match("COMMAND")
                t_rpc = self.matchIdentifier()  # RPC name

                self.match("L_PAREN")
                t_in = self.matchIdentifierPath()  # RPC input
                self.match("R_PAREN")

                self.match("RETURNS")
                self.match("L_PAREN")
                t_out = self.matchIdentifierPath()  # RPC response
                self.match("R_PAREN")

                self.match("EQ")
                t_tag = self.match("NUMBER")

                self.matcher.on_command_open(t_rpc.value, t_in.value, t_out.value, t_tag.value)
                t = self.la()
                if t == "L_CURLY_BRACKET":
                    self.match("L_CURLY_BRACKET")
                    t = self.la()
                    while True:
                        if t == "R_CURLY_BRACKET":
                            self.match("R_CURLY_BRACKET")
                            break
                        if t == "SEMI":  # Allow empty statements
                            self.match("SEMI")
                            continue
                        option = self.line_option()
                        t = self.la()
                self.match("SEMI")
                self.matcher.on_rpc_close()
            elif t == "EVENT":
                self.match("EVENT")
                t_rpc = self.matchIdentifier()  # RPC name

                self.match("RETURNS")
                self.match("L_PAREN")
                t_out = self.matchIdentifierPath()  # RPC response
                self.match("R_PAREN")

                self.match("EQ")
                t_tag = self.match("NUMBER")

                self.matcher.on_event_open(t_rpc.value, t_out.value, t_tag.value)
                t = self.la()
                if t == "L_CURLY_BRACKET":
                    self.match("L_CURLY_BRACKET")
                    t = self.la()
                    while True:
                        if t == "R_CURLY_BRACKET":
                            self.match("R_CURLY_BRACKET")
                            break
                        if t == "SEMI":  # Allow empty statements
                            self.match("SEMI")
                            continue
                        option = self.line_option()
                        t = self.la()
                self.match("SEMI")
                self.matcher.on_event_close()
            else:
                self.fail(("OPTION", "RPC", "COMMAND", "EVENT", "SEMI", "R_CURLY_BRACKET"), t)

    def line_option(self):
        self.match("OPTION")
        name, value, paren = self._option()
        self.match("SEMI")
        self.matcher.on_option(name, value, paren)

    def inline_options(self, allow_default=False):
        t = self.la()
        if t == "L_SQUARE_BRACKET":
            self.match("L_SQUARE_BRACKET")
            self.debugl("OPTION_INLINE start")
            while True:
                t = self.la()
                if t == "R_SQUARE_BRACKET":
                    self.match("R_SQUARE_BRACKET")
                    break
                if t == "COMMA":  # Allow empty statement
                    self.match("COMMA")
                    continue

                name, value, paren = self._option()
                if allow_default and name == "default":
                    self.matcher.on_default(value)
                else:
                    self.matcher.on_option(name, value, paren)
            self.debugl("OPTION_INLINE done")

    def _option(self):
        t = self.la()
        paren = False
        if t == "L_PAREN":
            self.match("L_PAREN")
            paren = True
        t_name = self.matchIdentifier()  # Name of option
        if paren:
            self.match("R_PAREN")
        self.match("EQ")
        t_value = self.match(("TYPE", "IDENTIFIER", "NUMBER", "BOOL", "STRING"))  # Possible option values
        return (t_name.value, t_value.value, paren)

class Loader(object):
    def __init__(self, lexer):
        self.lexer = lexer

    def copy(self):
        loader = copy(self)
        loader.lexer = self.lexer.copy()
        return loader

    def load(self, filename, matcher):
        data = open(filename).read()
        parser = Parser(self.lexer, matcher, filename)
        parser.parse(data)

    def loads(self, text, matcher, name=None):
        parser = Parser(self.lexer, matcher, name)
        parser.parse(text)

def parse_string_token(value):
    return value[1:-1].replace("\\\"", "\"").replace("\\\\", "\\")

class BuildError(Exception):
    pass

class Symbol(object):
    def __init__(self, name, owner=None, attribute=None, kind=None, current=None, callback=None):
        self.name = name
        self.owner = owner
        self.attribute = attribute
        self.kind = kind
        self.current = current
        self.callback = callback

    def __repr__(self):
        extras = ""
        for p in ("owner", "attribute", "kind", "current"):
            v = getattr(self, p)
            if v:
                extras += ",%s=%r" % (p, v)
        return "Symbol(%r%s)" % (self.name, extras)

# TODO: Parse docblocks and place in various elements
class PackageBuilder(Matcher):
    def __init__(self, package, loader):
        self.package = package
        self.loader = loader
        self.syntax = None

        # Current pointers, changes depending on context
        self._symbols = []
        self._docs = []
        self._comments = []
        self._item_stack = []
        self._item = None

    def _push_item(self, item):
        self._item_stack.append(self._item)
        self._item = item

    def _pop_item(self):
        self._item = self._item_stack.pop()

    def _iter_docs(self):
        for doc in self._docs:
            if re.match("///", doc):
                yield parse_docblock(doc)
            elif doc.startswith("/**") and doc.endswith("*/"):
                yield parse_docblock(doc)

    def _docblock(self):
        if self._docs:
            doc = "".join(self._iter_docs())
            return DocBlock(doc)
        else:
            return None

    def _reset_docs(self):
        self._docs = []

    def _resolveSymbols(self):
        for sym in self._symbols:
            current = sym.current
            while isinstance(current, Symbol):
                current = current.resolved
            item = self.package.find(sym.name, current)
            sym.resolved = item
            if not isinstance(item, sym.kind):
                raise TypeError("Expected type %s for symbol %s" % (sym.kind.__name__, ".".join(current.path() + [sym.name])))
            if sym.attribute != None:
                setattr(sym.owner, sym.attribute, item)
            if sym.callback:
                sym.callback(sym, item)
        self._symbols = []

    def on_syntax(self, syntax):
        self.syntax = syntax

    def on_package_open(self):
        self._reset_docs()
        self._push_item(self.package)

    def on_package_close(self):
        self._reset_docs()
        self._resolveSymbols()
        self._pop_item()

    def on_package_name(self, name):
        self._reset_docs()
        self.package.setName(name)

    def on_import(self, filename):
        self._reset_docs()
        filename = parse_string_token(filename)
        pkg = Package(filename)
        loader = self.loader.copy()
        builder = PackageBuilder(pkg, loader)
        loader.load(filename, builder)
        if not hasattr(self._item, "addImport"):
            raise BuildError("Cannot add imported package to current item (%r)" % type(self._item))
        self._item.addImport(pkg)

    def on_message_open(self, name):
        doc = self._docblock()
        self._reset_docs()
        msg = Message(name, doc=doc)
        self._push_item(msg)

    def on_message_close(self):
        self._reset_docs()
        msg = self._item
        self._pop_item()
        if not hasattr(self._item, "addMessage"):
            raise BuildError("Cannot add message to current item (%r)" % type(self._item))
        self._item.addMessage(msg)

# TODO: Make extend work as it should
    def on_extend_open(self, name):
        doc = self._docblock()
        self._reset_docs()
#        msg = self.package.getMessage(name)
        msg = Message(name, is_extension=True, doc=doc)
#        if not msg:
#            raise BuildError("Message %s not found, cannot extend it" % name)
        self._push_item(msg)

    def on_extend_close(self):
        self._reset_docs()
        msg = self._item
        self._pop_item()
        if not hasattr(self._item, "addMessage"):
            raise BuildError("Cannot add extended message to current item (%r)" % type(self._item))
        self._item.addMessage(msg)

    def on_extensions(self, start, end):
        self._reset_docs()
        start = int(start)
        if end == "max":
            end = 536870911  # 2^29 - 1
        else:
            end = int(end)
        if not hasattr(self._item, "addExtension"):
            raise BuildError("Cannot add extensions to current item (%r)" % type(self._item))
        if self._item.extensions:
            for ext_start, ext_end in self._item.extensions:
                if (start >= ext_start and start <= ext_end) or (end >= ext_start and end <= ext_end) or (start < ext_start and end > ext_end):
                    raise BuildError("Extension range %d to %d overlaps with already-defined range %d to %d." % ((start, end) + self._item.extensions[0][0:2]))
        self._item.addExtension(start, end)

    def on_field_open(self, quantifier, symbol, name, tag):
        doc = self._docblock()
        self._reset_docs()
        type_ = Proto.find(symbol)
        if type_ == None:
            type_ = Proto.Message
            # TODO: Lookup message or use lookup names after everything is parsed?
        if quantifier == "required":
            q = Quantifier.Required
        elif quantifier == "optional":
            q = Quantifier.Optional
        elif quantifier == "repeated":
            q = Quantifier.Repeated
        else:
            raise NameError("Unknown quantifier %s" % quantifier)
        field = Field(type_, name, int(tag), q, doc=doc)
        if type_ == Proto.Message:

            def updatefield(sym, item):
                field = sym.owner
                if isinstance(field.message, Enum):
                    field.type = Proto.Int32

            sym = field.message = Symbol(symbol, field, "message", (Message, Enum), self._item, callback=updatefield)
            self._symbols.append(sym)
        self._push_item(field)

    def on_field_close(self):
        self._reset_docs()
        field = self._item
        self._pop_item()
        if not hasattr(self._item, "addField"):
            raise BuildError("Cannot add field to current item (%r)" % type(self._item))
        ids = {}
        for existing_field in self._item.fields:
            ids[existing_field.number] = existing_field
        if field.number in ids:
            raise BuildError("Field number %d has already been used in \"%s\" by field \"%s\"" % (field.number, self._item.name, ids[field.number].name))
        self._item.addField(field)

    def on_enum_open(self, name):
        doc = self._docblock()
        self._reset_docs()
        enum = Enum(name, doc=doc)
        self._push_item(enum)

    def on_enum_close(self):
        self._reset_docs()
        enum = self._item
        self._pop_item()
        if not hasattr(self._item, "addEnum"):
            raise BuildError("Cannot add enum to current item (%r)" % type(self._item))
        self._item.addEnum(enum)

    def on_enum_value_open(self, name, value):
        doc = self._docblock()
        self._reset_docs()
        key = EnumValue(name, int(value), doc=doc)
        self._push_item(key)

    def on_enum_value_close(self):
        self._reset_docs()
        key = self._item
        self._pop_item()
        if not hasattr(self._item, "addValue"):
            raise BuildError("Cannot add enum value to current item (%r)" % type(self._item))
        self._item.addValue(key)

    def on_service_open(self, name):
        doc = self._docblock()
        self._reset_docs()
        service = Service(name, doc=doc)
        self._push_item(service)

    def on_service_close(self):
        self._reset_docs()
        service = self._item
        self._pop_item()
        if not hasattr(self._item, "addService"):
            raise BuildError("Cannot add service to current item (%r)" % type(self._item))
        self._item.addService(service)

    def on_rpc_open(self, name, message_in, message_out):
        if not hasattr(self._item, "getCommandCount"):
            raise BuildError("Cannot get command count from current item (%r)" % type(self._item))
        tag = str(self._item.getCommandCount())
        self.on_command_open(name, message_in, message_out, tag)

    def on_rpc_close(self):
        self.on_command_close()

    def on_command_open(self, name, message_in, message_out, tag):
        doc = self._docblock()
        self._reset_docs()
        if message_in == "Default":
            message_in = False
        else:
            message_in = Symbol(message_in, None, "message", Message)
        if message_out == "Default":
            message_out = False
        else:
            message_out = Symbol(message_out, None, "response", Message)
        rpc = Request(int(tag), name, message_in, message_out, doc=doc)
        if message_in != False:
            self._symbols.append(message_in)
            message_in.owner = rpc
        if message_out != False:
            self._symbols.append(message_out)
            message_out.owner = rpc
        self._push_item(rpc)

    def on_command_close(self):
        self._reset_docs()
        rpc = self._item
        self._pop_item()
        if not hasattr(self._item, "addCommand"):
            raise BuildError("Cannot add command to current item (%r)" % type(self._item))
        self._item.addCommand(rpc)

    def on_event_open(self, name, message_out, tag):
        doc = self._docblock()
        self._reset_docs()
        if message_out == "Default":
            message_out = False
        else:
            message_out = Symbol(message_out, None, "message", Message)
        evt = Event(int(tag), name, message_out, doc=doc)
        if message_out != False:
            self._symbols.append(message_out)
            message_out.owner = evt
        self._push_item(evt)

    def on_event_close(self):
        self._reset_docs()
        evt = self._item
        self._pop_item()
        if not hasattr(self._item, "addCommand"):
            raise BuildError("Cannot add event to current item (%r)" % type(self._item))
        self._item.addCommand(evt)

    def on_option(self, name, value, is_custom):
        doc = self._docblock()
        self._reset_docs()
        if not hasattr(self._item, "options"):
            raise BuildError("Cannot add option to current item (%r)" % type(self._item))
        option = OptionValue.loads(value)
        option.doc = doc
        self._item.options[name] = option

    def on_default(self, value):
        self._reset_docs()
        if not hasattr(self._item, "setDefault"):
            raise BuildError("Cannot set default value on current item (%r)" % type(self._item))
        self._item.setDefaultText(value)

        # If the field is an enum then the value must be looked up later, the name is enum value of the enum
        if isinstance(self._item, Field) and self._item.message:
            def set_default_enum(sym, item):
                if isinstance(sym.owner.message, Enum):
                    # Compatibility with code that expects a number in item.default
                    if isinstance(item, EnumValue):
                        sym.owner.setDefaultValue(item.value)
                        sym.owner.setDefaultText(str(item.value))
                    sym.owner.setDefaultObject(item)
                else:
                    raise BuildError("Messages can't have default values")
            enum_sym = Symbol(value, self._item, None, EnumValue, current=self._item.message, callback=set_default_enum)
            self._symbols.append(enum_sym)
        else:
            parsed_value = parse_option_value(value)
            if isinstance(self._item, Field):
                if self._item.type == Proto.Bool:
                    if type(parsed_value) != bool:
                        raise BuildError("Default value for bool field must be either true or false")
                elif self._item.type in (Proto.Int32, Proto.Sint32, Proto.Uint32, Proto.Int64, Proto.Sint64, Proto.Uint64, Proto.Fixed32, Proto.Fixed64):
                    if type(parsed_value) != int:
                        raise BuildError("Default value for numeric field must be a number")
                elif self._item.type == Proto.String:
                    if not isinstance(parsed_value, str):
                        raise BuildError("Default value for string field must be a string or identifier")
            self._item.setDefault(parsed_value)

    def on_comment(self, comment):
        self._comments.append(comment)

    def on_docblock(self, doc):
        self._docs.append(doc)
