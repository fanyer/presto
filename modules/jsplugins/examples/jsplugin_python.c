#include "jsplugin.h"

#include <string.h>
#include <stdlib.h>

#include "Python.h"

#ifndef WIN32
# define INLINE inline
# define DECLSPEC
#else
# define INLINE
# define DECLSPEC __declspec(dllexport)
#endif

#ifdef WIN32
/* Implementation of strsep, since one didn't seem to exist. */
char *strsep(char **stringp, const char *delim)
{
	char *s, *p;

	if (!stringp)
		return 0;
	
	s = p = *stringp;
	while (*p && !strchr(delim, *p))
		++p;

	if (*p)
		*p = 0;

	*stringp = p + 1;

	return s;
}
#endif


/* Global callbacks structure. */
static struct jsplugin_callbacks *cbs;


/* Global use count (uses of this plugin, controls calls to 
   Py_Initialize and Py_Finalize. */
static int use_count = 0;


/* Document/Python/plugin context structure.  Created per document by 
   global_init and global_destroy.  Holds and owns references to some
   Python modules and functions, a temporary reference to any string
   converted using import_object and a Python dictionary used to map 
   PyObject* to jsplugin_obj*, so that a Python object will be 
   represented using the same jsplugin_obj every time it is imported 
   into Opera.

   The objects are reference counted; each jsplugin_python_obj owns a
   reference, one is owned globally and released by global_destroy. */
typedef struct
{
  unsigned refcount;
  
  PyObject *builtin;
  PyObject *builtin__int;
  PyObject *builtin__eval;
  PyObject *imported_string;

  PyObject *mappings;
} jsplugin_python_ctx;


/* Structure used as the plugin_private part of jsplugin_obj.  Holds
   (and owns) a reference to a Python object and a pointer to the 
   document/Python/plugin context. */
typedef struct
{
  PyObject *obj;
  jsplugin_python_ctx *ctx;
} jsplugin_python_obj;


/* Convenience: jsplugin_obj* -> PyObject* */
static INLINE PyObject *
OBJ (jsplugin_obj *obj)
{
  return ((jsplugin_python_obj *) obj->plugin_private)->obj;
}


/* Convenience: jsplugin_obj* -> jsplugin_python_ctx* */
static INLINE jsplugin_python_ctx *
CTX (jsplugin_obj *obj)
{
  return ((jsplugin_python_obj *) obj->plugin_private)->ctx;
}


static void acquire_python_ctx (jsplugin_python_ctx *ctx);
static void release_python_ctx (jsplugin_python_ctx *ctx);


/* Create the plugin_private part of an object.  'V' is assumed to
   contain an object created with callbacks->create_object of 
   callbacks->create_function, 'ctx' is the document/Python/plugin
   context to use and 'obj' is Python object.  Steals a reference to
   'obj'. */
static void
setup_python_obj (jsplugin_value *v, jsplugin_python_ctx *ctx,
                  PyObject *obj)
{
  jsplugin_python_obj *o;
  
  o = PyMem_Malloc (sizeof (jsplugin_python_obj));
  o->obj = obj;
  o->ctx = ctx;
  acquire_python_ctx (ctx);
  
  v->u.object->plugin_private = o;
}


/* Destroys the plugin_private part of 'obj'.  Releases a reference to
   the Python object, as well as to the document/Python/plugin
   context. */
static void
destroy_python_obj (jsplugin_obj *obj)
{
  jsplugin_python_obj *o;
  
  o = (jsplugin_python_obj *) obj->plugin_private;
  Py_DECREF (o->obj);
  release_python_ctx (o->ctx);
  
  PyMem_Free (o);
}


/* Find a previous jsplugin_obj used to represent 'obj'.  Returns NULL
   if one is not found. */
static jsplugin_obj *
find_mapping (jsplugin_python_ctx *ctx, PyObject *obj)
{
  PyObject *key, *value;
  
  key = PyInt_FromLong ((long) obj);
  value = PyDict_GetItem (ctx->mappings, key);

  if (value)
    return (jsplugin_obj *) PyInt_AsLong (value);
  else
    return NULL;
}


/* Store a PyObject* -> jsplugin_obj* mapping for 'o1' and 'o2', so 
   that the jsplugin_obj can be reused later through 'find_mapping'. */
static void
add_mapping (jsplugin_python_ctx *ctx, PyObject *o1, jsplugin_obj *o2)
{
  PyObject *key, *value;

  key = PyInt_FromLong ((long) o1);
  value = PyInt_FromLong ((long) o2);

  PyDict_SetItem (ctx->mappings, key, value);
}


/* Delete a PyObject* -> jsplugin_obj* mapping. */
static void
delete_mapping (jsplugin_python_ctx *ctx, PyObject *obj)
{
  PyObject *key;

  key = PyInt_FromLong ((long) obj);

  PyDict_DelItem (ctx->mappings, key);
}


/* Create a document/Python/plugin context. */
static jsplugin_python_ctx *
setup_python_ctx ()
{
  jsplugin_python_ctx *ctx = PyMem_Malloc (sizeof (jsplugin_python_ctx));

  ctx->builtin = PyImport_ImportModule ("__builtin__");
  if (!ctx->builtin)
    {
      PyErr_Print ();
      PyMem_Free (ctx);
      return NULL;
    }

  ctx->refcount = 0;
  
  ctx->builtin__int = PyObject_GetAttrString (ctx->builtin, "int");
  Py_INCREF (ctx->builtin__int);
  
  ctx->builtin__eval = PyObject_GetAttrString (ctx->builtin, "eval");
  Py_INCREF (ctx->builtin__eval);

  ctx->imported_string = 0;
  ctx->mappings = PyDict_New ();
  
  return ctx;
}


/* Acquire a reference to a context object. */
static void
acquire_python_ctx (jsplugin_python_ctx *ctx)
{
  ++ctx->refcount;
}


/* Releases a reference to a context object.  Frees the context if this
   was the last reference. */
static void
release_python_ctx (jsplugin_python_ctx *ctx)
{
  if (--ctx->refcount == 0)
    {
      Py_DECREF (ctx->builtin);
      Py_DECREF (ctx->builtin__int);
      Py_DECREF (ctx->builtin__eval);
	  
	  if (ctx->imported_string)
		  Py_DECREF (ctx->imported_string);

      Py_DECREF (ctx->mappings);

      PyMem_Free (ctx);
  
      if (--use_count == 0)
        Py_Finalize ();
    }
}


static int
object_getter (jsplugin_obj *this, const char *n, jsplugin_value *r);

static int
object_setter (jsplugin_obj *this, const char *n, jsplugin_value *v);

static int
object_call (jsplugin_obj *this, jsplugin_obj *func, int argc,
             jsplugin_value *argv, jsplugin_value *r);

static int
object_construct (jsplugin_obj *this, jsplugin_obj *func, int argc,
                  jsplugin_value *argv, jsplugin_value *r);

static int
object_destroy (jsplugin_obj *this);

static int
list_getter (jsplugin_obj *this, const char *n, jsplugin_value *r);

static int
list_setter (jsplugin_obj *this, const char *n, jsplugin_value *v);

static int
tuple_getter (jsplugin_obj *this, const char *n, jsplugin_value *r);

static int
tuple_setter (jsplugin_obj *this, const char *n, jsplugin_value *v);


/* Imports a Python object into Opera by converting it to an
   appropriate primitive type (null, number or string) or wraps it
   in a python_obj.   

   Upon entry, we own a reference to 'obj'.  It will be released by
   this function, unless the object is wrapped as an object, in which
   case it will be stolen by the jsplugin_obj created; or the object
   is converted to a string, in which case we temporarily borrow the
   reference in order to be able to return a pointer to the string's
   internal buffer.  Either way, this function effectively steals the
   reference. */
static int
import_object (jsplugin_obj *ref, PyObject *obj, jsplugin_value *r)
{
  int result;

  if (obj == Py_None)
    {
      Py_DECREF (obj);
      r->type = JSP_TYPE_NULL;
      return JSP_GET_VALUE;
    }
  else if (PyInt_Check (obj))
    {
      r->type = JSP_TYPE_NUMBER;
      r->u.number = (double) PyInt_AsLong (obj);
      Py_DECREF (obj);
      return JSP_GET_VALUE;
    }
  else if (PyLong_Check (obj))
    {
      r->type = JSP_TYPE_NUMBER;
      r->u.number = PyLong_AsDouble (obj);
      Py_DECREF (obj);
      return JSP_GET_VALUE;
    }
  else if (PyFloat_Check (obj))
    {
      r->type = JSP_TYPE_NUMBER;
      r->u.number = PyFloat_AsDouble (obj);
      Py_DECREF (obj);
      return JSP_GET_VALUE;
    }
  else if (PyString_Check (obj))
    {
      /* PyString_AsString returns a pointer to an internal buffer, so
         we can't decrease the reference count of the string here
         (unless we copy the value returned by PyString_AsString too).

         Instead, we hold on to a reference to it for now, and store
         pointer to it in 'ctx->imported_string'.  If we already have
         such a reference, we decrease the reference count of the old
         one and forget about it (unless it is the same object).

         'Destroy_python_ctx' will forget about the last imported string. */
      
      if (CTX (ref)->imported_string && CTX (ref)->imported_string != obj)
        Py_DECREF (CTX (ref)->imported_string);
      
      r->type = JSP_TYPE_STRING;
      r->u.string = PyString_AsString (obj);

      CTX (ref)->imported_string = obj;
      return JSP_GET_VALUE;
    }
  else
    {
      /* Read mapping PyObject -> jsplugin_obj. */
      r->type = JSP_TYPE_OBJECT;
      r->u.object = find_mapping (CTX (ref), obj);
      
      /* Use stored jsplugin_obj if found. */
      if (r->u.object)
        return JSP_GET_VALUE;

      /* Otherwise, create a new with suitable callbacks. */
      if (PyCallable_Check (obj))
        result = cbs->create_function
          (ref, object_getter, object_setter, object_call,
           object_construct, NULL, object_destroy, &r->u.object);
      else if (PyList_Check (obj))
        result = cbs->create_object
          (ref, list_getter, list_setter, object_destroy, &r->u.object);
      else if (PyTuple_Check (obj))
        result = cbs->create_object
          (ref, tuple_getter, tuple_setter, object_destroy,
           &r->u.object);
      else
        result = cbs->create_object
          (ref, object_getter, object_setter, object_destroy,
           &r->u.object);
      
      switch (result)
        {
        case JSP_CREATE_OK:
          setup_python_obj (r, CTX (ref), obj);
          add_mapping (CTX (ref), obj, r->u.object);
          return JSP_GET_VALUE;
          
        case JSP_CREATE_NOMEM:
          Py_DECREF (obj);
          return JSP_GET_NOMEM;

        default:
          Py_DECREF (obj);
          return JSP_GET_ERROR;
        }
    }
}


/* Exports a jsplugin_value to Python by wrapping it in an appropriate
   Python object. */
static PyObject *
export_value (jsplugin_value *v, int *owned)
{
  PyObject *value = 0;

  switch (v->type)
    {
    case JSP_TYPE_OBJECT:
      if (v->u.object)
        value = OBJ (v->u.object);
      *owned = 0;
      break;
      
    case JSP_TYPE_STRING:
      value = PyString_FromString (v->u.string);
      *owned = 1;
      break;

    case JSP_TYPE_NUMBER:
      value = PyFloat_FromDouble (v->u.number);
      *owned = 1;
      break;

    case JSP_TYPE_BOOLEAN:
      value = PyInt_FromLong (v->u.boolean ? 1 : 0);
      *owned = 1;
      break;

    default:
      value = Py_None;
      *owned = 0;
    }
  
  return value;
}


/* Helper function that creates a function through the create_function
   callback. */
static int
create_function (jsplugin_obj *ref, jsplugin_function *f, const char *s,
                 jsplugin_value *r)
{
  switch (cbs->create_function (ref, 0, 0, f, 0, s, 0, &r->u.object))
    {
    case JSP_CREATE_OK:
      r->type = JSP_TYPE_OBJECT;
      setup_python_obj (r, CTX (ref), 0);
      return JSP_GET_VALUE_CACHE;

    case JSP_CREATE_NOMEM:
      return JSP_GET_NOMEM;

    default:
      return JSP_GET_ERROR;
    }
}


/* Implementation of the global function PyImport.  Takes a string
   argument, imports a Python module by that name and returns it.

   If the name includes package names ("package.module"), the module
   named after the last period is returned, not the top level package
   or anything else. */
static int
PyImport_call (jsplugin_obj *this, jsplugin_obj *func, int argc,
               jsplugin_value *argv, jsplugin_value *r)
{
  PyObject *module, *submodule;
  char *name, *namep, *tok;

  if (argc != 1 || argv[0].type != JSP_TYPE_STRING)
    return JSP_CALL_ERROR;

  module = PyImport_ImportModuleEx
    ((char *) argv[0].u.string, Py_None, Py_None, Py_None);

  if (!module)
    {
      PyErr_Clear ();
      return JSP_CALL_ERROR;
    }
  
  name = strdup (argv[0].u.string);
  
  if (!name)
    {
      Py_DECREF (module);
      return JSP_CALL_NOMEM;
    }

  if (strchr (name, '.'))
    {
      namep = name;

      while ((tok = strsep (&namep, ".")) != 0)
        {
          submodule = PyObject_GetAttrString (module, tok);
          Py_DECREF (module);
          module = submodule;
          
          if (!module)
            {
              free (name);
              PyErr_Clear ();
              return JSP_CALL_ERROR;
            }
        }
    }
  
  free (name);
  
  switch (import_object (func, module, r))
    {
    case JSP_GET_VALUE:
      return JSP_CALL_VALUE;

    case JSP_GET_NOMEM:
      return JSP_CALL_NOMEM;

    default:
      return JSP_CALL_ERROR;
    }
}


/* Generic property getter.  Gets a Python attribute (__getattr__). */
static int
object_getter (jsplugin_obj *this, const char *n, jsplugin_value *r)
{
  PyObject *result;
  
  result = PyObject_GetAttrString (OBJ (this), (char *) n);
  
  if (!result)
    {
      PyErr_Clear ();
      return JSP_GET_NOTFOUND;
    }

  return import_object (this, result, r);
}


/* Generic property setter.  Sets a Python attribute (__setattr__). */
static int
object_setter (jsplugin_obj *this, const char *n, jsplugin_value *v)
{
  PyObject *value;
  int owned = 0;
  
  value = export_value (v, &owned);

  if (!value)
    return JSP_PUT_ERROR;

  if (PyObject_SetAttrString (OBJ (this), (char *) n, value) == -1)
    {
      if (owned)
        Py_DECREF (value);

      PyErr_Clear ();
      return JSP_PUT_ERROR;
    }
  
  if (owned)
    Py_DECREF (value);
  
  return JSP_PUT_OK;
}


/* Generic object call. */
static int
object_call (jsplugin_obj *this, jsplugin_obj *func, int argc,
             jsplugin_value *argv, jsplugin_value *r)
{
  PyObject *args, *arg, *result;
  int idx, owned;
  
  if (argc > 0)
    {
      args = PyTuple_New (argc);
      
      if (!args)
        return JSP_CALL_ERROR;

      for (idx = 0; idx < argc; ++idx)
        {
          arg = export_value (&argv[idx], &owned);

          if (!arg)
            {
              Py_DECREF (args);
              PyErr_Clear();
              return JSP_CALL_ERROR;
            }

          if (!owned)
            Py_INCREF (arg);

          if (PyTuple_SetItem (args, idx, arg))
            {
              Py_DECREF (args);
              PyErr_Clear();
              return JSP_CALL_ERROR;
            }
        }
    }
  else
    args = PyTuple_New (0);
  
  result = PyObject_CallObject (OBJ (func), args);

  Py_DECREF (args);
  
  if (!result)
    {
      PyErr_Clear();
      return JSP_CALL_ERROR;
    }
  
  switch (import_object (func, result, r))
    {
    case JSP_GET_VALUE:
      return JSP_CALL_VALUE;

    case JSP_GET_NOMEM:
      return JSP_CALL_NOMEM;

    default:
      return JSP_CALL_ERROR;
    }
}


/* Generic object construct. */
static int
object_construct (jsplugin_obj *this, jsplugin_obj *func, int argc,
                  jsplugin_value *argv, jsplugin_value *r)
{
  PyObject *args, *arg, *kw, *result;
  int idx, owned;
  
  if (argc > 0)
    {
      args = PyTuple_New (argc);
      
      if (!args)
        return JSP_CALL_ERROR;

      for (idx = 0; idx < argc; ++idx)
        {
          arg = export_value (&argv[idx], &owned);

          if (!arg)
            {
              Py_DECREF (args);
              PyErr_Clear();
              return JSP_CALL_ERROR;
            }

          if (!owned)
            Py_INCREF (arg);

          if (PyTuple_SetItem (args, idx, arg))
            {
              Py_DECREF (args);
              PyErr_Clear();
              return JSP_CALL_ERROR;
            }
        }
    }
  else
    {
      args = PyTuple_New (0);

      if (!args)
        return JSP_CALL_ERROR;
    }

  kw = PyDict_New ();

  if (!kw)
    {
      Py_DECREF (args);
      PyErr_Clear();
      return JSP_CALL_ERROR;
    }
  
  result = PyInstance_New (OBJ (func), args, kw);

  Py_DECREF (args);
  Py_DECREF (kw);
  
  if (!result)
    {
      PyErr_Print ();
      PyErr_Clear();
      return JSP_CALL_ERROR;
    }
  
  switch (import_object (func, result, r))
    {
    case JSP_GET_VALUE:
      return JSP_CALL_VALUE;

    case JSP_GET_NOMEM:
      return JSP_CALL_NOMEM;

    default:
      return JSP_CALL_ERROR;
    }
}


/* List-specific getter.  Lists have no attributes, so all property
   accesses can be translated into __getitem__, if the property name
   is an integer. */
static int
list_getter (jsplugin_obj *this, const char *n, jsplugin_value *r)
{
  PyObject *index, *item;
  int idx;

  index = PyObject_CallFunction (CTX (this)->builtin__int, "s", n);
  
  if (PyErr_Occurred ())
    {
      PyErr_Print ();
      PyErr_Clear ();
      return JSP_GET_NOTFOUND;
    }

  idx = PyInt_AsLong (index);
  Py_DECREF (index);
  
  item = PyList_GetItem (OBJ (this), idx);

  if (!item)
    return JSP_GET_NOTFOUND;

  Py_INCREF (item);

  return import_object (this, item, r);
}


/* List-specific Setter.  Lists have no attributes, so all property
   accesses can be translated into __setitem__, if the property name
   is an integer. */
static int
list_setter (jsplugin_obj *this, const char *n, jsplugin_value *v)
{
  PyObject *index, *item;
  int idx, owned;
  
  index = PyObject_CallFunction (CTX (this)->builtin__int, "s", n);
  
  if (PyErr_Occurred ())
    {
      PyErr_Clear ();
      return JSP_GET_NOTFOUND;
    }

  idx = PyInt_AsLong (index);
  Py_DECREF (index);

  item = export_value (v, &owned);

  if (!item)
    return JSP_PUT_ERROR;

  if (!owned)
    Py_INCREF (item);

  /* PyList_SetItem steals a reference to 'item'. */
  if (PyList_SetItem (OBJ (this), idx, item) == -1)
    return JSP_PUT_ERROR;
  
  return JSP_PUT_OK;
}


/* Tuple-specific getter.  Tuples have no attributes, so all property
   accesses can be translated into __getitem__, if the property name
   is an integer. */
static int
tuple_getter (jsplugin_obj *this, const char *n, jsplugin_value *r)
{
  PyObject *index, *item;
  int idx;

  index = PyObject_CallFunction (CTX (this)->builtin__int, "s", n);
  
  if (PyErr_Occurred ())
    {
      PyErr_Clear ();
      return JSP_GET_NOTFOUND;
    }

  idx = PyInt_AsLong (index);
  Py_DECREF (index);
  
  item = PyTuple_GetItem (OBJ (this), idx);

  if (!item)
    return JSP_GET_NOTFOUND;

  Py_INCREF (item);

  return import_object (this, item, r);
}


/* Tuple-specific setter.  Tuples have no attributes, so all property
   accesses can be translated into __setitem__, if the property name
   is an integer. */
static int
tuple_setter (jsplugin_obj *this, const char *n, jsplugin_value *v)
{
  PyObject *index, *item;
  int idx, owned;
  
  index = PyObject_CallFunction (CTX (this)->builtin__int, "s", n);
  
  if (PyErr_Occurred ())
    {
      PyErr_Clear ();
      return JSP_GET_NOTFOUND;
    }

  idx = PyInt_AsLong (index);
  Py_DECREF (index);

  item = export_value (v, &owned);

  if (!item)
    return JSP_PUT_ERROR;

  if (!owned)
    Py_INCREF (item);

  /* PyTuple_SetItem steals a reference to 'item'. */
  if (PyTuple_SetItem (OBJ (this), idx, item) == -1)
    return JSP_PUT_ERROR;
  
  return JSP_PUT_OK;
}


/* Object destructor. */
static int
object_destroy (jsplugin_obj *this)
{
  delete_mapping (CTX (this), OBJ (this));
  destroy_python_obj (this);
  
  return JSP_DESTROY_OK;
}


/* Global getter.  Handles 'PyImport' only. */
static int
global_getter (jsplugin_obj *g, const char *n, jsplugin_value *r)
{
  if (strcmp (n, "PyImport") == 0)
    return create_function (g, PyImport_call, "s", r);
  else
    return JSP_GET_ERROR;
}


/* Global setter.  Doesn't support any properties. */
static int
global_setter (jsplugin_obj *g, const char *n, jsplugin_value *r)
{
  return JSP_PUT_OK;
}


/* Global initialization, creates a context and calls Py_Initialize if
   this is the first use of the plugin. */
static void
global_init (jsplugin_obj *g)
{
  jsplugin_value v;
  jsplugin_python_ctx *ctx;
    
  if (use_count++ == 0)
    Py_Initialize ();

  ctx = setup_python_ctx ();
  
  v.u.object = g;

  setup_python_obj (&v, ctx, 0);
}


/* Global destruction, releases the global reference to the
   document/Python/plugin context. */
static void
global_destroy (jsplugin_obj *g)
{
  release_python_ctx (CTX (g));
}


static int
handle_object (jsplugin_obj *global_obj, jsplugin_obj *object_obj,
               int attrs_count, jsplugin_attr *attrs,
               jsplugin_getter **getter, jsplugin_setter **setter,
               jsplugin_destructor **destructor,
               jsplugin_notify **inserted, jsplugin_notify **removed)
{
  PyObject *module, *submodule;
  char *modname, *name, *namep, *tok;
  int index;
  jsplugin_value value;

  for (index = 0; index < attrs_count; ++index)
    if (strcmp (attrs[index].name, "import") == 0)
      {
        modname = (char *) attrs[index].value;
        break;
      }

  module = PyImport_ImportModuleEx
    (modname, Py_None, Py_None, Py_None);

  if (!module)
    {
      PyErr_Clear ();
      return JSP_OBJECT_ERROR;
    }
  
  name = strdup (modname);
  
  if (!name)
    {
      Py_DECREF (module);
      return JSP_OBJECT_NOMEM;
    }

  if (strchr (name, '.'))
    {
      namep = name;

      while ((tok = strsep (&namep, ".")) != 0)
        {
          submodule = PyObject_GetAttrString (module, tok);
          Py_DECREF (module);
          module = submodule;
          
          if (!module)
            {
              free (name);
              PyErr_Clear ();
              return JSP_OBJECT_ERROR;
            }
        }
    }
  
  free (name);

  value.type = JSP_TYPE_OBJECT;
  value.u.object = object_obj;

  setup_python_obj (&value, CTX (global_obj), module);
  add_mapping (CTX (global_obj), module, object_obj);

  *getter = object_getter;
  *setter = object_setter;
  *destructor = object_destroy;

  return JSP_OBJECT_OK;
}


static const char *global_names[] = { "PyImport", NULL };
static const char *object_types[] = { "application/x-python-module",
                                      NULL };

static struct jsplugin_cap cap =
  {
    global_names,
    object_types,
    
    global_getter,
    global_setter,

    global_init,
    global_destroy,

    handle_object,
  };


DECLSPEC int
jsplugin_capabilities (jsplugin_cap **r, jsplugin_callbacks *c)
{
  *r = &cap;
  cbs = c;
  return 0;
}

