/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  1995 - 2011
 *
 * EcmaScript_Manager -- API for access to the ECMAScript engine
 * Lars T Hansen
 */

#ifndef ECMASCRIPT_MANAGER_H
#define ECMASCRIPT_MANAGER_H

#ifndef _STANDALONE
#  include "modules/hardcore/mh/messobj.h"
#endif

#include "modules/util/simset.h"

class TempBuffer;
class ByteBuffer;
class ES_Runtime;
class ES_Heap;
class ES_EngineDebugBackendGetPropertyListener;
class ES_DebugObjectChain;
class ES_Thread;

enum GetNativeStatus
{
    GETNATIVE_SUCCESS = 1,
    GETNATIVE_NOT_OWN_NATIVE_PROPERTY,
    GETNATIVE_NEEDS_CALLSTACK,
    GETNATIVE_SCRIPT_GETTER
};

#ifdef ES_HEAP_DEBUGGER
#include "modules/util/tempbuf.h"
#endif

class EcmaScript_Manager
#ifndef _STANDALONE
    : public MessageObject
#endif
{
friend class ES_Runtime;

public:
    /* Helper class for calling methods that need either a runtime or a context. */
    struct RuntimeOrContext
    {
        RuntimeOrContext(ES_Runtime *runtime);
        RuntimeOrContext(ES_Context *context);
        ES_Runtime *runtime;
        ES_Context *context;
    };

private:
    BOOL pending_collections; // flag: TRUE if there are delayed unconditional collections pending
    BOOL maintenance_gc_running; // flag: TRUE if the maintenance garbage collector mechanism is running
    Head active_heaps, inactive_heaps, destroy_heaps;

    ES_HostObjectCloneHandler *first_host_object_clone_handler;

protected:
    EcmaScript_Manager();

public:
    static EcmaScript_Manager* MakeL();
        /**< Create an EcmaScript_Manager (usually a subclass thereof).
             Resource consumption: May initialize the underlying engine, but
             may also perform initialization lazily.

             There should never be more than one EcmaScript_Manager live in
             the system at any one time.

             @return An ECMAScript manager, never NULL */

    virtual ~EcmaScript_Manager();
        /**< Delete the manager and free all its resources. */

#ifndef _STANDALONE
    virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2) = 0;
        /**< Handle a callback */
#endif

    BOOL IsScriptSupported(const uni_char* type, const uni_char* language);
        /**< Test if a combination of "type" and "language" attribute is supported
             by this scripting engine.

             @param type (in) The value of the "type" attribute of the script element,
                         ignored if NULL or empty string
             @param language (in) The value of the "language" attribute of the script
                             element, ignored if NULL or empty string.
             @return TRUE if the script type is fully supported, FALSE if it is not.

             If both type and language are null or empty string, then the implied
             script type is JavaScript 1.3. */

    long GetCurrHeapSize();
        /**< Get current heap size from engine */

    BOOL Protect(ES_Object *obj);
        /**< Protect the object from being garbage collected even if there are no
             references to it from within the collected heap.  If called several
             times for the same object, the effect is to require a matching number
             of Unprotect calls before the object becomes collectible -- effectively,
             a reference counting scheme.
             @param  obj  The object to be protected
             @return      FALSE on OOM, otherwise TRUE */

    void Unprotect(ES_Object *obj);
        /**< Allow the object to be garbage collected.
             @param  obj  The object to be unprotected
             */

    void GarbageCollect(BOOL unconditional=TRUE, unsigned int timeout=0);
        /**< Run the garbage collector.
             @param  unconditional  (in) If TRUE, then the collection is always performed.
                                    If FALSE, then the collection is performed if the
                                    heuristics of the collector indicate that it is
                                    appropriate to do so.
             @param  timeout (in) If unconditional is TRUE, then this parameter specifies
                             how long to wait (in milliseconds) before performing the
                             collection.  The idea is that unconditional GC requests come
                             in lumps, yet we don't want to collect more than one, so we
                             wait to see if more arrive.  If another unconditional request
                             arrives later, then its timeout setting overrides the one of
                             any pending collection.  The value ~0 is reserved for internal
                             use. */

    void ScheduleGarbageCollect();
        /**< Post a message that will eventually trigger garbage collection. */

    void MaintenanceGarbageCollect();

    void PurgeDestroyedHeaps();
        /**< Purge all pending destroyed heaps. */

    void HandleOutOfMemory();
        /**< Called when Opera has run out of memory. As a response the
             ecmascript code should try to reduce memory usage by clearing
             caches and possibly tune the behaviour.
             */

#ifdef VBSCRIPT_SUPPORT

    OP_STATUS TranslateVBScript(ES_ProgramText *prog, int elements, TempBuffer* out);
        /**< Translate the program text to ECMAScript. The translation is partial:
             translation may fail (by returning an error) and execution may fail (by
             throwing any exception). If either failure occurs this MUST be interpreted
             as a signal to execute any following NOSCRIPT block, exactly as if the
             script language was not recognized to begin with.
             @param prog        The text of the VBScript
             @param elements    The number of text elements in prog
             @param out         A tempbuffer that will be point to the resulting script
             @return            OpStatus::ERR if a translation failure occured
                                OpStatus::ERR_NO_MEMORY if OOM
                                Otherwise OpStatus::OK and valid contents in out. */

#endif // VBSCRIPT_SUPPORT

    BOOL IsDebugging(ES_Runtime *runtime);
        /**< Returns TRUE if debugger is attached to the specified runtime.
             @param runtime The runtime which should be checked. */

#ifdef ECMASCRIPT_DEBUGGER
private:
    ES_ObjectReference reformat_function;
        /**< A compiled function used to determine whether code should be
             reformatted. */
    ES_Runtime *reformat_runtime;
        /**< The runtime in which reformatting function is evaluated. */
    BOOL want_reformat;
        /**< Flag indicating that debugger wishes that source of the scripts
             gets reformatted. */

public:
    /* General documentation about debugging may be found ahead of the debugger
       support classes in ecmascript_types.h.

       Note that many of the values returned by this interface may be invalidated by
       the engine if the engine is run, since that may cause the garbage collector
       to be run and values may be moved or deleted. */

    void SetDebugListener(ES_DebugListener *listener);
        /**< Set global debug listener. Called when a debugger is started.
             @param listener (in) The listener to set, or NULL */

    ES_DebugListener *GetDebugListener();
        /**< Returns the debug listener. */

    void IgnoreError(ES_Context *context, ES_Value value);
        /**< Ignore a run-time error (during an Event::ERROR event)
             If the computation was supposed to produce a value, then the supplied
             value is produced before computation continues.
             @param context (in) the context of the error */

    void GetScope(ES_Context *context, unsigned frame_index, unsigned *length, ES_Object ***scope);
        /**< Retrieve scope chain of function currently executing in context.
             May be called at any time, if the running code has been compiled for
             debugging.
             @param context (in) the context
             @param frame_index (in) the index of the stack frame for which the scope is sought.
                                The index is 0 for the bottommost frame, the topmost frame has
                                index equal to stacklength-1, where stacklength is the value
                                returned through the length parameter of GetStack(). The special
                                value ES_DebugControl::OUTERMOST_FRAME denotes the outermost
                                frame (stacklength-1), allowing its scope to be gotten without
                                calling GetStack first.
             @param length (out) location to receive the number of objects in the scope chain
             @param scope (out) location to receive pointer to static storage containing
                          pointers to variable objects */

    void GetStack(ES_Context *context, unsigned maxframes, unsigned *length, ES_DebugStackElement **stack);
        /**< Retrieve current call stack of context. May be called at any time, if
             the running code has been compiled for debugging.
             @param context (in) the context
             @param maxframes (in) the maximum number of frames to retrieve; pass the value
                              ES_DebugControl::ALL_FRAMES to get all frames
             @param length (out) location to receive number of stack elements in call stack
             @param stack (out) location to receive pointer to static array of stack elements */

    void GetExceptionValue(ES_Context *context, ES_Value *exception_value);
        /**< Retrieve current exception object (during Event::ERROR or Event::EXCEPTION events)
             @param context (in) the context of the error
             @param exception_value (in/out) object in which to receive the the error or exception value */

    void GetReturnValue(ES_Context *context, ES_Value *return_value);
        /**< Retrieve current return value (during Event::CALLCOMPLETED event)
             @param context (in) the context of the return
             @param error (in/out) object in which to receive the returned value */

    ES_Object *GetCallee(ES_Context *context);
        /**< @return the function about to be called (during Event::CALLSTARTING and Event::ENTERFN)
                     or that was just called (during Event::CALLCOMPLETED and Event::LEAVEFN)
             @param context (in) the context in which the function call takes/took place */

    void GetCallDetails(ES_Context *context, ES_Object **this_object, ES_Object **callee, int *argc, ES_Value **argv);
        /**< Retrieve details of the function call in question during the
             Event::CALLSTARTING, Event::CALLCOMPLETED, Event::ENTERFN and Event::LEAVEFN.
             @param context (in) the context in which the function call takes/took place
             @param this_object (out) the current this object
             @param callee (out) the current calling object
             @param argc (out) number of call argument values
             @param argv (out) functioncall argument values */

    void GetCallDetails(ES_Context *context, ES_Value *this_value, ES_Object **callee, int *argc, ES_Value **argv);
        /**< Retrieve details of the function call in question during the
             Event::CALLSTARTING, Event::CALLCOMPLETED, Event::ENTERFN and Event::LEAVEFN.
             @param context (in) the context in which the function call takes/took place
             @param this_value (out) the current ThisBinding
             @param callee (out) the current calling object
             @param argc (out) number of call argument values
             @param argv (out) functioncall argument values */

    static OP_STATUS GetFunctionPosition(ES_Object *function_object, unsigned &script_guid, unsigned &line_no);
        /**< Retrieves script ID and line number for a native function object.
             @param function_object (in) Object containing the native function to inspect.
             @param script_guid (out) The ID of the script where the function was defined.
             @param line_no (out) The line number where the function was defined.
             @return OpStatus::OK if the position was retrieved,
                     or OpStatus::ERR if the object is not a native function. */

    void GetObjectProperties(const RuntimeOrContext &runtime_or_context, ES_Object *object, ES_PropertyFilter *filter, BOOL skip_non_enum, uni_char ***names, ES_Value **values, GetNativeStatus **getstatus);
    /**< Retrieve all non-private non-prototype properties of the object, including
         foreign properties and indexed properties.
         @param runtime_or_context (in) an ES_Runtime or ES_Context with which to access foreign objects
                       If an ES_Runtime is supplied, getters will not be called and their value
                       will default to undefined. With a context getters will be called with
                       their results used as their value.
         @param object (in) the object whose properties we want; Identity() is
                       applied to this object before it is processed.
         @param filter (in) a filter to apply when retrieving the properties. The filter
                       can decide which properties are in/out based the property name and
                       value. The filter can be set to NULL if no filtering is required.
         @param skip_non_enum (in) TRUE if non-enumerable properties should be excluded.
         @param names (out) location in which to store a pointer to an array of
                      the names of the object properties. The array and the names
                      are heap allocated by the engine; the caller must free them.
                      >> NOTE:  The array itself is allocated by new[], while
                      >>        the strings are allocated by op_malloc().
                      The last entry of the array is always NULL.
         @param values (out) location in which to store a pointer to an array of
                       the values of the object properties. The array is heap
                       allocated by the engine; the caller must free it.
         @param getstatus (out) read property status for the corresponding names and values.

         *names and *values may be NULL if OOM occurred, but OOM is not otherwise
         signalled. */

    void GetObjectAttributes(ES_Runtime *runtime, ES_Object *object, ES_DebugObjectElement *attr);
        /**< Retrieve the attributes (not the properties) of the object.
             @param runtime (in) the runtime
             @param object (in) the object whose attributes we want; Identity() is
                           applied to this object before it is processed
             @param attr (in/out) Object in which to store the object attributes */

    void ObjectTrackedByDebugger(ES_Object *object, BOOL tracked);
        /**< Set whether object is tracked by the debugger. */

    void SetWantReformatScript(BOOL enable) { want_reformat = enable; }
        /**< Set a flag indicating that debugger wishes that the scripts are to
             be reformatted.
             @param enable True to enable reformatting flag, false to disable. */

    BOOL GetWantReformatScript(ES_Runtime *runtime, const uni_char *program_text = NULL, unsigned program_text_length = 0);
        /**< Get the current state of code reformatting flag and optionally run
             a function that will determine whether that particular script
             source should be reformatted.
             @param runtime (in) The runtime in whose context reformatting takes
                                 place.
             @param program_text (in) An optional source of the script that is
                                      to be reformatted. If reformat function is
                                      set (@see SetReformatConditionFunction),
                                      the source will be evaluated in the context
                                      of that function to determine whether it
                                      should be reformatted.
             @param program_text_length (in) The length of the program_text string.
                                             Must be set if program_text is set.
             @return True if debugger is attached to the specified runtime
                     and it was determined that the source code should be
                     reformatted. */

    OP_STATUS SetReformatConditionFunction(const uni_char *source);
        /**< Compile the function used to determine whether script should be
             reformatted.
             @param source (in) The source of the function to be compiled. Can
                                be set to NULL pointer or empty string to disable
                                the function.
             @return OpStatus::OK if function was compiled successfully or
                     when source argument is a NULL pointer or an empty string.
                     OpStatus::ERR, or OpStatus::ERR_NO_MEMORY if compilation
                     failed. */
#endif // ECMASCRIPT_DEBUGGER

    enum { LONGEST_PROPERTY_NAME = 32 };

    class PropertyNameTranslator
    {
    public:
        virtual int TranslatePropertyName(const uni_char *name) = 0;
        /**< Translate property name into integer counterpart. The host is free
             to assign numbers except that INT_MIN must not be used. If called
             with name==NULL, the value corresponding to an unrecognized
             property name should be returned. */

        virtual ~PropertyNameTranslator() {}
        /**< Object not destroyed through this interface. */
    };

    void SetPropertyNameTranslator(PropertyNameTranslator *translator) { property_name_translator = translator; }
    /**< Set property name translator. Must be set during initialization or not
         at all, since otherwise properties might be created before it is set,
         and remain untranslated until garbage collected. Can be unset by
         setting to NULL. The object is owned by EcmaScript_Manager and will be
         deleted by it during shutdown (if not unset before that.) */

    void RegisterHostObjectCloneHandler(ES_HostObjectCloneHandler *handler);
    /**< Register a "clone handler" that will be called upon to clone host
         objects when cloning values via the cloning functions in ES_Runtime.
         The handler is owned by the EcmaScript_Manager object and is deleted
         when Opera is shut down, unless unregistered first.

         @param handler Handler to register. */

    void UnregisterHostObjectCloneHandler(ES_HostObjectCloneHandler *handler);
    /**< Unregister a previously registered "clone handler". */

#ifdef ES_HEAP_DEBUGGER

    unsigned GetHeapId() { return ++heap_id_counter; }

    void GetHeapLists(Head *&active, Head *&inactive, Head *&destroy) { active = &active_heaps; inactive = &inactive_heaps; destroy = &destroy_heaps; }

    ES_Heap *GetHeapById(unsigned id);

    void ForceGCOnHeap(ES_Heap *heap);

    TempBuffer *GetHeapDebuggerBuffer() { return &heap_debugger_buffer; }

    ES_Runtime *GetNextRuntimePerHeap(ES_Runtime *runtime);

#endif // ES_HEAP_DEBUGGER

    ES_HostObjectCloneHandler *GetFirstHostObjectCloneHandler() { return first_host_object_clone_handler; }
    ES_HostObjectCloneHandler *GetNextHostObjectCloneHandler(ES_HostObjectCloneHandler *from) { return from->next; }

protected:
    friend class ES_Heap;
    friend class JString;

    void AddHeap(ES_Heap* heap);
    void RemoveHeap(ES_Heap* heap);
    void MoveHeapToDestroyList(ES_Heap* heap);
    void MoveHeapToActiveList(ES_Heap* heap);
    void MoveHeapToInactiveList(ES_Heap* heap);

    void StartMaintenanceGC();
    void StopMaintenanceGC();

    PropertyNameTranslator *property_name_translator;

#ifdef ES_HEAP_DEBUGGER
    unsigned heap_id_counter;
    TempBuffer heap_debugger_buffer;
#endif // ES_HEAP_DEBUGGER
};

#endif // ECMASCRIPT_MANAGER_H
