/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "modules/dom/src/domclonehandler.h"
#include "modules/dom/src/canvas/domcontext2d.h"
#include "modules/dom/src/opera/domformdata.h"
#include "modules/dom/src/domfile/domblob.h"

/* virtual */ OP_STATUS
DOM_CloneHandler::Clone(EcmaScript_Object *source_object, ES_Runtime *target_runtime, EcmaScript_Object *&target_object)
{
#ifdef CANVAS_SUPPORT
	if (source_object->IsA(DOM_TYPE_CANVASIMAGEDATA))
	{
		DOMCanvasImageData *target;
		RETURN_IF_ERROR(DOMCanvasImageData::Clone(static_cast<DOMCanvasImageData *>(source_object), target_runtime, target));
		target_object = target;
		return OpStatus::OK;
	}
#endif // CANVAS_SUPPORT
#ifdef DOM_HTTP_SUPPORT
	if (source_object->IsA(DOM_TYPE_FORMDATA))
	{
		DOM_FormData *target;
		RETURN_IF_ERROR(DOM_FormData::Clone(static_cast<DOM_FormData *>(source_object), target_runtime, target));
		target_object = target;
		return OpStatus::OK;
	}
#endif // DOM_HTTP_SUPPORT
	if (source_object->IsA(DOM_TYPE_BLOB))
	{
		DOM_Blob *target;
		RETURN_IF_ERROR(DOM_Blob::Clone(static_cast<DOM_Blob *>(source_object), target_runtime, target));
		target_object = target;
		return OpStatus::OK;
	}
	return OpStatus::ERR;
}

#ifdef ES_PERSISTENT_SUPPORT

/* virtual */ OP_STATUS
DOM_CloneHandler::Clone(EcmaScript_Object *source_object, ES_PersistentItem *&target_item)
{
#ifdef CANVAS_SUPPORT
	if (source_object->IsA(DOM_TYPE_CANVASIMAGEDATA))
	{
		RETURN_IF_ERROR(DOMCanvasImageData::Clone(static_cast<DOMCanvasImageData *>(source_object), target_item));
		return OpStatus::OK;
	}
#endif // CANVAS_SUPPORT
#ifdef DOM_HTTP_SUPPORT
	if (source_object->IsA(DOM_TYPE_FORMDATA))
	{
		RETURN_IF_ERROR(DOM_FormData::Clone(static_cast<DOM_FormData *>(source_object), target_item));
		return OpStatus::OK;
	}
#endif // DOM_HTTP_SUPPORT
	if (source_object->IsA(DOM_TYPE_BLOB))
	{
		RETURN_IF_ERROR(DOM_Blob::Clone(static_cast<DOM_Blob *>(source_object), target_item));
		return OpStatus::OK;
	}
	return OpStatus::ERR;
}

/* virtual */ OP_STATUS
DOM_CloneHandler::Clone(ES_PersistentItem *&source_item, ES_Runtime *target_runtime, EcmaScript_Object *&target_object)
{
#ifdef CANVAS_SUPPORT
	if (source_item->IsA(DOM_TYPE_CANVASIMAGEDATA))
	{
		DOMCanvasImageData *target;
		RETURN_IF_ERROR(DOMCanvasImageData::Clone(source_item, target_runtime, target));
		target_object = target;
		return OpStatus::OK;
	}
#endif // CANVAS_SUPPORT
#ifdef DOM_HTTP_SUPPORT
	if (source_item->IsA(DOM_TYPE_FORMDATA))
	{
		DOM_FormData *target;
		RETURN_IF_ERROR(DOM_FormData::Clone(source_item, target_runtime, target));
		target_object = target;
		return OpStatus::OK;
	}
#endif // DOM_HTTP_SUPPORT
	if (source_item->IsA(DOM_TYPE_BLOB))
	{
		DOM_Blob *target;
		RETURN_IF_ERROR(DOM_Blob::Clone(source_item, target_runtime, target));
		target_object = target;
		return OpStatus::OK;
	}

	return OpStatus::ERR;
}

#endif // ES_PERSISTENT_SUPPORT
