/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef NS4P_COMPONENT_PLUGINS
#include "modules/ns4plugins/component/browser_functions.h"
#include "modules/ns4plugins/component/plugin_component.h"
#include "modules/ns4plugins/component/plugin_component_helpers.h"
#include "modules/ns4plugins/src/plug-inc/npapi.h"
#include "modules/opdata/UniStringUTF8.h"


/* static */ OP_STATUS
PluginComponentHelpers::SetNPString(NPString* out_string, const UniString& string)
{
	out_string->UTF8Length = 0;
	out_string->UTF8Characters = NULL;

	char* utf8_string_ = NULL;
	RETURN_IF_ERROR(UniString_UTF8::ToUTF8(&utf8_string_, string));
	OpAutoArray<char> utf8_string(utf8_string_);
	size_t utf8_string_len = op_strlen(utf8_string.get());

	/* Make a copy of the string that is allocated by NPN_MemAlloc. This is a requirement of NPStrings. */
	out_string->UTF8Characters = static_cast<NPUTF8*>(BrowserFunctions::NPN_MemAlloc(utf8_string_len + 1));
	RETURN_OOM_IF_NULL(out_string->UTF8Characters);

	out_string->UTF8Length = utf8_string_len;
	op_memcpy(const_cast<NPUTF8*>(out_string->UTF8Characters), utf8_string.get(), utf8_string_len);
	const_cast<NPUTF8*>(out_string->UTF8Characters)[utf8_string_len] = '\0';

	return OpStatus::OK;
}

/* static */ OP_STATUS
PluginComponentHelpers::SetNPVariant(NPVariant* out_variant, const PluginVariant& variant, PluginComponent* pc /* = g_plugin_component */)
{
	if (variant.HasNullValue())
		out_variant->type = NPVariantType_Null;
	else if (variant.HasBoolValue())
	{
		out_variant->type = NPVariantType_Bool;
		out_variant->value.boolValue = !!variant.GetBoolValue();
	}
	else if (variant.HasIntValue())
	{
		out_variant->type = NPVariantType_Int32;
		out_variant->value.intValue = variant.GetIntValue();
	}
	else if (variant.HasDoubleValue())
	{
		out_variant->type = NPVariantType_Double;
		out_variant->value.doubleValue = variant.GetDoubleValue();
	}
	else if (variant.HasStringValue())
	{
		out_variant->type = NPVariantType_String;
		RETURN_IF_ERROR(SetNPString(&out_variant->value.stringValue, variant.GetStringValue()));
	}
	else if (variant.HasObjectValue())
	{
		if (!pc)
			return OpStatus::ERR_NO_SUCH_RESOURCE;

		out_variant->type = NPVariantType_Object;
		RETURN_OOM_IF_NULL(out_variant->value.objectValue = pc->Bind(*variant.GetObjectValue()));
	}
	else
		out_variant->type = NPVariantType_Void;

	return OpStatus::OK;
}

/* static */ void
PluginComponentHelpers::ReleaseNPVariant(NPVariant* variant)
{
	if (variant->type == NPVariantType_String)
		BrowserFunctions::NPN_MemFree(const_cast<NPUTF8*>(variant->value.stringValue.UTF8Characters));

	variant->type = NPVariantType_Void;
}

/* static */ OP_STATUS
PluginComponentHelpers::SetPluginVariant(PluginVariant& out_variant, const NPVariant& variant, PluginComponent* pc /* = g_plugin_component */)
{
	out_variant = PluginVariant();

	switch (variant.type)
	{
		case NPVariantType_Null:
			out_variant.SetNullValue(true);
			break;

		case NPVariantType_Bool:
			out_variant.SetBoolValue(variant.value.boolValue);
			break;

		case NPVariantType_Int32:
			out_variant.SetIntValue(variant.value.intValue);
			break;

		case NPVariantType_Double:
			out_variant.SetDoubleValue(variant.value.doubleValue);
			break;

		case NPVariantType_String:
		{
			UniString s;
			if (variant.value.stringValue.UTF8Characters)
				RETURN_IF_ERROR(UniString_UTF8::FromUTF8(s, variant.value.stringValue.UTF8Characters, variant.value.stringValue.UTF8Length));
			out_variant.SetStringValue(s);
			break;
		}

		case NPVariantType_Object:
		{
			if (!variant.value.objectValue)
			{
				out_variant.SetNullValue(true);
				break;
			}

			if (!pc)
				return OpStatus::ERR_NO_SUCH_RESOURCE;

			UINT64 obj = pc->Lookup(variant.value.objectValue);
			if (!obj)
				return OpStatus::ERR_OUT_OF_RANGE;

			PluginObject* object = ToPluginObject(obj);
			RETURN_OOM_IF_NULL(object);

			out_variant.SetObjectValue(object);
			break;
		}

		case NPVariantType_Void:
		default:
			out_variant.SetVoidValue(true);
			break;
	}

	return OpStatus::OK;
}

/* static */ PluginComponentHelpers::PluginObject*
PluginComponentHelpers::ToPluginObject(UINT64 np_object)
{
	PluginObject* object = OP_NEW(PluginObject, ());
	RETURN_VALUE_IF_NULL(object, NULL);

	SetPluginObject(*object, np_object);
	return object;
}

/* static */ void
PluginComponentHelpers::SetPluginObject(PluginObject& out_object, UINT64 np_object)
{
	out_object.SetObject(np_object);
}

/* static */ void
PluginComponentHelpers::SetPluginIdentifier(PluginIdentifier& out_identifier, NPIdentifier identifier, IdentifierCache* ic)
{
	if (identifier)
	{
		if (ic->IsOrphan(identifier))
		{
			/* The identifier is a raw value allocated by a third party thread, and is not known to
			 * the browser. Register the value properly, and use the new identifier to communicate
			 * with the browser. */
			ic->Adopt(identifier);

			if (ic->IsString(identifier))
				BrowserFunctions::NPN_GetStringIdentifier(ic->GetString(identifier));
			else
				BrowserFunctions::NPN_GetIntIdentifier(ic->GetInteger(identifier));

			OP_ASSERT(!ic->IsOrphan(identifier));
		}

		out_identifier.SetIdentifier(ic->GetBrowserIdentifier(identifier));
		out_identifier.SetIsString(ic->IsString(identifier));

		if (!ic->IsString(identifier))
			out_identifier.SetIntValue(ic->GetInteger(identifier));
	}
}

/* static */ OpPluginResultMessage*
PluginComponentHelpers::BuildPluginResultMessage(bool success, const NPVariant* result)
{
	OpPluginResultMessage* message = OpPluginResultMessage::Create(success);

	if (message && success && result)
	{
		PluginComponentHelpers::PluginVariant* res = OP_NEW(PluginComponentHelpers::PluginVariant, ());
		if (!res || OpStatus::IsError(PluginComponentHelpers::SetPluginVariant(*res, *result)))
		{
			OP_DELETE(res);
			OP_DELETE(message);
			return NULL;
		}

		message->SetResult(res);
	}

	return message;
}

/* static */ void
PluginComponentHelpers::SetOpRect(OpRect* out_rect, const PluginRect& rect)
{
	out_rect->x = rect.GetX();
	out_rect->y = rect.GetY();
	out_rect->width = rect.GetWidth();
	out_rect->height = rect.GetHeight();
}

/* static */ void
PluginComponentHelpers::SetOpRect(OpRect* out_rect, const NPRect& rect)
{
	out_rect->x = rect.left;
	out_rect->y = rect.top;
	out_rect->width = rect.right - rect.left;
	out_rect->height = rect.bottom - rect.left;
}

/* static */ void
PluginComponentHelpers::SetPluginRect(PluginRect& out_rect, const OpRect& rect)
{
	out_rect.SetX(rect.x);
	out_rect.SetY(rect.y);
	out_rect.SetWidth(rect.width);
	out_rect.SetHeight(rect.height);
}

/* static */ void
PluginComponentHelpers::SetPluginRect(PluginRect& out_rect, const NPRect& rect)
{
	out_rect.SetX(rect.left);
	out_rect.SetY(rect.top);
	out_rect.SetWidth(rect.right - rect.left);
	out_rect.SetHeight(rect.bottom - rect.top);
}

/* static */ void
PluginComponentHelpers::SetPluginPoint(PluginPoint* out_point, const OpPoint& point)
{
	out_point->SetX(point.x);
	out_point->SetY(point.y);
}

/* static */ OpPoint
PluginComponentHelpers::OpPointFromPluginPoint(PluginPoint& point)
{
	return OpPoint(point.GetX(), point.GetY());
}

/* static */ bool
PluginComponentHelpers::IsBooleanValue(NPPVariable variable)
{
	return variable == NPPVpluginWindowBool
		|| variable == NPPVpluginTransparentBool
		|| variable == NPPVjavascriptPushCallerBool
		|| variable == NPPVpluginKeepLibraryInMemory
		|| variable == NPPVpluginUrlRequestsDisplayedBool
		|| variable == NPPVpluginUsesDOMForCursorBool
		|| variable == NPPVsupportsAdvancedKeyHandling
		|| variable == NPPVpluginWantsAllNetworkStreams
		|| variable == NPPVpluginCancelSrcStream;
}

/* static */ bool
PluginComponentHelpers::IsIntegerValue(NPPVariable variable)
{
#ifdef XP_MACOSX
	return variable == NPPVpluginDrawingModel
		|| variable == NPPVpluginEventModel;
#else
	return false;
#endif // XP_MACOSX
}

/* static */ bool
PluginComponentHelpers::IsObjectValue(NPPVariable variable)
{
	return variable == NPPVpluginScriptableNPObject;
}

#endif // NS4P_COMPONENT_PLUGINS
