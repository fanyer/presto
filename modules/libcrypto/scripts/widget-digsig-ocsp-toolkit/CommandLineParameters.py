#!/usr/bin/python

import os


class Parameter:
    _short      = None
    _name       = None
    _descr      = None
    _number     = None
    _types      = None
    _values     = None
    _defaults   = None
    _flags      = None
    
    def __init__(self, short, name, descr, number):
        self._short  = short
        self._name   = name
        self._descr  = descr
        self._number = int(number)
        self._types  = []
        self._flags  = {}
        
        if (self._number == 0): self._values = [False] # ON/OFF arguments
    # def __init__
    
    def setFlags(self, flags):
        error = (False, "")
        
        for flag in flags:
            self._flags[flag] = True
            
        return error
    # def setFlags
    
    def setDefaults(self, defaults):
        error          = (False, "")
        self._defaults = defaults
        
        if (self._flags.has_key('pick')):
            for i in range(len(self._defaults)):
                self._defaults[i] = self._defaults[i].split(",")
            # for in
        # if
        
        return error
    # def setDefaults
    
    def setTypes(self, types):
        error = (False, "")
        
        if (len(types) == self._number):
            for t in types:
                if (t == 'number'): self._types.append(True)
                if (t == 'string'): self._types.append(False)
            # for in
        else:
            error = (True, "Wrong number of argument types: is %d, should be %d" % (len(types), self._number))
        # if else
        
        return error
    # def __init__
    
    def setValues(self, values):
        error = (False, "")
        
        if ((self._values != True) and (len(values) == self._number)):
            if (self._number == 0): self._values = [True]
            else:                   self._values = values
            
            for arg in range(self._number):
                if (self._types[arg] == True):
                    if (self._values[arg].isdigit()):
                        self._values[arg] = int(self._values[arg])
                    else:
                        error = (True, "Invalid argument format: is %s, should be numeric" % (self._values[arg]))
                    # if else
                # if
                
                if (self._flags.has_key('pick')):
                    found  = False
                    params = ""
                    
                    for default in self._defaults[arg]:
                        params += " %s" % (default)
                        if (default == self._values[arg]): found = True
                    # for in
                    
                    if not (found):
                        error = (True, "Invalid value '%s' for argument '%s' (possible values:%s)" % (self._values[arg], self._name, params))
                        break
                    # if not
                # if
            # for in
        else:
            error = (True, "Wrong number of arguments: is %d, should be %d" % (len(list), self._number))
        # if else
        
        return error
    # def setValues
    
    def getValue(self, value = False):
        if (self._values == None):
            return None
        # if
        
        if (value):
            if (value >= len(self._values)):
                value = None
            else:
                value = self._values[value]
            # if else
        else:
            if (len(self._values) > 1): value = self._values
            else:                       value = self._values[0]
        # if else
        
        return value
    # def getValue
# class

class CommandLineParameters:
    _argfull    = None
    _argshort   = None
    _parameters = None
    _caller     = None
    
    def __init__(self):
        self._argfull    = {}
        self._argshort   = {}
        self._parameters = []
    # def __init__
    
    def readFromFile(self, filename):
        error       = (False, "")
        
        if (not os.path.exists(filename)):
            error = (True, "Configuration file not found: '%s'" % (filename))
        else:
            file        = open(filename, "rb")
            line_number = 0
            for line in file:
                line_number += 1
                line         = line.split("^")
                
                short        = line[0].strip()
                name         = line[1].strip()
                descr        = line[6].strip()
                number       = line[2].strip()
                types        = line[3].strip()
                
                if (types != ""): types = types.split(" ")
                else:             types = []
                
                flags        = line[4].strip()
                
                if (flags != ""): flags = flags.split(" ")
                else:             flags = []
                
                defaults     = line[5].strip()
                
                if (defaults != ""): defaults = defaults.split(" ")
                else:                defaults = []
                
                if (self.addParameter(short, name, descr, number, types, flags, defaults)):
                    errstr  = "ERROR: while parsing '%s' at line %d: '%s'" % (filename, line_number, line)
                    errstr += "\nMESSAGE: %s\n" % (error[1])
                    error   = (True, errstr)
                    break
                # if else
            # for in
            file.close()
        # if else
        
        if (error[0]): self._help(error[1])
        
        return error[0]
    # def readFromFile
    
    def addParameter(self, short, name, descr, number, types, flags, defaults):
        error = (False, "")
        
        if (self._argfull.has_key(name) or self._argshort.has_key(short)):
            error = (True, "Parameter '%s' ('%s') already exists." % (name, short))
        else:
            if (number.isdigit()):
                param = Parameter(short, name, descr, number)
                error = param.setTypes(types)
                
                if  (not error[0]):                                         error = param.setFlags(flags)
                if  (not error[0]):                                         error = param.setDefaults(defaults)
                if ((not error[0]) and (param._flags.has_key('defaults')) and (param._flags.has_key('pick'))): defaults = [defaults[0][0]]
                if ((not error[0]) and (param._flags.has_key('defaults'))): error = param.setValues(defaults)
            else:
                error = (True, "Invalid number of arguments: is %s, should be numeric" % (number))
            # if else
            
            if not (error[0]):
                self._argfull [name ] = param
                self._argshort[short] = param
                self._parameters.append(param)
            # if not
        # if else
        
        if (error[0]): self._help(error[1])
        
        return error[0]
    # def addParameter
    
    def readArgs(self, args):
        error = (False, "")
        
        for i in range(len(args)):
            arg = args[i]
            
            if (i == 0):
                self._caller = arg
                continue
            # if
            
            if   (arg.find("--") == 0): arg = arg[2:]
            elif (arg.find("-" ) == 0): arg = arg[1:]
            else:                       continue
            
            if (arg == "h" or arg == "help"):
                error = (True, "")
                break
            # if
            
            if (self._argfull.has_key(arg)):
                error = self._argfull [arg].setValues(args[i+1:i+1 + self._argfull [arg]._number])
                
                if (error[0]): break
            elif (self._argshort.has_key(arg)):
                error = self._argshort[arg].setValues(args[i+1:i+1 + self._argshort[arg]._number])
                
                if (error[0]): break
            else:
                error = (True, "Unknown argument: '%s'" % (arg))
                break
            # if elif else
        # for in
        
        if (not error[0]):
            for name, arg in self._argfull.iteritems():
                if ((arg._flags.has_key('mandatory')) and (arg._values == None)):
                    error = (True, "Missing mandatory parameter: '%s'" % (name))
                    break
                # if and
            # for in
        # if
        
        if (error[0]): self._help(error[1])
        
        return error[0]
    # def readArgs
    
    def getValue(self, arg, value_number = False):
        value = None
        
        if (self._argfull.has_key (arg)): value = self._argfull [arg].getValue(value = value_number)
        if (self._argshort.has_key(arg)): value = self._argshort[arg].getValue(value = value_number)

        return value
    # def getValue
    
    def _help(self, msg):
        string  = "\n"
        
        if (msg != ""): string += "%12s  %s\n\n" % ("Usage error:", msg)
        
        string += "%12s  %s\n" % ("Syntax:", "%s --<mandatory parameters> <arguments> [--<parameters> <arguments>]" % (self._caller))
        
        for arg in self._parameters:
            mandatory = ""
            default   = ""
            pick      = ""
            
            if (arg._flags.has_key('mandatory')): mandatory += " (mandatory)"
            if (arg._flags.has_key('defaults' )):
                default += " [default: "
                
                if (arg._flags.has_key('pick')): defaults = [arg._defaults[0][0]]
                else:                            defaults = arg._defaults
                
                for value in defaults:
                    default += " %s" % (value)
                # for in
                
                default += "]"
            # if
            
            if (arg._flags.has_key('pick'     )):
                for values in arg._defaults:
                    pickstr = ""
                    
                    for value in values:
                        pickstr += "|%s" % (value)
                    # for in
                    
                    pickstr = pickstr[1:]
                    pick   += " %s" % (pickstr)
                # for in
            # if
            
            string += "%12s  %s%s\n"    % ("--%s" % (arg._name), arg._descr, mandatory)
            
            if (arg._short == arg._name): short = ""
            else:                         short = "-%s" % (arg._short)
            
            string += "%12s  Values:" % (short)
            
            if (pick == ""):
                for t in arg._types:
                    if (t): string += " <number>"
                    else:   string += " <string>"
                # for in
            else:
                string += pick
            # if else
            
            if (arg._number == 0): string += " none"
            else:                  string += default
            
            string += "\n"
        # for in
        
        print string
    # def _help
# class CommandLineParameters

CommandLineParameters = CommandLineParameters()
#CommandLineParameters.readFromFile("help.cfg")
#CommandLineParameters.readArgs(["-b", "CORE-22016-20",  "--profile", "smartphone", "-suf", "test", "-e", "3", "-h"])
