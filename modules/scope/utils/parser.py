#!/usr/bin/env python

tokens = (
	'COMMENT','ASSIGNMENT','ENDRULE','NONTERMINAL','STRING','OR','LEFTPAREN','RIGHTPAREN','OPTIONAL','WHITESPACE','KLEENESTAR','NONZERO', 'RANGE','TEXTUALDATA'
	)

# Tokens

t_ignore_WHITESPACE  = r'[ \r\n]+' # tab purposly dropped - it should generate an error
t_ASSIGNMENT  = r'::='
t_ENDRULE     = r';'
t_OR          = r'\|'
t_KLEENESTAR  = r'\*'
t_NONZERO     = r'\+'
t_OPTIONAL    = r'\?'
t_LEFTPAREN   = r'\('
t_RIGHTPAREN  = r'\)'
t_STRING      = r'"[^"]*"'
t_NONTERMINAL = r'[A-Z][A-Z0-9-]*'
t_RANGE       = r'\[[^\]]*\](\{[0-9]*\})?'
t_TEXTUALDATA = r'textual-data'
#t_COMMENT     = r'\#[^$]*'
	
def t_COMMENT(t):
	r'\#[^\r\n]*'
	#print "Ignoring comment: %s" % t.value
	# ignored

# Build the lexer
try:
	import ply.lex as lex
except ImportError:
	print 'You need the module ply, available from http://www.dabeaz.com/ply/'
	import sys
	sys.exit()
lex.lex()

# RULES
class BaseRule:
	def is_valid_xml(self, prodname):
		return True

class String(BaseRule):
	def __init__(self, str):
		self.str = str
	def __str__(self):
		return '"%s"' % self.str
	def is_valid_xml(self, prodname):
		return not self.str.startswith('<') or self.str.endswith('/>')

class Nonterminal(BaseRule):
	def __init__(self, name):
		nonterminals_used.add(name)
		self.name = name
	def __str__(self):
		return '%s' % self.name

class Ignored(BaseRule):
	def __init__(self):
		pass
	def __str__(self):
		return '<IGNORED>'

class RuleAnd:
	def __init__(self, rule1, rule2):
		self.rules = []
		for rule in rule1, rule2:
			if rule.__class__ == RuleAnd:
				self.rules.extend(rule.rules)
			else:
				self.rules.append(rule)

	def __str__(self):
		return '(%s)' % ' '.join([str(s) for s in self.rules])
	def is_valid_xml(self, prodname):
		valid = [rule.is_valid_xml(prodname) for rule in self.rules]
		if all(valid):
			return True
		if any(r.__class__ != String for r in (self.rules[0], self.rules[-1])):
			print "Rule %s not valid XML: does not start and end with a string" % prodname
			return False
		if self.rules[0].str.startswith('<') and self.rules[0].str.endswith('>'):
			xmlname = self.rules[0].str[1:-1].split(' ',1)[0]
			if xmlname.upper() != prodname:
				print "Rule %s does not pass XML validation because xmlentity '%s' does not match production name '%s'" % (prodname, xmlname, prodname)
				return False
			if self.rules[-1].str.startswith('</' + xmlname) and self.rules[-1].str.endswith('>'):
				return True
		# A lot of false negatives can slip through, but it's ok for the protocols this far
		return False

class RuleOr(BaseRule):
	def __init__(self, rule1, rule2):
		self.rules = []
		for rule in rule1, rule2:
			if rule.__class__ == RuleOr:
				self.rules.extend(rule.rules)
			else:
				self.rules.append(rule)

	def __str__(self):
		return '(%s)' % ' | '.join([str(s) for s in self.rules])

	def is_valid_xml(self, prodname):
		return all([rule.is_valid_xml(prodname) for rule in self.rules])

class Optional:
	def __init__(self, rule):
		self.rule = rule

	def __str__(self):
		return '(%s)?' % self.rule

	def is_valid_xml(self, prodname):
		return self.rule.is_valid_xml(prodname)

class Multiple(BaseRule):
	def __init__(self, rule):
		self.rule = rule

	def __str__(self):
		return '(%s)*' % self.rule
	def is_valid_xml(self, prodname):
		return self.rule.is_valid_xml(prodname)

class Nonzero:
	def __init__(self, rule):
		self.rule = rule

	def __str__(self):
		return '(%s)+' % self.rule

	def is_valid_xml(self, prodname):
		return self.rule.is_valid_xml(prodname)

class Textual(BaseRule):
	def __init__(self):
		pass

	def __str__(self):
		return 'textual-data'
# END RULES


class Production:
	def __init__(self, prod, rule):
		self.prod = prod
		self.rule = rule
		if prod in productions_defined:
			print 'Redefinition of production %s' % prod
			import sys
			sys.exit(1) # FIXME: Propagate error instead
		productions_defined.add(prod)
	def __str__(self):
		return "%s ::= %s ;" % (self.prod, self.rule)
	def is_valid_xml(self, prodname):
		return self.rule.is_valid_xml(prodname)
		
def p_grammar(t):
	'''grammar : production grammar
               | production'''
	if len(t) == 3:
		t[0] = [t[1]] + t[2]
	else:
		t[0] = [t[1]]

def p_production(t):
	'production : NONTERMINAL ASSIGNMENT rule ENDRULE'
	t[0] = Production(t[1], t[3])

def p_rule_nonterm(t):
	'rule : NONTERMINAL'
	t[0] = Nonterminal(t[1])

def p_rule_string(t):
	'rule : STRING'
	t[0] = String(t[1][1:-1].replace('&lt;', '<').replace('&gt;', '>'))

def p_rule_parens(t):
	'rule : LEFTPAREN rule RIGHTPAREN'
	t[0] = t[2]

def p_rule_and(t):
	'rule : rule rule'
	t[0] = RuleAnd(t[1], t[2])

def p_rule_or(t):
	'rule : rule OR rule'
	t[0] = RuleOr(t[1], t[3])

def p_rule_optional(t):
	'rule : rule OPTIONAL'
	t[0] = Optional(t[1])

def p_rule_multiple(t):
	'rule : rule KLEENESTAR'
	t[0] = Multiple(t[1])

def p_rule_nonzero(t):
	'rule : rule NONZERO'
	t[0] = Nonzero(t[1])

def p_rule_textual(t):
	'rule : TEXTUALDATA'
	t[0] = Textual()

def p_rule(t):
	'rule : RANGE'
	t[0] = Ignored()

#def p_error(t):
	#if t:
		#print "Syntax error at '%s'" % t.value
	#else:
		#print "Syntax error: Did not expect end of line"

import ply.yacc as yacc
yacc.yacc()

def parsefile(file):
	global productions_defined
	global nonterminals_used
	productions_defined = set()
	nonterminals_used = set()

	data = open(file).read()
	startstr = '<pre id="grammar">'
	if data.find(startstr) != -1:
		data = data[data.find(startstr)+len(startstr):data.find('</pre>')]
	from ply.lex import LexError
	try:
		grammar = yacc.parse(data)
	except LexError, err:
		print "Could not parse %s" % file
		if verbose: raise err
		return False

	if verbose:
		print '\n'.join(['%s' % rul for rul in grammar])

	ok = True
	invalid_xmls = [rul.prod for rul in grammar if not rul.is_valid_xml(rul.prod)]
	unused_productions = productions_defined - nonterminals_used
	undefined_nonterminals = nonterminals_used - productions_defined

	if verbose:
		print
		if undefined_nonterminals:
			print 'Undefined nonterminals: %s' % ', '.join(undefined_nonterminals)
		if unused_productions:
			print 'Unused productions: %s' % ', '.join(unused_productions)
		if invalid_xmls:
			print 'Invalid XML: %s' % ', '.join(rul.prod for rul in grammar if not rul.is_valid_xml(rul.prod))
	return len(invalid_xmls) == 0 and len(unused_productions) == 1 and len(undefined_nonterminals) == 0

def main():
	from sys import argv
	global verbose

	if len(argv) == 1:
		print
		print 'Usage: %s <documentation file in html> to analyse a file' % argv[0]
		print 'This is the interactive console to write BNFs'
		while 1:
			try:
				s = raw_input('rule > ')
			except EOFError:
				break
			yacc.parse(s)
	else:
		verbose = len(argv) <= 2
		for file in argv[1:]:
			res = parsefile(file)
			print '%s: %s' % (file, ('passed' if res else 'failed'))

if __name__ == '__main__':
	main()

