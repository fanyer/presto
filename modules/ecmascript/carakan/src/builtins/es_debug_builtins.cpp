/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*- */

#include "core/pch.h"

#include "modules/ecmascript/carakan/src/es_pch.h"
#include "modules/ecmascript/carakan/src/builtins/es_builtins.h"

#ifdef ES_DEBUG_BUILTINS
#include "modules/ecmascript/carakan/src/builtins/es_debug_builtins.h"

#ifdef ES_HEAP_DEBUGGER

#ifndef _STANDALONE
#include "modules/dom/domenvironment.h"
#include "modules/dom/domtypes.h"
#include "modules/dom/domutils.h"
#endif // _STANDALONE

#include "modules/ecmascript/carakan/src/es_program_cache.h"

const char *g_ecmaClassName[] = {
    "free",
    "JStringStorage",
    "ES_Box",
    "JString",
    "JStringSegmented",
    "ES_Identifier_Array",
    "ES_IdentifierCell_Array",
    "ES_Property_Table",
    "ES_Property_Value_Table",
    "ES_Compact_Indexed_Properties",
    "ES_Sparse_Indexed_Properties",
    "ES_Byte_Array_Indexed",
    "ES_Type_Array_Indexed",
    "ES_Identifier_Hash_Table",
    "ES_Identifier_List",
    "ES_Identifier_Boxed_Hash_Table",
    "ES_Boxed_Array",
    "ES_Boxed_List",
    "ES_Class_Node",
    "ES_Class_Compact_Node",
    "ES_Class_Compact_Node_Frozen",
    "ES_Class_Singleton",
    "ES_Class_Class_Hash",
    "ES_Class_Extra",
    "ES_Class_Data",
    "ES_ProgramCode",
    "ES_FunctionCode",
    "ES_Properties",
    "ES_Special_Aliased",
    "ES_Special_Mutable_Access",
    "ES_Special_Global_Variable",
    "ES_Special_Builtin_Function_Name",
    "ES_Special_Builtin_Function_Length",
    "ES_Special_Builtin_Function_Prototype",
    "ES_Special_Function_Arguments",
    "ES_Special_Function_Caller",
    "ES_Special_RegExp_Capture",
    "ES_Special_Error_StackTrace",
    "ES_Object",
    "ES_Object_Number",
    "ES_Object_String",
    "ES_Object_Date",
    "ES_Object_Boolean",
    "ES_Object_Array",
    "ES_Object_Function",
    "ES_Object_RegExp",
    "ES_Object_RegExp_CTOR",
    "ES_Object_Error",
    "ES_Object_Arguments",
    "ES_Object_Variables",
    "ES_Object_TypedArray",
    "ES_Object_ArrayBuffer"
};

/* static */ BOOL
ES_DebugBuiltins::getHeapInformation(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
	Head *active, *inactive, *destroy;

	g_ecmaManager->GetHeapLists(active, inactive, destroy);

	TempBuffer *buffer = g_ecmaManager->GetHeapDebuggerBuffer();
	buffer->Clear();

	buffer->Append("{ \"heaps\": { ");

	ES_Heap *heads[3] = { static_cast<ES_Heap *>(active->First()), static_cast<ES_Heap *>(inactive->First()), static_cast<ES_Heap *>(destroy->First()) };

	for (unsigned head = 0; head < 3; ++head)
		for (ES_Heap *heap = heads[head]; heap; heap = static_cast<ES_Heap *>(heap->Suc()))
		{
			buffer->AppendFormat(UNI_L("\"%u\": { \"bytesLive\": %u, \"bytesLivePeak\": %u, \"bytesLimit\": %u, \"runtimes\": ["), heap->Id(), heap->GetBytesLive(), heap->GetBytesLivePeak(), heap->GetBytesLimit());

			ES_Runtime *runtime = heap->GetFirstRuntime();

			while (runtime)
			{
#ifndef _STANDALONE
				ES_Object *global_object = runtime->GetGlobalObject();
				if (global_object->IsHostObject() && ES_Runtime::GetHostObject(global_object)->IsA(DOM_TYPE_WINDOW))
				{
					OpString url;

					DOM_Utils::GetOriginURL(DOM_Utils::GetDOM_Runtime(runtime)).GetAttribute(URL::KUniName, url);

					for (unsigned index = 0; index < static_cast<unsigned>(url.Length()); ++index)
						if (url.CStr()[index] == '"')
							url.Insert(index++, "\\");

					buffer->AppendFormat(UNI_L("\"%s\""), url.CStr());
				}
				else
#endif // _STANDALONE
					buffer->Append("\"<unidentified runtime>\"");

				runtime = g_ecmaManager->GetNextRuntimePerHeap(runtime);

				if (runtime)
					buffer->Append(", ");
			}

			buffer->Append("] }, ");
		}

	buffer->AppendFormat(UNI_L("\"count\": %u }, \"allocators\": ["), active->Cardinal() + inactive->Cardinal() + destroy->Cardinal());

	for (ES_PageAllocator *allocator = static_cast<ES_PageAllocator *>(g_ecmaPageAllocatorList->First()); allocator; allocator = static_cast<ES_PageAllocator *>(allocator->Suc()))
	{
		buffer->AppendFormat(UNI_L("{ \"chunks\": %u, \"chunkSize\": %u, \"pages\": %u, \"pageSize\": %u, \"heaps\": ["), allocator->CountChunks(), allocator->ChunkSize(), allocator->CountPages(), allocator->PageSize());

		for (ES_HeapHandle *heaph = allocator->GetFirstHeapHandle(); heaph; heaph = static_cast<ES_HeapHandle *>(heaph->Suc()))
		{
			buffer->AppendUnsignedLong(heaph->heap->Id());

			if (heaph->Suc())
				buffer->Append(", ");
		}

		buffer->Append("] }");

		if (allocator->Suc())
			buffer->Append(", ");
	}

	buffer->Append("], \"cachedPrograms\": [");

	for (Link *link = RT_DATA.program_cache->GetCachedPrograms()->First(); link; link = link->Suc())
	{
		ES_ProgramCodeStatic *program = static_cast<ES_ProgramCodeStatic *>(link);

		buffer->AppendFormat(UNI_L("{ \"url\": \"\", \"length\": %u }"), program->source.GetSource()->length);

		if (link->Suc())
			buffer->Append(", ");
	}

	buffer->Append("] }");

    return_value->SetString(JString::Make(context, buffer->GetStorage(), buffer->Length()));

    return TRUE;
}

/* static */ BOOL
ES_DebugBuiltins::getObjectDemographics(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    return_value->SetNull();

	if (argc == 1)
	{
        if (!argv[0].ToNumber(context) || !argv[0].IsUInt32())
            return FALSE;

		if (ES_Heap *heap = g_ecmaManager->GetHeapById(argv[0].GetNumAsUInt32()))
		{
			TempBuffer *buffer = g_ecmaManager->GetHeapDebuggerBuffer();

			buffer->Clear();
			buffer->Append("{ ");

			unsigned *live_objects = heap->live_objects;

			for (unsigned index = 0; index < GCTAG_UNINITIALIZED; ++index)
			{
				if (index != 0)
					buffer->Append(", ");

				buffer->Append("\"");
				buffer->Append(g_ecmaClassName[index]);
				buffer->Append("\": ");
				buffer->AppendUnsignedLong(live_objects[index]);
			}

			buffer->Append(" }");

            return_value->SetString(JString::Make(context, buffer->GetStorage(), buffer->Length()));
		}
	}

	return TRUE;
}

#endif // ES_HEAP_DEBUGGER

#ifndef ES_HEAP_DEBUGGER
/* static */ BOOL
do_nothing(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    return TRUE;
}
#endif // ES_HEAP_DEBUGGER

/* static */ void
ES_DebugBuiltins::PopulateDebug(ES_Context *context, ES_Global_Object *global_object, ES_Object *prototype)
{
    #define MAKE_DEBUG_BUILTIN_WITH_NAME(length, name) ES_Function::Make(context, global_object, length, name, 0, TRUE)
    ES_Value_Internal undefined;

    ASSERT_CLASS_SIZE(ES_DebugBuiltins);

#ifdef ES_HEAP_DEBUGGER
    APPEND_PROPERTY(ES_DebugBuiltins, getHeapInformation,    MAKE_DEBUG_BUILTIN_WITH_NAME(0, getHeapInformation));
    APPEND_PROPERTY(ES_DebugBuiltins, getObjectDemographics, MAKE_DEBUG_BUILTIN_WITH_NAME(1, getObjectDemographics));
#else
    APPEND_PROPERTY(ES_DebugBuiltins, getHeapInformation,    MAKE_DEBUG_BUILTIN_WITH_NAME(0, do_nothing));
    APPEND_PROPERTY(ES_DebugBuiltins, getObjectDemographics, MAKE_DEBUG_BUILTIN_WITH_NAME(1, do_nothing));
#endif // ES_HEAP_DEBUGGER

    ASSERT_OBJECT_COUNT(ES_DebugBuiltins);
    #undef MAKE_DEBUG_BUILTIN_WITH_NAME
}

/* static */ void
ES_DebugBuiltins::PopulateDebugClass(ES_Context *context, ES_Class_Singleton *prototype_class)
{
    OP_ASSERT(prototype_class->GetPropertyTable()->Capacity() >= ES_DebugBuiltinsCount);

    ES_Layout_Info layout;

    #define DECLARE_DEBUG_PROPERTY(class, property, attributes, type) prototype_class->AddL(context, JString::Make(context, #property), ES_Property_Info(attributes | CP), type, FALSE)

    DECLARE_DEBUG_PROPERTY(ES_DebugBuiltins, getHeapInformation,    DE | DD | RO, ES_STORAGE_OBJECT);
    DECLARE_DEBUG_PROPERTY(ES_DebugBuiltins, getObjectDemographics, DE | DD | RO, ES_STORAGE_OBJECT);

    #undef DECLARE_DEBUG_PROPERTY
}

#endif // ES_DEBUG_BUILTINS
