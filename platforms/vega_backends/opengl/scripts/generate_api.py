#!/usr/bin/env python
# -*- tab-width: 4; indent-tabs-mode: t; -*-
#
# This script parses the machine-readable description of the OpenGL
# API in vega_backends/opengl/registry/* (files downloaded from
# http://www.opengl.org/registry/) and generates the
# vega_backends/opengl/vegaglapi_{c,h}.inc that are included by
# vega_backends/opengl/vegaglapi.{cpp,h}, respectively. Regenerated
# files should be committed to the source control.
#
# The script takes no command line arguments.

import os
import sys
import string
import re

# Return iterable sequence of non-empty lines from f after stripping comments
def strippedLines(f):
	for line in f:
		line = line.split('#')[0].rstrip()
		if line:
			yield line

# Strip suffixes like _ARB from enum sym defined by extension ext, return (stripped sym, suffix)
def stripEnumSuffix(ext, sym):
	if not ext.startswith('VERSION_'):
		if ext in ('ARB_shader_objects', 'ARB_vertex_shader') and sym.startswith('OBJECT_'): # these two also prefix OBJECT_ to some names
			sym = sym[7:]
		suffix = ext.split('_')[0]
		if suffix.isupper() and sym.endswith('_' + suffix):
			return (sym[ : len(sym) - len(suffix) - 1], suffix)
	return (sym, None)

# Modify the value string so it is interpretable by python (e.g. "0xfful" -> "0xff")
def normalizeEnumValue(value):
	if value.startswith('0x') and value.endswith('ull'):
		return value[:-3]
	return value

# Return something that a C compiler will parse as the parameter's value
def valueToC(value):
	try:
		intvalue = int(value, 0)
	except ValueError:
		intvalue = 0

	if (intvalue > 0xffffffff):
		return value + 'ull'
	return value

# Resolve a type using the type map, if necessary
def resolveType(type):
	global type_map
	return type_map[type] if type in type_map else type

# Wrap code in #ifdef for macro if non-empty; macro can be a list, which will be OR'ed
def ifdef(code, macro):
	if macro:
		if type(macro) == list:
			if len(macro) == 1:
				macro = macro[0]
			else:
				return ["#if %s" % ' || '.join(["defined(%s)" for m in macro])] + code + ["#endif // %s" % ' || '.join(macro)]
		return ["#ifdef %s" % macro] + code + ["#endif // %s" % macro]
	else:
		return code

# Wrap code in #ifndef for macro if non-empty; macro must be a single define
def ifndef(code, macro):
	if macro:
		if type(macro) == list:
			assert "ifndef macro must be a single define"
		return ["#ifndef %s" % macro] + code + ["#endif // !%s" % macro]
	else:
		return code

# Generate code to compare two version numbers (tuples of two expressions each) for relation rel
def comparison(a, rel, b):
	return "%(major1)s %(rel1)s %(major2)s || %(major1)s == %(major2)s && %(minor1)s %(rel2)s %(minor2)s" % {
		'major1': a[0], 'minor1': a[1], 'rel1': rel.replace('=', ''), 'rel2': rel, 'major2': b[0], 'minor2': b[1] }

# Pretty-print code, indenting nested lists further
def dumpCode(code, indent=0, hash_indent=0):
	res = ''
	for item in code:
		if type(item) == str:
			if not item:
				res += '\n'
			elif item == ';' and res.endswith('}\n'):
				res = res[0:-1] + ';\n'
			elif item.startswith('#'):
				if item.startswith('#endif') or item.startswith('#el'):
					hash_indent -= 1
				res += '#' + ' ' * hash_indent + item[1:] + '\n'
				if item.startswith('#if') or item.startswith('#el'):
					hash_indent += 1
			elif re.search(r'^\s*(public:|protected:|private:|case\s|default:)', item):
				res += '\t' * (indent - 1) + item + '\n'
			else:
				res += '\t' * indent + item + '\n'
		else:
			if (len(item) != 1):
				res += '\t' * indent + '{\n'
			res += dumpCode(item, indent + 1, hash_indent)
			if (len(item) != 1):
				res += '\t' * indent + '}\n'
	return res

# Return (format, expression) for dumping expr of type
def dumpValue(type, expr):
	resolved = resolveType(type)
	if type == 'CharPointer':
		return '\\"%s\\"', expr
	if '*' in resolved:
		return '%p', expr
	if resolved in ('GLbyte', 'GLshort', 'GLint', 'GLsizei'):
		return '%d', expr
	if resolved in ('GLubyte', 'GLushort', 'GLuint'):
		return '%u', expr
	if resolved == 'GLfloat' or resolved == 'GLclampf':
		return '%g', "double(%s)" % expr
	if resolved == 'GLdouble' or resolved == 'GLclampd':
		return '%g', expr
	if resolved == 'GLchar':
		return "'%c'", expr
	if resolved == 'GLintptr' or resolved == 'GLsizeiptr':
		return '%ld', "long(%s)" % expr
	if resolved == 'GLboolean':
		return '%s', '%s ? "GL_TRUE" : "GL_FALSE"' % expr
	if resolved == 'GLenum':
		return '%s', "FormatGLenum(%s).CStr()" % expr
	if resolved == 'GLbitfield':
		return '%08x', expr
	if resolved == 'GLDEBUGPROCARB':
		return '%08x', expr
	if resolved == 'GLsync':
			return '%p', expr
	sys.exit("Don't know how to dump type %s" % resolved)

# Recursively go through a list of strings and replace strings
def replaceWordInWrapperCode(word, replace, code):
	out = []
	if type(code) == str:
		return code.replace(word, replace)
	elif type(code) == list:
		for c in code:
			out.append(replaceWordInWrapperCode(word, replace, c))
	else:
		assert "Can only handle strings and list of strings"
	return out

# Function argument
class Parameter(object):
	def __init__(self, name):
		self.name = name + '_val' if name in ('near', 'far') else name # rename to avoid clashes with C reserved words
		self.original_name = name   # name before such renaming
		self.type = None            # OpenGL type
		self.dir = None             # 'in'|'out'
		self.mode = None            # 'value'|'reference'|'array'
		self.arraysize = None       # for 'array' modes: size of array
	# Return argument declaration
	def decl(self):
		return '%s%s%s %s' % ('const ' if self.dir == 'in' and self.mode != 'value' else '',
							  resolveType(self.type),
							  '' if self.mode == 'value' else ' *',
							  self.name)

# OpenGL function
class Function(object):
	def __init__(self, name, paramnames):
		self.name = name                            # function name
		self.category = None                        # VERSION_x_y or extension name
		self.version = None                         # core version
		self.return_type = None                     # OpenGL type
		self.params = map(Parameter, paramnames)    # [Parameter object]
		self.alias = None                           # function name
		self.alternatives = {}                      # extension-defined substitutes
	# Check if function is call-compatible with another
	def compatible(self, other):
		if self.alias == other.name or other.alias == self.name:
			return True
		if self.return_type != other.return_type or len(self.params) != len(other.params):
			return False
		for i in xrange(len(self.params)):
			if self.params[i].mode != other.params[i].mode or self.params[i].dir != other.params[i].dir:
				return False
		return True
	# Return comma-separated argument declarations
	def argList(self):
		return ', '.join([p.decl() for p in self.params])
	# Return function prototype with name symbol
	def decl(self, symbol):
		return '%s %s(%s)' % (resolveType(self.return_type), symbol, self.argList())
	# Return function pointer type declaration with name symbol
	def declP(self, symbol):
		return '%s (VEGA_GL_API_ENTRY * %s) (%s)' % (resolveType(self.return_type), symbol, self.argList())
	# Is it a core OpenGL function?
	def isCore(self):
		return self.category.startswith('VERSION_')

# Change to platforms/vega_backends/opengl
if not os.path.isdir('platforms'):
    os.chdir(os.path.dirname(sys.argv[0]))
    while not os.path.isdir('platforms'):
        prevdir = os.getcwd()
        os.chdir("..")
        if prevdir == os.getcwd():
            break
os.chdir('platforms/vega_backends/opengl')

type_map = {}               # {OpenGL type -> C type}
enum_symbols = {}           # {enum symbol -> value}
enum_sets = {}              # {enum set name -> [enum symbol]}
enum_sets_used = {}         # {enum set name used -> True}
enum_used = {}              # {enum symbol used -> value}
enum_groups = []            # [(enum group name, [enum symbol])]
functions = {}              # {function name -> function object}
func_sets = {}              # {function set name -> [function name]}
func_used = []              # [(function object, required?, [ifdef symbol])]
ext_used = []               # [extension name used]
gles_enum = {}              # {enum symbol -> True}
non_gles_enum = {}          # {enum symbol -> True}
min_version = [1, 1]        # minimum OpenGL version required
min_shader_version = None   # minimum GLSL version required
typedefs = []               # list of all typedefs in gl.spec
typedefs_used = []          # list of typedefs to expose
gles_funcs = []             # list of functions available in gles

# return typedef string for type name as list
def getTypedef(name, condition):
	for l in typedefs:
		n = ""
		# this is a function pointer
		if l[-2] == ')':
			n = l.split(')')[0].split()[-1]
			l = l.replace("APIENTRY", "VEGA_GL_API_ENTRY")
		else:
			n = l.split()[-1]
			if n[-1] == ';':
				n = n[:-1]
		if n[0] == '*':
			n = n[1:]
		if n == name:
			return ifdef([l], condition)
	sys.exit("Could not find typedef: %s" % name)

# Read type map
for line in strippedLines(file('registry/gl.tm')):
	fields = map(string.strip, line.split(','))
	if fields[3] != '*':
		type_map[fields[0]] = fields[3]

# Read enum list
last_set = None
for line in strippedLines(file('registry/enumext.spec')):
	words = line.split()
	if words[0] == 'passthru:':
		pass
	elif words[1] == 'enum:':
		last_set = words[0]
		if last_set in enum_sets:
			sys.exit("Duplicate enum set in enumext.spec: %s" % last_set)
		enum_sets[last_set] = []
	elif words[0] == 'use':
		sym, suffix = stripEnumSuffix(words[1], words[2])
		enum_sets[last_set].append(sym)
	elif words[1] == '=':
		sym, suffix = stripEnumSuffix(last_set, words[0])
		if sym not in enum_symbols or not suffix:
			enum_symbols[sym] = normalizeEnumValue(words[2])
		enum_sets[last_set].append(sym)
	else:
		sys.exit("Malformed line in enumext.spec: %s" % line)

# Read enum groups
last_group = None
for line in strippedLines(file('registry/enum.spec')):
	words = line.split()
	if words[1] == 'define:':
		last_group = None
	elif 'enum:' in words or 'enum::' in words:
		if not last_group or last_group[1]:
			if 'additional' in line:
				last_group = None
			else:
				enum_groups.append((words[0], []))
				last_group = enum_groups[-1]
	elif not last_group:
		pass
	elif words[0] == 'use':
		sym, suffix = stripEnumSuffix(words[1], words[2])
		last_group[1].append((sym, False))
	elif words[1] == '=':
		sym, suffix = stripEnumSuffix(last_group[0], words[0])
		last_group[1].append((sym, True))
	else:
		sys.exit("Malformed line in enum.spec: %s" % line)

# Read function registry
last_func = None
for line in strippedLines(file('registry/gl.spec')):
	if line[0].isspace():
		words = line.split()
		if words[0] == 'return':
			last_func.return_type = words[1]
		elif words[0] == 'param':
			for param in last_func.params:
				if param.original_name == words[1]:
					break
			else:
				sys.exit("Undefined parameter %s for function %s mentioned in enumext.spec" % (words[1], last_func.name))
			param.type = words[2]
			param.dir = words[3]
			param.mode = words[4]
			if param.mode == 'array':
				assert words[5].startswith('[')
				assert words[5].endswith(']')
				param.arraysize = words[5][1:-1]
				for sizeparam in last_func.params:
					if sizeparam.original_name == param.arraysize and sizeparam.mode == 'array':
						param.arraysize = '*' + param.arraysize
		elif words[0] == 'category':
			last_func.category = words[1]
			if words[1] not in func_sets:
				func_sets[words[1]] = [last_func.name]
			else:
				func_sets[words[1]].append(last_func.name)
		elif words[0] == 'version':
			last_func.version = map(int, words[1].split('.'))
		elif words[0] == 'alias':
			last_func.alias = words[1]
	else:
		m = re.match(r'(\w+)\(([\w\s,]*)\)$', line)
		if m:
			if m.group(1) in functions:
				sys.exit("Duplicate function in gl.spec: %s" % m.group(1))
			last_func = Function(m.group(1), map(string.strip, m.group(2).split(',') if m.group(2) else []))
			functions[m.group(1)] = last_func
		else:
			words = line.split()
			if words[0].endswith(':'):
				if words[0] == 'newcategory':
					func_sets[words[1]] = []
				elif words[0] == 'passthru:' and len(words) > 1 and words[1] == 'typedef':
					typedefs.append(' '.join(words[1:]))
			else:
				sys.exit("Malformed line in gl.spec: %s" % line)

# Register aliases as alternatives to aliased functions
for s in func_sets:
	func_set = func_sets[s]
	for i in xrange(len(func_set)):
		f = functions[func_set[i]]
		if f.alias in functions:
			if not functions[f.alias].compatible(f):
				sys.exit("%s mentions %s as alias, but their signatures are incompatible" % (f.name, f.alias))
			func_set[i] = f.alias
			functions[f.alias].alternatives[f.category] = f.name

# Read gl2.h (the only formal description of OpenGL ES 2.0 API)
for line in file('registry/gl2.h'):
	words = line.split()
	if not words:
		pass
	elif words[0] == '#define' and words[1].startswith('GL_'):
		gles_enum[words[1][3:]] = words[2]
	elif words[0] == 'GL_APICALL':
		for w in words:
			if w.startswith('gl'):
				gles_funcs.append(w[2:])

# Read glapi.txt
for line in strippedLines(file('glapi.txt')):
	words = line.replace('|', ' ').split()
	if words[0] == 'use':
		if words[1] == 'optional':
			required = False
			words[1:2] = []
		else:
			required = True
		if words[1] not in functions:
			sys.exit("glapi.txt: %s is undefined" % words[1])
		if len(words) >= 4 and words[-2] == 'ifdef':
			condition = words[-1]
			words = words[:-2]
		else:
			condition = None
		if 'ifdef' in words:
			sys.exit("glapi.txt: misplaced ifdef in use declaration: %s" % line)
		for f in func_used:
			if f[0] == functions[words[1]]:
				sys.exit("glapi.txt: duplicate use lines for %s" % f[0])
		func_used.append((functions[words[1]], required, condition))
		for alt in words[2:]:
			if alt not in functions:
				sys.exit("glapi.txt: %s is undefined" % alt)
			if not functions[alt].compatible(functions[words[1]]):
				sys.exit("glapi.txt: %s is incompatible with %s" % (alt, words[1]))
			functions[words[1]].alternatives[functions[alt].category] = alt
		if not functions[words[1]].isCore() and functions[words[1]].category in enum_sets:
			enum_sets_used[functions[words[1]].category] = True
		for alt in functions[words[1]].alternatives:
			if alt in enum_sets:
				enum_sets_used[alt] = True
		if functions[words[1]].isCore() and functions[words[1]].version > min_version and not functions[words[1]].alternatives:
			print 'Warning: %s has no extension-provided alternatives, raising minimum OpenGL version to %d.%d' % tuple([words[1]] + functions[words[1]].version)
			min_version = functions[words[1]].version
	elif words[0] == 'require':
		ext_used.append(words[1:])
		if len(words) == 2 and '.' in words[1]:
			version = map(int, words[1].split('.'))
			if min_version < version:
				min_version = version
		else:
			for ext in words[1:]:
				if ext in enum_sets:
					enum_sets_used[ext] = True
	elif words[0] == 'enum':
		enum_sets_used[words[1]] = True
	elif words[0] == 'glsl':
		min_shader_version = map(int, words[1].split('.'))
	elif words[0] == 'non-gles':
		non_gles_enum[words[1]] = True
	elif words[0] == 'typedef':
		if len(words) >= 4 and words[-2] == 'ifdef':
			condition = words[-1]
			words = words[:-2]
		else:
			condition = None
		typedefs_used += getTypedef(words[1], condition)
	else:
		sys.exit("glapi.txt: syntax error: %s" % line)

# Use enum sets for required core versions
pattern = re.compile(r'VERSION_(\d+)_(\d+)(_DEPRECATED)?')
for s in enum_sets:
	match = pattern.match(s)
	if match:
		ver = map(int, match.groups()[:2])
		if ver <= min_version:
			enum_sets_used[s] = True
if min_version >= [1, 2]:
	enum_sets_used['ARB_imaging'] = True

# Use all symbols in used enum sets
for s in enum_sets_used:
	for e in enum_sets[s]:
		enum_used[e] = enum_symbols[e]

loader_code = []
pre_loader_code = []
check_ext = {}
header_decls = []
header_macros = []
code_undefs = []
wrapper_decls = []
wrappers = []
format_enum_code = []

# Generate version/extension checks
for alternatives in ext_used:
	ver = alternatives[0].split('.')
	if len(ver) > 1:
		ver = map(int, ver)
		cond = comparison(('ver_major', 'ver_minor'), '<', ver)
		if len(alternatives) > 1:
			if ver > min_version:
				cond = "(%s)" % cond
			else:
				cond = ''
	else:
		cond = "!e_%s" % alternatives[0]
		check_ext[alternatives[0]] = []
	for alt in alternatives[1:]:
		if cond:
			cond += ' && '
		cond += "!e_%s" % alt
		check_ext[alt] = []
	loader_code.append("if (%s)" % cond)
	loader_code.append(ifdef(['g_vega_backends_module.SetCreationStatus(UNI_L("Unsupported backend version"));'], 'VEGA_BACKENDS_USE_BLOCKLIST') +
					   ['return OpStatus::ERR;'])

# Generate per-function code fragments
for f, required, condition in func_used:
	header_decls += ifdef(['typedef %s;' % f.declP(f.name + '_t'), '%s_t m_%s;' % (f.name, f.name)], condition) + ['']
	header_macros += ifdef(["#define gl%-28s %s(%s)" % (f.name, 'VEGA_GLREALSYM' if f.name == 'GetError' else 'VEGA_GLSYM', f.name)], condition)
	if f.name != 'GetError':
		code_undefs += ifdef(["#undef gl%s" % f.name], condition)
	do_load = 'm_%(name)s = reinterpret_cast<%(name)s_t>(dev->getGLFunction("gl%(name)s"));' % {'name': f.name}
	code_arrayvalues = []
	if f.name != 'GetError':
		wrapper_code = ['OP_NEW_DBG("gl%s", "opengl");' % f.name]
		param_format = []
		param_expr = []
		for p in f.params:
			if p.mode == 'value':
				format, expr = dumpValue(p.type, p.name)
				if p.dir == 'in' and resolveType(p.type) in ('GLclampf', 'GLclampd'):
					wrapper_code.append("OP_ASSERT(%s >= 0. && %s <= 1.);" % (p.name, p.name))
			elif p.mode == 'reference':
				format, expr = dumpValue(p.type, '*' + p.name)
			elif (p.mode == 'array' and
				  resolveType(p.type) != 'GLvoid' and
				  p.arraysize and
				  not p.arraysize.startswith('COMPSIZE')):
				code_arrayvalues.append("char _str_" + p.name + "[50];")
				code_arrayvalues.append("_str_" + p.name + "[0] = 0;")
				format, expr = dumpValue(p.type, p.name + '[_i]')
				lengthguardelse = ""
				if p.arraysize.startswith("*"):
					length_guard = "!" + p.arraysize[1:]
					code_arrayvalues.append("if (" + length_guard + ")")
					code_arrayvalues.append([ "_str_" + p.name + "[0] = '?';",  "_str_" + p.name + "[1] = 0;"])
					lengthguardelse = "else "
				code_arrayvalues.append(lengthguardelse + "if (" + p.name + ")")
				code_arrayvalues.append( [
						"size_t _snoffs = 0;",
						"for (int _i = 0; _i < " + p.arraysize + " && _snoffs < 50; _i++)",
						[ "_snoffs += op_snprintf(_str_" + p.name + ' + _snoffs, 50 - _snoffs, " ' + format + '", ' + expr + ');' ]
						] )
				format = '[%s]'
				expr = '_str_' + p.name
			elif (p.mode == 'array' and
				  resolveType(p.type) == 'GLvoid' and
				  p.name == 'string'):
				code_arrayvalues += [
					"int _strlen_" + p.name + " = " + p.arraysize + ";",
					"if (_strlen_" + p.name + " == 0) _strlen_" + p.name + " = op_strlen((char*)" + p.name + ");"
					]
				format = "'%.*s'"
				expr = "_strlen_" + p.name + ", " + p.name
			elif (p.mode == 'array' and
				  resolveType(p.type) == 'GLvoid'):
					format = "%p"
					expr = p.name
			else:
				format = '?'
				expr = None
			param_format.append(format)
			if expr:
				param_expr.append(expr)
		if f.return_type == 'void':
			wrapper_code.append("VEGA_GLREALSYM(%s) (%s);" % (f.name, ', '.join([p.name for p in f.params])))
		else:
			wrapper_code.append("%s _return = VEGA_GLREALSYM(%s) (%s);" % (resolveType(f.return_type), f.name, ', '.join([p.name for p in f.params])))
		wrapper_code.append("%s _error = glGetError();" % resolveType(functions['GetError'].return_type))
		for codeline in code_arrayvalues:
			wrapper_code.append(codeline)
		wrapper_code.append('if (_error != GL_NO_ERROR)')
		error_format, error_expr = dumpValue(functions['GetError'].return_type, '_error')
		wrapper_code.append([
			'OP_DBG(("(%s) -> error %s", %s%s));' % (', '.join(param_format), error_format, ', '.join(param_expr) + ', ' if param_expr else '', error_expr),
			'OP_ASSERT(!"OpenGL error in gl%s");' % f.name])
		wrapper_code.append('else')
		if f.return_type == 'void':
			if param_format:
				wrapper_code.append(['OP_DBG(("(%s)", %s));' % (', '.join(param_format), ', '.join(param_expr))])
			else:
				wrapper_code.append(['OP_DBG(("()"));'])

		else:
			return_format, return_expr = dumpValue(f.return_type, '_return')
			wrapper_code.append(['OP_DBG(("(%s) = %s", %s%s));' % (', '.join(param_format), return_format, ', '.join(param_expr) + ', ' if param_expr else '', return_expr)])
			wrapper_code.append('return _return;');

		wrapper_code = ifdef([f.decl('VEGAGlAPI::debug_' + f.name), wrapper_code], condition)
		wrapper_decl = ifdef(["static %s;" % f.decl('debug_' + f.name)], condition)
		if f.name not in gles_funcs:
			wrappers += ifndef(wrapper_code, 'VEGA_OPENGLES')
			wrapper_decls += ifndef(wrapper_decl, 'VEGA_OPENGLES')
			# Check if there is a float equivalent to this function, solves glClearDepth(f) and glDepthRange(f) issues
			if f.name+"f" in gles_funcs:
				wrapper_code = replaceWordInWrapperCode(f.name, f.name+"f", wrapper_code)
				wrapper_code = replaceWordInWrapperCode("GLclampd", "GLclampf", wrapper_code)
				wrapper_code = replaceWordInWrapperCode("GLdouble", "GLfloat", wrapper_code)
				wrapper_decl = replaceWordInWrapperCode(f.name, f.name+"f", wrapper_decl)
				wrapper_decl = replaceWordInWrapperCode("GLclampd", "GLclampf", wrapper_decl)
				wrapper_decl = replaceWordInWrapperCode("GLdouble", "GLfloat", wrapper_decl)
				wrappers += ifdef(wrapper_code, 'VEGA_OPENGLES')
				wrapper_decls += ifdef(wrapper_decl, 'VEGA_OPENGLES')
		else:
			wrappers += wrapper_code
			wrapper_decls += wrapper_decl
	loader_fragment = []
	if f.isCore() and f.version <= min_version:
		loader_fragment.append(do_load)
	else:
		if f.isCore():
			loader_fragment.append("if (%s)" % comparison(('ver_major', 'ver_minor'), '>=', f.version))
		else:
			if condition:
				if f.category not in check_ext:
					check_ext[f.category] = [condition]
				elif check_ext[f.category]:
					check_ext[f.category].append(condition)
			else:
				check_ext[f.category] = []
			loader_fragment.append("if (e_%s)" % f.category)
		loader_fragment.append([do_load])
		for cat in f.alternatives:
			if condition:
				if cat not in check_ext:
					check_ext[cat] = [condition]
				elif check_ext[cat]:
					check_ext[cat].append(condition)
			else:
				check_ext[cat] = []
			alt = functions[f.alternatives[cat]]
			loader_fragment.append("else if (e_%s)" % cat)
			loader_fragment.append(['m_%(name)s = reinterpret_cast<%(name)s_t>(dev->getGLFunction("gl%(alt)s"));' % {'name': f.name, 'alt': alt.name}])
		loader_fragment.append('else')
		if required:
			loader_fragment.append(ifdef(['g_vega_backends_module.SetCreationStatus(UNI_L("Unsupported backend version"));'], 'VEGA_BACKENDS_USE_BLOCKLIST') +
					   ['return OpStatus::ERR;'])
		elif f.name == 'StringMarkerGREMEDY':
			loader_fragment.append(["m_%s = local_ignore_stringmarkergremedy;" % f.name])
		else:
			loader_fragment.append(["m_%s = 0;" % f.name])
	if required:
		loader_fragment.append("if (!m_%s)" % f.name)
		loader_fragment.append(['return OpStatus::ERR;'])
	if f.name == 'GetString':
		pre_loader_code += ifdef(loader_fragment, condition)
	else:
		loader_code += ifdef(loader_fragment, condition)

# Generate version/extension string parsing code
ext_check_code = []
use_else = ''
pre_loader_code += [
	'',
	'int ver_major, ver_minor;',
	'const char * ver_string = reinterpret_cast<const char*>(m_GetString(GL_VERSION));',
	'if (!ver_string || op_sscanf(ver_string, "%d.%d", &ver_major, &ver_minor) != 2)',
	['return OpStatus::ERR;'],
	'']
for ext in sorted(check_ext):
	pre_loader_code += ifdef(['bool e_%s = false;' % ext], check_ext[ext])
	if not use_else and check_ext[ext]:
		ext_check_code += 'if (false) {}'
		use_else = 'else '
	ext_check_code += ifdef(['%sif (len == %d && op_strncmp(ext_string, "GL_%s", %d) == 0)' % (use_else, len(ext)+3, ext, len(ext)+3),
							 ['e_%s = true;' % ext]], check_ext[ext])
	use_else = 'else '
pre_loader_code += [
	'',
	'const char * ext_string = reinterpret_cast<const char*>(m_GetString(GL_EXTENSIONS));',
	'while (*ext_string)',
	['size_t len = op_strcspn(ext_string, " ");'] + ext_check_code + [
		'ext_string += len;',
		"while (*ext_string == ' ')",
		['ext_string++;']
	 ]
	]
if min_shader_version:
	loader_code += [
		'',
		'int shader_ver_major, shader_ver_minor;',
		'const char * shader_ver_string = reinterpret_cast<const char*>(m_GetString(GL_SHADING_LANGUAGE_VERSION));',
		'if (!shader_ver_string || op_sscanf(shader_ver_string, "%d.%d", &shader_ver_major, &shader_ver_minor) != 2)',
		['return OpStatus::ERR;'],
		"if (%s)" % comparison(('shader_ver_major', 'shader_ver_minor'), '<', min_shader_version),
		ifdef(['g_vega_backends_module.SetCreationStatus(UNI_L("Unsupported backend version"));'], 'VEGA_BACKENDS_USE_BLOCKLIST') +
		['return OpStatus::ERR;']
		]

common_header = [
	'/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-',
	'** Auto-generated by scripts/generate_api.py from glapi.txt.',
	'** DO NOT EDIT THIS FILE MANUALLY.',
	'** To update, run the script mentioned above.',
	'**',
	'** Copyright Opera Software ASA. All rights reserved.',
	'** This file is part of the Opera web browser. It may not be distributed',
	'** under any circumstances.',
	'*/',
	'',
	'#ifdef OPENGL_REGISTRY',
	'']

common_footer = [
	'',
	'#endif // OPENGL_REGISTRY']

# Generate per-enum code
header_enum = []
header_enum_mentioned = {}
code_enum_mentioned = {0: True, 1: True}
format_enum_code += ['case 0:', 'res.Set("0");', 'break;', 'case 1:', 'res.Set("1");', 'break;']
for group in enum_groups:
	header_enum_sec = []
	defined_any = False
	for sym, do_define in group[1]:
		if do_define and sym in enum_used:
			if sym in gles_enum:
				prefix = 'GL_'
			elif sym in non_gles_enum:
				prefix = 'non_gles_GL_'
			else:
				continue
			if sym not in header_enum_mentioned:
				header_enum_sec.append("#define %-55s %s" % (prefix + sym, valueToC(enum_symbols[sym])))
				header_enum_mentioned[sym] = True
				defined_any = True
			if sym.endswith('_BIT') or sym.endswith('_BITS') or enum_symbols[sym].startswith('GL_'):
				continue
			if sym == 'TIMEOUT_IGNORED':
				# Not used as a GLenum and causes a compiler warning.
				continue
			if int(enum_symbols[sym], 0) not in code_enum_mentioned:
				enum_code = ["case %s:" % (prefix + sym), 'res.Set("GL_%s");' % sym, 'break;']
				if sym in non_gles_enum:
					format_enum_code += ifndef(enum_code, 'VEGA_OPENGLES')
				else:
					format_enum_code += enum_code
				code_enum_mentioned[int(enum_symbols[sym], 0)] = True
	if defined_any:
		header_enum += header_enum_sec + ['']
format_enum_code += ['default:', 'res.AppendFormat("0x%04x", value);']

# Put all generated code together
header_code = common_header + typedefs_used + [''] + [
	'class VEGAGlDevice;',
	'',
	'class VEGAGlAPI',
	['public:',
	 '#ifndef VEGA_OPENGLES',
	 ''
	 'OP_STATUS Init(VEGAGlDevice * dev);',
	 ''] + header_decls + ['#endif // !VEGA_OPENGLES', ''] + ifdef(['static OpString8 FormatGLenum(GLenum value);', ''] + wrapper_decls, 'VEGA_GL_DEBUG'),
	';', '', '#ifndef VEGA_OPENGLES', ''] + header_enum + ['#endif // !VEGA_OPENGLES', ''] + header_macros + common_footer
generated_code = common_header + code_undefs + [
	'',
	'void local_ignore_stringmarkergremedy(GLsizei, const GLvoid*) {};',
	'',
	'#ifndef VEGA_OPENGLES',
	'OP_STATUS VEGAGlAPI::Init(VEGAGlDevice * dev)',
	pre_loader_code + [''] + loader_code + ['', 'return OpStatus::OK;'],
	'#endif // VEGA_OPENGLES',
	''] + ifdef(['', 'OpString8 VEGAGlAPI::FormatGLenum(GLenum value)',
				['OpString8 res;', 'switch (value)', format_enum_code, 'return res;'],
				 ''] + wrappers + [''], 'VEGA_GL_DEBUG') + common_footer

# Write out generated code
open('vegaglapi_h.inc', 'w').write(dumpCode(header_code))
print 'Updated vegaglapi_h.inc'
open('vegaglapi_c.inc', 'w').write(dumpCode(generated_code))
print 'Updated vegaglapi_c.inc'
