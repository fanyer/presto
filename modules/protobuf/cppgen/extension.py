"""cppgen

Generates/Updates C++ code from protocol buffer definitions.
"""

from hob.cmd import CommandTable
from hob.utils import _
from hob.extension import ExtensionError
from hob.cmd import ProgramError
import os

def checkversion(ui):
    if not hasattr(ui, "version"):
        raise ExtensionError("cppgen extension requires hob 0.2 or higher")
    from distutils.version import LooseVersion as Version
    if Version(ui.version) < "0.2":
        raise ExtensionError("cppgen extension requires hob 0.2 or higher")

cmds = CommandTable()

def splitLines(text):
    """
    >>> list(splitLines(""))
    []
    >>> text = chr(10).join(['# a comment', '#', '', 'foo', 'bar #comment'])
    >>> list(splitLines(text))
    ['foo', 'bar']
    """
    import re
    reg = re.compile("#.*$")
    for line in text.splitlines():
        line = reg.sub("", line).strip()
        if len(line) > 0:
            yield line

@cmds.add(
    [('r', 'replace', False,
      _("Replace the input file with the output")),
     ('p', 'print', False,
      _("Print the output to stdout")),
     ('x', 'stop', False,
      _("Stop processing files on the first error")),
     ('', 'diff', False,
      _("Display the difference between the input file and the output")),
     ('', 'show_all', False,
      _("Show all generated files, not just the ones which are modified")),
      ],
    [(('file', 'files'), [],
      _("C++ files to update with generated code. If empty the file list from the configuration file is used.")),
      ],
    mutex=[('replace', 'diff')]
    )
def cpp(ui, files, replace, verbose, stop, diff, show_all, **kwargs):
    """Generates/Updates C++ code from protocol buffer definitions.
    """
    checkversion(ui)

    print_stdout = kwargs["print"]
    cpp_conf_file = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "cpp.conf"))

    from cppgen.script import hobExtension, readCppConf
    cpp_conf = readCppConf(cpp_conf_file)
    hobExtension(ui, cpp_conf, files, replace=replace, stop=stop, diff=diff, print_stdout=print_stdout, show_all=show_all)
