/* Copyright 2003 Opera Software ASA.   ALL RIGHTS RESERVED.

   Opera JavaScript plugin use case: file system access
   2003-01-24 / Lars T Hansen

   There is a constructor called 'File' in the global namespace:
      new File( pathname )     create file object, associate a path name with it

   Objects returned by 'File' have operations and properties:
      file.open(mode)         => nothing; exception on failure
          mode is "read" (open for reading, do not create, fail if does not exist)
                  "write" (open for writing, truncate, create if necessary)
                  "append" (open for writing at end, create if necessary)
      file.close()            => nothing
      file.write(string)      => nothing; writes datum as a quoted escaped string
      file.read()             => string;  reads the next quoted escaped string
      file.stat()             => object
      file.pathname           => string;  associated pathname (read-only)

      Read, write, and stat fail if file is not open.


   Commentary on this code:
      It's a bit of a hack.  All strings are unicode internally but I want
      to write them as 8-bit ASCII externally, so I just lop off the top 8
      bits (and throw away anything that comes out as zero in the process :-)
      Clearly this is inadequate for 'real' use; I imagine UTF-8 is an OK
      solution for most.  Perhaps a second argument to .open() can say how
      we want it done.

      Object serialization (pickling) would be neater than string
      data, but there are limits to how complicated I want to make
      this right now.

      I have neglected to implement putters on File and file objects that
      reject attempts to assign already-defined properties.  As a result
      there are no putters in this file at all.


   Commentary on the design of the JS plugin API:
      Right now the API mimics the standard JS semantics where functions are
      first-class values, that is, (new File("foo")).read actually returns the
      read function as an object.  This is desirable for symmetry, but it
      places a burden on the plugin author.

      An alternative strategy is to explicitly export a pointer to the read
      function from the plugin (through a mechanism that does not yet exist) 
      and let the plugin manager inside Opera automatically wrap this pointer
      in a function object, so that the plugin does not have to.  This would
      also increase security, because the read function could not be treated
      like a first-class object: the this_object would be guaranteed always
      to be of the right type.

      On the other hand, there is a pretty straightforward pattern
      that is used to create methods; it is easily mimicked.

      Another issue is that the pervasive use of unicode may be a hardship 
      for some.  */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "jsplugin.h"

static jsplugin_callbacks *opera_callbacks;
static jsplugin_cap cap;
//static const char *global_names[] = { "File", NULL };
//static struct jsplugin_slot global_name = { "File", 0 };
static char *global_names[] = { "File", NULL };

/** Global data used for file reading.  Read_buffer persists between calls to read(),
    so it can be used to return the read data in without worrying about storage
    management much. */
static char *read_buffer = NULL;
static char *read_buffer_limit;

/** Data structure associated with File objects */
struct filedata 
{
    char *pathname;   /* a private copy */
    char mode;            /* i for input or o for output */
    FILE *fp;             /* NULL if file is not open, otherwise pointer */
};

/* Prototypes; we like our programs top-down */
static void global_unload(void);
static jsplugin_getter global_getname;
static jsplugin_function File_constructor;
static jsplugin_getter file_getname;
static jsplugin_function file_open;
static jsplugin_function file_close;
static jsplugin_function file_write;
static jsplugin_function file_read;
static jsplugin_function file_stat;
static jsplugin_destructor file_gchook;
static int create_method( jsplugin_obj *this_obj, jsplugin_function *method, const char *sign, jsplugin_value *result );

# ifdef WIN32	// {
#  define DECLSPEC __declspec(dllexport)
# else
#  define DECLSPEC
# endif

DECLSPEC int jsplugin_capabilities( jsplugin_cap **result, jsplugin_callbacks *cbs )
{
    opera_callbacks = cbs;

    cap.global_names = global_names;
	cap.object_types = NULL;


 //   cap.object_type_names = NULL;
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
    free( read_buffer );
    read_buffer = NULL;
}

static int global_getname( jsplugin_obj *ctx, const char *name, jsplugin_value *result )
{
    int r;

    if (strcmp( name, "File" ) == 0)
    {
	jsplugin_obj *thefile;
	if ((r = opera_callbacks->create_function( ctx, 
						   NULL,
						   NULL, NULL,
						   File_constructor, "s",
						   0, 
						   &thefile )) < 0)
	    return JSP_GET_ERROR; /* FIXME, too generic */
	else
	{
	    result->type = JSP_TYPE_OBJECT;
	    result->u.object = thefile;
	    return JSP_GET_VALUE_CACHE;
	}
	
    }
    else
	return JSP_GET_NOTFOUND;
}

/** File_constructor is the function invoked when the script does 'new File(...)'. */

static int File_constructor( jsplugin_obj *global, jsplugin_obj *function_obj, int argc, jsplugin_value *argv, jsplugin_value *result )
{
    char *pathname;
    jsplugin_obj *theobj;
    struct filedata *filedata;
    int r;

    if (argc == 0)
		return JSP_CALL_ERROR;
    pathname = strdup( argv[0].u.string );

    if (pathname == NULL)
		return JSP_CALL_NOMEM;

    filedata = (struct filedata*)malloc( sizeof(filedata) );
    if (filedata == NULL)
    {
		free( pathname );
		return JSP_CALL_NOMEM;
    }
    if ((r = opera_callbacks->create_object( global == NULL ? function_obj : global, file_getname, NULL, file_gchook, &theobj )) < 0)
    {
		free( pathname );
		free( filedata );
		return JSP_CALL_NOMEM;
    }
    else
    {
		filedata->pathname = pathname;
		filedata->mode = ' ';
		filedata->fp = NULL;
		theobj->plugin_private = (void*)filedata;
		result->type = JSP_TYPE_OBJECT;
		result->u.object = theobj;
		return JSP_CALL_VALUE;
    }
}

static int file_getname( jsplugin_obj *this_obj, const char *name, jsplugin_value *result )
{
    if (strcmp( name, "open" ) == 0)
		return create_method( this_obj, file_open, "s", result );
    else if (strcmp( name, "close" ) == 0)
		return create_method( this_obj, file_close, "", result );
    else if (strcmp( name, "write" ) == 0)
		return create_method( this_obj, file_write, "s", result );
    else if (strcmp( name, "read" ) == 0)
		return create_method( this_obj, file_read, "", result );
    else if (strcmp( name, "stat" ) == 0)
		return create_method( this_obj, file_stat, "", result );
    else if (strcmp( name, "pathname" ) == 0)
    {
		result->type = JSP_TYPE_STRING;
		result->u.string = ((struct filedata*)(this_obj->plugin_private))->pathname;
		return JSP_GET_VALUE_CACHE;
    }
    else
		return JSP_GET_NOTFOUND;
}

static int create_method( jsplugin_obj* this_obj, jsplugin_function *method, const char *sign, jsplugin_value *result )
{
    int r;

    jsplugin_obj *thefunction;
    if ((r = opera_callbacks->create_function( this_obj, 
											   NULL, NULL,
											   method, NULL, sign,
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

static int file_open( jsplugin_obj *this_obj, jsplugin_obj *function_obj, int argc, jsplugin_value *argv, jsplugin_value *result )
{
    struct filedata *filedata = (struct filedata*)(this_obj->plugin_private);  /* actually unsafe */
    char *mode;

    if (argc == 0 || filedata->fp != NULL)
		return JSP_CALL_ERROR;

    if (strcmp( argv[0].u.string, "read") == 0)
		mode = "r", filedata->mode = 'i';
    else if (strcmp( argv[0].u.string, "write") == 0)
		mode = "w", filedata->mode = 'o';
	else if (strcmp( argv[0].u.string, "append") == 0)
		mode = "a", filedata->mode = 'o';
	else
		return JSP_CALL_ERROR;
	
    filedata->fp = fopen( filedata->pathname, mode );
    if (filedata->fp == NULL)
		return JSP_CALL_ERROR;

    return JSP_CALL_NO_VALUE;
}

static int file_close( jsplugin_obj *this_obj, jsplugin_obj *function_obj, int argc, jsplugin_value *argv, jsplugin_value *result  )
{
    struct filedata *filedata = (struct filedata*)(this_obj->plugin_private);  /* actually unsafe */

    if (filedata->fp != NULL)
    {
		fclose( filedata->fp );  /* ignore errors */
		filedata->fp = NULL;
    }
    return JSP_CALL_NO_VALUE;
}

static int file_write( jsplugin_obj *this_obj, jsplugin_obj *function_obj, int argc, jsplugin_value *argv, jsplugin_value *result )
{
    struct filedata *filedata = (struct filedata*)(this_obj->plugin_private);  /* actually unsafe */
    const char *s;

    if (argc == 0 || filedata->fp == NULL || filedata->mode != 'o')
		return JSP_CALL_ERROR;

    /* FIXME: ignores write errors */
    fputc( '"', filedata->fp );
    s = argv[0].u.string;
    /* NOTE!  Converts from unicode to 8bit. */
    while (*s)
    {
		char c = *s & 255;
		++s;
		switch (c)
		{
			case 0: 
				continue;
			case '"': case '\\':
				fputc( '\\', filedata->fp );
				break;
		}
		fputc( c, filedata->fp );
    }
    fputc( '"', filedata->fp );
    fputc( '\n', filedata->fp );
    return JSP_CALL_NO_VALUE;
}

static int file_read( jsplugin_obj *this_obj, jsplugin_obj *function_obj, int argc, jsplugin_value *argv, jsplugin_value *result )
{
    struct filedata *filedata = (struct filedata*)(this_obj->plugin_private);  /* actually unsafe */
    char *p;
    char c;

    if (argc != 0 || filedata->fp == NULL || filedata->mode != 'i')
	return JSP_CALL_ERROR;

    while ((c = fgetc(filedata->fp)) != '"' && c != EOF)
	;
    
    if (c == EOF)
    {
	result->type = JSP_TYPE_NULL;
	return JSP_CALL_VALUE;
    }

    if (read_buffer == NULL)
    {
	read_buffer = malloc(256);
	if (read_buffer == NULL)
	    return JSP_CALL_NOMEM;
	read_buffer_limit = read_buffer+256;
    }

	p = read_buffer;

    while ((c = fgetc(filedata->fp)) != EOF && c != '"')
    {
	if (p+1 == read_buffer_limit)
	{
	    /* Grow buffer */
	    char *new_buffer = malloc((read_buffer_limit-read_buffer)*2 );
	    if (new_buffer==NULL)
		return JSP_CALL_NOMEM;
	    memcpy( new_buffer, read_buffer, (read_buffer_limit-read_buffer) );
	    p = new_buffer + (p - read_buffer);
	    read_buffer_limit = new_buffer + (read_buffer_limit - read_buffer)*2;
	    free( read_buffer );
	    read_buffer = new_buffer;
	}

	if (c == '\\')
	{
	    c = fgetc(filedata->fp);
	    if (c == EOF)
		return JSP_CALL_ERROR;
	}
	*p++ = c;
    }
    *p = 0;

    result->type = JSP_TYPE_STRING;
    result->u.string = read_buffer;
    return JSP_CALL_VALUE;
}

static int file_stat( jsplugin_obj *this_obj, jsplugin_obj *function_obj, int argc, jsplugin_value *argv, jsplugin_value *result )
{
    /* Not yet implemented */
    return JSP_CALL_NO_VALUE;
}

static int file_gchook( jsplugin_obj *this_obj )
{
    struct filedata *filedata = (struct filedata*)(this_obj->plugin_private);  /* actually unsafe */

    if (filedata->fp != NULL)
		fclose( filedata->fp );
    free( filedata->pathname );
    return JSP_DESTROY_OK;
}

/* eof */

