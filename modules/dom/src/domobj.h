/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2006 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef DOM_OBJECT_H
#define DOM_OBJECT_H

#include "modules/dom/domcapabilities.h"
#include "modules/dom/domtypes.h"
#include "modules/dom/src/domdefines.h"
#include "modules/dom/src/domruntime.h"
#include "modules/dom/src/domprivatenames.h"
#include "modules/dom/src/opatom.h"
#include "modules/dom/src/dompropertystorage.h"

#include "modules/ecmascript/ecmascript.h"

#include "modules/security_manager/include/security_manager.h"

class HTML_Element;
class DOM_EnvironmentImpl;
class DOM_BuiltInConstructor;
class FramesDocument;
class HTML_Document;
class HLDocProfile;
class ES_Thread;
class DOM_EventTarget;
class DOM_EventTargetOwner;
class DOM_EventListener;
class ServerName;

class LogicalDocument;

class DOM_Object : public EcmaScript_Object
{
public:
	static BOOL OriginCheck(URLType type1, int port1, const uni_char *domain1, URLType type2, int port2, const uni_char *domain2);
	/**< Perform a basic security check and return TRUE if the check passed,
	     FALSE if not.  The check is based on the protocol, fqdn, and port of the
	     document that performs an access (script) and of the document that
	     is accessed (target).  The check is passed if these match.
	     The domains may be NULL, in which case they are forced to empty string. */

	static BOOL OriginCheck(const URL &url1, const URL &url2);
	/**< Like above, but take the values to be checked from the two URLs. */

	BOOL OriginCheck(const URL& target_url, ES_Runtime* origining_runtime);
	/**< Like above, but take the values to be checked from the URL and the runtime. */

	BOOL OriginCheck(ES_Runtime* target_runtime, ES_Runtime* origining_runtime);
	/**< Like above, but take the values to be checked from the domain information
	     in the two runtimes.  (The domain information can be changed through the
	     JavaScript 'document.domain' mechanism.) */

	BOOL OriginCheck(ES_Runtime* origining_runtime);
	/**< Like above, but uses this->GetRuntime() as target_runtime. */

	BOOL3 OriginLoadCheck(ES_Runtime *target_runtime, ES_Runtime* origining_runtime);
	/**< Like above, but return YES if the check passed, MAYBE if the check did not
	     pass but the target's is security status is SECURITY_STATE_NONE or
	     SECURITY_STATE_UNKNOWN and otherwise NO. */

	virtual BOOL SecurityCheck(ES_Runtime *origining_runtime);

	virtual BOOL SecurityCheck(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	/**< Called after GetName(property_atom, ..., origining_runtime) has returned
	     GET_SUCCESS and before PutName(property)name, ..., origining_runtime) if
	     the property is supported.  If this function returns FALSE, GetName
	     returns GET_SECURITY_VIOLATION instead of GET_SUCCESS and PutName returns
	     PUT_SECURITY_VIOLATION.

	     @param property_name The property's name.
	     @param value NULL if the property is being read, otherwise the value
	                  being written to it.
	     @param origining_runtime The script's origining runtime.
	     @return TRUE if the access is allowed, FALSE if it is not. */

	virtual ES_GetState GetName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_GetState GetNameRestart(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime, ES_Object* restart_object);
	virtual ES_GetState GetNameRestart(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime, ES_Object* restart_object);

	virtual ES_PutState PutName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutNameRestart(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime, ES_Object* restart_object);
	virtual ES_PutState PutNameRestart(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime, ES_Object* restart_object);

	virtual ES_GetState FetchPropertiesL(ES_PropertyEnumerator *enumerator, ES_Runtime *origining_runtime);
	/**< Generate the set of enumerable properties that this object
	     supports. This includes both named and indexed properties.

	     The properties are communicated back to the engine through
	     the ES_PropertyEnumerator argument.

	     Typically, collection objects and object with dynamic property
	     sets will override FetchPropertiesL(). It is currently also
	     required for objects that support event handler names (e.g.,
	     "onprogress" for XMLHttpRequest) to override this method.

	     @param enumerator The object for registering properties.
	     @return One of the following:

	        GET_SUCCESS The operation completed successfully.
	        GET_SUSPEND The operation had to suspend and must be restarted.
	        GET_NO_MEMORY OOM condition encountered.

	        No other values of ES_GetState must be returned by an implementation
	        of this method.

	        As the ES_PropertyEnumerator methods will leave upon encountering
	        OOM, an implementation must expect this and handle the condition
	        correctly. An implementation is currently allowed to report
	        OOM either by leaving or a GET_NO_MEMORY return	value. */

	/* Note: the GetIndexedPropertiesLength() method is also part of the
	   engine's prototcol for handling enumeration of host objects.
	   The DOM_Object base class has no indexed properties and hence
	   does not override this method, but objects that do, must
	   provide an implementation for this method.

	   See EcmaScript_Object::GetIndexedPropertiesLength() for more. */

	/* Convenience functions used mostly by prototype classes' Initialise functions. */
	void AddFunctionL(DOM_Object *function, const char *name, const char *arguments = NULL, DOM_PropertyStorage **insecure_storage = NULL);
	void AddFunctionL(DOM_Object *function, const uni_char *name, const char *arguments = NULL, DOM_PropertyStorage **insecure_storage = NULL);
	void AddFunctionL(DOM_FunctionImpl *functionimpl, const char *name, const char *arguments = NULL, DOM_PropertyStorage **insecure_storage = NULL);
#ifdef DOM_ACCESS_SECURITY_RULES
	void AddFunctionL(DOM_FunctionImpl *functionimpl, const char *name, const char *arguments, DOM_PropertyStorage **insecure_storage, const DOM_FunctionSecurityRuleDesc *security_rule);
#endif // DOM_ACCESS_SECURITY_RULES
	void AddFunctionL(DOM_FunctionWithDataImpl *functionimpl, int data, const char *name, const char *arguments = NULL, DOM_PropertyStorage **insecure_storage = NULL);

	static void AddFunctionL(ES_Object *target, DOM_Runtime *runtime, DOM_Object *function, const uni_char *name, const char *arguments = NULL, DOM_PropertyStorage **insecure_storage = NULL);
	static void AddFunctionL(ES_Object *target, DOM_Runtime *runtime, DOM_FunctionImpl *functionimpl, const char *name, const char *arguments = NULL, DOM_PropertyStorage **insecure_storage = NULL);
	static void AddFunctionL(ES_Object *target, DOM_Runtime *runtime, DOM_FunctionWithDataImpl *functionimpl, int data, const char *name, const char *arguments = NULL, DOM_PropertyStorage **insecure_storage = NULL);
#ifdef DOM_ACCESS_SECURITY_RULES
	static void AddFunctionL(ES_Object *target, DOM_Runtime *runtime, DOM_FunctionImpl *functionimpl, const char *name, const char *arguments, DOM_PropertyStorage **insecure_storage, const DOM_FunctionSecurityRuleDesc *security_rule);
#endif // DOM_ACCESS_SECURITY_RULES

	BOOL GetInternalValue(const uni_char *name, ES_Value *value);
	OP_STATUS SetInternalValue(const uni_char *name, const ES_Value &value);

	/* This function should be overridden by all subclasses that ever need to be identified. */
	virtual BOOL IsA(int type) { return type == DOM_TYPE_OBJECT; }

	/* This function should be overridden by all subclasses that "own" other objects. */
	virtual void DOMChangeRuntime(DOM_Runtime *new_runtime);

	void DOMChangeRuntimeOnPrivate(DOM_PrivateName private_name);
	/**< Fetches the private property 'private_name' and changes its runtime
	     to this object's runtime if the property's value was an object.
	     Obviously this function should be called from this object's
	     DOMChangeRuntime after DOM_Object::DOMChangeRuntime has been
	     called. */

	static OP_STATUS DOMSetObjectRuntime(DOM_Object *object, DOM_Runtime *runtime);
	/**< Initializes 'object'.  'Object' can be NULL; this is treated as an
	     OOM condition and signalled by returning OpStatus::ERR_NO_MEMORY.
	     If 'object' is not NULL, SetObjectRuntime is called to initialize it
	     using the Object prototype and "Object" as class name.  If that
	     fails, 'object' is deleted.

	     @param object Object to initialize.  Can be NULL.
	     @param runtime Runtime to initialize with.  Can not be NULL.

	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY on OOM. */

	static OP_STATUS DOMSetObjectRuntime(DOM_Object *object, DOM_Runtime *runtime, ES_Object *prototype, const char *class_name);
	/**< Initializes 'object'.  'Object' can be NULL; this is treated as an
	     OOM condition and signalled by returning OpStatus::ERR_NO_MEMORY.
	     If 'object' is not NULL, SetObjectRuntime is called to initialize it
	     using 'prototype' and 'class_name'.  If that fails, 'object' is
	     deleted.

	     @param object Object to initialize.  Can be NULL (interpreted as OOM).
	     @param runtime Runtime to initialize with.  Can not be NULL.
	     @param prototype Prototype object.  Can be NULL (interpreted as OOM).
		 @param class_name Object class name.  Can not be NULL.

	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY on OOM. */

	static void DOMSetObjectRuntimeL(DOM_Object *object, DOM_Runtime *runtime, ES_Object *prototype, const char *class_name);
	/**< Calls DOMSetObjectRuntime with the same arguments and LEAVEs on OOM.

	     @param object Object to initialize.  Can be NULL (interpreted as OOM).
	     @param runtime Runtime to initialize with.  Can not be NULL.
	     @param prototype Prototype object.  Can be NULL (interpreted as OOM).
		 @param class_name Object class name.  Can not be NULL. */

	static OP_STATUS DOMSetFunctionRuntime(DOM_Object *object, DOM_Runtime *runtime, const char *class_name = "Function");
	/**< Initializes 'object' as a function.  'Object' can be NULL; this is
	     treated as an OOM condition.  If the initialization fails, 'object'
	     is deleted.

	     @param object Object to initialize.  Can be NULL (interpreted as OOM).
	     @param runtime Runtime to initialize with.  Can not be NULL.
	     @param class_name Object class name.  Can not be NULL. */

	static void DOMSetFunctionRuntimeL(DOM_Object *object, DOM_Runtime *runtime, const char *class_name = "Function");
	/**< Initializes 'object' as a function.  'Object' can be NULL; this is
	     treated as an OOM condition.  If the initialization fails, 'object'
	     is deleted.

	     @param object Object to initialize.  Can be NULL (interpreted as OOM).
	     @param runtime Runtime to initialize with.  Can not be NULL.
	     @param class_name Object class name.  Can not be NULL. */

	static void DOMSetFunctionRuntimeL(DOM_Object *function, DOM_Runtime *runtime, const uni_char *name, const char *arguments);

	static void DOMSetUndefined(ES_Value *v) { if (v) v->type = VALUE_UNDEFINED; }
	static void DOMSetNull(ES_Value *v) { if (v) v->type = VALUE_NULL; }
	static void DOMSetBoolean(ES_Value *v, BOOL b);
	static void DOMSetNumber(ES_Value *v, double n);
	static void DOMSetObject(ES_Value *v, ES_Object* o);
	static void DOMSetObject(ES_Value *v, EcmaScript_Object* o);
	static void DOMSetObject(ES_Value *v, DOM_EventListener *l);
	static void DOMSetString(ES_Value* v, const uni_char *s = NULL);
	static void DOMSetString(ES_Value* v, const uni_char *s, unsigned length);
	/**< Set 'value' to a VALUE_STRING with an explicit length.

	     @param v The value result. If NULL, then the operation is a no-op.
	     @param s The host string, can be NULL.
	     @param length The accurate length of 's'. Must be an initial,
	            possibly empty, prefix of the string starting at 's'. */

	static void DOMSetString(ES_Value* v, const uni_char *s, const uni_char *ds);
	static void DOMSetString(ES_Value* v, TempBuffer *s);
	/**
	 * @param[in] string_holder The object that will encapsulate the
	 * string/length pair inside the ES_Value.
	 * @param[in] string_length. The length of the string or -1 (default) if
	 * it will be determined with uni_strlen.
	 */
	static void DOMSetStringWithLength(ES_Value* v, ES_ValueString* string_holder, const uni_char *string, int string_length = -1);
	static OP_STATUS DOMSetDate(ES_Value* value, ES_Runtime* runtime, double milliseconds_timestamp);
	/**< Creates native Date object.

		 @param value - value set to the date object
		 @param runtime - ES_Runtime to which the Date object will be added.
		 @param milliseconds_timestamp - the value to be set as datetime. */

	static OP_STATUS DOMSetDate(ES_Value* value, ES_Runtime* runtime, time_t timestamp);
	/**< Creates native Date object.

		 @param value - value set to the date object
		 @param runtime - ES_Runtime to which the Date object will be added.
		 @param timestamp - the value to be set as datetime. */

	ES_GetState DOMSetPrivate(ES_Value *value, int private_name);
	/**< If 'value' is NULL, returns GET_SUCCESS; otherwise calls
	     'result=GetPrivate(private_name, value)' and returns
	     GET_SUCCESS if 'result' is OpBoolean::IS_TRUE, GET_FAILED
	     if 'result' is OpBoolean::IS_FALSE and else returns
	     GET_NO_MEMORY.

	     @param value Value to set.  Can be NULL.
	     @param private_name Private name to get.

	     @return GET_SUCCESS, GET_FAILED or GET_NO_MEMORY on OOM.
	*/

	static OP_STATUS DOMCopyValue(ES_Value &destination, const ES_Value &source);
	static void DOMFreeValue(ES_Value &value);

	BOOL DOMGetArrayLength(ES_Object *array, unsigned &length);
	/**< Read the length property of an array object.

	     This function requires an object that has been filtered by the array
	     argument conversion specifier (e.g.: [s]), so the length is readily
	     available, does not require executing accessors, accessing host
	     properties or type conversion.

	     It is NOT safe to use this function to read properties of arbitrary
	     objects!

	     Failure to read the property due to OOM should be excessively rare, and
	     is handled by returning FALSE.

	     @param array An array type object.
	     @param value Pointer to integer where the length will be stored.
	     @return TRUE if the length was successfully read, FALSE otherwise. */

	BOOL DOMGetDictionaryBoolean(ES_Object *dictionary, const uni_char *name, BOOL default_value);
	/**< Read a boolean member of a dictionary type object.

	     A dictionary object is one where all (accessed) members are guaranteed
	     to be simple (not accessor or host properties) and of the expected
	     type.  This is guaranteed if the object is a function argument and the
	     function parameter conversion format string used a dictionary type
	     conversion specifier for the argument in question.

	     It is typically NOT safe to use this function to read properties of
	     arbitrary objects!

	     Failure to read the property due to OOM should be excessively rare, and
	     is handled by pretending the member wasn't present in the dictionary.

	     @param dictionary A dictionary type object.
	     @param name Member name.
	     @param default_value Value returned if the member is not present.
	     @return Member value if present or 'default_value' if not.	*/

	double DOMGetDictionaryNumber(ES_Object *dictionary, const uni_char *name, double default_value);
	/**< Read a number member of a dictionary type object.

	     See DOMGetDictionaryBoolean() for details.

	     @param dictionary A dictionary type object.
	     @param name Member name.
	     @param default_value Value returned if the member is not present.
	     @return Member value if present or 'default_value' if not.	*/

	const uni_char *DOMGetDictionaryString(ES_Object *dictionary, const uni_char *name, const uni_char *default_value);
	/**< Read a string member of a dictionary type object.

	     See DOMGetDictionaryBoolean() for details.

	     @param dictionary A dictionary type object.
	     @param name Member name.
	     @param default_value Value returned if the member is not present.
	     @return Member value if present or 'default_value' if not.	*/

	enum DOMException
	{
		// For internal use only.
		DOMEXCEPTION_NO_ERR            = -1,

		// DOM Level 1
		INDEX_SIZE_ERR                 = 1,
		DOMSTRING_SIZE_ERR             = 2,
		HIERARCHY_REQUEST_ERR          = 3,
		WRONG_DOCUMENT_ERR             = 4,
		INVALID_CHARACTER_ERR          = 5,
		NO_DATA_ALLOWED_ERR            = 6,
		NO_MODIFICATION_ALLOWED_ERR    = 7,
		NOT_FOUND_ERR                  = 8,
		NOT_SUPPORTED_ERR              = 9,
		INUSE_ATTRIBUTE_ERR            = 10,
		// DOM Level 2
		INVALID_STATE_ERR              = 11,
		SYNTAX_ERR                     = 12,
		INVALID_MODIFICATION_ERR       = 13,
		NAMESPACE_ERR                  = 14,
		INVALID_ACCESS_ERR             = 15,
		// DOM Level 3
		VALIDATION_ERR                 = 16,
		TYPE_MISMATCH_ERR              = 17,
		// Web DOM Core
		SECURITY_ERR                   = 18,
		NETWORK_ERR                    = 19,
		ABORT_ERR                      = 20,
		URL_MISMATCH_ERR               = 21,
		QUOTA_EXCEEDED_ERR             = 22,
		TIMEOUT_ERR                    = 23,
		INVALID_NODE_TYPE_ERR          = 24,
		// Structured clone (html5)
		DATA_CLONE_ERR                 = 25,

		DOMEXCEPTION_COUNT
	};

	enum EventException
	{
		// For internal use only.
		EVENTEXCEPTION_NO_ERR          = -1,

		UNSPECIFIED_EVENT_TYPE_ERR     = 0,
		DISPATCH_REQUEST_ERR           = 1,

		EVENTEXCEPTION_COUNT           = 3
	};

#ifdef DOM3_XPATH
	enum XPathException
	{
		// For internal use only.
		XPATHEXCEPTION_NO_ERR          = -1,

		INVALID_EXPRESSION_ERR         = 51,
		TYPE_ERR                       = 52
	};
#endif // DOM3_XPATH

#ifdef SVG_DOM
	enum SVGException
	{
		SVG_WRONG_TYPE_ERR           = 0,
		SVG_INVALID_VALUE_ERR        = 1,
		SVG_MATRIX_NOT_INVERTABLE    = 2
	};
#endif // SVG_DOM

#ifdef DOM_GADGET_FILE_API_SUPPORT
	enum FileAPIException
	{
		FILEAPI_WRONG_ARGUMENTS_ERR,
		FILEAPI_WRONG_TYPE_OF_OBJECT_ERR,
		FILEAPI_GENERIC_ERR,
		FILEAPI_NO_ACCESS_ERR,
		FILEAPI_FILE_NOT_FOUND_ERR,
		FILEAPI_FILE_ALREADY_EXISTS_ERR,
		FILEAPI_NOT_SUPPORTED_ERR,
		FILEAPI_RESOURCE_UNAVAILABLE_ERR,
		FILEAPI_TYPE_NOT_SUPPORTED_ERR
	};
#endif // DOM_GADGET_FILE_API_SUPPORT

#ifdef WEBSERVER_SUPPORT
	enum WebServerException
	{
		WEBSERVER_TYPE_NOT_SUPPORTED_ERR,
		WEBSERVER_USER_EXISTS_ERR,
		WEBSERVER_USER_NOT_FOUND_ERR,
		WEBSERVER_AUTH_PATH_EXISTS_ERR,
		WEBSERVER_SANDBOX_EXISTS_ERR,
		WEBSERVER_ALREADY_SHARED_ERR
	};
#endif // WEBSERVER_SUPPORT

#ifdef DOM_JIL_API_SUPPORT
	enum JILException
	{
		JIL_INVALID_PARAMETER_ERR,
		JIL_SECURITY_ERR,
		JIL_UNKNOWN_ERR,
		JIL_UNSUPPORTED_ERR,
		// More to come
	};
#endif // DOM_JIL_API_SUPPORT

	enum InternalException
	{
		// For internal use only.
		INTERNALEXCEPTION_NO_ERR       = -1,

		UNEXPECTED_MUTATION_ERR        = 1,
		/**< An unexpected mutation occured during the processing of
		     a mutation event and caused the operation that sent the
		     mutation event to abort. */

		RESOURCE_BUSY_ERR,
		/**< An attempt was made to modify a resource that was in the
		     process of being modified already. */

		RESOURCE_UNAVAILABLE_ERR,
		/**< An attempt was made to access a resource (usually a
		     document) that has been unloaded and is unavailable. */

#ifdef DOM_XSLT_SUPPORT
		XSLT_PARSING_FAILED_ERR,
		XSLT_PROCESSING_FAILED_ERR,
#endif // DOM_XSLT_SUPPORT

		WRONG_ARGUMENTS_ERR,
		/**< Two few arguments to function, or wrong types. */

		WRONG_THIS_ERR,
		/**< Function called with the wrong this object. */

		UNSUPPORTED_DOCUMENT_OPEN_ERR,
		/**< Unsupported document.open(). */

		LAST_INTERNAL_EXCEPTION
	};

	ES_GetState GetNameDOMException(DOMException code, ES_Value *value);
	/**< Create an DOM exception object with the code 'code,' set
	     'value' to the exception object and return GET_EXCEPTION.

	     @param code Exception code.
	     @param value Set to the new exception.

	     @return GET_EXCEPTION or GET_NO_MEMORY. */

#ifdef DOM3_XPATH
	ES_GetState GetNameXPathException(XPathException code, ES_Value *value);
	/**< Create an XPath exception object with the code 'code,' set
	     'value' to the exception object and return GET_EXCEPTION.

	     @param code Exception code.
	     @param value Set to the new exception.

	     @return GET_EXCEPTION or GET_NO_MEMORY. */
#endif // DOM3_XPATH

	ES_PutState PutNameDOMException(DOMException code, ES_Value *value);
	/**< Create an DOM exception object with the code 'code,' set
	     'value' to the exception object and return PUT_EXCEPTION.

	     @param code Exception code.
	     @param value Set to the new exception.

	     @return PUT_EXCEPTION or PUT_NO_MEMORY. */

	int CallDOMException(DOMException code, ES_Value *value);
	/**< Create an DOMException object with the code 'code,' set
	     'value' to the exception object and return ES_EXCEPTION.

	     @param code Exception code.
	     @param value Set to the new exception.

	     @return ES_EXCEPTION or ES_NO_MEMORY. */

	int CallEventException(EventException code, ES_Value *value);
	/**< Create an EventException object with the code 'code,' set
	     'value' to the exception object and return ES_EXCEPTION.

	     @param code Exception code.
	     @param value Set to the new exception.

	     @return ES_EXCEPTION or ES_NO_MEMORY. */

#ifdef DOM3_XPATH
	int CallXPathException(XPathException code, ES_Value *value, const uni_char *detail = NULL);
	/**< Create an XPathException object with the code 'code,' set
	     'value' to the exception object and return ES_EXCEPTION.

	     @param code Exception code.
	     @param value Set to the new exception.

	     @return ES_EXCEPTION or ES_NO_MEMORY. */
#endif // DOM3_XPATH

#ifdef SVG_DOM
	int CallSVGException(SVGException code, ES_Value* value);
	/**< Create a SVGException object with the code 'code', set
		 'value' to the exception object and return ES_EXCEPTION.

		 @param code Exception code.
		 @param value Set to the new exception.

		 @return ES_EXCEPTION or ES_NO_MEMORY. */
#endif // SVG_DOM

#ifdef DOM_GADGET_FILE_API_SUPPORT
	int CallFileAPIException(FileAPIException code, ES_Value* value);
	/**< Create a FileAPIException object with the code 'code', set
		 'value' to the exception object and return ES_EXCEPTION.

		 @param code Exception code.
		 @param value Set to the new exception.

		 @return ES_EXCEPTION or ES_NO_MEMORY. */
#endif // DOM_GADGET_FILE_API_SUPPORT

#ifdef WEBSERVER_SUPPORT
	int CallWebServerException(WebServerException code, ES_Value* value);
	/**< Create a FileAPIException object with the code 'code', set
		 'value' to the exception object and return ES_EXCEPTION.

		 @param code Exception code.
		 @param value Set to the new exception.

		 @return ES_EXCEPTION or ES_NO_MEMORY. */
#endif // WEBSERVER_SUPPORT

#ifdef DOM_JIL_API_SUPPORT

	static OP_STATUS CreateJILException(DOM_Object::JILException code, ES_Value *value, DOM_Runtime* runtime, const uni_char* custom_message = NULL);
	/**< Create a JIL exception object with the code 'code',
		 assign it to the 'value' object.

		 @param code Exception code.
		 @param value Set to the new exception object.
		 @param runtime The runtime in which the exception is created.
		 @param custom_message Optional message to be set for the 'message'
		                       property of the exception. If NULL, a default
							   message is used.
		 */

#endif // DOM_JIL_API_SUPPORT

	int CallInternalException(InternalException code, ES_Value *value, const uni_char *detail = NULL);
	/**< Create an internal exception object with the code 'code,' set
	     'value' to the exception object and return ES_EXCEPTION.

	     @param code Exception code.
	     @param value Set to the new exception.

	     @return ES_EXCEPTION or ES_NO_MEMORY. */

	int CallCustomException(const char *classname, const char *message, unsigned code, ES_Value *value, ES_Object *prototype = NULL);
	/**< Create an exception object of the class 'classname' with the
	     message property set to 'message' and the code property set to
		 'code'; set 'value' to the exception object and return
		 ES_EXCEPTION.

	     @param classname The exception's class name.
	     @param classname The exception's message property or NULL for
	                      no message.
	     @param code The exception's code property.
	     @param value Set to the new exception.
	     @param proto if provided, the prototype of the exception object.
	                  If NULL, the runtime's error prototype is used.

	     @return ES_EXCEPTION or ES_NO_MEMORY. */

	int CallNativeException(ES_NativeError type, const uni_char *message, ES_Value *value);
	/**< Create a native exception object including a context-specific
	     message. The 'value' is set to the exception object and
	     ES_EXCEPTION is returned.

	     @param type The type of native exception, @see ES_NativeError.
	     @param message The message property of the exception object.
	     @param value Set to the new exception.

	     @return ES_EXCEPTION or ES_NO_MEMORY. */

	static ES_GetState ConvertCallToGetName(int result, ES_Value *value);
	static ES_PutState ConvertCallToPutName(int result, ES_Value *value);
	static int ConvertGetNameToCall(ES_GetState result, ES_Value *value);
	static int ConvertPutNameToCall(ES_PutState result, ES_Value *value);

	int CallAsGetNameOrGetIndex(const uni_char *arg, ES_Value *return_value, DOM_Runtime *origining_runtime);
	/**< Convenience function used to translate a call with a single
	     string argument to either a call to GetName or GetIndex
	     on the same object.  Usable for calling a collection as a
	     function the MSIE way. */

	BOOL IsNode() { return IsA(DOM_TYPE_NODE); }
	BOOL IsElement() { return IsA(DOM_TYPE_ELEMENT); }
	BOOL IsHTMLElement() { return IsA(DOM_TYPE_HTML_ELEMENT); }
	BOOL IsAttr() { return IsA(DOM_TYPE_ATTR); }

	void PutL(const char *name, const ES_Value &value, int flags = 0);
	/**< Calls EcmaScript_Object::Put with the same arguments ('name' upgraded
	     to uni_char) and LEAVEs on OOM.

	     @param name Property name.
	     @param value Property value.
	     @param flags Property flags sent to ES_Runtime::PutName. */

	static void PutL(ES_Object *target, const char *name, const ES_Value &value, DOM_Runtime *runtime, int flags = 0);
	/**< Calls ES_Runtime::PutName with the same arguments ('name' upgraded
	     to uni_char) and LEAVEs on OOM.

	     @param name Property name.
	     @param value Property value.
	     @param flags Property flags sent to ES_Runtime::PutName. */

	void PutPrivateL(int name, DOM_Object *object);
	/**< Puts 'object' as a private property on the object.  LEAVEs on OOM.

	     @param name A name.  Usually one from the DOM_PrivateName enumeration,
	                 but any integer value will do as long as collisions are
	                 avoided on this object.
	     @param object A DOM object.  Can not be NULL. */

	enum PropertyFlagExtra { PROP_IS_FUNCTION = 8, PROP_MASK = 7 };

	void PutObjectL(const char *name, DOM_Object *object, const char *classname, int flags = 0);
	/**< If 'object' is NULL: LEAVE(OpStatus::ERR_NO_MEMORY).  Otherwise,
	     calls SetObjectRuntime on 'object' using this object's runtime,
	     that runtime's Object prototype and 'classname'.  If that fails,
	     'object' is deleted.  Then it puts the object as a property named
	     'name' on this object.

	     @param name Property name.  Can not be NULL.
	     @param function Object.  Can be NULL (interpreted as OOM).
	     @param classname Object class name.  Can not be NULL.
	     @param flags Property flags sent to ES_Runtime::PutName. */

	ES_Object *PutSingletonObjectL(const char *name, int flags = 0);
	/**< Create and return a plain native object with class name 'name' and put
	     it as a property named 'name' on this object. */

	void PutFunctionL(const char *name, DOM_Object *function, const char *classname, const char *arguments = NULL, BOOL is_object = FALSE);
	/**< If 'function' is NULL: LEAVE(OpStatus::ERR_NO_MEMORY).  Otherwise,
	     calls SetFunctionRuntime on 'function' using this object's runtime,
	     that runtime's Function prototype and 'classname'.  If that fails,
	     'function' is deleted, otherwise it puts the function as a property
	     named 'name' on this object.

	     @param name Property name.  Can not be NULL.
	     @param function Function object.  Can be NULL (interpreted as OOM).
	     @param classname Function object class name.  Can not be NULL.
	     @param arguments Argument conversion specification.  If NULL, no
	                      conversion will be done.
	     @param is_object This is really a callable object, so use a
	                      toString function that returns "[object XXX]". */

	void PutConstructorL(DOM_BuiltInConstructor *constructor, const char *arguments = NULL);
	/**< If 'constructor' is NULL: LEAVE(OpStatus::ERR_NO_MEMORY).  Otherwise,
	     calls SetFunctionRuntime on 'constructor' using this object's runtime,
	     that runtime's Function prototype and the name registered for this constructor as class
	     name.  If that fails, 'constructor' is deleted, otherwise it puts the constructor as a
	     dont-enum property named by the constructor's regsistered name on this object.
	     The prototype this constructor is related to will be fetched from
	     constructor. It is used to locate this object from
	     the constructor property on prototype objects.

	     @param constructor Function object.  Can be NULL (interpreted as OOM).
	     @param arguments Argument conversion specification.  If NULL, no
	                      conversion will be done. */

	DOM_Object *PutConstructorL(const char *name, DOM_Runtime::Prototype type, BOOL singleton = FALSE);
	DOM_Object *PutConstructorL(const char *name, DOM_Runtime::HTMLElementPrototype type);
#ifdef SVG_DOM
	DOM_Object *PutConstructorL(const char *name, DOM_Runtime::SVGObjectPrototype type);
	DOM_Object *PutConstructorL(const char *name, DOM_Runtime::SVGElementPrototype type, DOM_SVGElementInterface ifs);
#endif // SVG_DOM

	static void PutNumericConstantL(ES_Object *target, const char *name, int value, DOM_Runtime *runtime);
	/**< Call PutL on this object to set a constant property named
	     'name' with the signed integer value 'value'. */

	static void PutNumericConstantL(ES_Object *target, const char *name, unsigned value, DOM_Runtime *runtime);
	/**< Call PutL on this object to set a constant property named
	     'name' with the unsigned integer value 'value'. */

	static OP_STATUS PutNumericConstant(ES_Object *object, const uni_char *name, int value, DOM_Runtime *runtime);
	/**< Puts a constant property named 'name' with the signed integer value 'value'
		 to a object 'object'. */

	OP_STATUS PutStringConstant(const uni_char *name, const uni_char *value);
	/**< Puts a constant property named 'name' with the string value 'value'
		 to a this object. */

	static OP_STATUS PutStringConstant(ES_Object* object, const uni_char *name, const uni_char *value, DOM_Runtime* runtime);
	/**< Puts a constant property named 'name' with the string value 'value'
		 to a object 'object'. */
	/* Convenience functions. */

	DOM_EnvironmentImpl *GetEnvironment();
	DOM_Runtime *GetRuntime();
	FramesDocument *GetFramesDocument();
	HTML_Document *GetHTML_Document();
	HLDocProfile *GetHLDocProfile();
	LogicalDocument *GetLogicalDocument();
	ServerName *GetHostName();

	OP_STATUS CreateEventTarget();
	DOM_EventTargetOwner *GetEventTargetOwner();
	DOM_EventTarget *GetEventTarget();

	static ES_Thread *GetCurrentThread(ES_Runtime *runtime);
	static const ServerName *GetServerName(const URL url);

	static void GCMark(ES_Runtime *runtime, ES_Object *object);
	/**< If 'object' is not NULL, call ES_Runtime::GCMark(object). */

	static void GCMark(ES_Runtime *runtime, DOM_Object *object);
	/**< If 'object' is not NULL, call ES_Runtime::GCMark(*object). */

	void GCMark(ES_Object *object);
	/**< If 'object' is not NULL, call ES_Runtime::GCMark(object). */

	static void GCMark(DOM_Object *object);
	/**< If 'object' is not NULL, call ES_Runtime::GCMark(object). */

	static void GCMark(ES_Runtime *runtime, ES_Value &value);
	/**< If 'value.type' == VALUE_OBJECT and the object is a DOM,
	     call ES_Runtime::GCMark(object). */

	void GCMark(ES_Value &value);
	/**< If 'value.type' == VALUE_OBJECT and the object is a DOM,
	     call ES_Runtime::GCMark(object). */

	static void GCMark(DOM_EventTarget *event_target);
	/**< If 'event_target' is not NULL, call event_target->GCTrace(). */

	ES_GetState GetEventProperty(const uni_char *property_name, ES_Value *value, DOM_Runtime *origining_runtime);

	ES_PutState PutEventProperty(const uni_char *property_name, ES_Value *value, DOM_Runtime* origining_runtime);
	/**< Put an old style event property ("onload", "onclick" and so on).
	     If value is an object, that object will be called as a function to
	     handle events.  If value is a string, that string will be compiled
	     as a function and called to handle events.  If value is anything
	     else, any existing old style event handler will be removed.  If
	     this_object is not NULL, it will be used as the this object when
	     the function is called to handle an event.

	     If property_name isn't a known event property, PUT_FAILED is
	     returned (and nothing else happens).

	     @return PUT_SUCCESS, PUT_FAILED or PUT_NO_MEMORY on OOM. */

	// Definitions moved to DOM_Object from EcmaScript_Object
	static TempBuffer *GetTempBuf();
	/**< Return a pointer to the global TempBuffer object for EcmaScript_Objects.
		 (The tempbuffer is for use by derived classes; it is not used by
		 EcmaScript_Object.)

		 May return NULL if a DOM_Object survives the destruction of the
		 DOM_GlobalData (which is part of the DomModule).
		 */

	static TempBuffer *GetEmptyTempBuf();
	/**< As GetTempBuf(), but empty the buffer before returning it. */

	static OP_STATUS DOMSetObjectAsHidden(ES_Value *value, DOM_Object *object);
	/**< Set and return the given 'object' as a hidden object, but without marking
	     the object itself as hidden. Used to divert sniffing scripts. */

	static int TruncateDoubleToInt(double d);
	/**< Converts a double to an integer as described in section 9.5
	     ToInt32: (Signed 32 Bit Integer) in the ecmascript
	     specification. */

	static unsigned TruncateDoubleToUInt(double d);
	/**< Converts a double to an integer as described in section 9.6
	     ToUInt32: (Unsigned 32 Bit Integer) in the ecmascript
	     specification. */

	static unsigned short TruncateDoubleToUShort(double d);
	/**< Converts a double to an short integer as described in section 9.7
	     ToUInt16: (Unsigned 16 Bit Integer) in the ecmascript
	     specification. */

	static float TruncateDoubleToFloat(double d);
	/**< Converts a double to an float as described in section 4.2.12.
	     float in the WebIDL specification. */

	/** The kinds of long long (signed and unsigned) conversions
	    possible. See their corresponding conversion methods for
	    details on how these kinds are interpreted. */
	enum LongLongConversion
	{
		ModuloRange = 0,
		EnforceRange,
		Clamp
	};

	static BOOL ConvertDoubleToLongLong(double number, LongLongConversion kind, double &result);
	/**< Perform the WebIDL (ECMAScript) translation of a 'double'
	     to a signed 'long long'. Three kinds of conversions are
		 supported:

	     [ModuloRange]: 0 if non-finite, otherwise (mod 2^64)
		 values.

	     [EnforceRange]: only admit doubles in the range [-2^53 - 1, 2^53 - 1];
	     values outside generate a TypeError.

	     [Clamp]: 0 if NaN, otherwise clamp the double to the
	     [-2^53 - 1, 2^53 - 1] range.

	     If the conversion results in a TypeError, FALSE is returned.
	     Otherwise TRUE and 'result' holds the long long value
	     (still represented as a double.) */

	static BOOL ConvertDoubleToUnsignedLongLong(double number, LongLongConversion kind, double &result);
	/**< Perform the WebIDL (ECMAScript) translation of a 'double'
	     to an unsigned 'long long'. Three kinds of conversions are
		 supported:

	     [ModuloRange]: 0 if non-finite, otherwise (mod 2^64)
		 values.

	     [EnforceRange]: only admit doubles in the range [0, 2^53-1];
	     values outside generate a TypeError.

	     [Clamp]: 0 if NaN, otherwise clamp the double to the
	     [0, 2^53 - 1] range.

	     If the conversion results in a TypeError, FALSE is returned.
	     Otherwise TRUE and 'result' holds the long long value
	     (still represented as a double.) */

protected:

#ifdef DOM_ACCESS_SECURITY_RULES
	enum AccessType {
		ACCESS_TYPE_FUNCTION_INVOCATION,
		ACCESS_TYPE_PROPERTY_ACCESS
	};

	virtual int PropertyAccessSecurityCheck(ES_Value* value, ES_Value* argv, int argc, DOM_Runtime* origining_runtime, ES_Value* restart_value, AccessType access_type, const char* operation_name, const uni_char* arg1, const uni_char* arg2);
	/**< Performs a security check for accessing a certain property or invoking
	     a function. The check may be asynchronous.

	     @param value
	     @param origining_runtime
	     @param restart_value

	     @return ES_VALUE if the check has finished successfully, ES_SUSPEND | ES_RESTART
	             or any of the ES_FAILED, ES_EXCEPTION or ES_NO_MEMORY.
	     */
private:
	void CheckAccessSecurity(OpSecurityCheckCallback* callback, OpSecurityManager::Operation op, const OpSecurityContext& source, const OpSecurityContext& target);
	/**< A helper wrapper for OpSecurityManager::CheckSecurity function that
	     matches the requirements for our security mechanism implementation:
		 is a member function and takes callback as the first argument.

		 @param callback Object notified of operation success or failuer
		 @param op Operation for which access is to be granted
		 @param source Source context
		 @param target Target context
		 */
#endif // DOM_ACCESS_SECURITY_RULES
};

class DOM_ProxyObject
	: public DOM_Object
{
public:
	virtual ~DOM_ProxyObject();

	class Provider
	{
	public:
		virtual void ProxyObjectDestroyed(DOM_Object *proxy_object) {}
		virtual OP_STATUS GetObject(DOM_Object *&object) = 0;

	protected:
		virtual ~Provider() {}
		/* This destructor is only here because some compilers
		   complain if it isn't, not because it is in any way
		   needed. */
	};

	static OP_STATUS Make(DOM_ProxyObject *&object, DOM_Runtime *runtime, Provider *provider);

	virtual OP_STATUS Identity(ES_Object **loc);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_PROXY_OBJECT || DOM_Object::IsA(type); }
	virtual void GCTrace();
	virtual BOOL SecurityCheck(ES_Runtime *origining_runtime);

	OP_STATUS GetObject(DOM_Object *&object);
	DOM_Object *GetObject();

	void SetObject(DOM_Object *object);
	void ResetObject();
	void SetProvider(Provider *provider);
	void DisableCaches();

protected:
	DOM_ProxyObject()
		: object(NULL),
		  provider(NULL)
	{
	}

	void Init(Provider* provider);

private:
	DOM_Object *object;
	Provider *provider;
};

class DOM_WindowProxyObject
	: public DOM_ProxyObject
{
public:
	static OP_STATUS Make(DOM_ProxyObject *&object, DOM_Runtime *runtime, Provider *provider);
	virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
};

class DOM_Function
	: public DOM_Object
{
protected:
	DOM_FunctionImpl *impl;

public:
	DOM_Function(DOM_FunctionImpl *impl) : impl(impl) {}

	virtual int Call(ES_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime);
	virtual BOOL IsA(int type) { return type == DOM_TYPE_FUNCTION || DOM_Object::IsA(type); }
};

class DOM_InsecureFunction
	: public DOM_Function
{
public:
	DOM_InsecureFunction(DOM_FunctionImpl *impl) : DOM_Function(impl) {}

	virtual int Call(ES_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime);
};

#ifdef DOM_ACCESS_SECURITY_RULES
/** A function that performs additional security check.
 *
 * The function implementation must check that this_object is of the expected
 * type, otherwise the security check may be bypassed.
 */
class DOM_FunctionWithSecurityRule
	: public DOM_Function
{
public:
	DOM_FunctionWithSecurityRule(DOM_FunctionImpl *impl, const DOM_FunctionSecurityRuleDesc *security_rule) : DOM_Function(impl), security_rule(security_rule) {}

	virtual int Call(ES_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime);
private:
	static const uni_char* GetStringArgument(ES_Value* argv, int argc, int arg_index);
	const DOM_FunctionSecurityRuleDesc *security_rule;
};

#endif // DOM_ACCESS_SECURITY_RULES

class DOM_FunctionWithData
	: public DOM_Function
{
protected:
	int data;

public:
	DOM_FunctionWithData(DOM_FunctionWithDataImpl *impl, int data) : DOM_Function((DOM_FunctionImpl *) impl), data(data) {}

	virtual int Call(ES_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime);
};

class DOM_InsecureFunctionWithData
	: public DOM_FunctionWithData
{
public:
	DOM_InsecureFunctionWithData(DOM_FunctionWithDataImpl *impl, int data) : DOM_FunctionWithData(impl, data) {}

	virtual int Call(ES_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime);
};

class DOM_Prototype
	: public DOM_Object
{
public:
	static ES_Object *MakeL(ES_Object *prototype, const char *class_name, const char *constructor_name, const DOM_FunctionDesc *functions, const DOM_FunctionWithDataDesc *functions_with_data, DOM_Runtime *runtime);
	static OP_STATUS Make(ES_Object *&object, ES_Object *prototype, const char *class_name, const char *constructor_name, const DOM_FunctionDesc *functions, const DOM_FunctionWithDataDesc *functions_with_data, DOM_Runtime *runtime);

#ifdef DOM_LIBRARY_FUNCTIONS
	DOM_Prototype(const DOM_FunctionDesc *functions, const DOM_FunctionWithDataDesc *functions_with_data, const DOM_LibraryFunctionDesc *library_functions = NULL)
		: insecure_storage(NULL),
		  functions(functions),
		  functions_with_data(functions_with_data),
		  library_functions(library_functions)
	{
	}
#else // DOM_LIBRARY_FUNCTIONS
	DOM_Prototype(const DOM_FunctionDesc *functions, const DOM_FunctionWithDataDesc *functions_with_data)
		: insecure_storage(NULL),
		  functions(functions),
		  functions_with_data(functions_with_data)
	{
	}
#endif // DOM_LIBRARY_FUNCTIONS

	virtual ~DOM_Prototype();

	void InitializeL();

	virtual ES_GetState GetName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_GetState FetchPropertiesL(ES_PropertyEnumerator *enumerator, ES_Runtime *origining_runtime);

	virtual void GCTrace();

protected:
	DOM_PropertyStorage *insecure_storage;

	const DOM_FunctionDesc *functions;
	const DOM_FunctionWithDataDesc *functions_with_data;
#ifdef DOM_LIBRARY_FUNCTIONS
	const DOM_LibraryFunctionDesc *library_functions;

	void AddLibraryFunctionL(const DOM_LibraryFunction *impl, const char *name);
#endif // DOM_LIBRARY_FUNCTIONS
};

/** Class used for internal objects on which we are not interested in checking
    security on property access.  Its SecurityCheck function always returns
    TRUE. */
class DOM_InternalObject
	: public DOM_Object
{
public:
	virtual BOOL SecurityCheck(ES_Runtime *origining_runtime);
};

class DOM_Constructor
	: public DOM_Object
{
public:
	virtual int Call(ES_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime);
	virtual int Construct(ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime *origining_runtime) = 0;
};

class DOM_BuiltInConstructor
	: public DOM_Constructor
{
protected:
	DOM_Runtime::Prototype prototype;
	DOM_Runtime::HTMLElementPrototype html_element_prototype;
#ifdef SVG_DOM
	DOM_Runtime::SVGElementPrototype svg_element_prototype;
	DOM_SVGElementInterface svg_element_ifs;
	DOM_Runtime::SVGObjectPrototype svg_object_prototype;
#endif // SVG_DOM
	DOM_Prototype *host_object_prototype;
	/* [The assumption is that the prototype is otherwise kept alive
	    and traced than by the constructor and the constructor doesn't
	    have to be traced by DOM_BuiltInConstructor.] */

	enum {
		OBJECT_PROTOTYPE,
		HTML_ELEMENT_PROTOTYPE,
#ifdef SVG_DOM
		SVG_ELEMENT_PROTOTYPE,
		SVG_OBJECT_PROTOTYPE,
#endif // SVG_DOM
		HOST_OBJECT_PROTOTYPE
	} prototype_type;

public:
	DOM_BuiltInConstructor(DOM_Runtime::Prototype prototype);
	DOM_BuiltInConstructor(DOM_Runtime::HTMLElementPrototype html_element_prototype);
#ifdef SVG_DOM
	DOM_BuiltInConstructor(DOM_Runtime::SVGElementPrototype prototype, DOM_SVGElementInterface ifs);
	DOM_BuiltInConstructor(DOM_Runtime::SVGObjectPrototype prototype);
#endif // SVG_DOM
	DOM_BuiltInConstructor(DOM_Prototype *prototype);

	virtual ES_GetState GetName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);

	virtual int Construct(ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime *origining_runtime);

	DOM_Runtime::Prototype GetPrototype() { OP_ASSERT(prototype_type == OBJECT_PROTOTYPE); return prototype; }
	ES_Object *GetPrototypeObject();
};

/**
 * Helper function for toString (object -> string generation).
 */
int DOM_toString(ES_Object* object, TempBuffer* buffer, ES_Value* return_value);

/**
 * Helper function for toString (object -> string generation).
 */
int DOM_toString(DOM_Object* this_object, ES_Value*, int, ES_Value* return_value, DOM_Runtime*);

extern int DOM_CheckType(DOM_Runtime *runtime, DOM_Object *object, DOM_ObjectType type, ES_Value *return_value, DOM_Object::InternalException exception);

extern int DOM_CheckFormat(DOM_Runtime *runtime, const char *format, int argc, const ES_Value *argv, ES_Value *return_value);
extern int DOM_CheckFormatNoException(const char *format, int argc, const ES_Value *argv);
#ifdef DOM_JIL_API_SUPPORT
extern int JIL_CheckFormat(DOM_Runtime *runtime, const char *format, int argc, const ES_Value *argv, ES_Value *return_value);
extern int JIL_CheckType(DOM_Runtime *runtime, DOM_Object *object, DOM_ObjectType type, ES_Value *return_value);
#endif // DOM_JIL_API_SUPPORT

extern DOM_Object *DOM_GetHostObject(ES_Object *foreignobject);

#endif // DOM_OBJECT_H
