/**
   Copyright (C) 1995-2011 Opera Software ASA. All rights reserved.

   THIS FILE DESCRIBES THE INTERFACE ("API") TO JSPLUGIN, THE MODULAR JAVASCRIPT
   PLUGIN TECHNOLOGY FOR THE OPERA 11 BROWSER LAUNCHED BY OPERA SOFTWARE ASA
   ("OPERA"). THE API TOGETHER WITH DOCUMENTATION AND EXAMPLE CODE IS PUBLICISED
   BY OPERA, AND OPERA IS WILLING TO PERMIT USE OF IT BY YOU ("LICENSEE"), ONLY
   UPON THE CONDITION THAT YOU ACCEPT ALL OF THE TERMS CONTAINED IN A SEPARATE
   API LICENSE AGREEMENT ("AGREEMENT"). PLEASE READ THE TERMS AND CONDITIONS OF
   THIS LICENSE CAREFULLY. BY READING, COPYING OR USING THE API IN ANY WAY, YOU
   ACCEPT THE TERMS AND CONDITIONS OF THIS AGREEMENT. IF YOU ARE NOT WILLING TO
   BE BOUND, YOU ARE NOT ALLOWED TO STUDY THIS API OR MAKE USE OF IT IN ANY WAY.
*/

/* API for Native JavaScript Extensions
   Version 1.9, 2011-10-14

   Basically: when the plugin is loaded, jsplugin_capabilities is called,
   which allows the plugin and Opera to exchange information.

   See documentation/HOWTO.html for more information.

*/

#ifndef JSPLUGIN_H
#define JSPLUGIN_H

#ifdef WIN32
# if _MSC_VER < 1300 // VC6.00 == 1200
#  pragma pack(push, 8)
#  define JSP_STRUCT_ALIGN
# else
#  define JSP_STRUCT_ALIGN __declspec(align(8))
# endif // _MSC_VER
#else // WIN32
# define JSP_STRUCT_ALIGN
#endif // !WIN32

typedef struct jsplugin_obj       jsplugin_obj;
typedef struct jsplugin_attr      jsplugin_attr;
typedef struct jsplugin_cap       jsplugin_cap;
typedef struct jsplugin_callbacks jsplugin_callbacks;
typedef struct jsplugin_value     jsplugin_value;

/** The plugin exports a function called jsplugin_capabilities,
    The type of this function is jsplugin_capabilities_fn.
	@param result the plugin's capability struct should be returned in here
	@param cbs the opera callbacks that the plugin can use
	@return 0 if everything went ok, anything else indicates that the capabilities couldn't be returned and the plugin won't be considered for anything by opera.
*/
typedef int (*jsplugin_capabilities_fn)( jsplugin_cap **result, jsplugin_callbacks *cbs );

/** type for getname functions
	@param obj the js plugin object in question
	@param name the name (in UTF-8) that is about to be getted
	@param result return the result here
	@return appropriate one of the status values in the JSP status values enum
*/
typedef int (jsplugin_getter)( jsplugin_obj *obj, const char *name, jsplugin_value *result );

/** type for putname functions
	@param obj the js plugin object in question
	@param name the name (in UTF-8) that is about to be set
	@param value the value to set
	@return appropriate one of the status values in the JSP status values enum
*/
typedef int (jsplugin_setter)( jsplugin_obj *obj, const char *name, jsplugin_value *value );

/** type for natively implemented javascript functions.
	@param this_obj
	@param function_obj
	@param argc how many arguments there are in argv
	@param argv the arguments array
	@param result the place for the function to return javascript objects etc
	@return appropriate one of the status values in the JSP status values enum
*/
typedef int (jsplugin_function)( jsplugin_obj *this_obj, jsplugin_obj *function_obj,
								 int argc, jsplugin_value *argv, jsplugin_value *result );

/** type for constructor used for various objects.
	@param this_obj
	@param function_obj
	@param argc how many arguments there are in argv
	@param argv the arguments array
	@param result the place for the function to return javascript objects etc
	@return appropriate one of the status values in the JSP status values enum
*/
typedef int (jsplugin_constructor)( jsplugin_obj *this_obj, jsplugin_obj *function_obj,
								 int argc, jsplugin_value *argv, jsplugin_value *result );

/** type for destructor used for various objects.
	@param obj the object about to be destroyed
	@return appropriate one of the status values in the JSP status values enum
 */
typedef int (jsplugin_destructor)( jsplugin_obj *obj );

/** called by the garbage collector when it's about to do house cleaning. 
    Invoke the jsplugin_cb_gcmark callback on any objects you hold references to.
	@param this_obj the js plugin object in question
*/
typedef void (jsplugin_gc_trace)( jsplugin_obj *this_obj );

/** used to notify a plugin about miscellaneous things (see eg jsplugin_handle_object).
	@param obj the javascript object being notified.
*/
typedef void (jsplugin_notify)( jsplugin_obj *obj );

/** used to notify a plugin about a change to an attribute of a handled object
	@param obj the js plugin object in question
	@param name the name of the attribute (in UTF-8) that was just changed
	@param value the value that the attribute (in UTF-8) has just been set to
	@return appropriate one of the status values in the JSP status values enum
*/
typedef void (jsplugin_attr_change_listener)( jsplugin_obj *obj, const char *name, const char *value );

/** used to notify a plugin about a changes related to param elements
    of a handled object. The listener will be called with the name and
    value of each <param> tag on the object as it is parsed. It will
    also be called when a param element is dynamically added or
    updated. When a param element is dynamically removed, the function
    will be called with the name and NULL as value.

    Limitation: Currently, this functionality does not fully support
    multiple param elements with the same name: There is no support for
    detecting if a callback is made on an added param element or an
    updated one. There is also no support for detecting if there is still
    other param elements on the object with the same name as a removed element.

	@Param obj the js plugin object in question
	@param name the name of the param (in UTF-8) that was just set or updated
	@param value the value that the param (in UTF-8) has just been set or updated to
	@return appropriate one of the status values in the JSP status values enum
*/
typedef void (jsplugin_param_set_listener)( jsplugin_obj *obj, const char *name, const char *value );

/** called after asynchronous function has finished executing
	@param status appropriate one of the status values in the JSP status values enum (JSP_CALLBACK_*)
	@param return_value returned value if status==JSP_CALLBACK_OK, "undefined" otherwise
	#param user_data additional user specified payload (as supplied in call to asynchronous function)
*/
typedef void (jsplugin_async_callback)(int status, jsplugin_value *return_value, void *user_data);

/** called when a plugin is initialized for a certain global context (roughly a new document)
	@param global_context passed in representing the (document) global context. needed if jsplugin_init wishes to create eg an object.
*/
typedef void (jsplugin_document_init)(jsplugin_obj *global_context);

/** called when a plugin is deinitialized with regards to a certain global context (roughly when the document in question is destroyed)
	@param global_context passed in representing the (document) global context.
*/
typedef void (jsplugin_document_destroy)(jsplugin_obj *global_context);

/** called when an OBJECT that the plugin claims to support is created.
	@param global_context passed in representing the (document) global context.
	@param object_obj the object that is about to be constructed
	@param attrs_count how many HTML attributes there are
	@param attrs the HTML attributes
	@param getter the getname function to be used for this OBJECT plugin
	@param setter the putname function to be used for this OBJECT plugin
	@param destructor the destructor (sic!)
	@param trace the garbage collector trace hook
	@param inserted called as notification that the OBJECT was inserted into the DOM structure
	@param removed called as notification that the OBJECT was removed from the DOM structure. note that this does absolutely not mean that the object is destroyed or even about to be destroyed
	@return appropriate one of the status values in the JSP status values enum
*/
typedef int  (jsplugin_handle_object)(jsplugin_obj *global_context, jsplugin_obj *object_obj,
									  int attrs_count, jsplugin_attr *attrs,
									  jsplugin_getter **getter, jsplugin_setter **setter,
									  jsplugin_destructor **destructor, jsplugin_gc_trace **trace,
									  jsplugin_notify **inserted, jsplugin_notify **removed);

/** called to determine whether scripts from a specific server should be allowed to access to the plugin
	@param protocol protocol name
	@param hostname host name
	@param port port number (0 means default port for protocol)
	@return non-zero to allow access
*/
typedef int (jsplugin_allow_access)(const char *protocol, const char *hostname, int port);

/** called when a TV placeholder is added or removed from display for an
    object supported by the plugin.
	@param object_obj the object that is becoming visible or invisible.
    @param visibility Zero if non-visible, non-zero if visible.
	@param unloading Non-zero if TV placeholder is being removed from the display
	       due to the document being unloaded (browser navigating to another page).
	       Relevant only if \ref visibility is zero.
*/
typedef void (jsplugin_tv_become_visible)(jsplugin_obj *object_obj, int visibility, int unloading);

/** called when a TV placeholder has its coordinates changed for an
    object supported by the plugin. If the width and height are zero,
    the object is removed from display.
	@param object_obj the object that is about to have its position changed.
	@param x X coordinate.
	@param y Y coordinate.
	@param w Width.
	@param h Height.
*/
typedef void (jsplugin_tv_position)(jsplugin_obj *object_obj,
                                    int x, int y, int w, int h);

/** callback function used in conjunction with jsplugin_cb_set_poll_interval
	@param global_context the global context object used in the call to jsplugin_cb_set_poll_interval
	@return zero to end the polling, non-zero to receive further calls
*/
typedef int (jsplugin_poll_callback)(const jsplugin_obj *global_context);

/* callbacks into opera */

/** used to request that a function be created on the opera side.
	@param refobj a reference object that already belongs to the document. it can be another object the plugin has created itself or the (document) global context passed in to some other functions from opera
	@param getter the getname function for this object
	@param setter the putname function for this object
	@param f_call the native function to call for this js function (can be NULL). most often you only want one of f_call and f_construct.
	@param f_construct make a constructor (can be NULL). most often you only want one of f_call and f_construct.
	@param f_signature the function's signature.
	@param destructor the destructor for this object
	@param trace the garbage collector trace hook
	@param result the function object from opera is returned in this parameter
	@return appropriate one of the status values in the JSP status values enum
*/
typedef int (jsplugin_cb_create_function)(const jsplugin_obj *refobj,
										  jsplugin_getter *getter,
										  jsplugin_setter *setter, 
										  jsplugin_function *f_call, 
										  jsplugin_constructor *f_construct,
										  const char *f_signature,
										  jsplugin_destructor *destructor,
										  jsplugin_gc_trace *trace,
										  jsplugin_obj **result);

/** used to request that an object be created on the opera side.
	@param refobj a reference object that already belongs to the document. it can be another object the plugin has created itself or the (document) global context passed in to some other functions from opera
	@param getter the getname function for this object
	@param setter the putname function for this object
	@param destructor the destructor for this object
	@param trace the garbage collector trace hook
	@param result the object from opera is returned in this parameter
	@return appropriate one of the status values in the JSP status values enum
*/
typedef int (jsplugin_cb_create_object)(const jsplugin_obj *refobj,
										jsplugin_getter *getter, jsplugin_setter *setter,
										jsplugin_destructor *destructor,
										jsplugin_gc_trace *trace,
										jsplugin_obj **result);

/** used to evaluate a string of ecmascript code.
	@param refobj a reference object that belongs to the document in which the code is to be evaluated
	@param code a UTF-8 string containing ecmascript code
	@param user_data an optional value that can be used by the caller (supplied back when callback is called)
	@param callback an optional callback that will be called when the script has been executed if this function returns JSP_CALLBACK_OK
	@return appropriate one of the status values in the JSP status values enum
*/
typedef int (jsplugin_cb_eval)(const jsplugin_obj *refobj, const char *code, void *user_data, jsplugin_async_callback *callback);

/** used to request that Opera calls the supplied plugin function once every N milliseconds, to allow the plugin to call Opera back safely.  the polling continues until jsplugin_document_destroy() is called for 'global_context', or the poll callback returns zero.
	@param global_context the global_context object (provided in the jsplugin_document_init callback)
	@param interval the requested interval in milliseconds.  depending on system timer resolution and load, delays may be greater than the requested interval
	@param callback plugin callback function to call
	@return appropriate one of the status values in the JSP status values enum
*/
typedef int (jsplugin_cb_set_poll_interval)(const jsplugin_obj *global_context, unsigned interval, jsplugin_poll_callback *callback);

/** used to execute a ecmascript function.
	Please note that this function is asynchronous!
	@param global_context the global_context object (provided in the jsplugin_document_init callback)
	@param this_obj pointer to native object on which to call member function on OR null
	@param function_obj pointer to native function that you wish to invoke
	@param argc the number of arguments supplied
	@param argv arguments OR null
	@param user_data User defined data (supplied in callback)
	@callback function that is invoked when the ecmascript function has finished executing
	@return appropriate one of the status values in the JSP status values enum
*/
typedef int (jsplugin_cb_call_function)(const jsplugin_obj *global_context, const jsplugin_obj *this_obj, const jsplugin_obj *function_obj,
								 int argc, jsplugin_value *argv, void *user_data, jsplugin_async_callback *callback);

/** used to retrieve properties from ecmascript objects
	Please note that this function is asynchronous!
	@param global_context the global_context object (provided in the jsplugin_document_init callback)
	@param obj pointer to object on which to retrieve property on
	@param name name of property to retrieve
	@param user_data user defined data (supplied in callback)
	@callback function that is invoked when the property has been retrieved
	@return appropriate one of the status values in the JSP status values enum
*/
typedef int (jsplugin_cb_get_prop)(const jsplugin_obj *global_context, const jsplugin_obj *obj, const char* name, void *user_data, jsplugin_async_callback *callback);

/** used to set properties on ecmascript objects
	Please note that this function is asynchronous!
	@param global_context the global_context object (provided in the jsplugin_document_init callback)
	@param obj pointer to object on which to set property on
	@param name name of property to set
	@param value value that you want to be stored
	@param user_data user defined data (supplied in callback)
	@callback function that is invoked when the property has been set
	@return appropriate one of the status values in the JSP status values enum
*/
typedef int (jsplugin_cb_set_prop)(const jsplugin_obj *global_context, const jsplugin_obj *obj, const char* name, jsplugin_value *value, void *user_data, jsplugin_async_callback *callback);

/** add an unload listener
	@param target object on which to listen for the unload event on
	@param listener called as a notification that the object is being unloaded
	@return appropriate one of the status values in the JSP status values enum
*/
typedef int (jsplugin_cb_add_unload_listener)(const jsplugin_obj *target, jsplugin_notify *listener);

/** add a listener that will be called if an attribute of the element handled
    by the jsplugin is changed.
	@param target object on which to listen for the unload event on
	@param listener called as a notification that an attribute has been changed
	@return appropriate one of the status values in the JSP status values enum
*/
typedef int (jsplugin_cb_set_attr_change_listener)(jsplugin_obj *target, jsplugin_attr_change_listener *listener);

/** add a listener that will be called if the name or value of a param element on an object handled
    by the jsplugin is set or updated.
	@param target object on which to listen for the unload event on
	@param listener called as a notification that a param has been set or updated
	@return appropriate one of the status values in the JSP status values enum
*/
typedef int (jsplugin_cb_set_param_set_listener)(jsplugin_obj *target, jsplugin_param_set_listener *listener);

/** add TV visualization callbacks. Must be called from jsplugin_capabilities()
    during initialization to enable visual representation for this plugin.
    @param become_visible called when TV visualization is added or removed.
    @param position called when the positioning is changed.
    @return 0 if everything went ok, anything else indicates that the callbacks couldn't be registered.

*/
typedef int (jsplugin_cb_add_tv_visual)(jsplugin_tv_become_visible *become_visible,
										jsplugin_tv_position *position);

/** synchronously retrieves an identifier for the specific Opera window a particular object is in.
    @param obj the object in question.
    @param identifier[out] window identifier (only set if 0 was returned).
    @return JSP_CALLBACK_OK if everything went ok, JSP_CALLBACK_ERROR if not.
*/
typedef int (jsplugin_cb_get_window_identifier)(const jsplugin_obj *obj, long int *identifier);

/** get the origin host of a jsplugin object.
    @param obj object to get the host from
    @param host[out] receives a pointer to URL of the hosting document. the value is temporary and should be copied by the receiver.
    @return JSP_CALLBACK_OK if everything went ok, JSP_CALLBACK_ERROR if not.
*/
typedef int (jsplugin_cb_get_object_host)(const jsplugin_obj *obj, char **host);

/** resume a previously suspended execution (function call or get).
	@param refobj a reference object that belongs to the document which was suspended
	@return JSP_CALLBACK_OK if execution was resumed, JSP_CALLBACK_ERROR if not.
	@note  The "this" object is a good reference object.
*/
typedef int (jsplugin_cb_resume)(const jsplugin_obj *refobj);

/** Mark object as in use so it won't be collected by the garbage collector.
	@param obj object of type jsplugin_obj you want to keep. Optional.
*/
typedef void (jsplugin_cb_gcmark)(const jsplugin_obj *obj);

/** a place to store private data for both plugin and opera */
struct JSP_STRUCT_ALIGN jsplugin_obj {
    void *plugin_private;
    void *opera_private;
};

/** struct to store HTML attributes for the OBJECT handling */
struct JSP_STRUCT_ALIGN jsplugin_attr {
	const char *name;
	const char *value;
};

/** jsplugin_cap is a structure containing plugin meta-information. */

struct JSP_STRUCT_ALIGN jsplugin_cap {
	/** global_names is NULL or an array of the names of global slots provided
	    by the plugin. */
	const char* const* global_names;

	/** object_types is NULL or an array of the names of mime types
        that the plugin provides OBJECT handling for. */
	const char* const* object_types;

	/** this is the global getname function */
	jsplugin_getter *global_getter;

	/** this is the global setname function */
	jsplugin_setter *global_setter;

	/** called when there is a new (document) global context, which is roughly when there is a new document that the plugin can be used in */
	jsplugin_document_init *init;

	/** called when the (document) global context is garbage collected, which is some time after the document is destroyed */
	jsplugin_document_destroy *destroy;

	/** Called when an OBJECT element is created. */
	jsplugin_handle_object *handle_object;

	/** Called when new document is created (before init, which will not be called at all if this function returns zero) */
	jsplugin_allow_access *allow_access;

	/** Called by the garbage collector. GCMark any objects you want to keep */
	jsplugin_gc_trace *gc_trace;
};

/** this struct holds callbacks that the plugin can use to ask opera to create functions and objects for it, or evaluate ecmascript code. */
struct JSP_STRUCT_ALIGN jsplugin_callbacks {
	jsplugin_cb_create_function *create_function;
	jsplugin_cb_create_object *create_object;
	jsplugin_cb_eval *eval;
	jsplugin_cb_set_poll_interval *set_poll_interval;
	jsplugin_cb_call_function *call_function;
	jsplugin_cb_get_prop *get_property;
	jsplugin_cb_set_prop *set_property;
	jsplugin_cb_add_unload_listener *add_unload_listener;
	jsplugin_cb_set_attr_change_listener *set_attr_change_listener;
        jsplugin_cb_set_param_set_listener *set_param_set_listener;
	jsplugin_cb_add_tv_visual *add_tv_visual;
	jsplugin_cb_get_window_identifier *get_window_identifier;
	jsplugin_cb_resume *resume;
	jsplugin_cb_get_object_host *get_object_host;
	jsplugin_cb_gcmark *gcmark;
};

/** JSP status and type values */
enum {
	/* different types: */
	JSP_TYPE_UNDEFINED,          /* value is implicit */
	JSP_TYPE_OBJECT,             /* value is in u.object */
	JSP_TYPE_NATIVE_OBJECT,      /* value is in u.object */
	JSP_TYPE_STRING,             /* value is in u.string, opera will also set u.len, but will not expect it from the jsplugin */
	JSP_TYPE_NUMBER,             /* value is in u.number */
	JSP_TYPE_BOOLEAN,            /* value is in u.boolean */
	JSP_TYPE_NULL,               /* value is implicit */
	JSP_TYPE_EXPRESSION,         /* value is in u.string and will be eval()'ed before it's returned to the calling script */
	JSP_TYPE_STRING_WITH_LENGTH  /* value is in u.string, and the length of the string is in u.len */
};

enum {
	/* getter return values: */
	JSP_GET_ERROR,            /* generic error, throw exception */
	JSP_GET_VALUE,            /* slot found, do not cache value */
	JSP_GET_VALUE_CACHE,      /* slot found, cache the value */
	JSP_GET_NOTFOUND,         /* no slot by that name found */
	JSP_GET_NOMEM,            /* get failed due to OOM */
	JSP_GET_EXCEPTION,        /* get exception */
	JSP_GET_DELAYED           /* get is delayed, will restart get at a later time with resume() */
};

enum {
	/* putter return values: */
	JSP_PUT_ERROR,            /* generic error, throw exception */
	JSP_PUT_OK,               /* value was put */
	JSP_PUT_NOTFOUND,         /* no slot by that name found */
	JSP_PUT_NOMEM,            /* put failed due to OOM */
	JSP_PUT_READONLY,         /* put failed because slot is read-only */
	JSP_PUT_EXCEPTION         /* put exception */
};

enum {
	/* call/construct return values: */
	JSP_CALL_ERROR,           /* generic error, throw exception */
	JSP_CALL_VALUE,           /* call returned a value */
	JSP_CALL_NO_VALUE,        /* call returned no value */
	JSP_CALL_NOMEM,           /* call failed due to OOM */
	JSP_CALL_EXCEPTION,       /* call exception */
	JSP_CALL_DELAYED          /* call is delayed, will restart call at a later time with resume() */
};

enum {
	/* destructor return values: */
	JSP_DESTROY_NOMEM,        /* destruction failed due to OOM */
	JSP_DESTROY_OK            /* destruction succeeded */
};

enum {
	/* OBJECT handler return values: */
	JSP_OBJECT_ERROR,         /* OBJECT not handled */
	JSP_OBJECT_NOMEM,         /* OBJECT not handled due to OOM */
	JSP_OBJECT_OK,            /* OBJECT handled okay */
	JSP_OBJECT_VISUAL         /* OBJECT handled, request visualization */
};

enum {
	/* create_function/create_object callbacks return values: */
	JSP_CREATE_ERROR,         /* object/function not created */
	JSP_CREATE_NOMEM,         /* object/function not created due to OOM */
	JSP_CREATE_OK             /* object/function created okay */
};

enum {
	/* eval callback return and status values: */
	JSP_CALLBACK_ERROR,       /* eval failed */
	JSP_CALLBACK_NOMEM,       /* eval failed due to OOM */
	JSP_CALLBACK_OK           /* eval started/completed okay */
};

enum {
	/* set_poll_interval return values: */
	JSP_POLL_ERROR,           /* poll not started due to generic error */
	JSP_POLL_INVALID,         /* poll not started due to invalid arguments */
	JSP_POLL_NOMEM,           /* poll not started due to OOM */
	JSP_POLL_OK               /* poll started */
};

/** jsplugin_value holds a javascript value. "type" is one of

	JSP_TYPE_STRING
	JSP_TYPE_NUMBER
	JSP_TYPE_BOOLEAN
	JSP_TYPE_OBJECT
	JSP_TYPE_NATIVE_OBJECT
	JSP_TYPE_EXPRESSION
	JSP_TYPE_STRING_WITH_LENGTH

	and the corresponding field in u is set. u.string is encoded in UTF-8.

	When the type is JSP_TYPE_STRING, and the value is passed from the jsplugin
	to opera, u.string is assumed to be null terminated, and u.len is ignored.

	When the type is JSP_TYPE_STRING, and the value is passed from opera to
	the jsplugin, both u.string and u.len will be set. u.string will still
	be null-terminated, but may also contain null bytes at earlier positions.

	When the type is JSP_TYPE_STRING_WITH_LENGTH, and the value is passed from
	the jsplugin to opera, u.string may contain null bytes, and does not have
	to be null terminated. u.len must contain the length in bytes of the
	string, excluding the optional null terminator.

	Opera will never pass values with type set to
	JSP_TYPE_STRING_WITH_LENGTH to the jsplugin.
*/
struct JSP_STRUCT_ALIGN jsplugin_value {
	int type;
	union {
		const char *string;
		struct {
			const char *string;
			int len;
		} s;
		double number;
		int boolean;
		jsplugin_obj *object;
	} u;
};

#ifdef WIN32
# if _MSC_VER < 1300
#  pragma pack(pop)
# endif // _MSC_VER
#endif // WIN32

#endif // JSPLUGIN_H
