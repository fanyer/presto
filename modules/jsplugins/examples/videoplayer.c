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
   Opera JavaScript plugin use case: video player interfacing
   2003-01-29 / rikard@opera.com

   There is a constructor called 'VideoPlayer' in the global namespace:
      new VideoPlayer()     create videoplayer object

   Objects returned by 'VideoPlayer' have operations and properties:
      VideoPlayer.tune(stream)    => nothing; exception on failure
      VideoPlayer.play            => nothing; exception on failure
      VideoPlayer.stop            => nothing; exception on failure
      VideoPlayer.currentstream   => string; the current stream (read-only)
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "jsplugin.h"

static jsplugin_callbacks *opera_callbacks;
static jsplugin_cap cap;

static const char* global_names[] = { "VideoPlayer", NULL };

static jsplugin_getter     global_getname;
static jsplugin_function   VideoPlayer_constructor;
static jsplugin_getter     vp_getname;
static jsplugin_function   vp_tune;
static jsplugin_function   vp_play;
static jsplugin_function   vp_stop;
static jsplugin_destructor vp_gchook;
static int create_method( jsplugin_obj *this_obj, jsplugin_function *method, const char *sign, jsplugin_value *result );

/* used for private data */
struct vpdata
{
    char *streamname;
};

/* Someone may want to compile and link under W*ndows */
# ifdef WIN32	// {
#  define DECLSPEC __declspec(dllexport)
#  define strdup _strdup
# else
#  define DECLSPEC
# endif

/* Exported capabilities function */
DECLSPEC int jsplugin_capabilities( jsplugin_cap **result, jsplugin_callbacks *cbs )
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

/* Answers to requests in the global namespace */
static int global_getname( jsplugin_obj *ctx, const char *name, jsplugin_value *result )
{
    int r;

    if (strcmp( name, "VideoPlayer" ) == 0)
    {
		jsplugin_obj *thevp;
		r = opera_callbacks->create_function(ctx, /* reference object, used for environment etc */
											 NULL, NULL, /* getter/setter, not needed for constructor */
											 NULL, /* not a regular function call... */
											 VideoPlayer_constructor, /* ... but a constructor */
											 "", /* signature is empty */
											 NULL, /* no destructor necessary */
											 NULL, /* no GC trace hook */
											 &thevp); /* here we get the function object */

		if (r < 0)
		{
			return JSP_GET_ERROR; /* FIXME, too generic */
		}
		else
		{
			result->type = JSP_TYPE_OBJECT;
			result->u.object = thevp;
			return JSP_GET_VALUE_CACHE;
		}
    }
    else
	{
		return JSP_GET_NOTFOUND;
	}
}

/** VideoPlayer_constructor is the function invoked when the script does 'new VideoPlayer(...)'. */
static int VideoPlayer_constructor( jsplugin_obj *global, jsplugin_obj *function_obj, int argc, jsplugin_value *argv, jsplugin_value *result )
{
    jsplugin_obj *theobj;
    int r;
	struct vpdata* priv_data;

    if (argc != 0)
	{
		return JSP_CALL_ERROR;
	}

    r = opera_callbacks->create_object(function_obj, vp_getname, NULL, vp_gchook, NULL, &theobj);
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
		priv_data = (struct vpdata*)malloc(sizeof(struct vpdata));
		if (priv_data == NULL)
		{
			return JSP_CALL_NOMEM;
		}
		priv_data->streamname = NULL;
		theobj->plugin_private = priv_data;

		return JSP_CALL_VALUE;
    }
}

static int vp_getname( jsplugin_obj *this_obj, const char *name, jsplugin_value *result )
{
    if (strcmp( name, "tune" ) == 0)
	{
		return create_method( this_obj, vp_tune, "s", result );
	}
    else if (strcmp( name, "play" ) == 0)
	{
		return create_method( this_obj, vp_play, "", result );
	}
    else if (strcmp( name, "stop" ) == 0)
	{
		return create_method( this_obj, vp_stop, "", result );
	}
    else if (strcmp( name, "currentStream" ) == 0)
    {
		result->type = JSP_TYPE_STRING;
		result->u.string = ((struct vpdata*)(this_obj->plugin_private))->streamname;
		return JSP_GET_VALUE_CACHE;
    }
    else
		return JSP_GET_NOTFOUND;
}

static int create_method( jsplugin_obj* this_obj, jsplugin_function *method, const char *sign, jsplugin_value *result )
{
    int r;

    jsplugin_obj *thefunction;
    r = opera_callbacks->create_function( this_obj,
										  vp_getname, NULL,
										  method, method, sign,
										  NULL,
										  NULL,
										  &thefunction );
	if (r < 0)
	{
		return JSP_GET_ERROR;  /* FIXME; too generic */
	}
    else
    {
		result->type = JSP_TYPE_OBJECT;
		result->u.object = thefunction;
		return JSP_GET_VALUE_CACHE;
    }
}

static int vp_tune( jsplugin_obj *this_obj, jsplugin_obj *function_obj, int argc, jsplugin_value *argv, jsplugin_value *result )
{
	struct vpdata* priv_data;

    if (argc != 1)
	{
		return JSP_CALL_ERROR;
	}

	if (this_obj == NULL)
	{
		/* this can happen under certain circumstances (wrong ecmascript code) */
		return JSP_CALL_ERROR;
	}

	printf("tuning to stream: %s\n", argv[0].u.string);

	priv_data = (struct vpdata*)this_obj->plugin_private;
	free(priv_data->streamname);
	priv_data->streamname = strdup(argv[0].u.string);

	if (priv_data->streamname == NULL && argv[0].u.string != NULL)
	{
		return JSP_CALL_NOMEM;
	}

	return JSP_CALL_NO_VALUE;
}

static int vp_play( jsplugin_obj *this_obj, jsplugin_obj *function_obj, int argc, jsplugin_value *argv, jsplugin_value *result )
{
    if (argc != 0)
		return JSP_CALL_ERROR;

	printf("playing stream\n");

	result->type = JSP_TYPE_BOOLEAN;
	result->u.boolean = 1;
    return JSP_CALL_NO_VALUE;
}

static int vp_stop( jsplugin_obj *this_obj, jsplugin_obj *function_obj, int argc, jsplugin_value *argv, jsplugin_value *result )
{
    if (argc != 0)
		return JSP_CALL_ERROR;

	printf("stopping stream\n");

    return JSP_CALL_NO_VALUE;
}

static int vp_gchook( jsplugin_obj *this_obj )
{
    struct vpdata *priv_data = (struct vpdata*)(this_obj->plugin_private);
    free(priv_data->streamname);
	free(priv_data);
	printf("videoplayer was destroyed\n");
    return JSP_DESTROY_OK;
}

/* eof */
