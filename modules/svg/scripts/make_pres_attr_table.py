# -*- Mode: python; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-

import sys
import os
import re
import math

# is_standalone is True if the script is not called from the build script
is_standalone = __name__ == "__main__"

sys.path.insert(1, os.path.join(sys.path[0], "..", "..", "hardcore", "scripts"))

import util
import basesetup

class SVGPresentationAttrGenerator(util.WarningsAndErrors):
    """
    Parses presentation_attributes.txt and generates the presentation attributes table.
    """
    def __init__(self):
        util.WarningsAndErrors.__init__(self, sys.stderr)
		self.debug_mode = False

	def __call__(self, sourceRoot, outputRoot, quiet, debug=False, standalone=False):
		global is_standalone
		self.standalone = standalone or is_standalone
		self.debug_mode = debug
		presentation_attrs = {}
		attributes = []
		num_attributes = 0

		if outputRoot is None:
			outputRoot = sourceRoot

		attr_file_name = os.path.join(outputRoot, "modules", "logdoc", "src", "html5", "attrtypes.inl")
		if debug:
			attr_file_name = os.path.join(sourceRoot, "modules", "logdoc", "scripts", "attrtypes.debug.inl")
		pres_attr_file_name = os.path.join(sourceRoot, "modules", "svg", "scripts", "presentation_attributes.txt")
		template_file_name = os.path.join(sourceRoot, "modules", "svg", "scripts", "pres_attrs_template.h")
		out_file_name = os.path.join(outputRoot, "modules", "svg", "presentation_attrs.h")
		if debug:
			out_file_name = os.path.join(sourceRoot, "modules", "svg", "scripts", "presentation_attrs.debug.h")

		# read the list of presentation attributes
		self.logmessage("Reading: %s" % pres_attr_file_name)
		pres_attr_file = open(pres_attr_file_name, "r")
		for line in pres_attr_file:
			new_attr_name = line.strip()[8:]
			presentation_attrs[new_attr_name] = True
		pres_attr_file.close()

		# read the list of all attributes
		self.logmessage("Reading: %s" % attr_file_name)
		skip_next = False
		current_placeholder = ""
		attr_file = open(attr_file_name, "r")
		for line in attr_file:
			found = re.match("[\s,]+(\w+)(\s=\s(\w+))*", line)
			if found:
				if skip_next:
					skip_next = False
				else:
					attr_name = found.group(1)
					attr_value = found.group(3)

					if attr_value:
						if attr_value == current_placeholder:
							attributes.append(attr_name) # put the real attribute name in the list
							current_placeholder = ""
						elif attr_name[0:5] == "SVGA_":
							attributes.pop() # remove the already inserted non-svg entry...
							attributes.append(attr_name) # ...and replace it with the svg one
					else:
						if attr_name[-13:] == "__PLACEHOLDER":
							current_placeholder = attr_name # store the placeholder to check later
						else:
							attributes.append(attr_name)
							current_placeholder = ""

		self.logmessage("Generating: %s" % out_file_name)
        if util.readTemplate(template_file_name, out_file_name, self.BitsetAction(attributes, presentation_attrs)):
			self.logmessage("Success")
            return 2

		self.logmessage("No change")
		return 0

	def logmessage(self, s):
		if self.debug_mode:
			print >>sys.stdout, s

	class BitsetAction:
		def __init__(self, attributes, presentation_attrs):
			self.attributes = attributes
			self.pres_attrs = presentation_attrs

		def __call__(self, action, output):
			if action == "bitset":
				# write the file with the presentation attributes map
				num_attributes = len(self.attributes)
				num_ranges = int(math.ceil(num_attributes / 32.0))

				for i in range(num_ranges):
					bits = 0
					base = i * 32
					for j in range(32):
						if base + j < num_attributes and self.attributes[base + j] in self.pres_attrs:
							bits |= 1 << j;

					if i == 0:
						output.write("\t0x%08x /* bits %d - %d */\n" % (bits, base, base + 31))
					else:
						output.write("\t, 0x%08x /* bits %d - %d */\n" % (bits, base, base + 31))
				return True
			return False

if is_standalone:
    sourceRoot = os.path.normpath(os.path.join(sys.path[0], "..", "..", ".."))

    option_parser = basesetup.OperaSetupOption(
        sourceRoot = sourceRoot,
        description=" ".join(
            ["Generates modules/svg/presentation_attrs.h from ",
             "modules/svg/scripts/presentation_attributes.txt and ",
             "the template modules/svg/scripts/pres_attrs_template.h.",
             "The script exists with status 0 if none of the output files",
             "was changed. The exist status is 2 if at least one of the",
             "output files was changed (see also option '--make').",
             "The exitstatus is 1 if an error was detected."]))
    option_parser.add_option("-d", "--debug", dest="debug_mode", action="store_true")
    option_parser.set_defaults(debug_mode=False)

    (options, args) = option_parser.parse_args(args=sys.argv[1:])

    generator = SVGPresentationAttrGenerator()
    result = generator(sourceRoot=sourceRoot, outputRoot=options.outputRoot, quiet=options.quiet, debug=options.debug_mode)
    if options.make and result == 2:
        sys.exit(0)
    else:
        sys.exit(result)
