/**
   Copyright (C) 1995-2010 Opera Software AS. All rights reserved.  

   THIS FILE CONTAINS EXAMPLE CODE DESCRIBING HOW TO USE THE INTERFACE
   ("API") TO JSPLUGIN, THE MODULAR JAVASCRIPT PLUGIN TECHNOLOGY FOR
   THE OPERA 6.0 BROWSER AND THE OPERA 7.0 BROWSER LAUNCHED BY OPERA
   SOFTWARE ASA ("OPERA"). THE API TOGETHER WITH DOCUMENTATION
   AND EXAMPLE CODE IS PUBLICISED BY OPERA, AND OPERA IS WILLING TO
   PERMIT USE OF IT BY YOU ("LICENSEE"), ONLY UPON THE CONDITION THAT
   YOU ACCEPT ALL OF THE TERMS CONTAINED IN A SEPARATE API LICENSE
   AGREEMENT ("AGREEMENT"). PLEASE READ THE TERMS AND CONDITIONS OF
   THIS LICENSE CAREFULLY. BY READING, COPYING OR USING THE API IN ANY
   WAY, YOU ACCEPT THE TERMS AND CONDITIONS OF THIS AGREEMENT. IF YOU
   ARE NOT WILLING TO BE BOUND, YOU ARE NOT ALLOWED TO STUDY THIS API
   OR MAKE USE OF IT IN ANY WAY.
*/

/*
   Opera JavaScript plugin use case: ATVEF trigger receiver OBJECT
   2003-03-10 / rikard@opera.com

   This plugin can be instantiated by eg
   <OBJECT TYPE="application/tve-trigger" ID="triggerReceiverObj"></OBJECT>
   (only TYPE is mandatory, but ID may be pretty useful)

   It has the properties that are prescribed by the ATVEF standard
   (see http://www.atvef.com/library/spec1_1a.html) :

   enabled
   sourceId
   releasable
   backChannel
   contentLevel

   Note that the implementation of these are highly system dependent,
   so in this example I've chosen to only include stubs. Implementing
   the last part should be straightforward, though.

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../jsplugin.h"

static jsplugin_cap cap;

static const char* global_names[] = { NULL }; /* no global names */
static const char* object_types[] = { "application/tve-trigger", NULL }; /* only one MIME type */

/* Someone may want to compile and link under W*ndows */
# ifdef WIN32
#  define DECLSPEC __declspec(dllexport)
# else
#  define DECLSPEC
# endif

static struct jsplugin_callbacks* opera_callbacks;

/* private plugin data, "state" if you will */
typedef struct
{
	int enabled;
	int releasable;
	char* backChannel;
} trr_data;

/* this function is called back when the OBJECT is inserted into the DOM structure */
void trr_inserted(jsplugin_obj *obj)
{
	printf("object inserted\n");
}

/* this function is called back when the OBJECT is taken out of the DOM structure */
void trr_removed(jsplugin_obj *obj)
{
	printf("object removed\n");
}

/* this function is called back when the eval in trr_simulateTrigger notifies us about its result */
void trr_simulateTrigger_result(int status, jsplugin_value *value)
{
	printf("got the result from trr_simulateTrigger eval: %d\n", status);
}

/* native implementation of simulateTrigger. this is not in the ATVEF
   standard, it is only here to test the eval functionality. the eval
   functionality is the proposed way of executing the ECMAScript that
   can be received in the triggers.

   this implementation only puts up a javascript alert.
*/
int trr_simulateTrigger(jsplugin_obj *this_obj, jsplugin_obj *function_obj,
						int argc, jsplugin_value *argv, jsplugin_value *result)
{
	printf("simulateTrigger\n");
	opera_callbacks->eval(function_obj, "alert('got a (simulated) trigger');", trr_simulateTrigger_result, NULL);
	return JSP_CALL_NO_VALUE;
}

/* getter for the properties that the OBJECT has */
int trr_getter(jsplugin_obj *obj, const char *name, jsplugin_value *result)
{
	printf("trr_getter: %s\n", name);

	if (strcmp(name, "enabled") == 0)
	{
		result->type = JSP_TYPE_BOOLEAN;
		result->u.boolean = ((trr_data*)obj->plugin_private)->enabled;
		printf("get_value: %d\n", ((trr_data*)obj->plugin_private)->enabled);
		return JSP_GET_VALUE;
	}
	else if (strcmp(name, "sourceId") == 0)
	{
		result->type = JSP_TYPE_STRING;
		result->u.string = "example sourceId";
		return JSP_GET_VALUE;
	}
	else if (strcmp(name, "releasable") == 0)
	{
		result->type = JSP_TYPE_BOOLEAN;
		result->u.boolean = ((trr_data*)obj->plugin_private)->releasable;
		return JSP_GET_VALUE;
	}
	else if (strcmp(name, "backChannel") == 0)
	{
		result->type = JSP_TYPE_STRING;
		result->u.string = ((trr_data*)obj->plugin_private)->backChannel;
		return JSP_GET_VALUE;
	}
	else if (strcmp(name, "contentLevel") == 0)
	{
		result->type = JSP_TYPE_NUMBER;
		result->u.number = 1.0;
		return JSP_GET_VALUE_CACHE;
	}
	else if (strcmp(name, "simulateTrigger") == 0)
	{
		jsplugin_obj *simulateTriggerFunctionObj;
		int r = opera_callbacks->create_function(obj, /* reference object, used for environment etc */
												 NULL, NULL, /* getter/setter, not needed for this call */
												 trr_simulateTrigger, /* a regular function call... */
												 NULL, /* ... not a constructor */
												 "", /* signature is empty */
												 NULL, /* no destructor necessary */
												 NULL, /* no GC hook necessary */
												 &simulateTriggerFunctionObj); /* here we get the function object */

		if (r < 0)
		{
			return JSP_GET_ERROR; /* FIXME, too generic */
		}
		else
		{
			result->type = JSP_TYPE_OBJECT;
			result->u.object = simulateTriggerFunctionObj;
			return JSP_GET_VALUE_CACHE;
		}
	}

	return JSP_GET_NOTFOUND;
}

/* setter for the properties on the OBJECT */
int trr_setter(jsplugin_obj *obj, const char *name, jsplugin_value *value)
{
	printf("trr_setter: %s\n", name);

	if (strcmp(name, "enabled") == 0)
	{
		if (value->type == JSP_TYPE_BOOLEAN)
		{
			((trr_data*)obj->plugin_private)->enabled = value->u.boolean;
			printf("put ok: %d\n", ((trr_data*)obj->plugin_private)->enabled);
			return JSP_PUT_OK;
		}
		printf("put error\n");
		return JSP_PUT_ERROR;
	}
	else if (strcmp(name, "sourceId") == 0)
	{
		return JSP_PUT_READONLY;
	}
	else if (strcmp(name, "releasable") == 0)
	{
		if (value->type == JSP_TYPE_BOOLEAN)
		{
			value->u.boolean = ((trr_data*)obj->plugin_private)->releasable;
			printf("put ok: %d\n", ((trr_data*)obj->plugin_private)->releasable);
			return JSP_PUT_OK;
		}
		printf("put error\n");
		return JSP_PUT_ERROR;
	}
	else if (strcmp(name, "backChannel") == 0)
	{
		return JSP_PUT_READONLY;
	}
	else if (strcmp(name, "contentLevel") == 0)
	{
		return JSP_PUT_READONLY;
	}

	return JSP_PUT_NOTFOUND;
}

/* destructor */
int trr_destructor(jsplugin_obj *obj)
{
	trr_data* privdata = (trr_data*)obj->plugin_private;
	printf("destructor\n");
	free(privdata->backChannel);
	free(privdata);
}

/* called back when an OBJECT that the plugin claims to support is
   created. note that the plugin also needs to have the appropriate
   access policy set up, or it won't be taken into account at all. */
int handle_object(jsplugin_obj *global_obj, jsplugin_obj *object_obj, int attrs_count, jsplugin_attr *attrs,
				  jsplugin_getter **getter, jsplugin_setter **setter, jsplugin_destructor **destructor,
				  jsplugin_notify **inserted, jsplugin_notify **removed)
{
	int i = 0;
	trr_data* priv_data;

	printf("handle_object, global_obj: %p, object_obj: %p\n", global_obj, object_obj);

	priv_data = (trr_data*)malloc(sizeof(priv_data));

	if (priv_data == NULL)
	{
		return JSP_OBJECT_NOMEM;
	}

	priv_data->enabled = 1;
	priv_data->releasable = 0;
	priv_data->backChannel = strdup("permanent");

	if (priv_data->backChannel == NULL)
	{
		free(priv_data);
		return JSP_OBJECT_NOMEM;
	}

	object_obj->plugin_private = (void*) priv_data;

	/* take a look at the attributes */

	while (i < attrs_count)
	{
		printf("attribute %d has name %s and value %s\n", i, attrs[i].name, attrs[i].value);
		i++;
	}

	/* set callbacks */
	*getter = &trr_getter;
	*setter = &trr_setter;
	*inserted = &trr_inserted;
	*destructor = &trr_destructor;
	*removed = &trr_removed;

	return JSP_OBJECT_OK;
}

/* the global plugin hook. */
DECLSPEC int jsplugin_capabilities(jsplugin_cap **result, jsplugin_callbacks *c)
{
	cap.global_names = global_names;
	cap.object_types = object_types;

	cap.global_getter = NULL;
	cap.global_setter = NULL;

	cap.init = NULL;
	cap.destroy = NULL;

	cap.handle_object = handle_object;

	*result = &cap;

	opera_callbacks = c;

	return 0;
}
