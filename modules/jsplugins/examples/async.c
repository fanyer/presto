/* Copyright 2003-2010 Opera Software ASA.   ALL RIGHTS RESERVED.

   Opera JavaScript plugin example: async calls
   20010-01-28 / Jan Borsodi

   Note: This code requires the win32 API.

   The following functions are available in the global namespace:
      async_sleep( msecs )     halts the ES execution for a number of milliseconds, uses a timer to let ES suspend execution

   This plugin is meant to give an example on how to use the new async return values
   from get and function calls. The async_* functions will tell the ES engine
   to suspend. When the functions have performed their async behaviour they will
   resume the execution and the get and function calls are restarted.
   */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <Windows.h>

#include "jsplugin.h"

static jsplugin_callbacks *opera_callbacks;
static jsplugin_cap cap;
static char *global_names[] = { "async_sleep", NULL };

static UINT_PTR async_timer_id = 0;
static jsplugin_obj *async_ref = NULL;

/* Prototypes; we like our programs top-down */
static void global_unload(void);
static jsplugin_getter global_getname;
static jsplugin_function async_sleep;
static jsplugin_destructor async_data_destruct;
static int create_function( jsplugin_obj *ctx, jsplugin_function *name, const char *sign, jsplugin_value *result );

void CALLBACK async_TimerProc ( HWND hParent, UINT uMsg, UINT uEventID, DWORD dwTimer );

struct async_data
{
	unsigned msecs;
};

# ifdef WIN32	// {
#  define EXPORT __declspec(dllexport)
# else
#  define EXPORT
# endif

EXPORT int jsplugin_capabilities( jsplugin_cap **result, jsplugin_callbacks *cbs )
{
    opera_callbacks = cbs;

    cap.global_names = global_names;
	cap.object_types = NULL;

    cap.global_getter = global_getname;
    cap.global_setter = NULL;
    cap.init = NULL;
    cap.destroy = NULL;
	cap.handle_object = NULL;
    *result = &cap;

    return 0;
}

static void global_unload()
{
}

static int global_getname( jsplugin_obj *ctx, const char *name, jsplugin_value *result )
{
    if (strcmp( name, "async_sleep" ) == 0)
		return create_function( ctx, async_sleep, "n", result );
    else
		return JSP_GET_NOTFOUND;
}

static int create_function( jsplugin_obj* ctx, jsplugin_function *name, const char *sign, jsplugin_value *result )
{
    int r;

    jsplugin_obj *thefunction;
    if ((r = opera_callbacks->create_function( ctx, 
											   NULL, NULL,
											   name, NULL, sign,
											   NULL, 
											   &thefunction )) < 0)
		return JSP_GET_ERROR;  /* FIXME; too generic */
    else
    {
		result->type = JSP_TYPE_OBJECT;
		result->u.object = thefunction;
		return JSP_GET_VALUE_CACHE;
    }  
}

void CALLBACK async_TimerProc ( HWND hParent, UINT uMsg, UINT uEventID, DWORD dwTimer )
{
	if (uEventID != async_timer_id)
		return;
	KillTimer(NULL, uEventID); // Kill timer to avoid repetition
	opera_callbacks->resume(async_ref);
}

static int async_sleep( jsplugin_obj *ctx, jsplugin_obj *function_obj, int argc, jsplugin_value *argv, jsplugin_value *result )
{
	struct async_data *data = NULL;
	unsigned msecs;

	if (argc >= 0)
	{
		if (argc == 0)
			return JSP_CALL_ERROR;
		if (argv[0].type != JSP_TYPE_NUMBER)
			return JSP_CALL_ERROR;

		msecs = (unsigned)argv[0].u.number;
		data = (struct async_data *)malloc( sizeof(struct async_data) );
		if (!data)
			return JSP_CALL_NOMEM;
		data->msecs = msecs;

		// Create an object, it will be available when the function restarts
		if (opera_callbacks->create_object(function_obj, NULL, NULL, async_data_destruct, &result->u.object) != JSP_CREATE_OK)
		{
			free(data);
			return JSP_CALL_ERROR;
		}
		result->type = JSP_TYPE_OBJECT;
		result->u.object->plugin_private = (void *)data;

		async_ref = function_obj;
		if ((async_timer_id = SetTimer(NULL, 1, msecs, async_TimerProc)) == 0)
		{
			free(data);
			return JSP_CALL_ERROR;
		}

		return JSP_CALL_DELAYED;
	}
	if (result->type != JSP_TYPE_OBJECT)
		return JSP_CALL_ERROR;
	data = (struct async_data *)result->u.object->plugin_private;
	if (!data)
		return JSP_CALL_ERROR;

	result->type = JSP_TYPE_NUMBER;
	result->u.number = data->msecs;
	return JSP_CALL_VALUE;
}

static int async_data_destruct( jsplugin_obj *obj )
{
    struct async_data *data = (struct async_data *)(obj->plugin_private);
	free(data);
    return JSP_DESTROY_OK;
}

/* eof */

