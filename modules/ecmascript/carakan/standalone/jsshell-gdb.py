

def parseAndEvaluate(exp):
    try:
        return gdb.parse_and_eval(exp)
    except AttributeError:
#        gdb.execute("set logging redirect on", True)
#        gdb.execute("set logging on", True)
        gdb.execute("print %s" % exp, True)
#        gdb.execute("set logging off", True)
        return gdb.history(0)

MASK_GCTAG_V = None

def MASK_GCTAG():
    global MASK_GCTAG_V
    if MASK_GCTAG_V == None:
        MASK_GCTAG_V = int(parseAndEvaluate("ES_Header::MASK_GCTAG"))
    return MASK_GCTAG_V

GC_Tag_V = None

def GC_Tag():
    global GC_Tag_V
    if GC_Tag_V == None:
        GC_Tag_V = gdb.lookup_type("GC_Tag")
    return GC_Tag_V

GCTAG_JString_V = None

def GCTAG_JString():
    global GCTAG_JString_V
    if GCTAG_JString_V == None:
        GCTAG_JString_V = int(parseAndEvaluate("GCTAG_JString"))
    return GCTAG_JString_V

GCTAG_JStringStorage_V = None

def GCTAG_JStringStorage():
    global GCTAG_JStringStorage_V
    if GCTAG_JStringStorage_V == None:
        GCTAG_JStringStorage_V = int(parseAndEvaluate("GCTAG_JStringStorage"))
    return GCTAG_JStringStorage_V

GCTAG_ES_FunctionCode_V = None

def GCTAG_ES_FunctionCode():
    global GCTAG_ES_FunctionCode_V
    if GCTAG_ES_FunctionCode_V == None:
        GCTAG_ES_FunctionCode_V = GCTAG_ES_FunctionCode = int(parseAndEvaluate("GCTAG_ES_FunctionCode"))
    return GCTAG_ES_FunctionCode_V

GCTAG_ES_ProgramCode_V = None

def GCTAG_ES_ProgramCode():
    global GCTAG_ES_ProgramCode_V
    if GCTAG_ES_ProgramCode_V == None:
        GCTAG_ES_ProgramCode_V = int(parseAndEvaluate("GCTAG_ES_ProgramCode"))
    return GCTAG_ES_ProgramCode_V

TYPE_ExecContext = gdb.lookup_type("ES_Execution_Context")
PTR_ExecContext = TYPE_ExecContext.pointer()

TYPE_Code = gdb.lookup_type("ES_Code")
PTR_Code = TYPE_Code.pointer()
PTR2_Code = PTR_Code.pointer()

TYPE_Value = gdb.lookup_type("ES_Value_Internal")
PTR_Value = TYPE_Value.pointer()
PTR2_Value = PTR_Value.pointer()

TYPE_FunctionCode = gdb.lookup_type("ES_FunctionCode")
PTR_FunctionCode = TYPE_FunctionCode.pointer()

TYPE_CodeStatic = gdb.lookup_type("ES_CodeStatic")
PTR_CodeStatic = TYPE_CodeStatic.pointer()

TYPE_FunctionCodeStatic = gdb.lookup_type("ES_FunctionCodeStatic")
PTR_FunctionCodeStatic = TYPE_FunctionCodeStatic.pointer()

TYPE_UniChar = gdb.lookup_type("uni_char")
PTR_UniChar = TYPE_UniChar.pointer()

TYPE_UINTPTR = gdb.lookup_type("UINTPTR")
TYPE_unsigned = gdb.lookup_type("unsigned int")

TYPE_JString = gdb.lookup_type("JString")
PTR_JString = TYPE_JString.pointer()

SIXTY_FOUR_BIT = int(parseAndEvaluate("sizeof(void *)")) == 8

def uni_strlen(string):
    "calculate length of 'uni_char *' string"

    length = 0
    while int(string[length]) != 0:
        length += 1
    return length
    
def filename(code):
    "ES_Code => file name from code->data->source.url"

    url = code["url"]

    if validate_pointer(url):
        return str(JString(url))
    else:
        return ""

def linenr(code):
    "ES_Code => line number from code->data->source.line_offset"

    return int(code["data"]["source"]["line_offset"])

class ES_Boxed(object):
    def __init__(self, value):
        self.value = value

    def header_bits(self):
        return int(self.value.cast(TYPE_unsigned.pointer())[0])

    def object_size(self):
        return int(self.value.cast(TYPE_unsigned.pointer())[1])

    def to_string(self):
        try:
            if is_valid_pointer(self.value):
                return "(ES_Boxed *) 0x%x [%s]" % (int(self.value.cast(TYPE_UINTPTR)), gdb.Value(self.header_bits() & MASK_GCTAG()).cast(GC_Tag()))
        except:
            pass

        return "(ES_Boxed *) 0x%x" % int(self.value.cast(TYPE_UINTPTR))

class ES_Object(ES_Boxed):
    def __init__(self, value):
        super(ES_Object, self).__init__(value)

    def to_string(self):
        try:
            if is_valid_pointer(self.value):
                return "(ES_Object *) 0x%x [%s]" % (int(self.value.cast(TYPE_UINTPTR)), gdb.Value(self.header_bits() & MASK_GCTAG()).cast(GC_Tag()))
        except: pass

        return "(ES_Object *) 0x%x" % int(self.value.cast(TYPE_UINTPTR))

class JString(ES_Boxed):
    def __init__(self, value):
        super(JString, self).__init__(value)

    def __str__(self):
        return self.to_string(True).encode("utf-8")

    def to_string(self, plain=False):
        ptrval = self.value.cast(TYPE_UINTPTR)
        if plain: format = "%(data)s"
        else: format = "(JString *) 0x%(ptrval)x %(data)s"
        if int(ptrval) != 0:
            if (self.header_bits() & MASK_GCTAG()) != GCTAG_JString():
                data = "<invalid>"
            else:
                offset = int(self.value["offset"]);
                length = int(self.value["length"])
                value = self.value["value"]

                if (int(value.cast(TYPE_UINTPTR)) & 1) != 0:
                    data = "<segmented>"
                else:
                    data = repr(value["storage"].address.cast(PTR_UniChar).string("utf-16", length=int(offset + length) * 2)[offset:][:length])[2:-1]
                    if not plain: data = '"%s"' % data
        else:
            data = "<null>"
        return format % { "ptrval": ptrval, "data": data }

class JStringStorage(ES_Boxed):
    def __init__(self, value):
        super(JStringStorage, self).__init__(value)

    def __str__(self):
        return self.to_string().encode("utf-8")

    def to_string(self):
        ptrval = self.value.cast(TYPE_UINTPTR)
        result = "(JStringStorage *) 0x%x" % ptrval
        try:
            if int(ptrval) != 0:
                if int(ptrval) & 1:
                    result += " <segmented>"
                else:
                    length = int(self.value["length"])
                    result += ' \"%s\"' % repr(self.value["storage"].address.cast(PTR_UniChar).string("utf-16", length=int(length) * 2))[2:-1]
        except: pass
        return result

class ES_Value_Internal:
    def __init__(self, value):
        self.value = value

    def get_type(self):
        if SIXTY_FOUR_BIT:
            return str(self.value["private_type"])
        else:
            if int(self.value["other"]["type"]) <= int(parseAndEvaluate("ESTYPE_DOUBLE")):
                return "ESTYPE_DOUBLE"
            else:
                return str(self.value["other"]["type"])

    def to_string(self):
        type = self.get_type()
        value = None

        if SIXTY_FOUR_BIT:
            if self.get_type() == "ESTYPE_INT32":
                value = str(self.value["private_value"]["i32"])
            elif self.get_type() == "ESTYPE_DOUBLE":
                value = str(self.value["private_value"]["number"])
            elif self.get_type() == "ESTYPE_BOOLEAN":
                if int(self.value["private_value"]["boolean"]) == 0: value = "false"
                else: value = "true"
            elif self.get_type() == "ESTYPE_STRING":
                value = JString(self.value["private_value"]["string"]).to_string()
            elif self.get_type() == "ESTYPE_BOXED":
                value = ES_Boxed(self.value["private_value"]["boxed"]).to_string()
            elif self.get_type() == "ESTYPE_OBJECT":
                value = ES_Object(self.value["private_value"]["object"]).to_string()
        else:
            if self.get_type() != "ESTYPE_NULL" and self.get_type() != "ESTYPE_UNDEFINED":
                if self.get_type() == "ESTYPE_INT32":
                    value = str(self.value["other"]["value"]["i32"])
                elif self.get_type() == "ESTYPE_BOOLEAN":
                    if int(self.value["other"]["value"]["boolean"]) == 0: value = "false"
                    else: value = "true"
                elif self.get_type() == "ESTYPE_STRING":
                    value = JString(self.value["other"]["value"]["string"]).to_string()
                elif self.get_type() == "ESTYPE_BOXED":
                    value = ES_Boxed(self.value["other"]["value"]["boxed"]).to_string()
                elif self.get_type() == "ESTYPE_OBJECT":
                    value = ES_Object(self.value["other"]["value"]["object"]).to_string()
                else:
                    value = str(self.value["double_value"])

        if value is None: return "{ %s }" % type
        else: return "{ %s: %s }" % (type, value)

class ES_SourceLocation:
    def __init__(self, value):
        self.__low = int(value["low"])
        self.__high = int(value["high"])

    def is_valid(self): return self.__low != 0xffffffff or self.__high != 0xffffffff
    def index(self): return self.__low & 0xffffff
    def line(self): return ((self.__low & 0xff000000) >> 24) | ((self.__high & 0xfff) << 8)
    def length(self): return (self.__high & 0xfffff000) >> 12

class ES_Code:
    def __init__(self, value):
        self.__value = value

    def to_string(self):
        if is_valid_pointer(self.__value):
            if str(self.__value["type"]) == "ES_Code::TYPE_FUNCTION":
                fncode = self.__value.cast(PTR_FunctionCode)
                fndata = fncode["data"].cast(PTR_FunctionCodeStatic)

                if int(fndata["name"]) != 0xffffffff:
                    name = JString(fncode["strings"][int(fndata["name"])])
                elif int(fndata["debug_name"]) != 0xffffffff:
                    name = JString(fncode["strings"][int(fndata["debug_name"])])
                else:
                    name = "<anonymous>"

                formals = []
                for index in range(int(fndata["formals_count"])):
                    formals.append(str(JString(fncode["strings"][int(fndata["formals"][index])])))
                formals = ", ".join(formals)

                return "(ES_FunctionCode *) 0x%x [%s(%s) at line %d in %s]" % (int(self.__value.cast(TYPE_UINTPTR)), name, formals, linenr(fncode), filename(fncode))
            else:
                return "(ES_ProgramCode *) 0x%x [%s]" % (int(self.__value.cast(TYPE_UINTPTR)), filename(self.__value))
        else:
            return "(ES_Code *) 0x%x" % int(self.__value.cast(TYPE_UINTPTR))

    def describe_frame(self, location=None):
        code = self.__value

        if str(code["type"]) == "ES_Code::TYPE_FUNCTION":
            fncode = code.cast(PTR_FunctionCode)
            fndata = code["data"].cast(PTR_FunctionCodeStatic)

            if int(fndata["name"]) != 0xffffffff:
                name = JString(fncode["strings"][int(fndata["name"])])
            elif int(fndata["debug_name"]) != 0xffffffff:
                name = JString(fncode["strings"][int(fndata["debug_name"])])
            else:
                name = "<anonymous>"

            formals = []
            for index in range(int(fndata["formals_count"])):
                formals.append(str(JString(fncode["strings"][int(fndata["formals"][index])])))
            formals = ", ".join(formals)

            if location and location.is_valid(): line = str(location.line())
            else: line = "(%d)" % linenr(code)

            return "function %s(%s) at line %s in %s" % (name, formals, line, filename(code))
        elif str(code["type"]) == "ES_Code::TYPE_PROGRAM":
            if is_valid_pointer(code["native_code_block"]) and str(code["native_code_block"]["type"]) == "ES_NativeCodeBlock::TYPE_LOOP":
                return "dispatcher for loop in %s" % filename(code["parent_code"])
            else:
                if location and location.is_valid(): return "program code at line %d in %s" % (location.line(), filename(code))
                return "program code in %s" % filename(code)
        else:
            if is_valid_pointer(code["native_code_block"]) and str(code["native_code_block"]["type"]) == "ES_NativeCodeBlock::TYPE_LOOP":
                return "dispatcher for loop in eval code"
            else:
                return "eval code"

    def resolve_location(self, ip):
        if not is_valid_pointer(ip): return None

        data = self.__value["data"]
        records = data["debug_records"]
        count = int(data["debug_records_count"])

        if not count: return None

        cw_index = (ip.cast(TYPE_UINTPTR) - self.__value["data"]["codewords"].cast(TYPE_UINTPTR)) / int(parseAndEvaluate("sizeof(ES_CodeWord)"))
        candidate = None

        for index in range(count):
            record = records[index]
            if cw_index < int(record["codeword_index"]): break
            if str(record["type"]) == "ES_CodeStatic::DebugRecord::TYPE_EXTENT_INFORMATION": candidate = record

        if candidate is None: return None
        else: return ES_SourceLocation(candidate["location"])

    def value(self):
        return self.__value

def validate_pointer(ptr):
    if ptr is None or int(ptr.cast(TYPE_UINTPTR)) == 0: return None
    else: return ptr

def is_valid_pointer(ptr):
    return validate_pointer(ptr) is not None
            
class StackFrame(object):
    def __str__(self):
        code = self.code()
        if code is None:
            next_instr = str(self.ip()["instruction"])
            if next_instr.startswith("ESI_EXIT"):
                if next_instr == "ESI_EXIT_TO_BUILTIN": return "<call to builtin>"
                elif next_instr == "ESI_EXIT_TO_EVAL": return "<eval>"
                else: return "<exit frame>"
            else: return "<internal frame>"
        else: return self.code().describe_frame(self.code().resolve_location(self.ip()))

class BCStackFrame(StackFrame):
    def __init__(self, context, ptr=None):
        self.__context = context
        self.__ptr = ptr

    def code(self):
        if self.__ptr is None: code = validate_pointer(self.__context["code"])
        else: code = validate_pointer(self.__ptr["code"])
        if code is not None: return ES_Code(code)
        else: return None

    def registers(self):
        if self.__ptr is None: return self.__context["reg"]
        else: return self.__ptr["first_register"]

    def ip(self):
        if self.__ptr is None: return self.__context["ip"]
        else: return self.__ptr["ip"]

    def is_native(self):
        return False

    def native_frame(self):
        if self.__ptr:
            ptr = validate_pointer(self.__ptr["native_stack_frame"])
            if ptr is not None: return JITStackFrame(ptr)
            else: return None
        else: return None

class JITStackFrame(StackFrame):
    def __init__(self, ptr):
        self.__ptr = ptr

    def code(self):
        return ES_Code(self.__ptr.cast(PTR2_Code)[-2])

    def registers(self):
        return self.__ptr.cast(PTR2_Value)[-1]

    def ip(self):
        return None

    def is_native(self):
        return True

    def next(self):
        next_frame = JITStackFrame(self.__ptr.cast(self.__ptr.type.pointer()).dereference())
        if next_frame.final(): return None
        else: return next_frame

    def final(self):
        return not is_valid_pointer(self.__ptr.cast(PTR2_Code)[-2])

def frame_stack(context):
    if is_valid_pointer(context["native_stack_frame"]):
        frame = JITStackFrame(context["native_stack_frame"])

        while frame:
            yield frame
            frame = frame.next()
    else:
        yield BCStackFrame(context)

    block = context["frame_stack"]["last_used_block"]

    while block:
        index = int(block["used"])

        while index != 0:
            index -= 1

            frame = BCStackFrame(context, block["storage"][index].address)
            native_frame = frame.native_frame()

            if native_frame is not None:
                while native_frame:
                    yield native_frame
                    native_frame = native_frame.next()
            else:
                yield frame

        block = validate_pointer(block["suc"].cast(block.type))

class ESCommand(gdb.Command):
    def __init__(self, *args):
        super(ESCommand, self).__init__(*args)

    def context(self, override=None):
        if override:
            return parseAndEvaluate(override).cast(PTR_ExecContext)
        elif gdb.selected_frame().name() is not None:
            try:
                return gdb.selected_frame().read_var("context").cast(PTR_ExecContext)
            except:
                return gdb.selected_frame().read_var("this")
        else:
            if SIXTY_FOUR_BIT:
                return parseAndEvaluate("$rbp").cast(PTR_ExecContext)
            else:
                return parseAndEvaluate("$ebp").cast(PTR_ExecContext)

class JITWhere(ESCommand):
    def __init__(self):
        super(JITWhere, self).__init__("jwhere", gdb.COMMAND_OBSCURE)

    def invoke(self, arguments, from_tty):
        print JITStackFrame(self.context()["native_stack_frame"])

class ESBacktrace(ESCommand):
    def __init__(self):
        super(ESBacktrace, self).__init__("esbt", gdb.COMMAND_OBSCURE)

    def invoke(self, arguments, from_tty):
        override = None
        
        try:
            count = int(arguments)
        except:
            count = 20
            if arguments: override = arguments

        level = 0

        for frame in frame_stack(self.context(override)):
            print "%-3s %s%s" % ("#%d" % level, frame.is_native() and "[JIT] " or "", frame)
            level += 1

            if level >= count: break

class ESNativeDisassemble(ESCommand):
    def __init__(self):
        super(ESNativeDisassemble, self).__init__("esndisass", gdb.COMMAND_OBSCURE)

    def invoke(self, arguments, from_tty):
        code = frame_stack(self.context()).next().code().value()
        ncblock = code["native_code_block"]

        if not is_valid_pointer(ncblock):
            print "current function not JIT:ed"
            return

        block = ncblock["block"]
        start = int(block["address"].cast(TYPE_UINTPTR))
        end = start + int(block["size"])

        try:
            gdb.execute("disassemble 0x%x 0x%x" % (start, end), from_tty)
        except:
            gdb.execute("disassemble 0x%x, 0x%x" % (start, end), from_tty)

class ESDisassemble(ESCommand):
    def __init__(self):
        super(ESDisassemble, self).__init__("esdisass", gdb.COMMAND_OBSCURE)

    def invoke(self, arguments, from_tty):
        if arguments == "":
            context = self.context()
            code = frame_stack(context).next().code().value()
        else:
            context = self.context("context")
            code = ES_Code(parseAndEvaluate(arguments)).value()

        parseAndEvaluate("ES_DisassembleFromDebugger((ES_Execution_Context *) %d, (ES_Code *) %d)" % (int(context.cast(TYPE_UINTPTR)), int(code.cast(TYPE_UINTPTR))))

class ESPrintClass(ESCommand):
    def __init__(self):
        super(ESPrintClass, self).__init__("esprintclass", gdb.COMMAND_OBSCURE)

    def invoke(self, arguments, from_tty):
        if arguments == "":
            arguments = "this"
        try:
            context = self.context()
            parseAndEvaluate("ES_PrintClass((ES_Execution_Context *) %d, (ES_Class *) %d)" % (int(context.cast(TYPE_UINTPTR)), int(parseAndEvaluate(arguments).cast(TYPE_UINTPTR))))
        except:
            parseAndEvaluate("ES_PrintClass(%d, (ES_Class *) %d)" % (int(0), int(parseAndEvaluate(arguments).cast(TYPE_UINTPTR))))

JITWhere()
ESBacktrace()
ESNativeDisassemble()
ESDisassemble()
ESPrintClass()

def jit_lookup_pretty_printer(value):
    if value.type.code == gdb.TYPE_CODE_PTR and value.type.target().tag == "JString":
        return JString(value)
    elif value.type.code == gdb.TYPE_CODE_PTR and value.type.target().tag == "JStringStorage":
        return JStringStorage(value)
    elif value.type.code == gdb.TYPE_CODE_PTR and value.type.target().tag == "ES_Boxed":
        return ES_Boxed(value)
    elif value.type.code == gdb.TYPE_CODE_PTR and value.type.target().tag == "ES_Object":
        return ES_Object(value)
    elif value.type.tag == "ES_Value_Internal":
        return ES_Value_Internal(value)
    elif value.type.code == gdb.TYPE_CODE_PTR and value.type.target().tag in ("ES_Code", "ES_ProgramCode", "ES_FunctionCode"):
        return ES_Code(value)
    else:
        return None

gdb.current_objfile().pretty_printers.append(jit_lookup_pretty_printer)

print "jsshell-gbb.py loaded"
