import os.path
import re
import sys

# import from modules/hardcore/scripts/:
if __name__ == "__main__":
    sys.path.insert(1, os.path.join(sys.path[0], "..", "scripts"))
import util
import basesetup
import opera_module

# import from modules/dom/scripts/:
sys.path.append(os.path.join(sys.path[0], "..", "..", "dom", "scripts"))
from keyevent import DOM

class Key:
    def __init__(self, filename, symbol, name, offset, flags):
        self.filename = filename
        self.symbol = symbol
        self.name = name
        self.offset = offset
        self.flags = set()
        self.values = dict()
        for flag in flags:
            match = re.match("^\\s*([^= \t]+)\\s*=(.*)$", flag)
            if match:
               self.values[match.group(1)] = match.group(2)
            else:
               self.flags.add(flag)

    def getSymbol(self): return self.symbol

class GenerateOpKeys(basesetup.SetupOperation):
    def __init__(self):
        basesetup.SetupOperation.__init__(self, message="keys system")
        # List of keys, ordered by occurrence.
        self.__keys = None
        # Dictionary mapping key symbols (OP_KEY_*) to Key objects.
        self.__keysBySymbol = None
        # Dictionary mapping key names to Key objects.
        self.__keysByName = None

    def parseKeys(self, filename, module):
        errors = []
        update_offset = True

        if util.fileTracker.addInput(filename):
            f = None
            try:
                f = open(filename)
                next_offset = self.__next_offset
                for linenr, line in enumerate(f):
                    match = re.match("^\\s*([A-Za-z0-9_]+)(\\s+([1-9][0-9]*|0x[0-9a-zA-Z]+))?\\s+([A-Za-z0-9_-]+)(?:\\s+([A-Za-z_0-9=, \t]+?))?\\s+$", line)
                    if match:
                        symbol = match.group(1)
                        if match.group(3):
                            offset = match.group(3)
                            # If the keys file uses an explicit offset, it is only
                            # used as a running counter within its scope.
                            update_offset = False
                            next_offset = int(offset, 0) + 1
                        else:
                            offset = str(next_offset)
                            next_offset = next_offset + 1
                        name = match.group(4)
                        if match.group(5):
                            flags = map(str.strip, match.group(5).split(","))
                        else: flags = []

                        if symbol in self.__keysBySymbol:
                            if self.__keysBySymbol[symbol].name != name:
                                util.addError(
                                    errors, filename, linenr,
                                    "multiple definitions of %(symbol)s defines different names; first defined in %(filename)s" % {
                                        "symbol": symbol,
                                        "filename": self.__keysBySymbol[symbol].filename })
                            continue

                        if name in self.__keysByName:
                            util.addError(
                                errors, filename, linenr,
                                "definition of %(symbol)s and %(other_symbol)s use the same name; %(other_symbol)s was defined in %(other_file)s" % {
                                    "symbol": symbol,
                                    "other_symbol": self.__keysByName[name].symbol,
                                    "other_file": self.__keysByName[name].filename })
                            continue

                        key = Key(filename, symbol, name, offset, flags)
                        self.__keys.append(key)
                        self.__keysBySymbol[symbol] = key
                        self.__keysByName[name] = key
            finally:
                if f: f.close()
                if update_offset:
                    self.__next_offset = next_offset
        return errors

    class OpKeysTemplateAction:
        def __init__(self, keys, keysBySymbol, keysByName):
            self.__keys = keys
            self.__keysBySymbol = keysBySymbol
            self.__keysByName = keysByName
            self.__case_template = "".join(
                ["#ifdef %(symbol)s_ENABLED\n",
                 "\tcase %(symbol)s:\n",
                 "#endif // %(symbol)s_ENABLED\n"])

        def __call__(self, action, output):
          if action == "check defines":
              for key in self.__keys:
                  if "enabled" in key.flags:
                      template = "".join(
                          ["#if defined ENABLE_%(symbol)s && ENABLE_%(symbol)s != YES\n",
                           "#\terror \"You cannot disable the always-enabled %(symbol)s (%(name)s).\"\n",
                           "#\telse\n",
                           "#undef ENABLE_%(symbol)s\n",
                           "#\tdefine ENABLE_%(symbol)s YES\n",
                           "#endif // !defined ENABLE_%(symbol)s && ENABLE_%(symbol)s != YES\n"])
                      text = template % { "symbol": key.symbol, "name": key.name }
                      output.write(text)
                  else:
                      template = "".join(
                          ["#if !defined ENABLE_%(symbol)s || (ENABLE_%(symbol)s != YES && ENABLE_%(symbol)s != NO)\n",
                           "#\terror \"You need to decide if %(symbol)s (%(name)s) should be enabled.\"\n",
                           "#endif // !defined ENABLE_%(symbol)s || (ENABLE_%(symbol)s != YES && ENABLE_%(symbol)s != NO)\n"])
                      text = template % { "symbol": key.symbol, "name": key.name }
                      output.write(text)
                  output.write(text)
                  template = "".join(
                      ["#if ENABLE_%(symbol)s == YES\n",
                       "#\tdefine %(symbol)s_ENABLED 1\n",
                       "#endif // ENABLE_%(symbol)s == YES\n",
                       "#undef ENABLE_%(symbol)s\n"])
                  text = template % { "symbol": key.symbol, "name": key.name }
                  output.write(text)
              return True

          elif action == "enum values":
              max_extended_offset = 0
              for key in self.__keys:
                  template = "".join(
                      ["#ifdef %(symbol)s_ENABLED\n",
                       "\t%(symbol)s", "%(offset)s", ",\n",
                       "#endif // %(symbol)s_ENABLED\n"])
                  if key.offset != 0:
                      base = " = OP_KEY_FIRST + "
                      if "extended" in key.flags:
                          base = " = OP_KEY_FIRST_EXTENDED + "
                          max_extended_offset = max(max_extended_offset, int(key.offset))
                      off = base + key.offset
                  else:
                      off = ""
                  output.write(template % {"symbol": key.symbol, "offset": off})
                  for k, value in key.values.iteritems():
                    if k == "key_synonym":
                       template = "".join(
                            ["#ifdef %(symbol)s_ENABLED\n",
                             "\t%(synonym)s", " = %(symbol)s", ",\n",
                             "#endif // %(symbol)s_ENABLED\n"])
                       output.write(template % {"symbol": key.symbol, "synonym": value})
              #
              # Terminator for all extended keys. Needed by
              # range checking predicate.
              #
              output.write("\tOP_KEY_LAST_EXTENDED = OP_KEY_FIRST_EXTENDED + " + str(max_extended_offset+1) + ",\n")
              return True

          elif action == "is modifier cases":
              for key in self.__keys:
                  if "modifier" in key.flags:
                      output.write(self.__case_template % {"symbol": key.symbol})
              return True

          elif action == "is mouse button cases":
              for key in self.__keys:
                  if "mouse_button" in key.flags:
                      output.write(self.__case_template % {"symbol": key.symbol})
              return True

          elif action == "is gesture cases":
              for key in self.__keys:
                  if "gesture" in key.flags:
                      output.write(self.__case_template % {"symbol": key.symbol})
              return True

          elif action == "is flip cases":
              for key in self.__keys:
                  if "flip" in key.flags:
                      output.write(self.__case_template % {"symbol": key.symbol})
              return True

          elif action == "key to string":
              for key in self.__keys:
                  if not "type_only" in key.flags:
                      template = "".join(
                          ["#ifdef %(symbol)s_ENABLED\n",
                           "\tcase %(symbol)s: return UNI_L(\"%(name)s\");\n",
                           "#endif // %(symbol)s_ENABLED\n"])
                      output.write(template % {"symbol": key.symbol, "name": key.name})
              return True

          elif action == "string to key":
              for key in self.__keys:
                  if not "type_only" in key.flags:
                      template = "".join(
                          ["#ifdef %(symbol)s_ENABLED\n",
                           "\tif (uni_stri_eq(str, \"%(name)s\")) return %(symbol)s;\n",
                           "#endif // %(symbol)s_ENABLED\n"])
                      output.write(template % {"symbol": key.symbol, "name": key.name})
                      for k, value in key.values.iteritems():
                          if k == "synonym":
                              template = "".join(
                                  ["#ifdef %(symbol)s_ENABLED\n",
                                   "\tif (uni_stri_eq(str, \"%(name)s\")) return %(symbol)s;\n",
                                   "#endif // %(symbol)s_ENABLED\n"])
                              output.write(template % {"symbol": key.symbol, "name": value})
              return True

          elif action == "location cases":
              for key in self.__keys:
                  template = "".join(
                      ["#ifdef %(symbol)s_ENABLED\n",
                       "\tcase %(symbol)s: return LOCATION_%(location)s;\n",
                       "#endif // %(symbol)s_ENABLED\n"])
                  location = ""
                  if "left" in key.flags:
                     location = "LEFT"
                  elif "right" in key.flags:
                     location = "RIGHT"
                  elif "numpad" in key.flags:
                     location = "NUMPAD"
                  elif "mobile" in key.flags:
                     location = "MOBILE"
                  elif "joystick" in key.flags:
                     location = "JOYSTICK"
                  if location != "":
                     output.write(template % {"symbol": key.symbol, "location": location})
              return True
          elif action == "has no key value":
              for key in self.__keys:
                  if "no_key_value" in key.flags:
                     template = "".join(
                          ["#ifdef %(symbol)s_ENABLED\n",
                           "\tcase %(symbol)s:\n",
                           "#endif // %(symbol)s_ENABLED\n"])
                     output.write(template % {"symbol": key.symbol})
              return True
          elif action == "with char value":
              for key in self.__keys:
                  for k, value in key.values.iteritems():
                    if k == "char_value":
                       template = "".join(
                            ["#ifdef %(symbol)s_ENABLED\n",
                             "\tcase %(symbol)s: return %(value)s;\n",
                             "#endif // %(symbol)s_ENABLED\n"])
                       output.write(template % {"symbol": key.symbol, "value": value})
              return True
          elif action == "is function key":
              for key in self.__keys:
                  if "function" in key.flags:
                      output.write(self.__case_template % {"symbol": key.symbol})
              return True

          # Generate DOM_VK_<X> constants on the key event object, corresponding to OP_KEY_<X>.
          # Only do this for the range [0, 0xff] of "non-extended" virtual keys that hardcore
          # predefines. That set of keys can be augmented by a product if it defines up a key
          # at an offset that is either currently unused or is at one of the OEM-unmapped entries.
          #
          elif action == "dom key constants":
              for key in self.__keys:
                   off = int(key.offset, 0)
                   if off < 256 or "extended" in key.flags:
                       dom_code = DOM.emitKeyConstant(key.symbol)
                       if dom_code:
                           template = "".join(
                               ["#ifdef %(symbol)s_ENABLED\n",
                                "\t%(code)s\n",
                                "#endif // %(symbol)s_ENABLED\n"])
                           output.write(template % {"symbol": key.symbol, "code": dom_code})

              return True

    def loadKeys(self, sourceRoot):
        self.__keys = []
        self.__keysBySymbol = {}
        self.__keysByName = {}
        self.__next_offset = 0
        errors = []

        for module in opera_module.modules(sourceRoot):
            errors.extend(self.parseKeys(os.path.join(sourceRoot, module.relativePath(), "module.keys"), module))

        if errors:
            for error in sorted(errors):
                print >>sys.stderr, error

            return False
        else:
            return True

    def __call__(self, sourceRoot, outputRoot=None, quiet=True):
        self.startTiming()
        if self.loadKeys(sourceRoot):
            hardcoreDir = os.path.join(sourceRoot, 'modules', 'hardcore')
            keysDir = os.path.join(hardcoreDir, 'keys')
            domModuleDir = os.path.join(sourceRoot, 'modules', 'dom')
            domDir = os.path.join(domModuleDir, 'src', 'domevents')
            if not outputRoot or outputRoot == sourceRoot:
                keysOutputDir = keysDir
                domOutputDir = domDir
            else:
                keysOutputDir = os.path.join(outputRoot, 'modules', 'hardcore', 'keys')
                domOutputDir = os.path.join(outputRoot, 'modules', 'dom', 'src', 'domevents')
            handleTemplateAction = GenerateOpKeys.OpKeysTemplateAction(
                self.__keys, self.__keysBySymbol, self.__keysByName)
            changed = util.readTemplate(os.path.join(keysDir, 'opkeys_template.h'),
                                        os.path.join(keysOutputDir, 'opkeys.h'),
                                        handleTemplateAction,
                                        hardcoreDir if keysOutputDir == keysDir else None,
                                        'keys/opkeys.h')
            changed = util.readTemplate(os.path.join(keysDir, 'opkeys_mappings_template.inc'),
                                        os.path.join(keysOutputDir, 'opkeys_mappings.inc'),
                                        handleTemplateAction,
                                        hardcoreDir if keysOutputDir == keysDir else None,
                                        'keys/opkeys_mappings.inc') or changed
            changed = util.readTemplate(os.path.join(domDir, 'domevent_keys_template.cpp'),
                                        os.path.join(domOutputDir, 'domevent_keys.cpp'),
                                        handleTemplateAction,
                                        domModuleDir if domOutputDir == domDir else None,
                                        'domevent_keys.cpp') or changed

            if changed: result = 2 # files have changed
            else: result = 0 # no changes
        else: result = 1 # error
        return self.endTiming(result, quiet=quiet)

if __name__ == "__main__":
    sourceRoot = os.path.normpath(os.path.join(sys.path[0], "..", "..", ".."))
    option_parser = basesetup.OperaSetupOption(
        sourceRoot=sourceRoot,
        description=" ".join(
            ["Generate the files opkeys_mappings.inc and opkeys.h in",
             "modules/hardcore/keys/ from",
             "{adjunct,modules,platforms}/readme.txt and",
             "{adjunct,modules,platforms}/*/module.keys.",
             "The script exists with status 0 if none of the output files",
             "was changed. The exist status is 2 if at least one of the",
             "output files was changed (see also option '--make').",
             "The exitstatus is 1 if an error was detected."]))
    (options, args) = option_parser.parse_args(args=sys.argv[1:])
    generate_actions = GenerateOpKeys()
    result = generate_actions(sourceRoot, outputRoot=options.outputRoot, quiet=options.quiet)
    if options.timestampfile and result != 1:
        util.fileTracker.finalize(options.timestampfile)
    if options.make and result == 2: sys.exit(0)
    else: sys.exit(result)
