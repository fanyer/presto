/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) Opera Software ASA  1999-2004
 */

#ifndef ES_REGEXP_OBJECT_H
#define ES_REGEXP_OBJECT_H

#include "modules/ecmascript/carakan/src/object/es_object.h"
#include "modules/regexp/include/regexp_advanced_api.h"

enum
{
    REGEXP_FLAG_GLOBAL     = 1,  // g
    REGEXP_FLAG_IGNORECASE = 2,  // i
    REGEXP_FLAG_MULTILINE  = 4,  // m
#ifdef ES_NON_STANDARD_REGEXP_FEATURES
    REGEXP_FLAG_EXTENDED   = 8,  // x
    REGEXP_FLAG_NOSEARCH   = 16  // y
#endif // ES_NON_STANDARD_REGEXP_FEATURES
};

class RegExp;
class RegExpMatch;

class ES_RegExp_Information
{
public:
    RegExp *regexp;
    unsigned source;
    unsigned flags;

    static void Destroy(ES_RegExp_Information *re_info);
};

class ES_RegExp_Object : public ES_Function
{
public:
    static BOOL ParseFlags(ES_Context *context, RegExpFlags &flags, unsigned &flagbits, JString *source);

    static ES_RegExp_Object *Make(ES_Context *context, ES_Global_Object *global_object, JString *source, RegExpFlags &flags, unsigned flagbits, BOOL escape_source = FALSE);
    static ES_RegExp_Object *Make(ES_Context *context, ES_Global_Object *global_object, const ES_RegExp_Information &info, JString *source);

    static ES_RegExp_Object *MakePrototypeObject(ES_Context *context, ES_Global_Object *global_object, ES_Class *&instance);

    RegExp *GetValue() { return value; }
    JString *GetSource() { ES_Value_Internal value; GetCachedAtIndex(ES_PropertyIndex(SOURCE), value); return value.GetString(); }
    JString *GetFlags(ES_Context *context);
    unsigned GetFlagBits() { return flags; }
    void GetFlags(RegExpFlags &flags, unsigned &flagbits);

    RegExpMatch *Exec(ES_Execution_Context *context, JString *string, unsigned index);

    unsigned GetNumberOfCaptures();
#ifdef ES_NATIVE_SUPPORT
    RegExpNativeMatcher *GetNativeMatcher() { return native_matcher; }
    RegExpNativeMatcher *CreateNativeMatcher(ES_Execution_Context *context);
#endif // ES_NATIVE_SUPPORT
    RegExpMatch *GetMatchArray() { return (dynamic_match_array ? reinterpret_cast<RegExpMatch *>(dynamic_match_array->Unbox()) : match_array); }

    ES_RegExp_Constructor *GetRegExpConstructor(ES_Execution_Context *context);

    void Update(ES_Context *context, RegExp *new_value, JString *new_source, unsigned new_flags);
    void Reset() { PutCachedAtIndex(ES_PropertyIndex(LASTINDEX), ES_Value_Internal(0u)); }

    enum
    {
        LASTINDEX,
        SOURCE,
        GLOBAL,
        IGNORECASE,
        MULTILINE
    };

private:
    friend class ESMM;
    friend void DestroyObject(ES_Boxed *);

    static void Initialize(ES_RegExp_Object *regexp, ES_Class *klass, ES_Global_Object *global_object, const ES_RegExp_Information &info);
    static void Destroy(ES_RegExp_Object *regexp);

    void SetProperties(ES_Context *context, JString *source);

    ES_RegExp_Constructor *last_ctor;
    RegExp *value;
    unsigned flags;

#ifdef ES_NATIVE_SUPPORT
    RegExpNativeMatcher *native_matcher;
    unsigned calls;
#endif // ES_NATIVE_SUPPORT

    JString *previous_string_positive;
    unsigned previous_index_positive;
    JString *previous_string_negative[2];
    unsigned previous_index_negative[2];

    ES_Box *dynamic_match_array;
    RegExpMatch match_array[1];
};

class ES_RegExp_Constructor : public ES_Function
{
public:
    static ES_RegExp_Constructor *Make(ES_Context *context, ES_Global_Object *global_object);

    void BeforeMatch(ES_RegExp_Object *object_) { if (object_ == object) BackupMatches(); }
    void SetCaptures(ES_RegExp_Object *object_, JString *string_) { object = object_; string = string_; matches = object_->GetMatchArray(); use_backup = FALSE; }
    void GetCapture(ES_Context *context, ES_Value_Internal &value, unsigned index);
    void GetLastMatch(ES_Context *context, ES_Value_Internal &value);
    void GetLastParen(ES_Context *context, ES_Value_Internal &value);
    void GetLeftContext(ES_Context *context, ES_Value_Internal &value);
    void GetRightContext(ES_Context *context, ES_Value_Internal &value);

    void SetMultiline(BOOL value) { multiline = value; }
    BOOL GetMultiline() { return multiline; }

    enum
    {
        INPUT = ES_Function::PROTOTYPE + 1
    };

    enum IndexType
    {
        INDEX_INPUT        = UINT_MAX,
        INDEX_LASTMATCH    = UINT_MAX - 1,
        INDEX_LASTPAREN    = UINT_MAX - 2,
        INDEX_LEFTCONTEXT  = UINT_MAX - 3,
        INDEX_RIGHTCONTEXT = UINT_MAX - 4,
        INDEX_MULTILINE    = UINT_MAX - 5
    };

private:
    friend class ESMM;

    static void Initialize(ES_RegExp_Constructor *regexp, ES_Global_Object *global_object);

    void BackupMatches();

    ES_RegExp_Object *object;
    JString *string;
    RegExpMatch *matches, backup_matches[11];
    BOOL multiline;
    BOOL use_backup;
};

class ES_SuspendedCreateRegExp
    : public ES_SuspendedCall
{
public:
    ES_SuspendedCreateRegExp(const uni_char *pattern, unsigned length, RegExpFlags *flags)
        : pattern(pattern),
          length(length),
          flags(flags),
          status(OpStatus::OK),
          regexp(NULL)
    {
    }

    virtual void DoCall(ES_Execution_Context *context);

    void Execute(ES_Context *context);

    /* Input: */
    const uni_char *pattern;
    unsigned length;
    RegExpFlags *flags;

    /* Output: */
    OP_STATUS status;
    RegExp *regexp;
};

class ES_SuspendedUpdateRegExp
    : public ES_SuspendedCall
{
public:
    ES_SuspendedUpdateRegExp(ES_Execution_Context *context, ES_RegExp_Object *update_object, JString *source, RegExpFlags *flags, unsigned flagbits)
        : update_object(update_object),
          source(source),
          flags(flags),
          flagbits(flagbits),
          status(OpStatus::OK)
    {
        context->SuspendedCall(this);
    }

    virtual void DoCall(ES_Execution_Context *context);

    ES_RegExp_Object *update_object;
    JString *source;
    RegExpFlags *flags;
    unsigned flagbits;

    OP_STATUS status;
};

#endif // ES_REGEXP_OBJECT_H
