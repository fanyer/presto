/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*- */

#ifndef ES_ARRAY_BUILTINS_H
#define ES_ARRAY_BUILTINS_H

class ES_Array_Property_Iterator_Elm : public Link
{
public:
    ES_Array_Property_Iterator_Elm(ES_Context *context, ES_Object *object)
        : iterator(context, object, object->GetIndexedPropertiesRef()),
          self(object)
    {
    }

    ES_Array_Property_Iterator_Elm* Pred() const { return (ES_Array_Property_Iterator_Elm*) Link::Pred(); }
    ES_Array_Property_Iterator_Elm* Suc() const { return (ES_Array_Property_Iterator_Elm*) Link::Suc(); }

    void Reset() { iterator.Reset(); }

    BOOL Previous(unsigned &index);
    BOOL Next(unsigned &index);
    BOOL LowerBound(unsigned &index, unsigned limit);
    BOOL UpperBound(unsigned &index, unsigned limit);

    BOOL GetValue(ES_Value_Internal &value);
    unsigned GetCurrentIndex();
    void FlushCache();

private:
    ES_Indexed_Property_Iterator iterator;
    ES_Object *self;
};

class ES_Array_Property_Iterator : public Head
{
public:
    ES_Array_Property_Iterator(ES_Execution_Context *context, ES_Object *this_object, unsigned length);
    ~ES_Array_Property_Iterator();

    ES_Array_Property_Iterator_Elm *First() { return (ES_Array_Property_Iterator_Elm*)Head::First(); }
    ES_Array_Property_Iterator_Elm *Last() { return (ES_Array_Property_Iterator_Elm*)Head::Last(); }

    inline BOOL GetValue(ES_Value_Internal &value) { return current->GetValue(value); }
    inline void FlushCache() { current->FlushCache(); }

    void Reset();

    BOOL Previous(unsigned &index);
    BOOL Next(unsigned &index);

    BOOL LowerBound(unsigned &index, unsigned limit);
    BOOL UpperBound(unsigned &index, unsigned limit);

private:
    ES_Execution_Context *context;
    ES_Array_Property_Iterator_Elm *current, first;
    unsigned current_index;
    unsigned length;
    BOOL started;
};

class ES_ArrayBuiltins
{
public:
    static BOOL ES_CALLING_CONVENTION constructor(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    /**< 'Array()' and ''new Array() */

    static BOOL ES_CALLING_CONVENTION toString(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION toLocaleString(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION join(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);

    static BOOL ES_CALLING_CONVENTION forEach(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION every(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION some(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION map(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION filter(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION reduce(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION reduceRight(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION push(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION pop(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION shift(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION unshift(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);

    static BOOL ES_CALLING_CONVENTION indexOf(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION lastIndexOf(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);

    static BOOL ES_CALLING_CONVENTION slice(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION splice(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);

    static BOOL ES_CALLING_CONVENTION concat(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION reverse(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION sort(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);

    static BOOL ES_CALLING_CONVENTION isArray(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);

    static void PopulateConstructor(ES_Context *context, ES_Global_Object *global_object, ES_Object *constructor);
    static void PopulatePrototype(ES_Context *context, ES_Global_Object *global_object, ES_Object *prototype);
    static void PopulatePrototypeClass(ES_Context *context, ES_Class_Singleton *prototype_klass);

    enum
    {
        ES_ArrayBuiltins_length = 0,
        ES_ArrayBuiltins_constructor,
        ES_ArrayBuiltins_toString,
        ES_ArrayBuiltins_toLocaleString,
        ES_ArrayBuiltins_join,
        ES_ArrayBuiltins_forEach,
        ES_ArrayBuiltins_every,
        ES_ArrayBuiltins_some,
        ES_ArrayBuiltins_map,
        ES_ArrayBuiltins_filter,
        ES_ArrayBuiltins_reduce,
        ES_ArrayBuiltins_reduceRight,
        ES_ArrayBuiltins_push,
        ES_ArrayBuiltins_pop,
        ES_ArrayBuiltins_shift,
        ES_ArrayBuiltins_unshift,
        ES_ArrayBuiltins_indexOf,
        ES_ArrayBuiltins_lastIndexOf,
        ES_ArrayBuiltins_slice,
        ES_ArrayBuiltins_splice,
        ES_ArrayBuiltins_concat,
        ES_ArrayBuiltins_reverse,
        ES_ArrayBuiltins_sort,

        ES_ArrayBuiltinsCount,
        ES_ArrayBuiltins_LAST_PROPERTY = ES_ArrayBuiltins_sort
    };
};

#endif // ES_ARRAY_BUILTINS_H
