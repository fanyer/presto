/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software ASA. All rights reserved.
**
** This file is part of the Opera web browser. It may not be distributed
** under any circumstances.
*/

#include <core/pch.h>

#ifdef DOM_EXTENSIONS_CONTEXT_MENU_API_SUPPORT

#include "modules/dom/src/extensions/domextensionmenuitem.h"
#include "modules/dom/src/extensions/domextensionmenucontext_proxy.h"
#include "modules/dom/src/extensions/domextensioncontexts.h"
#include "modules/dom/src/extensions/domextension_background.h"
#include "modules/dom/src/extensions/domextensionscope.h"
#include "modules/util/glob.h"
#include "modules/windowcommander/src/WindowCommander.h"
#include "modules/doc/frm_doc.h"
#include "modules/logdoc/urlimgcontprov.h"
#include "modules/dom/src/domcore/node.h"
#include "modules/dom/src/extensions/domextensionmenuevent.h"
#include "modules/content_filter/content_filter.h"

/* static */ OP_STATUS
DOM_ExtensionMenuItem::Make(DOM_ExtensionMenuItem*& new_obj, DOM_ExtensionSupport* extension_support, DOM_Runtime* origining_runtime)
{
	RETURN_IF_ERROR(DOMSetObjectRuntime(new_obj = OP_NEW(DOM_ExtensionMenuItem, (extension_support)), origining_runtime, origining_runtime->GetPrototype(DOM_Runtime::EXTENSION_MENUITEM_PROTOTYPE), "MenuItem"));
	return new_obj->Init();
}

DOM_ExtensionMenuItem::DOM_ExtensionMenuItem(DOM_ExtensionSupport* extension_support)
	: DOM_ExtensionMenuContext(extension_support)
	, m_disabled(FALSE)
	, m_type(MENU_ITEM_COMMAND)
	, m_parent_menu(NULL)
	, m_icon_bitmap(NULL)
{
}

DOM_ExtensionMenuItem::~DOM_ExtensionMenuItem()
{
	ResetIconImage();
	if (m_icon_loading_listener.InList())
		GetFramesDocument()->StopLoadingInline(m_icon_url, &m_icon_loading_listener);
}

/* static */ OP_STATUS
DOM_ExtensionMenuItem::CloneArray(ES_Value& es_array, DOM_Runtime* runtime)
{
	// We assume that input es_array is properly validated before.
	OP_ASSERT(es_array.type == VALUE_OBJECT);
	ES_Object* input = es_array.value.object;
	ES_Object* cloned = NULL;
	ES_Value tmp_val;
	OP_BOOLEAN result = runtime->GetName(input, UNI_L("length"), &tmp_val);
	RETURN_IF_ERROR(result);
	OP_ASSERT(result == OpBoolean::IS_TRUE || !"No length?");
	unsigned int len = TruncateDoubleToUInt(tmp_val.value.number);
	RETURN_IF_ERROR(runtime->CreateNativeArrayObject(&cloned, len));
	for (unsigned int i = 0; i < len; ++i)
	{
		result = runtime->GetIndex(input, i, &tmp_val);
		RETURN_IF_ERROR(result);
		if (result != OpBoolean::IS_TRUE)
			return OpStatus::ERR; // Sparse arrays are not supported.
		result = runtime->PutIndex(cloned, i, tmp_val);
		RETURN_IF_ERROR(result);
	}
	DOMSetObject(&es_array, cloned);
	return OpStatus::OK;
}

OP_STATUS
DOM_ExtensionMenuItem::SetMenuItemProperties(ES_Object* props)
{
	m_type = TypeString2TypeEnum(DOMGetDictionaryString(props, UNI_L("type"), UNI_L("")));
	m_disabled = DOMGetDictionaryBoolean(props, UNI_L("disabled"), FALSE);

	RETURN_IF_ERROR(m_title.Set(DOMGetDictionaryString(props, UNI_L("title"), UNI_L(""))));
	RETURN_IF_ERROR(m_id.Set(DOMGetDictionaryString(props, UNI_L("id"), UNI_L(""))));

	DOM_Runtime* runtime = GetRuntime();
	ES_Value icon_val;
	OP_BOOLEAN status = runtime->GetName(props, UNI_L("icon"), &icon_val);
	RETURN_IF_ERROR(status);
	if (status != OpBoolean::IS_TRUE || icon_val.type != VALUE_STRING)
		status = SetIcon(NULL);
	else
		status = SetIcon(icon_val.value.string);
	RETURN_IF_ERROR(status);

	ES_Value prop_val;
	if (runtime->GetName(props, UNI_L("onclick"), &prop_val) == OpBoolean::IS_TRUE)
	{
		ES_PutState status = PutName(UNI_L("onclick"), OP_ATOM_UNASSIGNED, &prop_val, runtime);
		if (status != PUT_SUCCESS)
		{
			OP_ASSERT(status == PUT_NO_MEMORY);
			return OpStatus::ERR_NO_MEMORY;
		}
	}

	if (runtime->GetName(props, UNI_L("contexts"), &prop_val) != OpBoolean::IS_TRUE)
	{
		ES_Object* default_value;
		RETURN_IF_ERROR(runtime->CreateNativeArrayObject(&default_value, 0));
		ES_Value page_str;
		DOMSetString(&page_str, UNI_L("page"));
		RETURN_IF_ERROR(runtime->PutIndex(default_value, 0, page_str));
		DOMSetObject(&prop_val, default_value);
	}
	else
		RETURN_IF_ERROR(CloneArray(prop_val, runtime));

	RETURN_IF_ERROR(runtime->PutName(GetNativeObject(), UNI_L("contexts"), prop_val));

	const uni_char* object_propnames[2]; /* ARRAY OK 2012-06-12 wmaslowski */
	object_propnames[0] = UNI_L("documentURLPatterns");
	object_propnames[1] = UNI_L("targetURLPatterns");

	for (unsigned int i = 0 ; i < ARRAY_SIZE(object_propnames); ++i)
	{
		if (runtime->GetName(props, object_propnames[i], &prop_val) != OpBoolean::IS_TRUE || prop_val.type != VALUE_OBJECT)
			DOMSetNull(&prop_val);
		else
			RETURN_IF_ERROR(CloneArray(prop_val, runtime));

		RETURN_IF_ERROR(runtime->PutName(GetNativeObject(), object_propnames[i], prop_val));
	}

	return OpStatus::OK;
}

const uni_char*
DOM_ExtensionMenuItem::GetTitle()
{
	if (m_title.HasContent())
		return m_title.CStr();
	else
	{
		OpGadget* gadget = m_extension_support->GetGadget();
		OP_ASSERT(gadget);
		BOOL unused;
		const uni_char* wgt_name = gadget->GetClass()->GetAttribute(WIDGET_NAME_SHORT, &unused);
		if (!wgt_name)
			wgt_name = gadget->GetClass()->GetAttribute(WIDGET_NAME_TEXT, &unused);
		return wgt_name;
	}
}

/* virtual */ OP_STATUS
DOM_ExtensionMenuItem::SetIcon(const uni_char* icon_url)
{
	ResetIconImage();

	if (m_icon_loading_listener.InList())
		GetFramesDocument()->StopLoadingInline(m_icon_url, &m_icon_loading_listener);

	if (icon_url && *icon_url)
	{
		OpString gadget_url_str;
		m_extension_support->GetGadget()->GetGadgetUrl(gadget_url_str);
		URL gadget_url = g_url_api->GetURL(gadget_url_str.CStr());
		OpString8 icon_url8; // Temporary workaround for CORE-46995. Use uni_char when it is fixed.
		RETURN_IF_ERROR(icon_url8.SetUTF8FromUTF16(icon_url));
		m_icon_url = g_url_api->GetURL(gadget_url, icon_url8, FALSE, m_extension_support->GetGadget()->UrlContextId());
		if (m_icon_url.GetAttribute(URL::KLoadStatus) != URL_LOADED)
			OpStatus::Ignore(GetFramesDocument()->LoadInline(m_icon_url, &m_icon_loading_listener));
	}
	else
		m_icon_url = URL();
	return OpStatus::OK;
}

/* virtual */ ES_PutState
DOM_ExtensionMenuItem::PutName(const uni_char* property_name, int property_atom, ES_Value* value, ES_Runtime* origining_runtime)
{
	return DOM_ExtensionMenuContext::PutName(property_name, property_atom, value, origining_runtime);
}

/* virtual */ ES_PutState
DOM_ExtensionMenuItem::PutName(OpAtom property_atom, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_atom)
	{
	case OP_ATOM_title:
		if (value->type != VALUE_STRING)
			return PUT_NEEDS_STRING;
		else
			PUT_FAILED_IF_ERROR(m_title.Set(value->value.string));
		return PUT_SUCCESS;

	case OP_ATOM_id:
	case OP_ATOM_parent:
	case OP_ATOM_type:
		return PUT_READ_ONLY;

	case OP_ATOM_icon:
		if (value->type == VALUE_NULL || value->type == VALUE_UNDEFINED)
			OpStatus::Ignore(SetIcon(NULL));
		else if (value->type != VALUE_STRING)
			return PUT_NEEDS_STRING;
		else
			PUT_FAILED_IF_ERROR(SetIcon(value->value.string));
		return PUT_SUCCESS;

	case OP_ATOM_disabled:
		if (value->type != VALUE_BOOLEAN)
			return PUT_NEEDS_BOOLEAN;
		m_disabled = value->value.boolean;
		return PUT_SUCCESS;
	}
	return DOM_ExtensionMenuContext::PutName(property_atom, value, origining_runtime);
}

/* virtual */ ES_GetState
DOM_ExtensionMenuItem::GetName(OpAtom property_atom, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_atom)
	{
		case OP_ATOM_disabled:
			DOMSetBoolean(value, GetDisabled());
			return GET_SUCCESS;

		case OP_ATOM_type:
			DOMSetString(value, TypeEnum2TypeString(m_type));
			return GET_SUCCESS;

		case OP_ATOM_title:
			DOMSetString(value, m_title.CStr());
			return GET_SUCCESS;

		case OP_ATOM_id:
			DOMSetString(value, GetId());
			return GET_SUCCESS;

		case OP_ATOM_icon:
			DOMSetString(value, GetIconURL());
			return GET_SUCCESS;

		case OP_ATOM_parent:
			DOMSetObject(value, m_parent_menu);
			return GET_SUCCESS;
	}
	return DOM_ExtensionMenuContext::GetName(property_atom, value, origining_runtime);
}

/* static */ void
DOM_ExtensionMenuItem::GetMediaType(HTML_Element* element, const uni_char*& type, const uni_char*& url)
{
	type = NULL;
	url = NULL;
	while (element)
	{
		if (element->IsMatchingType(Markup::HTE_IMAGE, NS_HTML))
			type = UNI_L("image");
#ifdef MEDIA_HTML_SUPPORT
		else if (element->IsMatchingType(Markup::HTE_AUDIO, NS_HTML))
			type = UNI_L("audio");
		else if (element->IsMatchingType(Markup::HTE_VIDEO, NS_HTML))
			type = UNI_L("video");
#endif // MEDIA_HTML_SUPPORT
		if (type)
		{
			url = element->GetStringAttr(Markup::HA_SRC, NS_HTML);
			return;
		}
		element = element->ParentActual();
	}
}

OP_BOOLEAN
DOM_ExtensionMenuItem::CheckContexts(OpDocumentContext* document_context, FramesDocument* document, HTML_Element* element)
{
	ES_Value contexts_val;
	DOM_Runtime* runtime = GetRuntime();
	OP_BOOLEAN get_result = runtime->GetName(GetNativeObject(), UNI_L("contexts"), &contexts_val);
	RETURN_IF_ERROR(get_result);
	if (get_result == OpBoolean::IS_TRUE && contexts_val.type == VALUE_OBJECT)
	{
		ES_Object* es_contexts = contexts_val.value.object;
		ES_Value len_val;
		get_result = runtime->GetName(es_contexts, UNI_L("length"), &len_val);
		RETURN_IF_ERROR(get_result);
		if (get_result == OpBoolean::IS_TRUE && len_val.type == VALUE_NUMBER)
		{
			unsigned int len = TruncateDoubleToUInt(len_val.value.number);
			for (unsigned int i = 0; i < len; ++i)
			{
				ES_Value context_val;
				get_result = runtime->GetIndex(es_contexts, i, &context_val);
				RETURN_IF_ERROR(get_result);
				if (get_result == OpBoolean::IS_TRUE && context_val.type == VALUE_STRING)
				{
					BOOL is_in_frame = document ? !!document->GetParentDoc() : FALSE; // Let's assume svg is a frame too...
					switch (MenuContextType type = ContextNameToEnum(context_val.value.string))
					{
					case MENU_CONTEXT_PAGE:
						if (!is_in_frame &&
						    !document_context->HasTextSelection() &&
						    !document_context->HasLink() &&
						    !document_context->HasEditableText() &&
#ifdef MEDIA_HTML_SUPPORT
						    !document_context->HasMedia() &&
#endif // MEDIA_HTML_SUPPORT
						    !document_context->HasImage()
						 )
							return OpBoolean::IS_TRUE;
						break;
					case MENU_CONTEXT_FRAME:
						if (is_in_frame)
							return OpBoolean::IS_TRUE;
						break;
					case MENU_CONTEXT_SELECTION:
						if (document_context->HasTextSelection())
							return OpBoolean::IS_TRUE;
						break;
					case MENU_CONTEXT_LINK:
						if (document_context->HasLink() && CheckLinkPatterns(document_context, document, element) == OpBoolean::IS_TRUE)
							return OpBoolean::IS_TRUE;
						break;
					case MENU_CONTEXT_EDITABLE:
						if (document_context->HasEditableText())
							return OpBoolean::IS_TRUE;
						break;
					case MENU_CONTEXT_IMAGE:
						if (document_context->HasImage())
							return OpBoolean::IS_TRUE;
						break;
#ifdef MEDIA_HTML_SUPPORT
					case MENU_CONTEXT_VIDEO:
					case MENU_CONTEXT_AUDIO:
						if (document_context->HasMedia())
						{
							const uni_char* media_type;
							const uni_char* unused;
							GetMediaType(element, media_type, unused);
							if ((uni_str_eq(media_type, UNI_L("video")) && type == MENU_CONTEXT_VIDEO) ||
								(uni_str_eq(media_type, UNI_L("audio")) && type == MENU_CONTEXT_AUDIO))
								return OpBoolean::IS_TRUE;
						}
						break;
#endif // MEDIA_HTML_SUPPORT
					case MENU_CONTEXT_ALL:
						return OpBoolean::IS_TRUE;
					default:
						OP_ASSERT(FALSE);
					case MENU_CONTEXT_NONE:
						break;
					}
				}
				else if (get_result != OpBoolean::IS_TRUE)
					break; // No support for sparse arrays.
			}
		}
	}
	return OpBoolean::IS_FALSE;
}

/* static */ BOOL
DOM_ExtensionMenuItem::URLMatchesRule(const uni_char* url, const uni_char* rule)
{
	return URLFilter::MatchUrlPattern(url, rule);
}

OP_BOOLEAN
DOM_ExtensionMenuItem::CheckDocumentPatterns(OpDocumentContext* document_context, FramesDocument* document, HTML_Element* element)
{
	ES_Value document_patterns_val;
	OP_BOOLEAN get_result = GetRuntime()->GetName(GetNativeObject(), UNI_L("documentURLPatterns"), &document_patterns_val);
	RETURN_IF_ERROR(get_result);
	if (get_result == OpBoolean::IS_TRUE && document_patterns_val.type == VALUE_OBJECT)
	{
		ES_Object* es_document_patterns = document_patterns_val.value.object;
		ES_Value len_val;
		get_result = GetRuntime()->GetName(es_document_patterns, UNI_L("length"), &len_val);
		RETURN_IF_ERROR(get_result);
		if (get_result == OpBoolean::IS_TRUE && len_val.type == VALUE_NUMBER)
		{
			unsigned int len = TruncateDoubleToUInt(len_val.value.number);
			for (unsigned int i = 0; i < len; ++i)
			{
				ES_Value rule_val;
				get_result = GetRuntime()->GetIndex(es_document_patterns, i, &rule_val);
				RETURN_IF_ERROR(get_result);
				if (get_result == OpBoolean::IS_TRUE && rule_val.type == VALUE_STRING)
				{
					FramesDocument* cur_doc = document;
					while (cur_doc)
					{
						const uni_char* cur_doc_url = cur_doc->GetURL().GetAttribute(URL::KUniName_With_Fragment, TRUE);
						if (URLMatchesRule(cur_doc_url, rule_val.value.string))
							return OpBoolean::IS_TRUE;
						else
							cur_doc = cur_doc->GetParentDoc();
					}
				}
				else if (get_result != OpBoolean::IS_TRUE)
					break; // No support for sparse arrays.
			}
		}
		return OpBoolean::IS_FALSE;
	}
	return OpBoolean::IS_TRUE; // No documentURLPatterns property or it being an object means accept all urls.
}

OP_BOOLEAN
DOM_ExtensionMenuItem::CheckLinkPatterns(OpDocumentContext* document_context, FramesDocument* document, HTML_Element* element)
{
	ES_Value target_patterns_val;
	OP_BOOLEAN get_result = GetRuntime()->GetName(GetNativeObject(), UNI_L("targetURLPatterns"), &target_patterns_val);
	RETURN_IF_ERROR(get_result);
	if (get_result == OpBoolean::IS_TRUE && target_patterns_val.type == VALUE_OBJECT)
	{
		URL anchor_url;
		HTML_Element* cur_elm = element;
		while (cur_elm)
		{
			anchor_url = cur_elm->GetAnchor_URL(document);
			if (!anchor_url.IsEmpty())
				break;
			cur_elm = cur_elm->ParentActual();
		}
		const uni_char* target_addr = anchor_url.GetAttribute(URL::KUniName_With_Fragment, FALSE);

		ES_Object* es_target_patterns = target_patterns_val.value.object;
		ES_Value len_val;
		get_result = GetRuntime()->GetName(es_target_patterns, UNI_L("length"), &len_val);
		RETURN_IF_ERROR(get_result);
		if (get_result == OpBoolean::IS_TRUE && len_val.type == VALUE_NUMBER)
		{
			unsigned int len = TruncateDoubleToUInt(len_val.value.number);
			for (unsigned int i = 0; i < len; ++i)
			{
				ES_Value rule_val;
				get_result = GetRuntime()->GetIndex(es_target_patterns, i, &rule_val);
				RETURN_IF_ERROR(get_result);
				if (get_result == OpBoolean::IS_TRUE && rule_val.type == VALUE_STRING &&
				    URLMatchesRule(target_addr, rule_val.value.string))
					return OpBoolean::IS_TRUE;
				else if (get_result != OpBoolean::IS_TRUE)
					break; // No support for sparse arrays.
			}
		}
		return OpBoolean::IS_FALSE;
	}
	return OpBoolean::IS_TRUE; // No targetURLPatterns property or it being an object means accept all link urls.
}

OP_BOOLEAN
DOM_ExtensionMenuItem::IsAllowedInContext(OpDocumentContext* document_context, FramesDocument* document, HTML_Element* element)
{
	if (m_disabled)
		return OpBoolean::IS_FALSE;

	OP_BOOLEAN status = CheckContexts(document_context, document, element);
	if (status == OpBoolean::IS_TRUE)
		status = CheckDocumentPatterns(document_context, document, element);
	return status;
}

//'all', 'page', 'frame', 'selection', 'link', 'editable', 'image', 'video' and 'audio'

DOM_ExtensionMenuItem::MenuContextType
DOM_ExtensionMenuItem::ContextNameToEnum(const uni_char* string)
{
	if (uni_str_eq(string, UNI_L("page")))
		return MENU_CONTEXT_PAGE;
	else if (uni_str_eq(string, UNI_L("frame")))
		return MENU_CONTEXT_FRAME;
	else if (uni_str_eq(string, UNI_L("selection")))
		return MENU_CONTEXT_SELECTION;
	else if (uni_str_eq(string, UNI_L("link")))
		return MENU_CONTEXT_LINK;
	else if (uni_str_eq(string, UNI_L("editable")))
		return MENU_CONTEXT_EDITABLE;
	else if (uni_str_eq(string, UNI_L("image")))
		return MENU_CONTEXT_IMAGE;
	else if (uni_str_eq(string, UNI_L("video")))
		return MENU_CONTEXT_VIDEO;
	else if (uni_str_eq(string, UNI_L("audio")))
		return MENU_CONTEXT_AUDIO;
	else if (uni_str_eq(string, UNI_L("all")))
		return MENU_CONTEXT_ALL;
	return MENU_CONTEXT_NONE;
}

DOM_ExtensionMenuItem::ItemType
DOM_ExtensionMenuItem::TypeString2TypeEnum(const uni_char* type)
{
	if (uni_str_eq(type, UNI_L("folder")))
		return MENU_ITEM_SUBMENU;
	else if (uni_str_eq(type, UNI_L("line")))
		return MENU_ITEM_SEPARATOR;
	return MENU_ITEM_COMMAND; // default
}

const uni_char*
DOM_ExtensionMenuItem::TypeEnum2TypeString(ItemType type)
{
	switch (type)
	{
		case MENU_ITEM_SUBMENU:
			return UNI_L("folder");
		case MENU_ITEM_SEPARATOR:
			return UNI_L("line");
		default:
			OP_ASSERT(FALSE);
			// Intentional fall through.
		case MENU_ITEM_COMMAND:
			return UNI_L("entry");
	}
}

/* virtual */ void
DOM_ExtensionMenuItem::GCTrace()
{
	GCMark(FetchEventTarget());
	for (UINT32 i = 0; i < m_items.GetCount(); ++i)
		GCMark(m_items.Get(i));

	DOM_ExtensionMenuContext::GCTrace();
}

BOOL
DOM_ExtensionMenuItem::IsAncestorOf(DOM_ExtensionMenuContext* item)
{
	while (item)
	{
		DOM_ExtensionMenuContext* parent = item->IsA( DOM_TYPE_EXTENSION_MENUITEM) ? static_cast<DOM_ExtensionMenuItem*>(item)->m_parent_menu : NULL;

		if (parent == this)
			return TRUE;
		item = parent;
	}
	return FALSE;
}

/* virtual */ DOM_ExtensionMenuContext::CONTEXT_MENU_ADD_STATUS
DOM_ExtensionMenuItem::AddItem(DOM_ExtensionMenuItem* item, DOM_ExtensionMenuItem* before)
{
	OP_ASSERT(item);
	if (m_type != MENU_ITEM_SUBMENU)
		return ContextMenuAddStatus::ERR_WRONG_MENU_TYPE; // Invalid node type
	if ((before && before->m_parent_menu != this) ||
		item == this ||
		item->IsAncestorOf(this))
		return ContextMenuAddStatus::ERR_WRONG_HIERARCHY; // Hierarchy Error
	if (before == item)
		return OpStatus::OK;
	INT32 insert_pos = before ? m_items.Find(before) : m_items.GetCount();

	OP_ASSERT(!item->m_parent_menu || item->m_parent_menu->IsA(DOM_TYPE_EXTENSION_MENUITEM)); // We catch adding top level item earlier.

	DOM_ExtensionMenuItem* parent = static_cast<DOM_ExtensionMenuItem*>(item->m_parent_menu);
	INT32 item_pos = parent ? parent->m_items.Find(item) : -1;
	RETURN_IF_ERROR(m_items.Insert(insert_pos, NULL)); // 'Allocate' slot in vector.
	if (parent == this && item_pos >= insert_pos)
		++item_pos;

	if (parent)
		parent->m_items.Remove(item_pos);
	if (parent == this && item_pos < insert_pos)
		--insert_pos;
	OpStatus::Ignore(m_items.Replace(insert_pos, item));

	item->m_parent_menu = this;
	return OpStatus::OK;
}

/* virtual */ void
DOM_ExtensionMenuItem::RemoveItem(unsigned int index)
{
	if (index < m_items.GetCount())
	{
		DOM_ExtensionMenuItem* item = m_items.Remove(index);
		OP_ASSERT(item);
		item->m_parent_menu = NULL;
	}
}

/* virtual */ DOM_ExtensionMenuItem*
DOM_ExtensionMenuItem::GetItem(unsigned int index)
{
	if (index < m_items.GetCount())
		return m_items.Get(index);
	else
		return NULL;
}

/* virtual */ unsigned int
DOM_ExtensionMenuItem::GetLength()
{
	return m_items.GetCount();
}

OP_STATUS
DOM_ExtensionMenuItem::AppendContextMenu(OpWindowcommanderMessages_MessageSet::PopupMenuRequest::MenuItemList& menu_items, OpDocumentContext* document_context, FramesDocument* document, HTML_Element* element)
{
	OP_BOOLEAN allowed = IsAllowedInContext(document_context, document, element);
	RETURN_IF_ERROR(allowed);
	if (allowed != OpBoolean::IS_TRUE)
		return OpStatus::OK;

	OpAutoPtr<OpWindowcommanderMessages_MessageSet::PopupMenuRequest_MenuItem> menu_item_info(OP_NEW(OpWindowcommanderMessages_MessageSet::PopupMenuRequest::MenuItem, ()));
	RETURN_OOM_IF_NULL(menu_item_info.get());
	RETURN_IF_ERROR(menu_item_info->Construct());
	menu_item_info->SetId(m_type == MENU_ITEM_COMMAND ? DocumentMenuItem::GetId() : 0);
	if (GetTitle())
		RETURN_IF_ERROR(menu_item_info->SetLabel(GetTitle()));
	menu_item_info->SetIconId((m_icon_url.IsValid() && m_icon_url.GetAttribute(URL::KLoadStatus) == URL_LOADED) ? DocumentMenuItem::GetId() : 0);

	if (m_type == MENU_ITEM_SUBMENU)
	{
		OpWindowcommanderMessages_MessageSet::PopupMenuRequest::MenuItemList* menu_item_list = OP_NEW(OpWindowcommanderMessages_MessageSet::PopupMenuRequest::MenuItemList, ());
		if (!menu_item_list || OpStatus::IsError(menu_item_list->Construct()))
		{
			OP_DELETE(menu_item_list);
			return OpStatus::ERR_NO_MEMORY;
		}
		menu_item_info->SetSubMenuItemList(menu_item_list);
	}
	for (UINT32 i = 0; i < m_items.GetCount(); ++i)
	{
		DOM_ExtensionMenuItem* child_menu_item = m_items.Get(i);
		RETURN_IF_ERROR(child_menu_item->AppendContextMenu(*menu_item_info->GetSubMenuItemList(), document_context, document, element));
	}
	RETURN_IF_ERROR(menu_items.GetMenuItemsRef().Add(menu_item_info.get()));
	menu_item_info.release();

	return OpStatus::OK;
}

/* virtual */ OP_STATUS
DOM_ExtensionMenuItem::GetIconBitmap(OpBitmap*& icon_bitmap)
{
	if (m_icon_image.IsEmpty())
	{
		if (m_icon_url.IsValid() && m_icon_url.GetAttribute(URL::KLoadStatus) == URL_LOADED)
		{
			// OOM here shouldnt cancel everything - If we fail with allocating/decoding images it is better to at least show text menus.
			UrlImageContentProvider* url_img_content_provider = UrlImageContentProvider::FindImageContentProvider(m_icon_url);
			if (!url_img_content_provider)
				url_img_content_provider = OP_NEW(UrlImageContentProvider, (m_icon_url));
			if (url_img_content_provider)
			{
				url_img_content_provider->IncRef();
				m_icon_image = url_img_content_provider->GetImage();
				if (OpStatus::IsSuccess(m_icon_image.IncVisible(null_image_listener)))
				{
					if (OpStatus::IsError(m_icon_image.OnLoadAll(url_img_content_provider)) || m_icon_image.IsFailed())
					{
						m_icon_image.DecVisible(null_image_listener);
						m_icon_image.Empty();
					}
				}
				else
					m_icon_image.Empty(); // Unset so we don't call when resetting image.
				url_img_content_provider->DecRef();
			}
		}
	}

	OP_ASSERT(!m_icon_image.IsEmpty() || !m_icon_bitmap); // assert that we never have bitmap if image is empty

	if (!m_icon_bitmap && !m_icon_image.IsEmpty())
		m_icon_bitmap = m_icon_image.GetBitmap(null_image_listener);

	icon_bitmap = m_icon_bitmap;
	return OpStatus::OK;
}

void DOM_ExtensionMenuItem::ResetIconImage()
{
	if (m_icon_bitmap)
	{
		OP_ASSERT(!m_icon_image.IsEmpty());
		m_icon_image.ReleaseBitmap();
	}

	if (!m_icon_image.IsEmpty())
		m_icon_image.DecVisible(null_image_listener);
	m_icon_image.Empty();
}

void
DOM_ExtensionMenuItem::OnClick(OpDocumentContext* document_context, FramesDocument* document, HTML_Element* element)
{
	if (m_disabled || m_type != MENU_ITEM_COMMAND)
		return;

	OpString document_url;
	OP_BOOLEAN status = document->GetURL().GetAttribute(URL::KUniName_With_Fragment, document_url, TRUE);

	DOM_Event* menu_event = NULL;
	if (OpStatus::IsSuccess(status))
		status = DOM_ExtensionMenuEvent_Background::Make(menu_event, document_context, document_url.CStr(), document->GetWindow()->Id(), m_extension_support, GetRuntime());

	if (OpStatus::IsSuccess(status))
	{
		menu_event->InitEvent(ONCLICK, this);
		menu_event->SetCurrentTarget(this);
		menu_event->SetEventPhase(ES_PHASE_ANY);

		status = GetEnvironment()->SendEvent(menu_event);
	}

	if (OpStatus::IsSuccess(status))
	{
		// Send the event also to top level background context.
		DOM_ExtensionBackground* background = m_extension_support->GetBackground();

		if (background)
		{
			DOM_ExtensionMenuContext* menu_context = background->GetContexts()->GetMenuContext();
			if (menu_context)
			{
				DOM_Event* toplevel_menu_event;
				status = DOM_ExtensionMenuEvent_Background::Make(toplevel_menu_event, document_context, document_url.CStr(), document->GetWindow()->Id(), m_extension_support, menu_context->GetRuntime());
				if (OpStatus::IsSuccess(status))
				{
					toplevel_menu_event->InitEvent(ONCLICK, this, NULL, menu_context);
					toplevel_menu_event->SetCurrentTarget(this);
					toplevel_menu_event->SetEventPhase(ES_PHASE_ANY);

					status = menu_context->GetEnvironment()->SendEvent(toplevel_menu_event);
				}
			}
		}
	}

	if (OpStatus::IsSuccess(status))
	{
		// Send the event also to userJS.
		// @note In multi_process this will be done via sending a message.
		DOM_ExtensionScope* userjs = m_extension_support->FindExtensionUserJS(document);
		if (userjs)
		{
			DOM_ExtensionMenuContextProxy* menu_context = userjs->GetMenuContext();
			if (menu_context)
				menu_context->OnMenuItemClicked(static_cast<DocumentMenuItem*>(this)->GetId(), GetId(), document_context, document, element);
		}
	}

	if (OpStatus::IsMemoryError(status))
		g_memory_manager->RaiseCondition(status);
}

#endif // DOM_EXTENSIONS_CONTEXT_MENU_API_SUPPORT