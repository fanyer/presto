import os
import os.path
import re
import sys
import time

# import from modules/hardcore/scripts
if __name__ == "__main__":
    sys.path.insert(1, os.path.join(sys.path[0], "..", "scripts"))

import util
import opera_module
import module_parser
import basesetup

from module_parser import DependentItem

class Message(module_parser.DependentItem):
    def __init__(self, filename, linenr, name, owners):
        DependentItem.__init__(self, filename, linenr, name, owners)
        self.__modules = set()

    def modules(self): return self.__modules
    def addModule(self, module): self.__modules.add(module)

    def __cmp__(self, other):
        # Sort by name grouped by condition; unconditional messages first.
        dependsOn = str(self.dependsOn())
        otherDependsOn = str(other.dependsOn())
        if dependsOn == otherDependsOn:
            return cmp(self.name(), other.name())
        elif dependsOn == "<empty>": return -1
        elif otherDependsOn == "<empty>": return 1
        else: return cmp(dependsOn, otherDependsOn)


class HandleTemplateAction:
    def __init__(self, messages_parser):
        self.__messages_parser = messages_parser

    def __call__(self, action, output):
        if action == "messages":
            previousCondition = None
            for message in self.__messages_parser.messages():
                condition = str(message.dependsOn())
                if condition == "<empty>": condition = None
                if condition != previousCondition:
                    if previousCondition: output.write("#endif // %s\n" % previousCondition)
                    if condition: output.write("#if %s\n" % condition)
                    previousCondition = condition
                output.write("\t%s,\n" % message.name())
            if previousCondition: output.write("#endif // %s\n" % previousCondition)
            return True

        elif action == "has message macros":
            previousCondition = None
            for message in self.__messages_parser.messages():
                condition = str(message.dependsOn())
                if condition == "<empty>": condition = None
                if condition:
                    if condition != previousCondition:
                        if previousCondition: output.write("#  endif // %s\n" % previousCondition)
                        output.write("#  if %s\n" % condition)
                        previousCondition = condition
                    output.write("#    define HAS_%s\n" % message.name())
            if previousCondition: output.write("#  endif // %s\n" % previousCondition)
            return True

        elif action == "message strings":
            for message in self.__messages_parser.messages():
                condition = str(message.dependsOn())
                if condition == "<empty>": condition = None
                name = message.name()
                if condition: output.write("#ifdef HAS_%s\n" % name)
                output.write("\tcase %s: name = \"%s\"; break;\n" % (name, name))
                if condition: output.write("#endif // HAS_%s\n" % name);
            return True


from module_parser import ModuleParser

class MessagesParser(ModuleParser):
    def __init__(self, feature_def):
        """
        @param feature_def is a loaded instance of the class
          util.FeatureDefinition. That instance provides the
          version number for the configuration to load and the
          dictionary of all features by name. If a file
          module.messages.{version} exists, the versioned file is
          parsed. Otherwise the file module.messages is parsed.
        """
        ModuleParser.__init__(self, feature_def)
        self.__messages = []
        self.__messagesByName = {}
        self.__current_module = None

    def messages(self): return self.__messages

    def createMessage(self, filename, linenr, name, owners):
        """
        This method is called by ModuleParser.parseFile() if a new
        message definition is started. This method creates a new
        Message instance unless a Message instance with that name
        already exists.

        If the message with the specified name exists, the specified
        owners are added to the message.
        """
        if name in self.__messagesByName:
            current_message = self.__messagesByName[name]
            current_message.addOwners(owners)
        else:
            current_message = Message(filename, linenr, name, owners)
            self.__messagesByName[name] = current_message
            self.__messages.append(current_message)
            current_message.addModule(self.__current_module)
        return current_message

    def loadMessages(self, sourceRoot):
        """
        Parses the 'module.messages' files for all modules relative to
        the specifed sourceRoot.
        @param sourceRoot is the root directory of the source tree
          that will be parsed.
        """

        def parseMessageDefinitions(line, current_item, what, value):
            """
            This method is called by ModuleParser.parseFile() if a
            "depends on" line was found.
            @param line is the current message_parser.Line
            @param current_item is the current Message instance
            @param what is expected to be "depends on"
            @param value is the condition on which the message depends.
            """
            assert what == "depends on"
            current_item.addDependsOnString(value)

        self.setParseDefinition([
                module_parser.ModuleItemParseDefinition(
                    type="message",
                    definitions=["depends on"],
                    create_item=self.createMessage,
                    parse_item_definition=parseMessageDefinitions)
                ])
        for module in list(opera_module.modules(sourceRoot)):
            self.__current_module = module
            self.parseFile(module.getMessagesFile())
            self.__current_module = None

        if self.printParserErrors():
            return False
        else:
            self.featureDefinition().setMessagesLoaded()
            self.__messages.sort()
            return True

    def writeOutputFiles(self, sourceRoot, outputRoot):
        hardcoreDir = os.path.join(sourceRoot, 'modules', 'hardcore')

        mhDir = os.path.join(hardcoreDir, 'mh')
        if outputRoot is None:
            targetDir = mhDir
        else:
            targetDir = os.path.join(outputRoot, 'modules', 'hardcore', 'mh')
            util.makedirs(targetDir)
        changed = util.readTemplate(os.path.join(mhDir, 'messages_template.h'),
                                    os.path.join(targetDir, 'generated_messages.h'),
                                    HandleTemplateAction(self))
        changed = util.readTemplate(os.path.join(mhDir, 'message_strings_template.inc'),
                                    os.path.join(targetDir, 'generated_message_strings.inc'),
                                    HandleTemplateAction(self)) or changed
        if targetDir == mhDir:
            util.updateModuleGenerated(hardcoreDir,
                                       ['mh/generated_messages.h',
                                        'mh/generated_message_strings.inc'])
        return changed

class GenerateMessages(basesetup.SetupOperation):
    def __init__(self):
        basesetup.SetupOperation.__init__(self, message="messages system")

    def __call__(self, sourceRoot, feature_def, outputRoot=None, quiet=True):
        self.startTiming()
        messages_parser = MessagesParser(feature_def)
        if messages_parser.loadMessages(sourceRoot):
            changed = messages_parser.writeOutputFiles(sourceRoot, outputRoot)
            if changed: result = 2 # files have changed
            else: result = 0 # no changes
        else: result = 1 # error
        return self.endTiming(result, quiet=quiet)

if __name__ == "__main__":
    sourceRoot = os.path.normpath(os.path.join(sys.path[0], "..", "..", ".."))
    option_parser = basesetup.OperaSetupOption(
        sourceRoot=sourceRoot,
        withMainlineConfiguration=True,
        description=" ".join(
            ["Generate the files",
             "{outroot}/modules/hardcore/mh/generated_messages.h,",
             "{outroot}/modules/hardcore/mh/generated_message_strings.inc",
             "from {adjunct,modules,platforms}/readme.txt,",
             "{adjunct,modules,platforms}/*/module.messages{version} and",
             "the template files modules/hardcore/mh/messages_template.h,",
             "and modules/hardcore/mh/message_strings_template.inc.",
             "The script exists with status 0 if none of the output files",
             "was changed. The exist status is 2 if at least one of the",
             "output files was changed (see also option '--make').",
             "The exitstatus is 1 if an error was detected."]))
    (options, args) = option_parser.parse_args(args=sys.argv[1:])
    try:
        feature_def = util.getFeatureDef(sourceRoot, options.mainline_configuration)
        generate_messages = GenerateMessages()
        result = generate_messages(sourceRoot=sourceRoot,
                                   feature_def=feature_def,
                                   outputRoot=options.outputRoot,
                                   quiet=options.quiet)
        if options.timestampfile and result != 1:
            util.fileTracker.finalize(options.timestampfile)
        if options.make and result == 2: sys.exit(0)
        else: sys.exit(result)
    except util.UtilError, e:
        print >>sys.stderr, e.value
        sys.exit(1)
