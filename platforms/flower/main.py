#!/usr/bin/env python

"""
Main module.
"""

import sys
import os.path
import errno
import optparse
import textwrap
import glob
import datetime

toplevel = sys.argv[0]
for i in range(3):
    toplevel = os.path.dirname(toplevel)
if toplevel:
    os.chdir(toplevel)

import flow
import util
import otree
import errors
import output
from config import facade as config, optParser as configOptParser

class HelpFormatter(optparse.IndentedHelpFormatter):
    """
    A help formatter object expected by optparse.OptionParser with
    extensions for formatting information about the goals and their
    arguments.
    """

    def format_description(self, description):
        """
        Overrides inherited implementation. Inject the information
        about the goals before the program description text.

        @param description Program description.

        @return Formatted help text.
        """
        self.current_indent += 2
        goals = flow.goalParser.formatHelp(self)
        self.current_indent -= 2
        return "{desc}\n{head}\n{goals}Default goal: {default}\n".format(
            desc=optparse.IndentedHelpFormatter.format_description(self, description),
            head=self.format_heading('Goals'),
            goals=goals,
            default=config.goal)

    def format_goal(self, goal, description, args):
        """
        Format the help text for a goal.

        @param goal Goal name.

        @param description Goal description.

        @param args A sequence of flow.GoalArgument objects describing
        the goal's arguments.

        @return Formatted help text.
        """
        if len(goal) > self.help_position - self.current_indent - 2:
            text = "{_:{indent}}{g}\n{_:{help}}".format(indent=self.current_indent, help=self.help_position, g=goal, _='')
        else:
            text = "{_:{indent}}{g:{width}}".format(indent=self.current_indent, width=self.help_position - self.current_indent, g=goal, _='')
        text += ('\n' + ' ' * self.help_position).join(textwrap.wrap(description, self.width - self.help_position))
        text += '\n'
        self.current_indent += 2
        for arg in args:
            text += arg.formatHelp(self)
        self.current_indent -= 2
        return text + '\n'

    def format_goal_argument(self, argument, description):
        """
        Format the help text for a goal argument.

        @param argument Argument metavar.

        @param description Argument description.

        @return Formatted help text.
        """
        if len(argument) > self.help_position - self.current_indent - 2:
            text = "{_:{indent}}{a}\n{_:{help}}".format(indent=self.current_indent, help=self.help_position, a=argument, _='')
        else:
            text = "{_:{indent}}{a:{width}}".format(indent=self.current_indent, width=self.help_position - self.current_indent, a=argument, _='')
        text += ('\n' + ' ' * self.help_position).join(textwrap.wrap(description, self.width - self.help_position))
        return text + '\n'

def loadExtraConfFiles():
    """
    Load all configuration files listed in config.confFiles,
    processing each file's possible override of config.confFiles in
    turn, until there are no unseen files mentioned in it.

    Each file will only be loaded once.
    """
    global config
    toLoad = config.confFiles
    while toLoad:
        g = toLoad.pop(0)
        for name in glob.glob(g):
            if name not in confFilesLoaded:
                with open(name) as f:
                    config += f
                confFilesLoaded.add(name)
                toLoad += config.confFiles

flowSpace = {'config': config,
             'otree': otree,
             'util': util,
             'flow': flow.flow,
             'command': flow.Command,
             'goal': flow.goalParser.decorator,
             'errors': errors,
             'output': output}

for m, module in otree.modules.iteritems():
    for name in module.glob('module.build/flow.py') + module.glob('module.build/*.flow.py'):
        with module.file(name) as f:
            exec f in flowSpace

# Adding config files, starting from lowest priority
with open('platforms/flower/default.py') as f:
    config += f

confFiles = []
for m, module in otree.modules.iteritems():
    confFiles += [(f, m) for f in module.glob('module.build/conf.py') + module.glob('module.build/*.conf.py')]
for name, m in sorted(confFiles):
    with otree.modules[m].file(name) as f:
        config += f

confFilesLoaded = set()

loadExtraConfFiles()

parser = optparse.OptionParser(prog='flower',
                               description=config.helpDescription,
                               formatter=HelpFormatter(),
                               usage='%prog [options] [goal [arguments]]')
configOptParser.addOptparseOptions(parser)
options, args = parser.parse_args()
configOptParser.handleValues(options)

loadExtraConfFiles()

if not args:
    if not config.goal:
        parser.print_help()
        sys.exit(0)
    args = [config.goal]

try:
    goal = flow.goalParser.parse(args)
except errors.CommandLineError, e:
    parser.error(e)

output.log += output.ConsoleLogger(colorize=config.colorizeOutput,
                                   echoMessage=config.echoMessage,
                                   echoCommands=config.echoCommands,
                                   echoStdout=config.echoStdout,
                                   echoStderr=config.echoStderr)

if config.logToFile:
    output.log.systemMessage("Logging to " + config.logName)
    fileLogger = output.TextFileLogger(config.logName, keep=config.logKeep)
    fileLogger.systemMessage(' '.join(['flower'] + [util.shellEscape(arg) for arg in sys.argv[1:]]))
    output.log += fileLogger

try:
    starttime = datetime.datetime.now()
    output.log.systemMessage("Started {0:%Y-%m-%d %H:%M:%S}".format(starttime))

    errorsCollected = flow.make(goal, output.log, quota=config.processQuota, failEarly=config.failEarly)
    if errorsCollected:
        sys.exit(1)

finally:
    endtime = datetime.datetime.now()
    output.log.systemMessage("Finished {0:%Y-%m-%d %H:%M:%S}".format(endtime))
    output.log.systemMessage("Time elapsed: {0}".format(endtime - starttime))
