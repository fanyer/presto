/**
   Copyright (C) 1995-2008 Opera Software AS. All rights reserved.  

   THIS FILE CONTAINS EXAMPLE CODE DESCRIBING HOW TO USE THE INTERFACE
   ("API") TO JSPLUGIN, THE MODULAR JAVASCRIPT PLUGIN TECHNOLOGY FOR
   THE OPERA BROWSER VERSIONS 6.x, 7.x, 8.x, and 9.x LAUNCHED BY OPERA
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
   Opera JavaScript plugin use case: /proc/ interfacing
   2007-03-08 / rikard@opera.com

   There is a constructor called 'SlashProc' in the global namespace:
      new SlashProc()     create /proc/ object

   Objects returned by 'SlashProc' have the following properties:
      SlashProc.uptime
      SlashProc.loadavg
      SlashProc.meminfo
      SlashProc.cpuinfo
	  SlashProc.self_[X], where X is what you can find under /proc/self/ . If X can't be read as a file,
	                      JSP_GET_ERROR will be returned.

	  They all return the first 2048 bytes read from their corresponding /proc/[property].
*/

#ifdef __linux__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "jsplugin.h"

static jsplugin_callbacks *opera_callbacks;
static jsplugin_cap cap;

static const char* global_names[] = { "SlashProc", NULL };

static jsplugin_getter     global_getname;
static jsplugin_function   SlashProc_constructor;
static jsplugin_getter     slp_getname;
static jsplugin_destructor slp_gchook;
static int create_method( jsplugin_obj *this_obj, jsplugin_function *method, const char *sign, jsplugin_value *result );

/* used for private data */
struct slpdata
{
    char buf[2048]; /* ARRAY OK 2010-11-18 lasse */
};

/* Someone may want to compile and link under on a platform where DECLSPEC is needed.
 * Even though the the plugin is highly platform specific and won't work under
 * eg Windows, this can be modified for other platforms.
 * */
# ifdef WIN32	// {
#  define DECLSPEC __declspec(dllexport)
# else
#  define DECLSPEC
# endif

/* Exported capabilities function */
DECLSPEC int jsplugin_capabilities( jsplugin_cap **result, jsplugin_callbacks *cbs )
{
	fprintf(stderr, "probing capabilities\n");
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

/* Answers to requests in the global namespace */
static int global_getname( jsplugin_obj *ctx, const char *name, jsplugin_value *result )
{
	fprintf(stderr, "global_getname\n");
	int r;

	if (strcmp( name, "SlashProc" ) == 0)
	{
		jsplugin_obj *theupt;
		r = opera_callbacks->create_function(ctx, /* reference object, used for environment etc */
				NULL, NULL, /* getter/setter, not needed for constructor */
				NULL, /* not a regular function call... */
				SlashProc_constructor, /* ... but a constructor */
				"", /* signature is empty */
				NULL, /* no destructor necessary */
				NULL, /* no GC trace hook necessary */
				&theupt); /* here we get the function object */

		if (r < 0)
		{
			return JSP_GET_ERROR; /* FIXME, too generic */
		}
		else
		{
			result->type = JSP_TYPE_OBJECT;
			result->u.object = theupt;
			return JSP_GET_VALUE_CACHE;
		}
	}
	else
	{
		return JSP_GET_NOTFOUND;
	}
}

/** SlashProc_constructor is the function invoked when the script does 'new SlashProc(...)'. */
static int SlashProc_constructor( jsplugin_obj *global, jsplugin_obj *function_obj, int argc, jsplugin_value *argv, jsplugin_value *result )
{
    jsplugin_obj *theobj;
    int r;
    struct slpdata* priv_data;

    if (argc != 0)
    {
	    return JSP_CALL_ERROR;
    }

    r = opera_callbacks->create_object(function_obj, slp_getname, NULL, slp_gchook, NULL, &theobj);
    if (r < 0)
    {
		return JSP_CALL_NOMEM;
    }
    else
    {
		result->type = JSP_TYPE_OBJECT;
		result->u.object = theobj;
		theobj->plugin_private = NULL;

		/* allocate private data struct */
		priv_data = (struct slpdata*)malloc(sizeof(struct slpdata));
		if (priv_data == NULL)
		{
			return JSP_CALL_NOMEM;
		}
		theobj->plugin_private = priv_data;

		return JSP_CALL_VALUE;
    }
}

static int slp_getname( jsplugin_obj *this_obj, const char *name, jsplugin_value *result )
{
	int is_self = !strncmp(name, "self_", 5) && strlen(name) > 5;
	/* fprintf(stderr, "getname: %s, is_self: %d\n", name, is_self); */

	if (strcmp(name, "uptime") == 0 ||
		strcmp(name, "loadavg") == 0 ||
		strcmp(name, "meminfo") == 0 ||
		strcmp(name, "cpuinfo") == 0 ||
		is_self) /* allow free-form access to /proc/self as well */
	{
			char filename[1024]; /* ARRAY OK 2010-11-18 lasse */
			if (is_self)
				sprintf(filename, "/proc/self/%s", &name[5]);
			else
				sprintf(filename, "/proc/%s", name);

			result->type = JSP_TYPE_STRING;
			char* buf = ((struct slpdata*)(this_obj->plugin_private))->buf;
			FILE* fd = fopen(filename, "r");
			if (!fd)
				return JSP_GET_ERROR;
			int cc = fread(buf, 1, 2048, fd);
			buf[cc] = '\0';
			fclose(fd);
			if (!cc)
				return JSP_GET_ERROR;
			result->u.string = buf;
			return JSP_GET_VALUE;
	}
	else
		return JSP_GET_NOTFOUND;
}

static int create_method( jsplugin_obj* this_obj, jsplugin_function *method, const char *sign, jsplugin_value *result )
{
	int r;

	jsplugin_obj *thefunction;
	r = opera_callbacks->create_function( this_obj, slp_getname, NULL, method, method, sign, NULL, NULL, &thefunction );
	if (r < 0)
	{
		return JSP_GET_ERROR;
	}
	else
	{
		result->type = JSP_TYPE_OBJECT;
		result->u.object = thefunction;
		return JSP_GET_VALUE_CACHE;
	}
}

static int slp_gchook( jsplugin_obj *this_obj )
{
    struct slpdata *priv_data = (struct slpdata*)(this_obj->plugin_private);
    free(priv_data);
    printf("Uptime object was destroyed\n");
    return JSP_DESTROY_OK;
}

#endif /* __linux__ */

/* eof */
