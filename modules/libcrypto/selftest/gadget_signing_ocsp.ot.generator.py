#!/usr/bin/env python

import sys
import os
import re

# import from modules/hardcore/scripts/:
if __name__ == "__main__":
	sys.path.insert(1, os.path.join(sys.path[0], "..", "..", "hardcore", "scripts"))
import util
import basesetup


# ----- begin functions -----

def resolve_signature_filename(sig_num):
	if sig_num > 0:
		return "signature" + str(sig_num) + ".xml"
	else:
		return "author-signature.xml"


statuses = ['OK_CHECKED_WITH_OCSP', 'OK_CHECKED_LOCALLY', 'CERTIFICATE_REVOKED']
status2regexp = {
	'OK_CHECKED_WITH_OCSP': re.compile(r'^(\d\d)\-(.*)g.*\.wgt$'),
	'OK_CHECKED_LOCALLY'  : re.compile(r'^(\d\d)\-(.*)u.*\.wgt$'),
	'CERTIFICATE_REVOKED' : re.compile(r'^(\d\d)\-(.*)r.*\.wgt$')
}

class HandleTemplateAction:
	def __init__(self, filelist):
		self.__filelist = filelist

	def output_testcase(self, output, filename):
		for status in statuses:
			regexp = status2regexp[status]
			match_object = regexp.match(filename)
			if match_object:
				group2 = match_object.group(2)
				sig_filename = resolve_signature_filename(len(group2))
				output.write("\n")
				output.write("\t{ \"%s\",\n" % filename)
				output.write("\t\t\"data/widget-digsig-ocsp/%s\",\n" % filename)
				output.write("\t\t\"%s\",\n" % sig_filename)
				output.write("\t\tCryptoXmlSignature::%s }\n" % status)
				break

	def __call__(self, action, output):
		if action == "testcases":
			for f in self.__filelist:
				self.output_testcase(output, f)
			return True

class GenerateGadgetSigningOcsp_ot(basesetup.SetupOperation):
	"""
	Generator class which reads all filenames in data/widget-digsig-ocsp
	and generate gadget_signing_ocsp.ot from its template.
	"""
	def __init__(self):
		basesetup.SetupOperation.__init__(self, message="gadget_signing_ocsp.ot")

	def __call__(self, sourceRoot, outputRoot=None, quiet=True):
		self.startTiming()

		libcryptoTestDir = os.path.join(sourceRoot, "modules", "libcrypto", "selftest")
		filelist = os.listdir(os.path.join(libcryptoTestDir, "data", "widget-digsig-ocsp"))

		regexp = re.compile(r'.*\.wgt$')
		for k,v in enumerate(filelist):
			if not regexp.match(v):
				del filelist[k]

		filelist.sort()

		changed = util.readTemplate(os.path.join(libcryptoTestDir, "gadget_signing_ocsp.ot.template"),
					    os.path.join(libcryptoTestDir, "gadget_signing_ocsp.ot"),
					    HandleTemplateAction(filelist))
		if changed: result = 2
		else: result = 0
		return self.endTiming(result, quiet=quiet)


# ----- end functions -----

# main execution

if __name__ == "__main__":
	sourceRoot = os.path.normpath(os.path.join(sys.path[0], "..", "..", ".."))
	option_parser = basesetup.OperaSetupOption(
		sourceRoot=sourceRoot,
		description=" ".join(
			["Generates gadget_signing_ocsp.ot from the wgt files in"
			 "modules/libcrypto/selftest/data/widget-digsig-ocsp/ and the"
			 "template gadget_signing_ocsp.ot.",
			 "The script exists with status 0 if none of the output files",
			 "was changed. The exist status is 2 if at least one of the",
			 "output files was changed (see also option '--make').",
			 "The exitstatus is 1 if an error was detected."]))
	(options, args) = option_parser.parse_args(args=sys.argv[1:])
	generate = GenerateGadgetSigningOcsp_ot()
	result = generate(sourceRoot=sourceRoot, quiet=options.quiet)
	if options.make and result == 2: sys.exit(0)
	else: sys.exit(result)
