import re
import os.path
import sys

# importing from modules/hardcore/scripts/:
import util


#
# The class Expression and its sub-classes are created by
# ModuleParser.parseExpression() on parsing an expression string that is used
# for a "depends on" or "import if" or similiar definition of some ModuleItem.
#
# An Expression can be transformed to a string, which can then be inserted into
# a cpp file e.g. in a "#if" condition.
#
# An Expression can also be written to an xml file that is used to generate the
# features, tweaks, api documentation.
#

OR_EXPRESSION = 1
AND_EXPRESSION = 2
OTHER = 3

class Expression:
    """
    Base class for expressions that are used in DependItem.
    """
    def names(self):
        """
        Reduces the expression to a set of item-names that are used in the
        expression. This can be used to resolve dependencies. If the expression
        uses no item, the reduced set is empty.
        """
        return set()

    def __str__(self): return self.toString(OR_EXPRESSION)
    def toString(self, level): return "<empty>"
    def toXML(self, indent, level): return ""
    def isExpressionItem(self): return False
    def isBoolExpression(self): return False
    def isEmpty(self):
        return self.toString(OR_EXPRESSION) == "<empty>"


class BoolExpression(Expression):
    def __init__(self, value): self.__value = value
    def toString(self, level):
        if self.isTrue(): return "1"
        else: return "0"
    def toXML(self, indent, level):
        if self.isTrue(): return "True"
        else: return "False"
    def isBoolExpression(self): return True
    def isTrue(self):
        if self.__value: return True
        else: return False


class ExpressionList(Expression):
    """
    Base class for a list of single expressions that are combined with an
    operator (like '&&' or '||').

    level       should be one of OR_EXPRESSION, AND_EXPRESSION or OTHER
    operator    should be the operator string, e.g. "&&"
    xml_name    should be the name of the operator to use in toXML
    expressions should be the list of expressions
    """
    def __init__(self, level, operator, xml_name, expressions=[]):
        self.__level = level
        self.__operator = operator
        self.__xml_name = xml_name
        self.__expressions = expressions

    def operator(self): return " %s " % (self.__operator)

    def names(self):
        """
        This is the set of all names that are used in each expression of the
        list.
        """
        return reduce(set.union, [e.names() for e in self.__expressions], set())

    def toString(self, level):
        result = self.operator().join([expression.toString(self.__level)
                                       for expression in self.__expressions])
        if level > self.__level: return "(" + result + ")"
        else: return result

    def toXML(self, indent, level):
        children = "".join([expression.toXML(indent, level + 1)
                            for expression in self.__expressions])
        return "%s<%s>\n%s%s</%s>\n" % (
            indent * level, self.__xml_name,
            children,
            indent * level, self.__xml_name)


class AndExpression(ExpressionList):
    """
    And combination of a list of expressions.
    """
    def __init__(self, expressions):
        ExpressionList.__init__(self, AND_EXPRESSION, "&&", "and", expressions[:])


class OrExpression(ExpressionList):
    """
    Or combination of a list of expressions.
    """
    def __init__(self, expressions):
        ExpressionList.__init__(self, OR_EXPRESSION, "||", "or", expressions[:])


class NotExpression(Expression):
    """
    Negation of an expression.
    """
    def __init__(self, expression):
        self.__subject = expression

    def names(self):
        """
        The set of names in a NotExpression is the same as the set in its
        subject.
        """
        return self.__subject.names()

    def toString(self, level):
        if self.__subject.isExpressionItem():
            return self.__subject.toStringInverted(OTHER)
        elif self.__subject.isBoolExpression():
            return BoolExpression(not self.__subject.isTrue()).toString(OTHER)
        else:
            return "!" + self.__subject.toString(OTHER)

    def toXML(self, indent, level):
        if self.__subject.isExpressionItem():
            return self.__subject.toXMLInverted(indent, level)
        else:
            return "%s<not>\n%s%s</not>\n" % (indent * level, self.__subject.toXML(indent, level + 1), indent * level)


class ExpressionItem(Expression):
    """
    This class is used for a single item that is used in an expression. The item
    can either correspond to a ModuleItem (i.e. a FEATURE, TWEAK, API, or
    SYSTEM) or it can correspond to a simple define.
    """
    def __init__(self, name):
        self.__name = name
        if self.isAction() and name.endswith("_ENABLED"):
            # remove "_ENABLED" part for actions
            self.__name = name[:-8]

    def isExpressionItem(self): return True

    def name(self): return self.__name
    def names(self): return set([self.name()])
    def type(self): return self.name().split('_', 1)[0]

    def isAction(self):
        """
        Returns whether or not this item is an action, i.e. if the
        name starts with "ACTION_".
        """
        return self.type() == "ACTION"

    def isApi(self):
        """
        Returns whether or not this item is an api, i.e., if the name starts
        with "API_".
        """
        return self.type() == "API"

    def isFeatureTweakOrSystem(self):
        """
        Returns whether or not this item is a feature, a tweak or a system,
        i.e., if the name starts with "FEATURE_", "TWEAK_" or "SYSTEM_".
        """
        return self.type() in ("FEATURE", "TWEAK", "SYSTEM")

    def toString(self, level):
        if self.isApi():
            return "(defined %s && %s == YES)" % (self.name(), self.name())
        elif self.isFeatureTweakOrSystem():
            return "%s == YES" % self.name()
        elif self.isAction():
            return "%s_ENABLED == YES" % self.name()
        else:
            return "defined %s" % self.name()

    def toStringInverted(self, level):
        if self.isApi():
            return "(!defined %s || %s == NO)" % (self.name(), self.name())
        if self.isFeatureTweakOrSystem():
            return "%s == NO" % self.name()
        elif self.isAction():
            return "%s_ENABLED == NO" % self.name()
        else:
            return "!defined %s" % self.name()

    def toXML(self, indent, level):
        return "%s<depends-on type='%s' name='%s' enabled='yes'/>\n" % (
            indent * level, self.type().lower(), self.name())

    def toXMLInverted(self, indent, level):
        return "%s<depends-on type='%s' name='%s' enabled='no'/>\n" % (
            indent * level, self.type().lower(), self.name())


def reCompileItemName(type):
    """
    Returns a compiled regular expression which detects an item name
    line, i.e. a line like

    TYPE_XXXX    owner{ , other_owner }

    where TYPE is the specified type in uppercase letters.

    The returned re instance can be used like

      result = re.match(line)

    If the result is not None, then it has two groups. The first group
    ("result.group(1)") matches the name of the item
    (i.e. "TYPE_XXXX") and the second group ("result.group(1)")
    matches the comma separated list of owners (i.e. "owner{ ,
    other_owner }").

    Note: the list of owners may be empty; in that case the
    parser instance should print a warning message.

    @return the compiled re instance.
    """
    if type in ("macro", "type"):
        # "macro" and "type" are not included in the name of that item:
        re_TYPE = type+"\\s+(\w+)"
    elif type in ("message"):
        # a message can be any valid C++ identifier
        re_TYPE = "([A-Za-z_]\w*)"
    else: re_TYPE = "("+type.upper()+"_\w+)"
    # the owners can be a comma-separated list of non whitespace
    # characters, but here we test only for anything after a non
    # whitespace character to be able to catch wrongly formated
    # owner-lists:
    re_OWNERS = "(\S+.*?)"
    return re.compile("^" # bind to the start of the line
                      + re_TYPE # the item's name (first group)
                      # either there is only the name (optionally
                      # followed by only whitespace):
                      + "(?:|"
                      # or there is at least one whitespace
                      + "\\s+"
                      # followed by the comma-separated list of owners:
                      + re_OWNERS
                      + ")\\s*" # maybe some whitespace at the end
                      + "$")    # bind to the end of the line


def reCompileItemDefinitions(definitions):
    """
    Returns a compiled regular expression which detects an item
    definition line, i.e. a line like

        Definition: Value

    where "Definition" can match one of the re fragments in the
    specified definitions list and "Value" is any not-whitespace
    content.

    The returned re instance can be used as

      result = re.match(line)

    If the result is not None, then it has two groups. The first group
    ("result.group(1)") matches the name of the definition
    (i.e. "Definition") and the second group ("result.group(1)")
    matches the value.

    @param definitions is a list of case insensitive regular
      expression fragments which are used to match a definition name.
    @return the compiled re instance.
    """
    return re.compile("^\\s+" # at least one ws at the start of the line
                      # then the first group with any of the specified
                      # definition names:
                      + "("+"|".join([d.replace(" ", "\\s+") for d in definitions])+")"
                      # a colon (with an arbitrary amount of whitespace)
                      # separates definition name from value:
                      + "\\s*:\\s*"
                      # the value can be any string (but exclude
                      # whitespace from the start and end of the value):
                      + "(.*?)"
                      + "\\s*"  # maybe some whitespace at the end
                      + "$",    # bind to the end of the line
                      re.IGNORECASE)


class ModuleItem:
    """
    This class can be used as a base class for the different items
    which are defined in the module.XXX files. Each item has is
    formatted like

    NAME_OF_THE_ITEM    owner{, other_owner}


        Description of the item, which can span several lines
        of the file.

        ...

    I.e. each item has a
    - name ("NAME_OF_THE_ITEM"),
    - one or more owners, where the owners are specified as a comma
      separated list,
    - a description of the item.

    An item corresponds to a set of symbols it defines. The default
    implementation returns the set that consists of the item's name. Each class
    that inherits from this class may override the method symbols() to return
    the set of symbols of the item. It is recommended that the item's name is
    always included.
    The set of symbols is used by the ModuleParser to resolve dependencies.
    If in a generated file one item depends on other items, then all other items
    need to be declared before the depending item. So symbols() returns the set
    of names other items may depend on.
    """
    def __init__(self, filename, linenr, name, owners=None, type=None):
        """
        Initializes an instance of ModuleItem. The instance is
        defined by the tupel (filename, linenr, name, owners):

        @param filename is the name of the file from which the
          definition is read.
        @param linenr is the line nr of the first line in the
          specified file from which the definition is read. Usually
          the definition spans several lines.
        @param name is the name of the item.
        @param owners is a string which contains a comma-separated
          list of owners of the item.
        """
        self.__line = util.Line(filename, linenr, name)
        self.__name = name
        self.__owners = set([])
        self.__type = type
        if owners is not None: self.addOwners(owners)
        self.__description = None

    def sortByName(a, b):
        """
        Sort items by 1. name, 2. filename, 3. linenr
        """
        result = cmp(a.name(), b.name())
        if result == 0:
            result = cmp(a.filename(), b.filename())
            if result == 0:
                result = a.linenr() - b.linenr()
        return result

    def name(self): return self.__name
    def id(self):
        """
        Returns a unique id of the item. Usually the name of the item should be
        unique, but there may be multiple APIImport items for one APIExport item
        which all have the same name.
        """
        return self.name()

    def filename(self): return self.__line.filename
    def linenr(self): return self.__line.linenr
    def symbols(self): return set([self.name()])
    def type(self): return self.__type

    def line(self):
        """
        Returns a util.Line() instance which contains the filename and
        linenr of this item.
        """
        return self.__line

    def dependencies(self):
        """
        Returns a set of all names, this item dependends on. This is usually the
        set of symbols that is used in a "depends on" or "import if" definition.
        If the item depends on nothing (like the default ModuleItem) it shall
        return an empty set.
        """
        return set()

    def hasOwners(self):
        """ Returns true if the item has at least one owner. """
        return len(self.__owners) > 0

    def addOwners(self, owners):
        """
        Adds the specified owners to the set of owners of this item.
        @param owners is a comma separated list of owners.
        """
        self.__owners |= set(filter(None, map(str.strip, owners.split(','))))

    def owners(self):
        """
        Returns the set of owners of the item. The owners were
        specified in the constructor as a comma separated list. This
        method returns the corresponding set.
        """
        return self.__owners

    def description(self):
        """
        Returns the description (as string), that was added to the
        item by appendToDescription().
        """
        if self.__description is None: return ""
        else: return self.__description

    def hasDescription(self):
        """
        Returns True if a description was added to the item by
        appendToDescription().
        """
        return self.__description is not None

    def appendToDescription(self, lines):
        """
        Appends the text of the specified list of util.Line instances
        to the description of this item.

        The method description() will return a list of all lines as
        strings.

        @param lines is a list of util.Line instance to add to the
          description.
        """
        if len(lines):
            if self.__description is None: self.__description = ""
            elif not self.isDeprecated(): self.__description += "\n"
            for line in lines:
                if self.isDeprecated():
                    # append the description to one single line
                    if len(self.__description) > 0: self.__description += " "
                    self.__description += re.sub("\\s+", " ", str(line).strip())
                else:
                    # keep whitespaces:
                    self.__description += str(line)+"\n"

    def isDeprecated(self):
        """
        Returns true if this definition is deprecated. An item can be
        deprecated by setting the owner (or one of the owner) to the
        string 'deprecated'.
        """
        return "deprecated" in self.__owners

    def ownersToXML(self, indentation="\t", level=0):
        """
        Creates an xml fragment which contains the list of owners of
        this item. The xml fragment has the format

        <owners>
            <owner name='XXXX'/>
        </owners>

        where XXXX is the name of the owner. If the item has no owners
        or the item is deprecated, an empty string is returned - even
        if a deprecated item has some other owners.

        @param identation specifies the indentation string to use
          for indenting the xml.
        @param level specifies by how many levels the xml entry should
          be indented. All <owner> items will be written in a new line
          and indented one more level.
        @return the xml fragment for the owners.
        """
        xml = ""
        if not self.isDeprecated() and self.hasOwners():
            xml += "%s<owners>\n" % (indentation*level)
            for owner in sorted(self.owners()):
                xml += "%s<owner name='%s'/>\n" % (indentation*(level+1), owner)
            xml += "%s</owners>\n" % (indentation*level)
        return xml;

    def descriptionToXML(self, indentation="\t", level=0):
        """
        Returns an xml fragment which contains the description of this
        item. The xml fragment has the format
        <description xml:space='preserve'><![CDATA[
        XXXX
        ]]></description>

        where XXXX is the description of the item.
        """
        return "%s<description xml:space='preserve'><![CDATA[\n%s%s]]></description>\n" % (indentation*level, self.description(), indentation*level)

    def nameToXMLAttr(self):
        """
        Returns an xml fragment which contains the name of this item
        and the deprecated status as xml-attributes. The returned
        value can be inserted as xml-attribute to an xml element for
        this item. The format of this fragment is like "name='XXXX'"
        or "name='XXXX' deprecated='deprecated'", where XXXX is the
        name of this item.
        """
        attr = "name='%s'" % self.name()
        if self.isDeprecated(): attr += " deprecated='deprecated'"
        return attr


class ItemWithConflictWith:
    """
    This class can be used to add a conflictsWith() field to a
    ModuleItem class. You can use the method addConflictsWith() to
    add a comma separated list of conflictees to the item. The method
    conflictsWith() returns a set of the conflictees.

    class MyItemWithConflicts(ModuleItem, ItemWithConflictWith):
        def __init__(self, name, filename, linenr, owners):
            ModuleItem.__init__(self, filename, linenr, name, owners)
            ItemWithConflictWith.__init__(self)
    ...

    An item which has a "Conflicts with:" definition should inherit
    from this class. The classes APIExport and Tweak inherit from this
    class.

    The TweaksParser calls addConflictsWith() for each "Conflicts
    with:" definition.
    """
    def __init__(self):
        self.__conflictsWith = set()

    def addConflictsWith(self, items):
        """
        Adds the specified items to the set of conflictees.

        @param items is either a string with a comma separated list of
          conflictees or it is a set of conflictees to add.
        """
        if isinstance(items, str):
            self.__conflictsWith |= set([i.strip() for i in items.split(",")])
        else: self.__conflictsWith |= set(items)
        self.__conflictsWith.discard('')

    def conflictsWith(self):
        """
        Returns the set of conflictees.
        """
        return self.__conflictsWith;

    def sortedConflictsWith(self):
        """
        Returns a sorted list of the set of conflictees.
        """
        return sorted(list(self.__conflictsWith));


class ItemWithDependsOn:
    """
    This class can be used for a ModuleItem which has a "depends on" definition.
    An item can specify one or more "depends on" values. Each value defines
    a condition based on features, tweaks, apis, systems, actions or simple
    defines. The expression is evaluated by a ModuleParser instance;
    see ModuleParser.finishDependsOn().
    To add a "depends on" value, call addDependsOnString().
    When parsing of the ModuleItem is finished, call
    ModuleParser.finishDependsOn() to evaluate the depends-on expression.
    The expression is then returned by dependsOn().

    An item which has a "Depends on:" definition should inherit from this class.
    E.g. the actions.Action inherits from this class.

    class MyItemWithDependencies(ModuleItem, ItemWithDependsOn):
        def __init__(self, name, filename, linenr, owners):
            ModuleItem.__init__(self, filename, linenr, name, owners)
            ItemWithDependsOn.__init__(self)
    ...
    """
    def __init__(self):
        self.__dependsOn = Expression()
        self.__dependsOnStrings = []

    def dependsOn(self):
        """
        Returns the Expression instance that was set by setDependsOn().
        """
        return self.__dependsOn

    def dependsOnNothing(self):
        """
        Returns True iff this item depends on nothing.
        """
        return ((len(self.__dependsOnStrings) == 0) or
                ("nothing" in map(str.lower, self.__dependsOnStrings)))

    def dependsOnStrings(self):
        """
        Returns the strings that were added to this instance by
        addDependsOnString().
        """
        if self.dependsOnNothing(): return []
        else: return self.__dependsOnStrings

    def dependencies(self):
        """
        The dependencies of an ItemWithDependsOn are the names defined by the
        dependsOn() expression.
        """
        return self.dependsOn().names()

    def addDependsOnString(self, string):
        """
        Adds a dependency to this item. This item will depend on the specified
        condition. ModuleParser.parseExpression() is expected to create an
        Expression instance from the string.

        See also ModuleParser.finishDependsOn().

        If the specified string is empty or "nothing", it is not added
        to the list.

        @param string is a comma separated list of conditions on which
          this item should depend on.
        """
        string = ", ".join(filter(None, map(str.strip, string.split(","))))
        if string and string not in self.__dependsOnStrings:
            self.__dependsOnStrings.append(string)

    def setDependsOn(self, expression):
        """
        Sets the dependency to the specified Expression. The specified instance
        is returned by dependsOn(). The expression can be used to create a
        #if condition.

        ModuleParser.parseExpression() creates the Expression instance from the
        list dependsOnStrings().

        @param expression is the Expression on which this item
          depends.
        """
        self.__dependsOn = expression


class DependentItem(ModuleItem, ItemWithDependsOn):
    """
    This is a convenience class which inherits from both ModuleItem and
    ItemWithDependsOn.
    """
    def __init__(self, filename, linenr, name, owners, type=None):
        ModuleItem.__init__(self, filename, linenr, name, owners, type)
        ItemWithDependsOn.__init__(self)

    def dependencies(self):
        """
        The dependencies of an ItemWithDependsOn are the names defined by the
        dependsOn() expression.
        """
        return ItemWithDependsOn.dependencies(self)


class ParseExpressionException(Exception): pass


class ModuleItemParseDefinition:
    """
    This class defines how a ModuleItem should be parsed. A
    ModuleItem starts with one line containing the name of the item
    followed by a list of owners of that definition. The
    (name,owners)-line is followed by one (or more) blocks of
    description lines and a block of definition lines.

    The simplest way to create a ModuleItemParseDefinition, which
    detects only items which start with "FOO_" and which only have a
    description (no definition section) is:

    parse_foo = ModuleItemParseDefinition("FOO", None, ModuleItem, None)
    """
    def __init__(self, type, definitions, create_item, parse_item_definition):
        """
        @param type specifies the type of the item to recognize. A
          regular expression is compiled to recognize the first line
          of an item definition of this type.
          @see reCompileItemDefinitions()

        @param definitions is a list of item definition. If an item of
          the specified type is recognized, the definitions in this
          list are used to define the item. Any line that does not
          match one of these definitions is considered a description.
          @see reCompileItemDefinitions()

        @param create_item is a function to create a new item for this
          type. The new item should be inherited from ModuleItem. The
          function is called with four arguments:

            create_item(filename, linenr, name, owners)

          where filename is the name of the file that is parsed,
          linenr is the number of the line, name is the name of the
          item (i.e. the content of the first group of the regular
          expression which is compiled by reCompileItemName(type)) and
          owners is the comma separated list of owners (i.e. the
          content of the second group of the regular expression which
          is compiled by reCompileItemName(type)).

          This function should return the created instance.

        @param parse_item_definition is a function which is called for
          each item definition that is recognized by the regular
          expression which is compiled by
          reCompileItemDefinitions(definitions). That regular
          expression detects lines like

            what: value

          where "what" is one of the items in the specified
          definitions list.

          The function is called with four arguments:

            parse_item_definition(line, current_item, what, value)

          where line is the current util.Line instance that is parsed,
          current_item is the instance returned by the create_item()
          function, what is the string that matches one of the
          specified definitions and value is the string which is
          specified after the colon.

          Any return value of the specified function is ignored.
        """
        self.__re_item_name = reCompileItemName(type)
        if definitions:
            self.__re_item_definitions = reCompileItemDefinitions(definitions)
        else: self.__re_item_definitions = None
        self.__create_item = create_item
        self.__parse_item_definition = parse_item_definition

    def reItemName(self): return self.__re_item_name
    def reItemDefinitions(self): return self.__re_item_definitions
    def createItem(self, filename, linenr, name, owners):
        return self.__create_item(filename, linenr, name, owners)
    def parseItemDefinition(self, line, current_item, what, value):
        self.__parse_item_definition(line, current_item, what, value)


class DependentItemStore:
    """
    This class keeps a collection of ModuleItem which can be sorted by
    dependency - see ModuleParser.resolveDependencies().

    Call addItem() to add a ModuleItem to this instance.
    """
    def __init__(self):
        self.__item_ids_by_symbol = {}
        self.__item_by_id = {}
        self.__dependency_cache = {}

    def itemIdsBySymbol(self, symbol):
        """
        Returns a set of ModuleItem ids that define the specified symbol or an
        empty set if no such item was added.
        """
        return self.__item_ids_by_symbol.get(symbol, set())

    def hasItem(self, symbol):
        """
        Returns true if a ModuleItem that defines the specified symbol was added
        to this DependentItemStore.
        """
        return symbol in self.__item_ids_by_symbol

    def addItem(self, item):
        for symbol in item.symbols():
            self.__item_ids_by_symbol.setdefault(symbol, set()).add(item.id())
        self.__item_by_id[item.id()] = item

    def getItem(self, id): return self.__item_by_id.get(id, None)

    def items(self):
        """
        Returns a set of all items that were added.
        """
        return self.__item_by_id.values()

    def hasDependencyCacheFor(self, item):
        """
        Return True if this instance already has calculated the dependency cache
        for the specified ModuleItem.
        """
        return item.id() in self.__dependency_cache

    def dependencyCache(self, item):
        """
        Return the set of item-names the specified item depends on, or None if
        the depdency was not yet calculated for the item. If the returned set
        is empty, the item is independent.
        """
        return self.__dependency_cache.get(item.id(), None)

    def updateDependencyCache(self, item):
        """
        If the dependency cache for the specified ModuleItem was not yet created
        we call collectDependentItems() and store the result in the cache. If
        the resulting set is empty, the item is independent.
        """
        if not self.hasDependencyCacheFor(item):
            self.__dependency_cache[item.id()] = self.collectDependentItems(item, exclude=set())

    def collectDependentItems(self, item, exclude):
        """
        Recursively collects the names of all items the specified item depends
        on. This method is called by resolveDependencies().

        @param item is a ModuleItem.
        @param exclude is a set() of ModuleItem.id()s for which the dependencies
            should not be calculated to avoid a recursion-loop.
        @return a set of ModuleItem.name()s on which the specified item depends.
        """
        if item.id() in exclude:
            return set()
        elif self.hasDependencyCacheFor(item):
            # No need to recurse deeper if we already know the result
            return self.dependencyCache(item)
        else:
            exclude.add(item.id())
            dependencies = set()
            for symbol in item.dependencies():
                for dep_id in self.itemIdsBySymbol(symbol) - exclude:
                    dep_item = self.getItem(dep_id)
                    dependencies |= self.collectDependentItems(dep_item, exclude)
                    dependencies.add(dep_item.name())
                    exclude.add(dep_id)
            return dependencies

    def isIndependentItem(self, item):
        """
        Returns True iff the specified item does not depend on any other item in
        this DependentItemStore.
        """
        self.updateDependencyCache(item)
        return len(self.dependencyCache(item)) == 0

    def dependsOnOther(self, item, other):
        """
        Returns True iff the specified item depends on the specified other item.
        """
        self.updateDependencyCache(item)
        return other.name() in self.dependencyCache(item)


class ModuleParser(util.WarningsAndErrors):
    """
    This class can be used as a generic module.XXX file parser. To use
    an instance of this class you need to define how to parse a single
    item in that file by calling setParseDefinition() and then you can
    call parseFile() for each file to parse and parseFile() returns
    the list of items from that file.
    """
    def __init__(self, feature_def=None, stderr=sys.stderr):
        util.WarningsAndErrors.__init__(self, stderr)
        self.__parse_definitions = []
        self.__feature_def = feature_def
        self.__dependent_item_store = DependentItemStore()
        self.__dependent_items = []

    def dependentItems(self): return self.__dependent_items
    def dependentItemStore(self): return self.__dependent_item_store

    def featureDefinition(self):
        return self.__feature_def

    def addItems(self, items):
        """
        @param items is a list of ModuleItem instances to add to the
               DependentItemStore. This is needed to resolve dependencies.
               See also resolveDependencies()
        @return the same list of items
        """
        for item in items: self.dependentItemStore().addItem(item)
        return items

    def addParserWarning(self, line, message):
        """
        Adds the specified message to the list of warnings. To print
        all warnings you can call printParserWarnings().

        @param line is the util.Line instance (which contains at least
          a filename and a linenr) to which the message refers.
        @param message is some text describing the warning.
        """
        self.addWarning(line, message)

    def addParserError(self, line, message):
        """
        Adds the specified message to the list of errors. To print
        all errors you can call printParserErrors().

        @param line is the util.Line instance (which contains at least
          a filename and a linenr) to which the message refers.
        @param message is some text describing the error.
        """
        self.addError(line, message)

    def printParserWarnings(self):
        """
        If there are warnings, this method prints all warning messages
        to sys.stderr and returns True. If there are no warnings, this
        method returns False immediately.

        Instead of printing the warnings to sys.stderr, a different
        stream can be specified in the class constructor.

        @return True if there is at least one warnings message, False
          if there are no warnings.
        """
        return self.printWarnings()

    def printParserErrors(self):
        """
        If there are errors, this method prints all error messages to
        sys.stderr and returns True. If there are no errors, this
        method returns False immediately.

        Instead of printing the errors to sys.stderr, a different
        stream can be specified in the class constructor.

        @return True if there is at least one error message, False if
          there are no errors.
        """
        return self.printErrors()

    def coreVersion(self):
        """
        If a util.FeatureDefinition instance was specified in the
        constructor, then this method returns the util.FeatureDefinition's
        core version. Otherwise this method returns None.
        """
        if self.__feature_def is None: return None
        else: return self.__feature_def.coreVersion()

    def setParseDefinition(self, definitions):
        """
        The specified list of ModuleItemParseDefinition instance
        defines how to parse the inpuf file in the next call to
        parseFile(). You can use multiple instance of
        ModuleItemParseDefinition if the file to parse may contain
        different items (e.g. system.txt contains definitions for
        SYSTEM-items, for type-items and macro-items).

        @param definitions is a list of ModuleItemParseDefinition
          instances which is used to parse the file in parseFile()
        """
        self.__parse_definitions = definitions

    def readAllGroups(self, filename):
        """
        Reads all lines of the specified filename. One (or multiple)
        empty lines or lines which contain only a comment (i.e. the
        not-whitespace part of the line starts with the character
        '#'), separate groups of lines. All lines of one group are
        stored in one list as instance of the class util.Line. Empty
        lines and comment lines are not included. Whitespace at the
        right end of the line is removed.

        If the specified file does not exist, this method throws an
        exception.

        Note: this function is not memory efficient because the full
        file is read into memory before processing its content. It
        would be more efficient to process each single line when it is
        read.

        @param filename is the name of the file to read.
        @return an array of groups. Each group is a list of util.Line
          instances.
        """
        groups = []
        f = None
        try:
            f = open(filename)
            current_group = None
            for nr,text in enumerate(f):
                line = util.Line(filename, nr+1, text.rstrip())
                if line.isEmptyOrWhitespace() or line.isComment():
                    # on one (or multiple) empty or comment lines we
                    # start a new group
                    if current_group:
                        groups.append(current_group)
                        current_group = None

                elif (not line.startsWithSpace()
                      or (current_group and len(current_group) == 1 and
                          not current_group[0].startsWithSpace())):
                    # the new line starts with a whitespace and the
                    # current group contains one line that does not
                    # start with a whitespace, so lets start a new
                    # group with this line, because the last group was
                    # the group-definition.
                    if current_group:
                        groups.append(current_group)
                    current_group = [ line ]

                else:
                    # append the line to the current group:
                    if current_group is None: current_group = []
                    current_group.append(line)
            if current_group: groups.append(current_group)
        finally:
            if f is not None: f.close()
        return groups

    def matchItemName(self, line):
        """
        This method tries to match the specified line with any of the
        ModuleItemParseDefinition.reItemName() regular
        expressions. If the line matches, then a new item definition
        starts and this method returns the tuple of the result from
        re.match(line) and the ModuleItemParseDefinition instance that
        provided the regexp:

          match, item_def = self.matchItemName(line)
          if match: ...
          else: # no definition found that matches this line

        @param line is the text to match.
        @return the tuple match,item_def where match is the result of
          re.match() and item_def is the ModuleItemParseDefinition
          instance - or None,None if none ModuleItemParseDefinition
          matched.
        """
        for item_def in self.__parse_definitions:
            match = item_def.reItemName().match(line)
            if match:
                return match, item_def
        return None, None

    def parseFile(self, filename, allowed_types=None):
        """
        Parses the file with the specified filename according to the
        ModuleItemParseDefinition list which was specified in the last
        call to setParseDefinition().

        Warnings and errors in the parsing process are stored in a
        list of warnings() and errors().

        @param filename is the name of the file to parse. If a file
          with name "filename.{core_version}" exists, where "{core_version}"
          is the features.FeatureDefinition.coreVersion() that was specified
          in this instance's constructor, then the versioned file is
          parsed instead of the specified filename.
          If neither the specified filename nor the versioned filename
          exist, an empty list is returned.

        @param allowed_types if not None, then this may be a sub-set of the
          known item types ("FEATURE", "TWEAK", "API", "SYSTEM", "ACTION").
          If an expression prased from the specified file (e.g. an expression
          used for 'depends on') uses any of those known items types which is
          not in the specified set, a parser-error is generated. This is useful
          if you want to parse an expression that is used after features.h
          has undefined all FEATURE_* and after tweaks_and_apis.h has
          undefined all TWEAK_* and API_* defines.

        @return the list of ModuleItem instances that was parsed from
          the specified filename. The ModuleItem instance are created
          according to the ModuleItemParseDefinition instances.
        """

        if self.coreVersion():
            versioned_filename = "%s.%s" % (filename, self.coreVersion())
            if self.__feature_def.allowVersionedFilename():
                # using the versioning in the filename is only allowed
                # for the current mainline configuration:
                if util.fileTracker.addInput(versioned_filename):
                    filename = versioned_filename
            elif util.fileTracker.addInput(versioned_filename):
                # It is not allowed to use a versioned filename for
                # the next mainline configuration. Only the current
                # mainline configuration may have a version
                # number. This allows a smooth transition on releasing
                # one mainline version and switching to the next
                # release.
                self.addParserError(util.Line(filename+"."+self.coreVersion(), 1), "it is not allowed to use a versioned filename for the next mainline configuration. See hardcore's documentation about how to change an item for the next mainline configuration!")

        if not util.fileTracker.addInput(filename):
            return []

        result = []
        item = None
        item_def = None
        for group in self.readAllGroups(filename):
            match, parse_definition = self.matchItemName(str(group[0]))
            if match:
                # a new item starts and parse_definition is an
                # instance of ModuleItemParseDefinition which
                # describes how to continue to parse this item:
                item_def = parse_definition
                item = item_def.createItem(filename=group[0].filename,
                                           linenr=group[0].linenr,
                                           name=match.group(1),
                                           owners=match.group(2))
                if item in result:
                    self.addParserError(group[0], "the %s %s is specified multiple times" % (item.type(), item.name()) )

                item.appendToDescription(group[1:])
                result.append(item)

            elif item:
                # extend the definition of an existing item:
                re_item_definitions = item_def.reItemDefinitions()
                if re_item_definitions:
                    match = re_item_definitions.match(str(group[0]))
                else: match = None

                if match:
                    # this group contains the item's definitions
                    has_depends_on = False
                    lines = iter(group)
                    for line in lines:
                        match = re_item_definitions.match(str(line))
                        if match:
                            what = re.sub("\\s+", " ", match.group(1).lower())
                            value = match.group(2)
                            try:
                                # check if the current line continues
                                # on the next line:
                                while (value.endswith(",") or
                                       value.endswith("&&") or
                                       value.endswith("||") or 
                                       value.strip().endswith("\\")):
                                    value += " " + str(lines.next()).strip()
                            except StopIteration: pass
                            item_def.parseItemDefinition(line, item, what, value)
                            if what.lower() == "depends on":
                                has_depends_on = True

                        else:
                            self.addParserError(line, "unexpected line ignored while parsing " + item.name())

                    # at least one "depends on" field was updated, so update
                    # the item's dependsOn Expression:
                    if has_depends_on: self.finishDependsOn(item, allowed_types)

                else:
                    # if the first line of the group does not match
                    # the definition regexp, we add the full group to
                    # the item's description:
                    item.appendToDescription(map(str, group))

        return result

    def finishDependsOn(self, item, allowed_types=None):
        """
        Parses the "depends on" string of the specified item and assigns the
        resulting Expression to the item's dependsOn() field.

        @param item A ModuleItem that inherits from ItemWithDependsOn.
        @param allowed_types if not None, then this may be a sub-set of the
               known item types ("FEATURE", "TWEAK", "API", "SYSTEM", "ACTION").
               If any "depends on" uses any of those known items types which are
               not in the specified set, a parser-error is generated. This is
               useful if you want to parse an expression that is used after
               features.h has undefined all FEATURE_* and after
               tweaks_and_apis.h has undefined all TWEAK_* and API_* defines.
        """
        try:
            item.setDependsOn(self.parseExpression(
                    item.line(), item.dependsOnStrings(),
                    allowed_types=allowed_types))
        except ParseExpressionException, error:
            print >>sys.stderr, error
            self.addParserError(item.line(), "%s: while parsing dependencies: %s" % (item.name(), error))
        except AttributeError:
            # if the item is not derived from ItemWithDependsOn we get an
            # AttributeError, because item.dependsOnStrings is not defined;
            # just ignore this case...
            pass

    def parseExpression(self, line, sources, allowed_types=None):
        """
        Parses an expression that is used for the "depends on" or "import if"
        definition of an item. Returns an Expression instance that corresponds
        to the parsed expression.

        @param line is the util.Line() of the corresponding ModuleItem. Usually
               this argument is set to ModuleItem.line(). This value is used on
               printing error messages.
        @param sources is either a string containing the expression to parse,
               or a list of strings, where each string corresponds to a single
               "depends on" or "import if" line. The list of strings will be
               combined in an OrExpression.
        @param allowed_types if not None, then this may be a sub-set of the
               known item types ("FEATURE", "TWEAK", "API", "SYSTEM", "ACTION").
               If an expression uses any of those known items types which is not
               in the specified set, a parser-error is generated. This is useful
               if you want to parse an expression that is used after features.h
               has undefined all FEATURE_* and after tweaks_and_apis.h has
               undefined all TWEAK_* and API_* defines.
        """
        if type(sources) == list:
            if len(sources) > 1:
                return OrExpression([self.parseExpression(line, source, allowed_types) for source in sources])
            elif len(sources) == 1: sources = sources[0]
            else: return Expression()

        import clexer
        try:
            tokens = clexer.group(filter(lambda token: not token.isspace(),
                                         clexer.split(sources)))
        except clexer.CLexerException, error:
            raise ParseExpressionException, "failed to tokenize: %r\n%s" % (sources, error)

        def splitTokenSequence(tokens, at):
            """
            Splits an array of tokens at the specified token into smaller
            arrays. This can be used as an iterator.

            tokens = ["one", "two", ":", "three"]
            for sequence in splitTokenSequence(tokens, ":"):
                print sequence

            prints ["one", "two"] and ["three"].

            @param tokens is a list of tokens to split.
            @param at is the token at which to split.
            @return a sub-list of tokens from the last position until the token
               at (which is not included in any of the returned sub-lists).
            """
            hasSplit = False
            current = []
            for token in tokens:
                if token == at:
                    yield current
                    hasSplit = True
                    current = []
                else: current.append(token)
            if hasSplit: yield current

        def inner(tokens):
            """
            Recursively parse a list of tokens (a token is a string). Returns
            an Expression that corresponds to the list of tokens.

            @param tokens is a list of tokens to parse.
            @return Expression instance that corresponds to the list of tokens.
            """
            if len(tokens) == 1 and type(tokens[0]) == list:
                return inner(tokens[0])
            if tokens[0] == "(" and tokens[-1] == ")":
                return inner(tokens[1:-1])

            # or_expression: expression || expression
            if "||" in tokens:
                return OrExpression(map(inner, splitTokenSequence(tokens, "||")))

            # and_expression: expression && expression
            elif "&&" in tokens:
                return AndExpression(map(inner, splitTokenSequence(tokens, "&&")))

            # not_expression: !expresion
            elif tokens[0] == "!": return NotExpression(inner(tokens[1:]))

            # token == YES or token != YES or token == NO or token != NO
            if len(tokens) == 3 and tokens[1] in ("==", "!="):
                operator = tokens[1]
                # Put the YES/NO operand to the right:
                if tokens[0] in ("YES", "NO"):
                    lhs, rhs = tokens[2], tokens[0]
                else: lhs, rhs = tokens[0], tokens[2]

                if rhs not in ("YES", "NO"):
                    raise ParseExpressionException, "confusing comparison expression ('%s%s%s'); please only compare things to YES or NO" % (lhs, operator, rhs)

                item_type = lhs.split("_", 1)[0]
                if item_type in ("FEATURE", "SYSTEM", "TWEAK", "API", "ACTION"):
                    if (allowed_types is not None and
                        item_type not in allowed_types):
                        raise ParseExpressionException, "you are not allowed to use an item of type %s in this expression" % item_type
                else:
                    raise ParseExpressionException, "confusing comparison expression ('%s'); please only compare features, systems, tweaks, APIs or actions to YES or NO" % lhs

                if (self.dependentItemStore().hasItem(lhs) and
                    self.dependentItemStore().getItem(lhs).isDeprecated()):
                    self.addParserWarning(line, "%s is deprecated" % lhs)

                expression = ExpressionItem(lhs)
                if (rhs == "YES") == (operator == "=="): return expression
                else: return NotExpression(expression)

            # '"defined" token' or '"defined" "(" token ")"'
            elif len(tokens) == 2 and tokens[0] == "defined":
                if type(tokens[1]) == list:
                    if len(tokens[1]) == 3 and tokens[1][0] == "(" and tokens[1][2] == ")":
                        define = tokens[1][1]
                    else:
                        raise ParseExpressionException, "syntax error: invalid defined: 'defined %s'" % (" ".join(tokens[1]))
                else: define = tokens[1]

                item_type = define.split("_", 1)[0]
                if item_type in ("FEATURE", "SYSTEM", "TWEAK", "ACTION"):
                    raise ParseExpressionException, "you don't want to check whether a %s is defined: %s" % (item_type, define)

                else: return ExpressionItem(define)

            elif len(tokens) == 1:
                item_type = tokens[0].split("_", 1)[0]
                if item_type in ("FEATURE", "SYSTEM", "TWEAK", "API", "ACTION"):
                    if (allowed_types is not None and
                        item_type not in allowed_types):
                        raise ParseExpressionException, "you are not allowed to use an item of type %s in this expression" % item_type
                    else: return inner([tokens[0], "==", "YES"])
                else:
                    return inner(["defined", tokens[0]])

            raise ParseExpressionException, "syntax error on %r" % tokens

        and_expressions = map(inner, splitTokenSequence(tokens, ","))
        if and_expressions: return AndExpression(and_expressions)
        else: return inner(tokens)

    def resolveDependencies(self):
        """
        Resolves recursive dependency checks for all items in this instance. The
        items are thus added to a list such that an item does not depend on any
        item that has a lower index in that list. The calculated list is stored
        in this instance and can be accessed by dependentItems() and it is
        returned.

        @note If two items depend on each other, a parser error is added.

        @return the list of items sorted by dependency.
        """
        dependent_items = []
        independent_items = []
        items = list(self.dependentItemStore().items())
        items.sort(cmp=ModuleItem.sortByName)
        for item in items:
            if self.dependentItemStore().isIndependentItem(item):
                independent_items.append(item)
            else:
                inserted = False
                for index,other in enumerate(reversed(dependent_items)):
                    if self.dependentItemStore().dependsOnOther(item, other):
                        if self.dependentItemStore().dependsOnOther(other, item):
                            self.addParserError(item.line(), "%s and %s depend on each other" % (item.name(), other.name()))
                        dependent_items.insert(len(dependent_items) - index, item)
                        inserted = True
                        break

                if not inserted:
                    dependent_items.insert(0, item)

        dependent_items[0:0] = independent_items
        self.__dependent_items = dependent_items
        return dependent_items
