/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "modules/ecmascript_utils/esenginedebugger.h"
#include "modules/ecmascript_utils/esasyncif.h"
#include "modules/doc/frm_doc.h"
#include "modules/dochand/win.h"
#include "modules/dochand/winman.h"
#include "modules/dochand/fdelm.h"
#include "modules/ecmascript_utils/esobjman.h"

ESU_ObjectManager::ESU_ObjectManager()
	: nextid(1)
{
}

OP_STATUS
ESU_ObjectManager::GetId(ES_Runtime *rt, ES_Object *obj, unsigned &id)
{
	id = 0;

	OP_ASSERT(rt && obj);

	ObjectMap *objmap;
	RETURN_IF_ERROR(GetObjectMap(rt, objmap));

	if (OpStatus::IsError(objmap->objecttoid.GetData(obj, &id)))
	{
		// Add the element.
		OpAutoPtr<ES_ObjectReference> ref(OP_NEW(ES_ObjectReference, ()));
		RETURN_OOM_IF_NULL(ref.get());
		RETURN_IF_ERROR(ref->Protect(rt, obj));

		id = nextid++;

		OP_STATUS err = objmap->idtoobject.Add(id, ref.get());

		if (OpStatus::IsSuccess(err))
		{
			ref.release();
			err = objmap->objecttoid.Add(obj, id);
		}

		if (OpStatus::IsError(err))
		{
			Release(objmap, id);
			id = 0;
			return err;
		}
	}

	return OpStatus::OK;
}

ES_Object *
ESU_ObjectManager::GetObject(ES_Runtime *rt, unsigned id)
{
	ObjectMap *objmap = GetObjectMap(rt);

	return (objmap ? GetObject(objmap, id) : NULL);
}

ES_Object *
ESU_ObjectManager::GetObject(unsigned id)
{
	ObjectMap *objmap = FindObjectMap(id);

	return (objmap ? GetObject(objmap, id) : NULL);
}

void
ESU_ObjectManager::Release(ES_Runtime *rt, unsigned id)
{
	ObjectMap *objmap = GetObjectMap(rt);

	if (objmap)
		Release(objmap, id);
}

void
ESU_ObjectManager::Release(unsigned id)
{
	ObjectMap *objmap = FindObjectMap(id);

	if (objmap)
		Release(objmap, id);
}

void
ESU_ObjectManager::Release(ES_Runtime *rt)
{
	ObjectMap *objmap;

	if (OpStatus::IsSuccess(object_maps.Remove(rt, &objmap)))
		OP_DELETE(objmap);
}

void
ESU_ObjectManager::ReleaseAll()
{
	object_maps.DeleteAll();
}

void
ESU_ObjectManager::Reset()
{
	ReleaseAll();
	nextid = 1;
}

OP_STATUS
ESU_ObjectManager::GetObjectMap(ES_Runtime *rt, ObjectMap *&objmap)
{
	if (OpStatus::IsSuccess(object_maps.GetData(rt, &objmap)))
		return OpStatus::OK;

	objmap = OP_NEW(ObjectMap, ());
	RETURN_OOM_IF_NULL(objmap);

	OP_STATUS err = object_maps.Add(rt, objmap);

	if (OpStatus::IsError(err))
		OP_DELETE(objmap);

	return err;
}

ESU_ObjectManager::ObjectMap *
ESU_ObjectManager::GetObjectMap(ES_Runtime *rt)
{
	ObjectMap *objmap;

	if (OpStatus::IsSuccess(object_maps.GetData(rt, &objmap)))
		return objmap;

	return NULL;
}

ESU_ObjectManager::ObjectMap *
ESU_ObjectManager::FindObjectMap(unsigned id)
{
	OpAutoPtr<OpHashIterator> iterator(object_maps.GetIterator());
	
	if (!iterator.get())
		return NULL;

	RETURN_VALUE_IF_ERROR(iterator->First(), NULL);

	do
	{
		ObjectMap *objmap = static_cast<ObjectMap*>(iterator->GetData());

		if (objmap->idtoobject.Contains(id))
			return objmap;
	}
	while (OpStatus::IsSuccess(iterator->Next()));

	return NULL;
}

ES_Object *
ESU_ObjectManager::GetObject(ObjectMap *objmap, unsigned id)
{
	OP_ASSERT(objmap);

	if (id != 0)
	{
		ES_ObjectReference *ref;

		if (objmap->idtoobject.GetData(id, &ref) == OpStatus::OK)
			return ref->GetObject();
	}

	return NULL;
}

void 
ESU_ObjectManager::Release(ObjectMap *objmap, unsigned id)
{
	OP_ASSERT(objmap);

	ES_ObjectReference *ref;
	RETURN_VOID_IF_ERROR(objmap->idtoobject.Remove(id, &ref));

	unsigned removed;
	OpStatus::Ignore(objmap->objecttoid.Remove(ref->GetObject(), &removed));

	OP_DELETE(ref);
}

ESU_RuntimeIterator::ESU_RuntimeIterator(Window *first, BOOL create)
	: runtime(NULL), window(first), create(create)
{
}

OP_STATUS
ESU_RuntimeIterator::Next()
{
	if (window != NULL && iter.GetDocumentManager() == NULL)
	{
		iter = DocumentTreeIterator(window);
		iter.SetIncludeThis();
	}

	while (window != NULL)
	{
		while (iter.Next())
		{
			if (DocumentManager *docman = iter.GetDocumentManager())
			{
				if (FramesDocument *frm_doc = docman->GetCurrentDoc())
				{
					runtime = frm_doc->GetESRuntime();

					if (runtime == NULL && create)
					{
						// Create the runtime if none exists, DOM/CSS inspectors
						// needs this for non-scripted pages to work.
						OP_STATUS status = frm_doc->ConstructDOMEnvironment();

						if (OpStatus::IsSuccess(status))
							runtime = frm_doc->GetESRuntime();
						else if (OpStatus::IsMemoryError(status))
							return status;
						// else: Skip silently if we for some non-OOM reason couldn't
						//       construct DOM enviroment.
					}

					if (runtime != NULL)
						return OpStatus::OK;
				}
			}
		}

		window = window->Suc();

		if (window)
		{
			iter = DocumentTreeIterator(window);
			iter.SetIncludeThis();
		}
	}

	return OpStatus::ERR;
}

ES_Runtime *
ESU_RuntimeIterator::GetRuntime()
{
	return runtime;
}

ESU_RuntimeManager::ESU_RuntimeManager()
	: nextid(1)
{
}

OP_STATUS
ESU_RuntimeManager::GetId(ES_Runtime *rt, unsigned &id)
{
	id = 0;

	OP_ASSERT(rt);

	if (OpStatus::IsError(runtimetoid.GetData(rt, &id)))
	{
		// Add the element.
		id = nextid++;

		OP_STATUS err = idtoruntime.Add(id, rt);

		if (OpStatus::IsSuccess(err))
			err = runtimetoid.Add(rt, id);

		if (OpStatus::IsError(err))
		{
			ES_Runtime *tmp;
			idtoruntime.Remove(id, &tmp);
			id = 0;
			return err;
		}
	}

	return OpStatus::OK;
}

void
ESU_RuntimeManager::Reset()
{
	idtoruntime.RemoveAll();
	runtimetoid.RemoveAll();
}

ES_Runtime *
ESU_RuntimeManager::GetRuntime(unsigned id)
{
	ES_Runtime *rt;
	RETURN_VALUE_IF_ERROR(idtoruntime.GetData(id, &rt), NULL);

	ESU_RuntimeIterator i(g_windowManager->FirstWindow(), FALSE);

	while (OpStatus::IsSuccess(i.Next()))
		if (i.GetRuntime() == rt)
			return rt;

	return NULL;
}

/* static */ OP_STATUS
ESU_RuntimeManager::FindAllRuntimes(OpVector<ES_Runtime> &runtimes, BOOL create)
{
	ESU_RuntimeIterator i(g_windowManager->FirstWindow(), create);

	OP_STATUS err;
	while (OpStatus::IsSuccess(err = i.Next()))
		RETURN_IF_ERROR(runtimes.Add(i.GetRuntime()));

	return OpStatus::OK;
}

/* static */ unsigned
ESU_RuntimeManager::GetWindowId(ES_Runtime *rt)
{
	FramesDocument *doc = (rt ? rt->GetFramesDocument() : NULL);
	return (doc ? doc->GetWindow()->Id() : 0);
}

static OP_STATUS
ESU_GetFramePath(TempBuffer &buffer, FramesDocument *doc)
{
	if (FramesDocument *parent_doc = doc->GetParentDoc())
	{
		RETURN_IF_ERROR(ESU_GetFramePath(buffer, parent_doc));

		if (FramesDocElm *fde = doc->GetDocManager()->GetFrame())
		{
			const uni_char *name = fde->GetName();
			unsigned name_length = name ? uni_strlen(name) : 0;

			// Enough unless unsigned is more than 32 bits and the frame has at least 10 billion siblings.
			RETURN_IF_ERROR(buffer.Expand(15 + name_length));

			OpStatus::Ignore(buffer.Append("/"));

			if (name)
				OpStatus::Ignore(buffer.Append(name));

			unsigned index = 1;
			while ((fde = fde->Pred()) != NULL)
				++index;

			OpStatus::Ignore(buffer.Append("["));
			OpStatus::Ignore(buffer.AppendUnsignedLong(index));
			OpStatus::Ignore(buffer.Append("]"));
		}

		return OpStatus::OK;
	}
	else
		return buffer.Append("_top");
}

/* static */ OP_STATUS
ESU_RuntimeManager::GetFramePath(ES_Runtime *rt, OpString &out)
{
	FramesDocument *doc = (rt ? rt->GetFramesDocument() : NULL);

	if (doc)
	{
		TempBuffer buf;
		RETURN_IF_ERROR(ESU_GetFramePath(buf, doc));
		out.Set(buf.GetStorage());
	}
	else
		out.Empty();

	return OpStatus::OK;
}

/* static */ OP_STATUS
ESU_RuntimeManager::GetDocumentUri(ES_Runtime *rt, OpString &out)
{
	FramesDocument *doc = (rt ? rt->GetFramesDocument() : NULL);

	if (doc)
		RETURN_IF_ERROR(doc->GetURL().GetAttribute(URL::KUniName_Username_Password_Hidden, out));
	else
		out.Empty();

	return OpStatus::OK;
}

/* static */
OP_STATUS
ESU_ObjectExporter::ExportObject(ES_Runtime *rt, ES_Object *object, Handler *handler)
{
#ifdef ECMASCRIPT_DEBUGGER
	OP_ASSERT(rt && handler && object);

	ES_DebugObjectElement attr;
	g_ecmaManager->GetObjectAttributes(rt, object, &attr);

	BOOL is_callable = attr.type == OBJTYPE_NATIVE_FUNCTION || attr.type == OBJTYPE_HOST_FUNCTION;
	BOOL is_function = is_callable && op_strcmp(attr.classname, "Function") == 0;
	const char *class_name = attr.classname;
	const uni_char *function_name = NULL;
	if (is_function)
		function_name = attr.u.function.name;
	return handler->Object(is_callable, class_name, function_name, attr.prototype);
#else // ECMASCRIPT_DEBUGGER
	return OpStatus::ERR_NOT_SUPPORTED;
#endif // ECMASCRIPT_DEBUGGER
}

/*static*/
OP_STATUS
ESU_ObjectExporter::ExportProperties(ES_Runtime *rt, ES_Object *object, Handler *handler)
{
#ifdef ECMASCRIPT_DEBUGGER
	OP_ASSERT(rt && handler && object);
	uni_char **names;
	ES_Value *values;
	GetNativeStatus *getstatuses;

	g_ecmaManager->GetObjectProperties(rt, object, NULL, FALSE, &names, &values, &getstatuses);
	if (!names || !values || !getstatuses)
		return OpStatus::ERR_NO_MEMORY;

	uni_char **name = names;
	unsigned prop_count = 0;
	while (*name)
	{
		prop_count++;
		name++;
	}

	handler->OnDebugObjectChainCreated(NULL, prop_count);

	name = names;
	GetNativeStatus *getstatus = getstatuses;
	ES_Value *value = values;

	handler->PropertyCount(prop_count);

	OP_STATUS status = OpStatus::OK;
	for (unsigned index = 0; index < prop_count; index++)
	{
		if (*getstatus == GETNATIVE_SUCCESS || *getstatus == GETNATIVE_SCRIPT_GETTER)
		{
			status = handler->Property(*name, *value);
			if (OpStatus::IsError(status))
				handler->OnError();
		}
		else
		{
			ES_AsyncInterface *asyncif = rt->GetESAsyncInterface();
			if (!asyncif)
				handler->OnError();
			else
			{
				uni_char *aname = *name;
				ES_GetSlotHandler *getslothandler;
				OP_STATUS getslothandlerstat = ES_GetSlotHandler::Make(getslothandler, object, handler, NULL, index, aname, NULL);
				if (OpStatus::IsError(getslothandlerstat) || OpStatus::IsError(asyncif->GetSlot(object, aname, getslothandler, handler->GetBlockedThread())))
				{
					OP_DELETE(getslothandler);
					handler->OnError();
				}
			}
		}
		op_free(*name);
		value++;
		name++;
		getstatus++;
	}

	OP_DELETEA(names);
	OP_DELETEA(values);
	OP_DELETEA(getstatuses);

	return status;
#else // ECMASCRIPT_DEBUGGER
	return OpStatus::ERR_NOT_SUPPORTED;
#endif // ECMASCRIPT_DEBUGGER
}

