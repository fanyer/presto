/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*- */

#ifndef JSSHELL_OBJECT_H
#define JSSHELL_OBJECT_H

#include "modules/ecmascript/ecmascript.h"

class BuiltinObject;

typedef int (Function)(BuiltinObject *this_object, ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime);

class BuiltinObject : public EcmaScript_Object
{

};

class BuiltinFunction : public BuiltinObject
{
public:
    BuiltinFunction(Function *function) : function(function) {}

    virtual int Call(ES_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime);

private:
    Function *function;
};


class JSShell_Object : public EcmaScript_Object
{
public:
    JSShell_Object() { }
    virtual ~JSShell_Object();

    void InitializeL();

    static int write(BuiltinObject *this_object, ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime);
    static int writeln(BuiltinObject *this_object, ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime);
    static int getPropertyNames(BuiltinObject *this_object, ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime);
    static int quit(BuiltinObject *this_object, ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime);
    /** load('<file-name-1>', [<file-name-2>, ...]) attempts to open, concatenate then parse, compile and execute specified files */
    static int load(BuiltinObject *this_object, ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime);
    static int readline(BuiltinObject *this_object, ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime);

    /** loadFileIntoString('<file-name>') --> JString (each byte of the file contents is converted to uni_char) */
    static int loadFileIntoString(BuiltinObject *this_object, ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime);
    /** saveStringAsFile('<file-name>', '<str-content>') (each uni_char in the str should have codebase < 0xff and is converted to a byte in the file) */
    static int saveStringAsFile(BuiltinObject *this_object, ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime);
    /** appendStringToFile('<file-name>', '<str-to-append>') (each uni_char in the str should have codebase < 0xff and is converted to a byte in the file) */
    static int appendStringToFile(BuiltinObject *this_object, ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime);

#ifdef DEBUG_ENABLE_OPASSERT
    /** break('<boolean>') break if argument is true*/
    static int trap(BuiltinObject *this_object, ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime);
#endif

#ifdef ES_DISASSEMBLER_SUPPORT
    /** disassemble('<function>') print disassembled function */
    static int disassemble(BuiltinObject *this_object, ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime);
#endif // ES_DISASSEMBLER_SUPPORT

#ifdef ES_DEBUG_COMPACT_OBJECTS
    /** printClass('<object>') print object's class */
    static int printClass(BuiltinObject *this_object, ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime);

    /** printRootClass('<object>') print object's root class */
    static int printRootClass(BuiltinObject *this_object, ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime);
#endif // ES_DEBUG_COMPACT_OBJECTS

    virtual void GCTrace();
private:
    void AddFunction(Function *function, const uni_char *name, const char *format_list, unsigned flags = 0);
};

/* Object that behaves like an array. */
class HostArray : public EcmaScript_Object
{
public:
    HostArray(ES_Value *values, unsigned length)
        : values(values),
          length(length)
    {
    }

    virtual ~HostArray();

    virtual ES_GetState GetIndex(int property_index, ES_Value* value, ES_Runtime* origining_runtime);
    virtual ES_GetState GetName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime);
    virtual ES_PutState PutIndex(int property_index, ES_Value* value, ES_Runtime* origining_runtime);
    virtual ES_PutState PutName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime);
    virtual ES_DeleteStatus DeleteIndex(int property_index, ES_Runtime* origining_runtime);

	virtual ES_GetState FetchPropertiesL(ES_PropertyEnumerator *enumerator, ES_Runtime *origining_runtime);
	virtual ES_GetState GetIndexedPropertiesLength(unsigned &max_count, ES_Runtime *origining_runtime);

    virtual void GCTrace();

protected:
    ES_Value *values;
    unsigned length;
};

/* Host object with properties that allow testing of getter/setters + overriding via native object property puts. */
class HostObject : public EcmaScript_Object
{
public:
    virtual ES_GetState GetName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime);
    virtual ES_PutState PutName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime);

	virtual BOOL AllowOperationOnProperty(const uni_char *property_name, PropertyOperation op);
};

class HostArray_Constructor : public EcmaScript_Object
{
public:
    virtual int Call(ES_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime);
    virtual int Construct(ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime);
};

class Phase2_Constructor : public EcmaScript_Object
{
public:
    virtual int Call(ES_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime);
    virtual int Construct(ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime);
};

class HostObject_Constructor : public EcmaScript_Object
{
public:
    HostObject_Constructor()
        : prototype(NULL)
    {
    }

    virtual int Call(ES_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime);
    virtual void GCTrace();

private:
    ES_Object *prototype;
    virtual int Construct(ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime);
};

#endif // JSSHELL_OBJECT_H
