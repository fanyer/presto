/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "modules/logdoc/htm_elm.h"

#include "modules/util/opfile/opfile.h"
#include "modules/util/tempbuf.h"
#include "modules/encodings/encoders/outputconverter.h"
#include "modules/encodings/utility/charsetnames.h"

#include "modules/pi/OpFont.h"
#include "modules/pi/OpSystemInfo.h"

#include "modules/layout/box/box.h"
#include "modules/layout/box/blockbox.h"
#include "modules/layout/box/inline.h"
#include "modules/layout/box/tables.h"
#include "modules/layout/content/content.h"
#include "modules/layout/traverse/traverse.h"
#include "modules/layout/cascade.h"
#include "modules/layout/layout.h"
#include "modules/layout/layout_workplace.h"

#include "modules/logdoc/attr_val.h"
#include "modules/probetools/probepoints.h"
#include "modules/logdoc/src/textdata.h"
#include "modules/logdoc/logdoc_util.h"
#include "modules/logdoc/cssprop_storage.h"
#include "modules/logdoc/xml_ev.h"
#include "modules/logdoc/xmlevents.h"
#include "modules/logdoc/complex_attr.h"
#include "modules/logdoc/src/attribute.h"
#include "modules/logdoc/src/textdata.h"
#include "modules/logdoc/datasrcelm.h"
#include "modules/logdoc/src/xml.h"
#include "modules/logdoc/html5parser.h"
#include "modules/logdoc/elementref.h"
#include "modules/logdoc/stringtokenlist_attribute.h"
#include "modules/logdoc/source_position_attr.h"

#include "modules/url/url_api.h"
#include "modules/url/url2.h"

#include "modules/logdoc/urlimgcontprov.h"
#include "modules/logdoc/helistelm.h"

#include "modules/dom/domevents.h"
#include "modules/formats/argsplit.h"
#include "modules/forms/form.h"
#include "modules/forms/formvaluelistener.h"
#include "modules/forms/formvalueradiocheck.h"
#include "modules/forms/formvaluetext.h"
#include "modules/forms/formvaluetextarea.h"
#include "modules/forms/formvaluelist.h"
#include "modules/forms/formmanager.h"
#include "modules/forms/webforms2number.h"
#include "modules/forms/webforms2support.h"
#include "modules/display/VisDevListeners.h"
#include "modules/olddebug/timing.h"
#include "modules/debug/debug.h"
#include "modules/inputmanager/inputmanager.h"
#include "modules/viewers/viewers.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/locale/locale-enum.h"
#include "modules/logdoc/htm_lex.h"
#include "modules/logdoc/logdoc_constants.h"
#include "modules/logdoc/htm_ldoc.h"
#include "modules/logdoc/html5parser.h"
#include "modules/logdoc/src/html5/html5tokenwrapper.h"
#include "modules/logdoc/html5namemapper.h"
#include "modules/style/css.h"
#include "modules/style/css_media.h"
#include "modules/style/css_dom.h"
#include "modules/style/css_property_list.h"
#include "modules/style/css_style_attribute.h"
#include "modules/style/css_svgfont.h"
#include "modules/style/css_viewport_meta.h"
#include "modules/display/vis_dev.h"
#include "modules/display/styl_man.h"
#include "modules/forms/piforms.h"
#include "modules/forms/formsenum.h"
#include "modules/forms/formvalue.h"
#include "modules/doc/frm_doc.h"
#include "modules/doc/html_doc.h"
#include "modules/prefs/prefsmanager/collections/pc_doc.h"
#include "modules/prefs/prefsmanager/collections/pc_files.h"
#include "modules/prefs/prefsmanager/collections/pc_display.h"
#include "modules/prefs/prefsmanager/collections/pc_js.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/hardcore/mh/messages.h"
#include "modules/dochand/win.h"
#include "modules/dochand/winman.h"
#include "modules/util/handy.h"
#include "modules/util/htmlify.h"
#include "modules/security_manager/include/security_manager.h"
#include "modules/media/media.h"

#include "modules/ecmascript/ecmascript.h"
#include "modules/ecmascript_utils/essched.h"
#include "modules/ecmascript_utils/esthread.h"
#include "modules/ecmascript_utils/esterm.h"
#include "modules/ecmascript_utils/esasyncif.h"
#include "modules/dom/domenvironment.h"
#include "modules/dom/domutils.h"

#include "modules/wand/wandmanager.h"
#include "modules/windowcommander/src/TransferManagerDownload.h"

#ifdef DOCUMENT_EDIT_SUPPORT
# include "modules/documentedit/OpDocumentEdit.h"
#endif // DOCUMENT_EDIT_SUPPORT

#ifdef PICTOGRAM_SUPPORT
# include "modules/logdoc/src/picturlconverter.h"
#endif // PICTOGRAM_SUPPORT

#ifdef QUICK
# ifdef _KIOSK_MANAGER_
#  include "adjunct/quick/managers/KioskManager.h"
# endif // _KIOSK_MANAGER_
# include "adjunct/quick/hotlist/HotlistManager.h"
# include "adjunct/quick/windows/BrowserDesktopWindow.h"
#endif // QUICK

#ifdef SVG_SUPPORT
# include "modules/svg/svg_workplace.h"
# include "modules/svg/SVGManager.h"
# include "modules/svg/SVGContext.h"
#endif // SVG_SUPPORT

#ifdef JS_PLUGIN_SUPPORT
# include "modules/jsplugins/src/js_plugin_context.h"
# include "modules/jsplugins/src/js_plugin_object.h"
#endif // JS_PLUGIN_SUPPORT

#ifdef MEDIA_HTML_SUPPORT
# include "modules/media/mediaelement.h"
# include "modules/media/mediatrack.h"
#endif // MEDIA_HTML_SUPPORT

#ifdef CANVAS_SUPPORT
# include "modules/libvega/src/canvas/canvas.h"
#endif // CANVAS_SUPPORT

#ifdef ENCODINGS_HAVE_TABLE_DRIVEN
# include "modules/encodings/tablemanager/optablemanager.h"
#endif

#ifdef _WML_SUPPORT_
# include "modules/logdoc/wml.h"
# include "modules/windowcommander/src/WindowCommander.h"
#endif // _WML_SUPPORT_

#ifdef ARIA_ATTRIBUTES_SUPPORT
# include "modules/logdoc/ariaenum.h"
#endif // ARIA_ATTRIBUTES_SUPPORT

#ifdef M2_SUPPORT
# include "modules/logdoc/omfenum.h"
#endif // M2_SUPPORT

#include "modules/logdoc/xmlenum.h"
#include "modules/xmlutils/xmldocumentinfo.h"
#include "modules/xmlutils/xmlnames.h"
#include "modules/xmlparser/xmldoctype.h"

#ifdef XSLT_SUPPORT
# include "modules/xslt/xslt.h"
# include "modules/logdoc/src/xsltsupport.h"
#endif // XSLT_SUPPORT

#ifdef _SPAT_NAV_SUPPORT_
# include "modules/spatial_navigation/handler_api.h"
#endif // _SPAT_NAV_SUPPORT_

#ifdef SKINNABLE_AREA_ELEMENT
#include "modules/skin/OpSkinManager.h"
#endif //SKINNABLE_AREA_ELEMENT

#ifdef VEGA_OPPAINTER_SUPPORT
#include "modules/libgogi/pi_impl/mde_opview.h"
#include "modules/libgogi/pi_impl/mde_widget.h"
#endif // VEGA_OPPAINTER_SUPPORT

#define WHITESPACE_L UNI_L(" \t\n\f\r")

#ifdef DRAG_SUPPORT
#include "modules/logdoc/dropzone_attribute.h"
#include "modules/dragdrop/dragdrop_manager.h"
#endif // DRAG_SUPPORT

#ifdef USE_OP_CLIPBOARD
#include "modules/dragdrop/clipboard_manager.h"
#endif // USE_OP_CLIPBOARD

#ifdef _DEBUG
static void CheckIsFormValueType(HTML_Element* he)
{
	if (he->GetNsType() == NS_HTML)
	{
		switch (he->Type())
		{
		case HE_TEXTAREA:
		case HE_OUTPUT:
# if defined(_SSL_SUPPORT_) && !defined(_EXTERNAL_SSL_SUPPORT_)
		case HE_KEYGEN:
# endif
		case HE_INPUT:
		case HE_SELECT:
		case HE_BUTTON:
			return;
		}
	}
	OP_ASSERT(FALSE);
}

# define CHECK_IS_FORM_VALUE_TYPE(he) CheckIsFormValueType(he)
#else // !_DEBUG
# define CHECK_IS_FORM_VALUE_TYPE(he)
#endif // _DEBUG

#if defined(SELFTEST) || defined(_DEBUG)
BOOL HTML_Element::ElementHasFormValue()
{
	return GetSpecialAttr(ATTR_FORM_VALUE, ITEM_TYPE_COMPLEX, NULL, SpecialNs::NS_LOGDOC) != 0;
}
# endif // SELFTEST || _DEBUG


class MediaAttr : public ComplexAttr
{
private:
	CSS_MediaObject* m_media_object;
	OpString m_original_string;
	MediaAttr() : m_media_object(NULL) {}
public:
	static MediaAttr* Create(const uni_char* string, unsigned int string_len)
	{
		MediaAttr* media_attr = OP_NEW(MediaAttr, ());
		if (!media_attr ||
			OpStatus::IsError(media_attr->m_original_string.Set(string, string_len)) ||
			(media_attr->m_media_object = OP_NEW(CSS_MediaObject, ())) == NULL ||
			OpStatus::IsError(media_attr->m_media_object->SetMediaText(string, string_len, TRUE)))
		{
			OP_DELETE(media_attr);
			return NULL;
		}
		return media_attr;
	}

	static MediaAttr* Create(CSS_MediaObject* media_object)
	{
		MediaAttr* media_attr = OP_NEW(MediaAttr, ());
		if (!media_attr)
			return NULL;

		media_attr->m_media_object = media_object;
		return media_attr;
	}

	CSS_MediaObject* GetMediaObject() { return m_media_object; }

	virtual ~MediaAttr() { OP_DELETE(m_media_object); }

	/// Used to check the types of complex attr. SHOULD be implemented
	/// by all subclasses and all must match a unique value.
	///\param[in] type A unique type value.
	// virtual BOOL IsA(int type) { return type == T_UNKNOWN; }

	/// Used to clone HTML Elements. Returning OpStatus::ERR_NOT_SUPPORTED here
	/// will prevent the attribute from being cloned with the html element.
	///\param[out] copy_to A pointer to an new'ed instance of the same object with the same content.
	virtual OP_STATUS	CreateCopy(ComplexAttr **copy_to);

	/// Used to get a string representation of the attribute. The string
	/// value of the attribute must be appended to the buffer. Need not
	/// be implemented.
	///\param[in] buffer A TempBuffer that the serialized version of the obejct will be appended to.
	virtual OP_STATUS	ToString(TempBuffer *buffer);
};

/* virtual */
OP_STATUS	MediaAttr::CreateCopy(ComplexAttr **copy_to)
{
	*copy_to = MediaAttr::Create(m_original_string.CStr(), m_original_string.Length());
	return *copy_to ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}

/* virtual */
OP_STATUS	MediaAttr::ToString(TempBuffer *buffer)
{
	if (!m_original_string.IsEmpty())
	{
		return buffer->Append(m_original_string.CStr());
	}

	TRAPD(status, m_media_object->GetMediaStringL(buffer));
	RETURN_IF_ERROR(status);
	if (!buffer->GetStorage())
		status = buffer->Append(""); // So that we get "" and not NULL
	return status;
}

CSS_MediaObject* HTML_Element::GetLinkStyleMediaObject() const
{
	ComplexAttr* complex_attr = static_cast<ComplexAttr*>(GetAttr(ATTR_MEDIA, ITEM_TYPE_COMPLEX, NULL));
	if (!complex_attr)
		return NULL;
	return static_cast<MediaAttr*>(complex_attr)->GetMediaObject();
}

void HTML_Element::SetLinkStyleMediaObject(CSS_MediaObject* media_object)
{
	MediaAttr* attr = MediaAttr::Create(media_object);
	// No way to signal OOM . :-(
	if (attr)
	{
		SetAttr(ATTR_MEDIA, ITEM_TYPE_COMPLEX, attr, TRUE);
	}
}

URL HTML_Element::GetLinkOriginURL()
{
	if (DataSrc* src_head = GetDataSrc())
		if (!src_head->GetOrigin().IsEmpty())
			return src_head->GetOrigin();

	if (URL* source = GetLinkURL())
	{
		URL origin(*source), target(source->GetAttribute(URL::KMovedToURL, URL::KNoRedirect));

		while (!target.IsEmpty() && target.Type() != URL_JAVASCRIPT && target.Type() != URL_DATA)
		{
			origin = target;
			target = target.GetAttribute(URL::KMovedToURL, URL::KNoRedirect);
		}

		return origin;
	}
	else
	{
		URL empty;
		return empty;
	}
}

void
HTML_Element::RemoveLayoutBox(FramesDocument* doc, BOOL clean_references)
{
	OP_PROBE7(OP_PROBE_HTML_ELEMENT_REMOVELAYOUTBOX);

	OP_ASSERT(layout_box);

	FormObject* form_object = GetFormObject();
	// Any form data must be moved from the widget to the
	// the FormValue object, but don't look at elements
	// HE_INSERTED_BY_LAYOUT since those have FormObject pointers, but none that
	// logdoc/forms should know of. If we look at those we
	// will Unexternalize a FormObject to the wrong FormValue
	if (form_object && GetInserted() != HE_INSERTED_BY_LAYOUT)
	{
		// Optimization: avoid transferring form data from the
		// widget to the html element if we are being deleted
		if (!(doc && doc->IsBeingDeleted()))
		{
			// No form object unless there is a FormValue
			GetFormValue()->Unexternalize(form_object);
		}

		if (doc && doc->IsReflowing())
			layout_box->StoreFormObject();
	}

	if (doc)
	{
		if (clean_references && IsReferenced())
		   CleanReferences(doc);

#ifndef HAS_NOTEXTSELECTION
		doc->RemoveLayoutCacheFromSelection(this);
#endif

		OP_ASSERT(doc->GetLogicalDocument() && doc->GetLogicalDocument()->GetLayoutWorkplace());

		doc->GetLogicalDocument()->GetLayoutWorkplace()->LayoutBoxRemoved(this);

#ifdef ACCESS_KEYS_SUPPORT
		doc->GetHLDocProfile()->RemoveAccessKey(this, TRUE);
#endif // ACCESS_KEYS_SUPPORT
	}

	for (HTML_Element* child = FirstChild(); child; child = child->Suc())
		if (child->GetLayoutBox())
			child->RemoveLayoutBox(doc);

	OP_DELETE(layout_box);

	layout_box = NULL;
}

#ifdef CSS_SCROLLBARS_SUPPORT

void HTML_Element::GetScrollbarColor(ScrollbarColors* colors)
{
	// Run through propertys and set the colors.
	CSS_property_list* cp = GetCSS_Style();
	if (cp)
	{
		CSS_decl* cd = cp->GetFirstDecl();
		while(cd)
		{
			switch(cd->GetProperty())
			{
				case CSS_PROPERTY_scrollbar_base_color:
				case CSS_PROPERTY_scrollbar_face_color:
				case CSS_PROPERTY_scrollbar_arrow_color:
				case CSS_PROPERTY_scrollbar_track_color:
				case CSS_PROPERTY_scrollbar_shadow_color:
				case CSS_PROPERTY_scrollbar_3dlight_color:
				case CSS_PROPERTY_scrollbar_highlight_color:
				case CSS_PROPERTY_scrollbar_darkshadow_color:
					{
						long col = HTM_Lex::GetColValByIndex(cd->LongValue());
						switch(cd->GetProperty())
						{
							case CSS_PROPERTY_scrollbar_base_color: colors->SetBase(col); break;
							case CSS_PROPERTY_scrollbar_face_color: colors->SetFace(col); break;
							case CSS_PROPERTY_scrollbar_arrow_color: colors->SetArrow(col); break;
							case CSS_PROPERTY_scrollbar_track_color: colors->SetTrack(col); break;
							case CSS_PROPERTY_scrollbar_shadow_color: colors->SetShadow(col); break;
							case CSS_PROPERTY_scrollbar_3dlight_color: colors->SetLight(col); break;
							case CSS_PROPERTY_scrollbar_highlight_color: colors->SetHighlight(col); break;
							case CSS_PROPERTY_scrollbar_darkshadow_color: colors->SetDarkShadow(col); break;
						};
					}
					break;
			};
			cd = cd->Suc();
		}
	}
}

#endif // CSS_SCROLLBARS_SUPPORT

void HTML_Element::SetCheckForPseudo(unsigned int pseudo_bits)
{
	if (!GetIsPseudoElement())
	{
		if (pseudo_bits & CSS_PSEUDO_CLASS_FIRST_LETTER)
			packed2.has_first_letter = 1;
		if (pseudo_bits & CSS_PSEUDO_CLASS_FIRST_LINE)
			packed2.has_first_line = 1;
		if (pseudo_bits & CSS_PSEUDO_CLASS_AFTER)
			packed2.has_after = 1;
		if (pseudo_bits & CSS_PSEUDO_CLASS_BEFORE)
			packed2.has_before = 1;
	}

	if (pseudo_bits & (CSS_PSEUDO_CLASS_HOVER | CSS_PSEUDO_CLASS_VISITED |
					   CSS_PSEUDO_CLASS_LINK | CSS_PSEUDO_CLASS_FOCUS |
					   CSS_PSEUDO_CLASS_ACTIVE | CSS_PSEUDO_CLASS__O_PREFOCUS |
					   CSS_PSEUDO_CLASS_DEFAULT | CSS_PSEUDO_CLASS_INVALID |
					   CSS_PSEUDO_CLASS_VALID | CSS_PSEUDO_CLASS_IN_RANGE |
					   CSS_PSEUDO_CLASS_OUT_OF_RANGE | CSS_PSEUDO_CLASS_REQUIRED |
					   CSS_PSEUDO_CLASS_OPTIONAL | CSS_PSEUDO_CLASS_READ_ONLY |
					   CSS_PSEUDO_CLASS_READ_WRITE | CSS_PSEUDO_CLASS_ENABLED |
					   CSS_PSEUDO_CLASS_DISABLED | CSS_PSEUDO_CLASS_CHECKED |
					   CSS_PSEUDO_CLASS_INDETERMINATE))
		packed2.check_dynamic = 1;
}

void HTML_Element::ResetCheckForPseudoElm()
{
	packed2.has_first_letter = 0;
	packed2.has_first_line = 0;
	packed2.has_after = 0;
	packed2.has_before = 0;
}

void HTML_Element::SetCurrentDynamicPseudoClass( unsigned int flags )
{
	packed2.hovered	  = (flags & CSS_PSEUDO_CLASS_HOVER) ? 1 : 0;
	packed2.focused	  = (flags & CSS_PSEUDO_CLASS_FOCUS) ? 1 : 0;
	packed2.activated = (flags & CSS_PSEUDO_CLASS_ACTIVE)? 1 : 0;

	if (flags & (CSS_PSEUDO_CLASS_INVALID | CSS_PSEUDO_CLASS_VALID |
			CSS_PSEUDO_CLASS_DEFAULT |
			CSS_PSEUDO_CLASS_IN_RANGE | CSS_PSEUDO_CLASS_OUT_OF_RANGE |
			CSS_PSEUDO_CLASS_READ_WRITE | CSS_PSEUDO_CLASS_READ_ONLY |
			CSS_PSEUDO_CLASS_REQUIRED | CSS_PSEUDO_CLASS_OPTIONAL |
			CSS_PSEUDO_CLASS_DISABLED | CSS_PSEUDO_CLASS_ENABLED |
			CSS_PSEUDO_CLASS_CHECKED /*| CSS_PSEUDO_CLASS_INDETERMINATE */
			))
	{
		OP_ASSERT(IsFormElement()); // Maybe we have to do if (IsFormElement())...
		// but none of these flags should be set otherwise and if it is a
		// form element then at least one of the flags should be
		// set (either CSS_PSEUDO_CLASS_INVALID or CSS_PSEUDO_CLASS_VALID)
		FormValue* form_value = GetFormValue();
		form_value->SetMarkedPseudoClasses(flags);
	}
}

unsigned int HTML_Element::GetCurrentDynamicPseudoClass()
{
	unsigned int flags = 0;
	if (IsHovered())
		flags |= CSS_PSEUDO_CLASS_HOVER;
	if (IsFocused())
		flags |= CSS_PSEUDO_CLASS_FOCUS;
	if (IsActivated())
		flags |= CSS_PSEUDO_CLASS_ACTIVE;
	if (IsPreFocused())
		flags |= CSS_PSEUDO_CLASS__O_PREFOCUS;
	if (IsFormElement())
	{
		FormValue* form_value = GetFormValue();
		flags |= form_value->GetMarkedPseudoClasses();
	}
	return flags;
}

BOOL HTML_Element::IsDefaultFormElement(FramesDocument* frames_doc)
{
	if (frames_doc->current_default_formelement == this)
	{
		return TRUE;
	}

	if(frames_doc->current_default_formelement ||
		!frames_doc->current_focused_formobject ||
		frames_doc->current_focused_formobject->GetHTML_Element() != this)
	{
		// default form element is somewhere else
		return FALSE;
	}

	// Focused button will behave as "default button"
	OP_ASSERT(GetNsType() == NS_HTML);
	BOOL is_button_type = Type() == HE_BUTTON || Type() == HE_INPUT;
	if (is_button_type)
	{
		InputType inp_type = GetInputType();
		is_button_type =
			inp_type == INPUT_BUTTON || inp_type == INPUT_RESET ||
			inp_type == INPUT_SUBMIT;
	}
	return is_button_type;
}

void HTML_Element::MarkImagesDirty(FramesDocument* doc)
{
	if (GetIsListMarkerPseudoElement())
	{
		/* Currently there is no effective way to check whether particular list marker
		   is an image marker. We can only limit the situation where we have to mark extra dirty.
		   An that is a must if the content type might be unsuitable after the change. */
		if ((doc->GetShowImages() && !(GetLayoutBox() && GetLayoutBox()->GetBulletContent()))
			 || (!doc->GetShowImages() && GetLayoutBox() && GetLayoutBox()->GetBulletContent()))
			MarkExtraDirty(doc);
		else
			MarkDirty(doc);

		return;
	}

	if (GetLayoutBox())
	{
		BOOL is_image_content = GetLayoutBox()->IsContentImage();

#ifdef SVG_SUPPORT
		BOOL is_svg_content = (GetLayoutBox()->GetContent() && GetLayoutBox()->GetContent()->GetSVGContent());

		if (is_svg_content && IsMatchingType(Markup::SVGE_SVG, NS_SVG) && !doc->GetShowImages())
		{
			DocumentManager* docman = doc->GetDocManager();
			if(docman)
			{
				FramesDocElm* fdelm = docman->GetFrame();
				if(fdelm)
				{
					if(fdelm->IsInlineFrame())
					{
						HTML_Element* parent_elm = fdelm->GetHtmlElement();

						if(parent_elm && parent_elm->IsMatchingType(HE_OBJECT, NS_HTML))
						{
							// If it was an <embed> then don't do anything.
							// If we say don't render, then we need to disable plugins
							// that may render the same content. An SVG plugin would
							// render the image as soon as we say we don't handle it natively.
							if (!(parent_elm->GetInserted() == HE_INSERTED_BY_LAYOUT &&
								  parent_elm->Parent() &&
								  parent_elm->Parent()->IsMatchingType(HE_EMBED, NS_HTML)))
							{
								parent_elm->MarkExtraDirty(doc);
							}
						}
					}
				}
			}
		}
		else
#endif // SVG_SUPPORT

		if (is_image_content && IsMatchingType(HE_OBJECT, NS_HTML) && !doc->GetShowImages())
			MarkExtraDirty(doc);
		else
			if (is_image_content || GetLayoutBox()->IsContentEmbed())
			{
				if (HasAttr(ATTR_USEMAP))
					MarkExtraDirty(doc);

#ifdef SVG_SUPPORT
				BOOL svgHandledNatively = FALSE;
				if(GetLayoutBox()->IsContentEmbed() && doc->GetShowImages())
				{
					URL* url = GetEMBED_URL(doc->GetLogicalDocument());
					if(url && url->ContentType() == URL_SVG_CONTENT)
					{
						HTML_ElementType elm_type;
						OP_BOOLEAN resolve_status = GetResolvedObjectType(url, elm_type, doc->GetLogicalDocument());
						if(resolve_status == OpBoolean::IS_TRUE && elm_type == HE_IFRAME)
						{
							MarkExtraDirty(doc);
							svgHandledNatively = TRUE;
						}
					}
				}

				if(!svgHandledNatively)
#endif // SVG_SUPPORT
					MarkDirty(doc);
			}
			else
				if (IsMatchingType(HE_OBJECT, NS_HTML) && doc->GetShowImages())
				{
					URL* inline_url = GetUrlAttr(ATTR_DATA, NS_IDX_HTML, doc->GetLogicalDocument());
					if (inline_url)
					{
						HTML_ElementType element_type;
						OP_BOOLEAN resolve_status = GetResolvedObjectType(inline_url, element_type, doc->GetLogicalDocument());

						if (resolve_status == OpBoolean::IS_FALSE ||
							(resolve_status == OpBoolean::IS_TRUE &&
							 (element_type == HE_IMG
#ifdef SVG_SUPPORT
							 || (inline_url->ContentType() == URL_SVG_CONTENT && element_type == HE_IFRAME)
#endif // SVG_SUPPORT
							 )))
							MarkExtraDirty(doc);
					}
				}
				else if (HasAttr(ATTR_USEMAP))
				{
					MarkExtraDirty(doc);
				}
				else
					for (HTML_Element* he = FirstChild(); he; he = he->Suc())
						he->MarkImagesDirty(doc);
	}
}

void
HTML_Element::Remove(const DocumentContext &context, BOOL going_to_delete /* = FALSE */)
{
#ifdef DEBUG
	if (context.logdoc && context.logdoc->GetLayoutWorkplace())
	{
		OP_ASSERT(!context.logdoc->GetLayoutWorkplace()->IsReflowing());
	}
#endif

#ifdef NS4P_COMPONENT_PLUGINS
	if (context.logdoc && context.logdoc->GetLayoutWorkplace())
		if (context.logdoc->GetLayoutWorkplace()->IsReflowing() || context.logdoc->GetLayoutWorkplace()->IsTraversing())
			return;
#endif // NS4P_COMPONENT_PLUGINS

	if (GetNsType() == NS_HTML)
	{
		// FIXME: This could should move to HandleDocumentTreeChange or OutSafe
		switch (Type())
		{
		case HE_OPTION:
			{
				for (HTML_Element *parent = Parent(); parent; parent = parent->Parent())
					if (parent->Type() == HE_SELECT)
					{
						if (parent->GetLayoutBox())
							parent->GetLayoutBox()->RemoveOption(this);
						break;
					}
			}
			break;
		case HE_OPTGROUP:
			{
				for (HTML_Element *parent = Parent(); parent; parent = parent->Parent())
					if (parent->Type() == HE_SELECT)
					{
						if (parent->GetLayoutBox())
							parent->GetLayoutBox()->RemoveOptionGroup(this);
						break;
					}
			}
			break;

		default:
			break;
		}
	} // end NS_HTML

	HTML_Element* parent = Parent();
	FramesDocument *frames_doc = context.frames_doc;
	BOOL in_document = FALSE;
	if (frames_doc)
	{
#ifdef DOM_FULLSCREEN_MODE
		if (frames_doc->GetFullscreenElement() != NULL &&
			(frames_doc->GetFullscreenElement() == this || IsAncestorOf(frames_doc->GetFullscreenElement())))
			frames_doc->DOMExitFullscreen(NULL);
#endif // DOM_FULLSCREEN_MODE

		if (LogicalDocument* logdoc = context.logdoc)
		{
			if (parent && logdoc->GetRoot())
			{
				in_document = logdoc->GetRoot()->IsAncestorOf(parent);
				if (in_document)
				{
					if (parent->GetInserted() == HE_INSERTED_BY_LAYOUT)
						parent->MarkExtraDirty(frames_doc);
					else
					{
						if (GetLayoutBox())
							/* Removing this element may cause two anonymous layout boxes (table
							   objects or flex items), one preceding and one succeeding this
							   element, to be joined into one anonymous table object. Search for a
							   preceding anonymous table object, and if we find one, mark it extra
							   dirty, and the layout engine will take care of the rest. */

							for (HTML_Element* sibling = Pred(); sibling; sibling = sibling->Pred())
							{
								/* Find the nearest sibling layout box. Skip extra dirty boxes; we
								   cannot tell what they will become after reflow (maybe they have
								   become display:none, for instance). */

								if (sibling->GetLayoutBox() && !sibling->IsExtraDirty())
								{
									if (sibling->GetInserted() == HE_INSERTED_BY_LAYOUT &&
										sibling->GetLayoutBox()->IsAnonymousWrapper())
										sibling->MarkExtraDirty(context.frames_doc);

									break;
								}
							}

#ifdef SVG_SUPPORT
						// SVG size isn't affected by their children and the
						// internal invalidation is done in HandleDocumentTreeChange
						if (!parent->IsMatchingType(Markup::SVGE_SVG, NS_SVG))
#endif // SVG_SUPPORT
							parent->MarkDirty(frames_doc, TRUE, TRUE);
					}

					int sibling_subtrees = logdoc->GetHLDocProfile()->GetCssSuccessiveAdjacent();

					if (sibling_subtrees < INT_MAX && context.hld_profile->GetCSSCollection()->MatchFirstChild() && IsFirstChild())
						/* If the element removed was first-child, its sibling (if any) now becomes
						   first-child. Must recalculate its properties, and all siblings of it that may be
						   affected by this change. */

						++ sibling_subtrees;

					MarkPropsDirty(frames_doc, sibling_subtrees);
				}
			}
			logdoc->RemoveFromParseState(this);
		}
	}

	OutSafe(context, going_to_delete);

	if (in_document)
	{
#ifdef XML_EVENTS_SUPPORT
		for (XML_Events_Registration *registration = frames_doc->GetFirstXMLEventsRegistration();
		     registration;
		     registration = (XML_Events_Registration *) registration->Suc())
		{
			if (OpStatus::IsMemoryError(registration->HandleElementRemovedFromDocument(frames_doc, this)))
				frames_doc->GetWindow()->RaiseCondition(OpStatus::ERR_NO_MEMORY);
		}
#endif // XML_EVENTS_SUPPORT

		ES_Thread* thread = context.environment ? context.environment->GetCurrentScriptThread() : NULL;
		if (OpStatus::IsMemoryError(HandleDocumentTreeChange(context, parent, this, thread, FALSE)))
			frames_doc->GetWindow()->RaiseCondition(OpStatus::ERR_NO_MEMORY);
	}
}

void
HTML_Element::DisableContent(FramesDocument* doc)
{
	BOOL is_leaving_page = doc->IsUndisplaying();

	// Disable this subtree
	HTML_Element* stop = (HTML_Element*)NextSibling();
	for (HTML_Element *current = this; current != stop; current = (HTML_Element*)current->Next())
	{
		if (current->layout_box)
			current->layout_box->DisableContent(doc);

		if (is_leaving_page && current->Type() == HE_INPUT && current->GetInputType() == INPUT_PASSWORD)
		{
			// We don't want to leave any passwords in the history
			current->GetFormValue()->SetValueFromText(current, NULL);
		}
	}
}

OP_STATUS
HTML_Element::EnableContent(FramesDocument* doc)
{
	if (layout_box)
		if (layout_box->EnableContent(doc) == OpStatus::ERR_NO_MEMORY)
			return OpStatus::ERR_NO_MEMORY;

	for (HTML_Element* he = FirstChild(); he; he = he->Suc())
		if (he->EnableContent(doc) == OpStatus::ERR_NO_MEMORY)
			return OpStatus::ERR_NO_MEMORY;

	return OpStatus::OK;
}

int
HTML_Element::GetTextContentLength()
{
	int len = 0;
	if (Type() == HE_TEXT)
	{
		return data.text->GetTextLen();
	}
	else
		for (HTML_Element* element = FirstChild(); element; element = element->Suc())
		{
			if (!element->GetIsFirstLetterPseudoElement())
				len += element->GetTextContentLength();
		}

	return len;
}

int
HTML_Element::GetTextContent(uni_char* buf, int buf_len)
{
	if (buf_len < 1)
		return 0;

	int len = 0;
	if (Type() == HE_TEXT)
	{
		const uni_char* text = LocalContent();
		if (text)
		{
			len = uni_strlen(text);
			if (len >= buf_len)
				len = buf_len - 1;
			uni_strncpy(buf, text, len);
			buf[len] = '\0';
		}
	}
	else if (!IsScriptElement())
	{
		for (HTML_Element* element = FirstChild(); element; element = element->Suc())
		{
			if (!element->GetIsFirstLetterPseudoElement())
				len += element->GetTextContent(buf + len, buf_len - len);
		}
	}

	return len;
}

Box*
HTML_Element::GetInnerBox(int x, int y, FramesDocument* doc, BOOL text_boxes /* = TRUE */)
{
	if (layout_box)
	{
		IntersectionObject intersection(doc, LayoutCoord(x), LayoutCoord(y), text_boxes);

		intersection.Traverse(this);

		return intersection.GetInnerBox();
	}
	else
		return NULL;
}

FormObject*
HTML_Element::GetFormObject()
{
	return layout_box ? layout_box->GetFormObject() : GetFormObjectBackup();
}

FormValue*
HTML_Element::GetFormValue()
{
	CHECK_IS_FORM_VALUE_TYPE(this);
	FormValue* form_value = static_cast<FormValue*>(GetSpecialAttr(ATTR_FORM_VALUE,
		ITEM_TYPE_COMPLEX, NULL, SpecialNs::NS_LOGDOC));
	OP_ASSERT(form_value); // Nobody should call GetFormValue unless they
						  // know it's a form thing and then there should
						  // always be an ATTR_FORM_VALUE. If it's
						  // missing it's a bug that should be fixed

	return form_value;
}

void
HTML_Element::ReplaceFormValue(FormValue* value)
{
	SetSpecialAttr(ATTR_FORM_VALUE, ITEM_TYPE_COMPLEX, value, TRUE, SpecialNs::NS_LOGDOC);

	if (layout_box)
		if (FormObject *formobject = GetFormObject())
			value->Externalize(formobject);
}

OP_STATUS
HTML_Element::ConstructFormValue(HLDocProfile *hld_profile, FormValue*& value)
{
	CHECK_IS_FORM_VALUE_TYPE(this);
	// It can exist if someone has changed type attribute so that we
	// have to recreate the FormValue.
//	OP_ASSERT(!ElementHasFormValue()); // Not already set

	RETURN_IF_ERROR(FormValue::ConstructFormValue(this, hld_profile, value));
	OP_ASSERT(value);
	return OpStatus::OK;
}

void
HTML_Element::RemoveCachedTextInfo(FramesDocument* doc)
{
	if (layout_box)
		if (layout_box->RemoveCachedInfo() && doc)
			MarkDirty(doc);

	for (HTML_Element* child = FirstChild(); child; child = child->Suc())
		child->RemoveCachedTextInfo(doc);
}


void
HTML_Element::ERA_LayoutModeChanged(FramesDocument* doc, BOOL apply_doc_styles_changed, BOOL support_float, BOOL column_strategy_changed)
{
	OP_ASSERT(doc->GetDocRoot() == this);
	doc->GetLogicalDocument()->GetLayoutWorkplace()->ERA_LayoutModeChanged(apply_doc_styles_changed, support_float, column_strategy_changed);
}

OP_STATUS
HTML_Element::LoadAllCSS(const DocumentContext& context)
{
	OP_PROBE4(OP_PROBE_HTML_ELEMENT_LOADALLCSS);

	OP_STATUS status = OpStatus::OK;

	if (IsStyleElement())
	{
		if (!GetCSS())
			status = LoadStyle(context, FALSE);
	}
	else
		for (HTML_Element* he = FirstChild(); status != OpStatus::ERR_NO_MEMORY && he; he = he->Suc())
			status = he->LoadAllCSS(context);

	return status;
}

BOOL HTML_Element::HasWhiteSpaceOnly()
{
	if (Type() == HE_TEXTGROUP)
	{
		for (HTML_Element *child = FirstChild(); child; child = child->Suc())
			if (!child->HasWhiteSpaceOnly())
				return FALSE;
	}
	else
	{
		const uni_char* txt = LocalContent();
		if (txt)
		{
			while (*txt)
			{
				if (*txt != ' ' && !uni_iscntrl(*txt))
					return FALSE;
				txt++;
			}
		}
	}
	return TRUE;
}


BOOL HTML_Element::CanUseTextCursor()
{
#ifdef SVG_SUPPORT
	if(GetNsType() == NS_SVG)
		return (g_svg_manager->IsTextContentElement(this) && g_svg_manager->IsVisible(this));
	else
#endif //SVG_SUPPORT
	{
		return (layout_box && layout_box->IsTextBox());
	}
}

const uni_char*
HTML_Element::TextContent() const
{
	return Type() == HE_BR ? UNI_L("\n") : LocalContent();
}

#ifdef _WML_SUPPORT_
const uni_char* HTML_Element::GetWmlName() const
{
	const uni_char* name_str = GetStringAttr(WA_NAME, NS_IDX_WML);

	if (name_str)
		return name_str;
	else
		return GetName();
}

const uni_char* HTML_Element::GetWmlValue() const
{
	const uni_char* val_str = GetStringAttr(WA_VALUE, NS_IDX_WML);

	if (val_str)
		return val_str;
	else
		return GetValue();
}
#endif //_WML_SUPPORT_

HTML_Element* HTML_Element::GetAnchorTags(FramesDocument *document/*=NULL*/)
{
	HTML_Element* helm = this;
	while (helm)
	{
		if (helm->GetAnchor_HRef(document))
			return helm;

		helm = helm->ParentActualStyle();
	}
	return NULL;
}

HTML_Element* HTML_Element::GetA_Tag()
{
	HTML_Element* h = this;
	do
	{
		if (h->IsMatchingType(HE_A, NS_HTML))
			break;
		h = h->ParentActualStyle();
	}
	while (h);

	return h;
}

// returns a pointer to start of refresh url, or null pointer if no url specified.
const uni_char* ParseRefreshUrl(const uni_char* buf, int &refresh_url_len, short &refresh_int)
{
	// https://bugzilla.mozilla.org/show_bug.cgi?id=170021 has a decent
	// summary of what Mozilla implements. Follow it here (which is
	// what WebKit also does.)

#define FIND_CHAR(ptr, ch) { while (*ptr && *ptr != ch) ptr++; }
#define SKIP_SPACES(ptr) { while (uni_isspace(*ptr)) ptr++; }

	refresh_url_len = 0;
	refresh_int = uni_atoi(buf);

	const uni_char* tmp = buf;

	while (*tmp && *tmp != ';' && *tmp != ',' && !uni_isspace(*tmp))
		tmp++;
	SKIP_SPACES(tmp);
	if(*tmp == ';' || *tmp == ',')
		tmp++;

	SKIP_SPACES(tmp);
	if(uni_strni_eq(tmp, "URL", 3))
	{
		tmp+=3;
		SKIP_SPACES(tmp);

		if (*tmp == '=')
			tmp++;
		else
			return NULL;

		SKIP_SPACES(tmp);
	}

	refresh_url_len = uni_strlen(tmp);
	while (refresh_url_len > 0 && uni_isspace(tmp[refresh_url_len-1]))
		refresh_url_len--;
	return tmp;
}

const uni_char *GetInputTypeString(InputType type)
{
	switch(type)
	{
	case INPUT_TEXT:
		return UNI_L("text");
	case INPUT_CHECKBOX:
		return UNI_L("checkbox");
	case INPUT_RADIO:
		return UNI_L("radio");
	case INPUT_SUBMIT:
		return UNI_L("submit");
	case INPUT_RESET:
		return UNI_L("reset");
	case INPUT_HIDDEN:
		return UNI_L("hidden");
	case INPUT_IMAGE:
		return UNI_L("image");
	case INPUT_PASSWORD:
		return UNI_L("password");
	case INPUT_BUTTON:
		return UNI_L("button");
	case INPUT_FILE:
		return UNI_L("file");
	case INPUT_URI:
		return UNI_L("url");
	case INPUT_DATE:
		return UNI_L("date");
	case INPUT_WEEK:
		return UNI_L("week");
	case INPUT_TIME:
		return UNI_L("time");
	case INPUT_EMAIL:
		return UNI_L("email");
	case INPUT_NUMBER:
		return UNI_L("number");
	case INPUT_RANGE:
		return UNI_L("range");
	case INPUT_MONTH:
		return UNI_L("month");
	case INPUT_DATETIME:
		return UNI_L("datetime");
	case INPUT_DATETIME_LOCAL:
		return UNI_L("datetime-local");
	case INPUT_COLOR:
		return UNI_L("color");
	case INPUT_SEARCH:
		return UNI_L("search");
	case INPUT_TEL:
		return UNI_L("tel");
	case INPUT_TEXTAREA:
		break;
	}
	return NULL;
}

const uni_char *GetDirectionString(short value)
{
	switch(value)
	{
	case ATTR_VALUE_up: return UNI_L("up");
	case ATTR_VALUE_down: return UNI_L("down");
	case ATTR_VALUE_left: return UNI_L("left");
	case ATTR_VALUE_right: return UNI_L("right");
	}
	return NULL;
}

const uni_char *GetLiTypeString(short value)
{
	switch(value)
	{
	case CSS_VALUE_lower_greek:
		return UNI_L("lower_greek");
	case CSS_VALUE_armenian:
		return UNI_L("armenian");
	case CSS_VALUE_georgian:
		return UNI_L("georgian");
	case CSS_VALUE_disc:
		return UNI_L("disc");
	case CSS_VALUE_square:
		return UNI_L("square");
	case CSS_VALUE_circle:
		return UNI_L("circle");
	case CSS_VALUE_lower_alpha:
	case CSS_VALUE_lower_latin:
		return UNI_L("a");
	case CSS_VALUE_upper_alpha:
	case CSS_VALUE_upper_latin:
		return UNI_L("A");
	case CSS_VALUE_lower_roman:
		return UNI_L("i");
	case CSS_VALUE_upper_roman:
		return UNI_L("I");
//	case CSS_VALUE_decimal_leading_zero:
//	case CSS_VALUE_hebrew:
//	case CSS_VALUE_cjk_ideographic:
//	case CSS_VALUE_hiragana:
//	case CSS_VALUE_katakana:
//	case CSS_VALUE_hiragana_iroha:
//	case CSS_VALUE_katakana_iroha:
	case CSS_VALUE_decimal:
	default:
		return UNI_L("1");
	}
}

/**
 * A short text with an unterminated entity that should be decoded.
 */
OP_STATUS HTML_Element::ConstructUnterminatedText(const uni_char* text, unsigned text_len)
{
	OP_ASSERT(text_len <= SplitTextLen || !"Use AppendText/SetText if you are going to insert long text chunks");

	css_properties = NULL;
	layout_box = NULL;

	packed1_init = 0;
	packed2_init = 0;

	g_ns_manager->GetElementAt(GetNsIdx())->DecRefCount();
	SetNsIdx(NS_IDX_DEFAULT);
	g_ns_manager->GetElementAt(NS_IDX_DEFAULT)->IncRefCount();

	SetType(HE_TEXT);

	data.text = NEW_TextData(());
	if( ! data.text || data.text->Construct(text, text_len, TRUE, FALSE, TRUE) == OpStatus::ERR_NO_MEMORY )
	{
		DELETE_TextData(data.text);
		data.text = NULL;
		return OpStatus::ERR_NO_MEMORY;
	}

	return OpStatus::OK;
}

/* static */
HTML_Element* HTML_Element::CreateText(HLDocProfile* hld_profile, const uni_char* text, unsigned text_len, BOOL resolve_entities, BOOL is_cdata, BOOL expand_wml_vars)
{
	OP_ASSERT(!expand_wml_vars || hld_profile || !"Can't expand wml variables without an hld_profile");
	// Create empty text node and then add the text to it. AppendText does all the complicated stuff

	HTML_Element *new_elm = NEW_HTML_Element();
	if (new_elm &&
		(OpStatus::IsMemoryError(new_elm->Construct((const uni_char*)NULL, 0, resolve_entities, is_cdata)) ||
		 OpStatus::IsMemoryError(new_elm->AppendText(hld_profile, text, text_len, resolve_entities, is_cdata, expand_wml_vars))))
	{
		DELETE_HTML_Element(new_elm);
		new_elm = NULL;
	}

	return new_elm;
}

OP_STATUS HTML_Element::AppendText(HLDocProfile* hld_profile, const uni_char* text, unsigned text_len, BOOL resolve_entities, BOOL is_cdata, BOOL expand_wml_vars, BOOL is_foster_parented)
{
	OP_ASSERT(IsText() && !(Parent() && Parent()->IsText()));

	HTML_Element* group = Type() == HE_TEXTGROUP ? this : NULL;
	HTML_Element* last_text = group ? LastLeaf() : this;

	OP_ASSERT(!group || group->GetIsCDATA() == is_cdata || !"Adding cdata to a non cdata text or the opposite");
	OP_ASSERT(!last_text || last_text->Type() != HE_TEXT || !last_text->data.text || last_text->data.text->GetIsCDATA() == is_cdata || !"Adding cdata to a non cdata text or the opposite");

	TempBuffer temp_buffer; // Only needed if we have previous data
							// that will have to be put in front of the new buffer or for wml variable substitution
	if (resolve_entities &&
		last_text && last_text->Type() == HE_TEXT &&
		last_text->data.text &&
		last_text->data.text->IsUnterminatedEntity())
	{
		OP_ASSERT(group);

		// See if the new text will make it more terminated. This should seldom run so it shouldn't matter that it's a little expensive
		RETURN_IF_ERROR(temp_buffer.Append(last_text->data.text->GetUnresolvedEntity()));
		RETURN_IF_ERROR(temp_buffer.Append(text, text_len));
		HTML_Element* unterminated_elm = last_text;
		last_text = last_text->Pred();

		if (hld_profile)
		{
			/* We need to make sure that the element is removed from the cascade. */
			unterminated_elm->Remove(hld_profile->GetFramesDocument());

			/* Although ::Remove already called ::Clean, we need the return value to
			   determine if we can really Free the element. */
			if (unterminated_elm->Clean(hld_profile->GetFramesDocument()))
			{
				unterminated_elm->Free(hld_profile->GetFramesDocument());
			}
		}
		else
		{
			unterminated_elm->Out();
			DELETE_HTML_Element(unterminated_elm);
		}
		text = temp_buffer.GetStorage();
		text_len = temp_buffer.Length();
	}

	if (text_len == 0 && Type() == HE_TEXT && !data.text)
	{
		// Newly created element with NULL TextData -> make it non-null even though we have no string
		TextData* textdata = NEW_TextData(());
		if (!textdata ||
			OpStatus::IsError(textdata->Construct(NULL, 0, FALSE, is_cdata, FALSE)))
		{
			DELETE_TextData(textdata);
			return OpStatus::ERR_NO_MEMORY;
		}

		data.text = textdata;
		return OpStatus::OK;
	}

	BOOL need_group = FALSE; // Assume the best
	while (text_len > 0)
	{
		if (need_group)
		{
			// Need to convert the text to a textgroup to fit the new text
			OP_ASSERT(!group);
			OP_ASSERT(last_text == this);

			need_group = FALSE;

			HTML_Element *new_elm = NEW_HTML_Element();
			if (!new_elm ||
				OpStatus::IsMemoryError(new_elm->Construct((const uni_char*)NULL, 0, resolve_entities, is_cdata)))
			{
				if (new_elm)
				{
					DELETE_HTML_Element(new_elm);
				}
				return OpStatus::ERR_NO_MEMORY;
			}

			// Move the text to the child
			DELETE_TextData(new_elm->data.text);
			new_elm->data.text = last_text->data.text;
			last_text->data.text = NULL;
			last_text->SetType(HE_TEXTGROUP);
			group = last_text;
			new_elm->Under(group);

#ifdef DELAYED_SCRIPT_EXECUTION
			if (hld_profile && hld_profile->ESIsParsingAhead() && is_foster_parented)
				RETURN_IF_ERROR(hld_profile->ESAddFosterParentedElement(new_elm));
#endif // DELAYED_SCRIPT_EXECUTION
			new_elm->SetInserted(GetInserted());

			if (hld_profile)
				hld_profile->GetFramesDocument()->OnTextConvertedToTextGroup(group);

			last_text = new_elm;

			if (is_cdata)
			{
				group->SetSpecialBoolAttr(ATTR_CDATA, TRUE, SpecialNs::NS_LOGDOC);
			}

			if (hld_profile)
			{
				group->MarkExtraDirty(hld_profile->GetFramesDocument());
			}

			OP_ASSERT(group);
		}

#if defined _WML_SUPPORT_
		if (expand_wml_vars)
		{
			OP_ASSERT(hld_profile);

			if (hld_profile && WML_Context::NeedSubstitution(text, text_len))
			{
				if (!group)
				{
					need_group = TRUE;
					continue;
				}

				AutoTempBuffer subst_buf;

				// add just a little bit more space than the original
				RETURN_IF_ERROR(subst_buf->Expand(text_len + 64));

				uni_char *subst_txt = subst_buf->GetStorage();
				unsigned int subst_len = hld_profile->WMLGetContext()
					->SubstituteVars(text, text_len, subst_txt, subst_buf->GetCapacity());

				OP_STATUS status = OpStatus::OK;
				// Store the original value in an attribute so that we can redo the substitution
				uni_char *content = UniSetNewStrN(text, text_len);
				if (!content ||
					group->SetSpecialAttr(ATTR_TEXT_CONTENT, ITEM_TYPE_STRING, (void*)content, TRUE, SpecialNs::NS_LOGDOC) == -1)
				{
					OP_DELETEA(content);
					status = OpStatus::ERR_NO_MEMORY;
				}
				else
				{
					temp_buffer.Clear();
					status = temp_buffer.Append(subst_txt, subst_len);
				}

				RETURN_IF_ERROR(status);

				text = temp_buffer.GetStorage();
				text_len = temp_buffer.Length();
				if (text_len == 0)
				{
					if (last_text && last_text->Type() == HE_TEXT && !last_text->data.text)
					{
						// Need to make it non-null. Coming from CreateText it's our job to
						// do that.
						TextData* textdata = NEW_TextData(());
						if (!textdata ||
							OpStatus::IsError(textdata->Construct(NULL, 0, FALSE, is_cdata, FALSE)))
						{
							DELETE_TextData(textdata);
							return OpStatus::ERR_NO_MEMORY;
						}

						last_text->data.text = textdata;
					}
					break;
				}
			}
			expand_wml_vars = FALSE;
		}
#endif // _WML_SUPPORT_


#if 0 // FIXME :Optimization not yet implemented.
		if (!group && last_text)
		{
			// Here we might be able to reuse the existing HE_TEXT instead of creating a new one
			// Can just append to the existing text element, but if the first half is processed and the second half is not, this becomes messy
			....
				...
				return OpStatus::OK;
		}
#endif // 0

		unsigned len_for_this = MIN(SplitTextLen, text_len);

		// Don't cut an entity in two if it can be avoided;
		if (resolve_entities)
		{
			uni_char *buf_p = const_cast<uni_char*>(text+len_for_this);
			if (HTM_Lex::ScanBackIfEscapeChar(text, &buf_p, text_len > len_for_this))
			{
				if (text == buf_p)
				{
					if (!group)
					{
						need_group = TRUE;
						continue;
					}
					OP_ASSERT(group);
					// The unterminated entity was all there was. Put it in a special text node.
					HTML_Element *new_elm;
					if (last_text && last_text->Type() == HE_TEXT && !last_text->data.text)
					{
						// (Re)use the empty HE_TEXT we got from CreateText
						new_elm = last_text;
					}
					else
					{
						new_elm = NEW_HTML_Element();
					}
					if (!new_elm ||
						OpStatus::IsMemoryError(new_elm->ConstructUnterminatedText(text, len_for_this)))
					{
						if (new_elm &&
							new_elm != last_text &&
							new_elm->Clean(hld_profile->GetFramesDocument()))
							new_elm->Free(hld_profile->GetFramesDocument());
						hld_profile->SetIsOutOfMemory(TRUE);
						return OpStatus::ERR_NO_MEMORY;
					}
					if (new_elm != last_text)
					{
						new_elm->Under(group);
						HE_InsertType inserted = GetInserted();
#ifdef DELAYED_SCRIPT_EXECUTION
						if (hld_profile && hld_profile->ESIsParsingAhead())
						{
							inserted = HE_INSERTED_BY_PARSE_AHEAD;
							if (is_foster_parented)
								RETURN_IF_ERROR(hld_profile->ESAddFosterParentedElement(new_elm));
						}
#endif // DELAYED_SCRIPT_EXECUTION

						new_elm->SetInserted(inserted);
					}
					return OpStatus::OK;
				}
				len_for_this = buf_p - text;
			}
		}

		// Need a new element (or more)
		OP_ASSERT(len_for_this > 0);

		if (last_text && last_text->Type() == HE_TEXT &&
			(!last_text->data.text ||
				(last_text->data.text->GetTextLen() == 0 &&
				 !last_text->data.text->IsUnterminatedEntity())))
		{
			// Just insert the data into the existing node
			TextData* textdata = NEW_TextData(());
			if (!textdata ||
				OpStatus::IsError(textdata->Construct(text, len_for_this, resolve_entities, is_cdata, FALSE)))
			{
				DELETE_TextData(textdata);
				return OpStatus::ERR_NO_MEMORY;
			}

			DELETE_TextData(last_text->data.text);
			last_text->data.text = textdata;
		}
		else if (!group)
		{
			need_group = TRUE;
			continue;
		}
		else
		{
			HTML_Element *new_elm = NEW_HTML_Element();
			if (!new_elm ||
				OpStatus::IsMemoryError(new_elm->Construct(text, len_for_this, resolve_entities, is_cdata)))
			{
				if (new_elm)
				{
					DELETE_HTML_Element(new_elm);
				}
				return OpStatus::ERR_NO_MEMORY;
			}

			new_elm->Under(group);
			HE_InsertType inserted = GetInserted();
#ifdef DELAYED_SCRIPT_EXECUTION
			if (hld_profile && hld_profile->ESIsParsingAhead())
			{
				inserted = HE_INSERTED_BY_PARSE_AHEAD;
				if (is_foster_parented)
					RETURN_IF_ERROR(hld_profile->ESAddFosterParentedElement(new_elm));
			}
#endif // DELAYED_SCRIPT_EXECUTION

			new_elm->SetInserted(inserted);

			if (hld_profile)
				new_elm->MarkExtraDirty(hld_profile->GetFramesDocument());
		}

		text += len_for_this;
		text_len -= len_for_this;
	}

#ifdef _DEBUG
	HTML_Element* stop = static_cast<HTML_Element*>(NextSibling());
	HTML_Element* it = this;
	while (it != stop)
	{
		if (it->Type() == HE_TEXT)
		{
			OP_ASSERT(it->data.text);
		}
		it = it->Next();
	}
#endif // _DEBUG

	return OpStatus::OK;
}

const uni_char* GetMetaValue(const uni_char* &tmp, UINT &len, BOOL &end_found)
{
	const uni_char* val = 0;
	end_found = FALSE;
	const uni_char* current = tmp;		// avoid writing back to tmp all the time

	while (uni_isspace(*current))
		current++;

	if (*current == '=')
	{
		uni_char quote_char;

		current++;
		while (uni_isspace(*current))
			current++;

		quote_char = (*current == '"' || *current == '\'') ? *current++ : 0;

		val = current;
		while (*current)
		{
			if (quote_char)
			{
				if (quote_char == *current)
					break;
			}
			else
			{
				if (*current == ',' || *current == ';')
					break;
			}
			current++;
		}

		len = current - val;

		if (*current)
		{
			if (quote_char)
			{
				current++;
				while (*current && *current != ',' && *current != ';')
					current++;
			}
			end_found = (*current == ',');

			if (*current)
				current++;
		}
		else if (quote_char)
		{
			val = NULL;
			len = 0;
		}
	}
	tmp = current;
	return val;
}

void HTML_Element::HandleMetaRefresh(HLDocProfile* hld_profile, const uni_char* content)
{
	/*
	 * This section parses the http-equiv refresh meta content attribute
	 * The attribute has somewhat the following aspect:
	 *    content="xx;url=yyy"
	 * where xx is a number, and yyy is an url.
	 * yyy CAN be quoted both with quotes or apostrophes. Check CORE-17731
	 * All tokens can be padded with whitespace
	 *
	 * */
	while(*(content) && uni_isspace(*(content)))
		content++;

	if (uni_isdigit(*(content)) || *content == '.')
	{
		short refresh_sec;
		int refresh_url_len;
		URL refresh_url;

		const uni_char* refresh_tmp = ParseRefreshUrl(content, refresh_url_len, refresh_sec);

#ifdef GADGET_SUPPORT
		// If both url and gadget, the command is ignored.
		if(refresh_tmp && hld_profile->GetFramesDocument()->GetWindow()->GetGadget() != NULL)
			refresh_sec = -1;
		else
#endif //GADGET_SUPPORT

		if (refresh_tmp && *refresh_tmp)
		{
			AutoTempBuffer tmp_buf;
			if (OpStatus::IsMemoryError(tmp_buf.GetBuf()->Append(refresh_tmp, refresh_url_len)))
				hld_profile->SetIsOutOfMemory(TRUE);
			else
			{
				uni_char* uni_val_tmp = tmp_buf.GetBuf()->GetStorage();
				int copy_len = refresh_url_len;
				RemoveTabs(uni_val_tmp);

				/*
				 * The following if block and loop handle the case of the URL being quoted around
				 */
				uni_char c_url_quoter = uni_val_tmp[0];
				if (c_url_quoter == '\'' || c_url_quoter == '"')
				{
					uni_val_tmp++;
					copy_len--;
					int index = 0;

					while (index < copy_len)
					{
						if (uni_val_tmp[index] == c_url_quoter)
						{
							uni_val_tmp[index] = 0;
							break;
						}
						index++;
					}
				}

				uni_char *tmp = uni_val_tmp;
				uni_char *rel_start = 0;

				while (*tmp != '\0' && *tmp != '#')
					tmp++;
				if (*tmp == '#')
				{
					*tmp = '\0';
					rel_start = tmp+1;
				}

				if (rel_start == uni_val_tmp + 1) // only an anchor within the local page
					refresh_url = g_url_api->GetURL(*hld_profile->GetURL(), uni_val_tmp, rel_start);
				else if (hld_profile->BaseURL())
					refresh_url = g_url_api->GetURL(*hld_profile->BaseURL(), uni_val_tmp, rel_start);
				else
					refresh_url = g_url_api->GetURL(uni_val_tmp, rel_start);
			}
		}

		// Use this refresh URL if the timeout is valid and is
		// smaller or equal to the previously refresh set URL,
		// or if it is the first set refresh URL.
		short prev_refresh_sec = hld_profile->GetRefreshSeconds();
		if (refresh_sec >= 0 && (prev_refresh_sec < 0 || refresh_sec <= prev_refresh_sec))
			hld_profile->SetRefreshURL(&refresh_url, refresh_sec);
	}
}

// returns FALSE if we find that we have used wrong character set
// This function used to be called CheckHttpEquiv, but it now checks more than that (stighal, 2002-08-29)
BOOL HTML_Element::CheckMetaElement(const DocumentContext& context, HTML_Element* parent_elm, BOOL added)
{
	const uni_char* content = GetStringAttr(ATTR_CONTENT);
	const uni_char* name = GetStringAttr(ATTR_NAME);

	HLDocProfile* hld_profile = context.hld_profile;

#ifdef CSS_VIEWPORT_SUPPORT
	if (name && uni_stri_eq(name, "VIEWPORT"))
	{
		CSSCollection* coll = hld_profile->GetCSSCollection();
		if (added && content)
		{
			CSS_ViewportMeta* viewport_meta = GetViewportMeta(context, TRUE);
			if (viewport_meta)
			{
				viewport_meta->ParseContent(content);
				coll->AddCollectionElement(viewport_meta);
			}
		}
		else
			coll->RemoveCollectionElement(this);

		return TRUE;
	}
#endif // CSS_VIEWPORT_SUPPORT

	if (!added || !content)
		return TRUE;

	BOOL rc = TRUE;

	if (name && uni_stri_eq(name, "DESCRIPTION"))
	{
		if (OpStatus::IsMemoryError(hld_profile->SetDescription(content)))
			hld_profile->SetIsOutOfMemory(TRUE);

		// go on and see if there's any other interesting attribute on the element
	}

	const uni_char* http_equiv = GetStringAttr(ATTR_HTTP_EQUIV);
	if (http_equiv)
	{
		if (uni_stri_eq(http_equiv, "SET-COOKIE"))
		{
			uni_char *unitempcookie = NULL;
			if (UniSetStr(unitempcookie, content) == OpStatus::ERR_NO_MEMORY)
				hld_profile->SetIsOutOfMemory(TRUE);
			else
			{
				char *tempcookie=(char*) unitempcookie;
				make_singlebyte_in_place(tempcookie);

				URL* OP_MEMORY_VAR url = hld_profile->GetURL();
				if(url->GetAttribute(URL::KHaveServerName))
				{
#ifdef OOM_SAFE_API
					TRAPD(op_err, urlManager->HandleSingleCookieL(url->GetRep(), tempcookie, tempcookie, 0));
					if (OpStatus::IsMemoryError(op_err))
						hld_profile->SetIsOutOfMemory(TRUE);
#else
					TRAPD(op_err, g_url_api->HandleSingleCookieL(*url, tempcookie, tempcookie, 0)); // FIXME: OOM
					if (OpStatus::IsMemoryError(op_err))
						hld_profile->SetIsOutOfMemory(TRUE);
#endif //OOM_SAFE_API
				}
			}
			OP_DELETEA(unitempcookie);
		}
		else
		if(!hld_profile->IsXml() && uni_stri_eq(http_equiv, "CONTENT-TYPE") && parent_elm)
		{
			ParameterList type;

			uni_char* uni_val_tmp = (uni_char*)g_memory_manager->GetTempBuf();
			int val_max_len = UNICODE_DOWNSIZE(g_memory_manager->GetTempBufLen());
			int copy_len = uni_strlen(content);
			if (copy_len >= val_max_len)
				copy_len = val_max_len - 1;
			uni_strncpy(uni_val_tmp, content, copy_len);
			uni_val_tmp[copy_len] = 0;
			char* val_tmp=(char*) uni_val_tmp;
			make_singlebyte_in_place(val_tmp);

			if (type.SetValue(val_tmp,PARAM_SEP_SEMICOLON) == OpStatus::ERR_NO_MEMORY)
			{
				hld_profile->SetIsOutOfMemory(TRUE);
			}
			else
			{

				Parameters *element = type.GetParameter("charset", PARAMETER_ASSIGNED_ONLY, type.First());
				if(element)
				{
					// Workaround for PGO crash on Windows - see DSK-279424
					OpString8 charset;

					// Character set name given in this META tag
					if (OpStatus::IsMemoryError(charset.Set(element->Value())))
						hld_profile->SetIsOutOfMemory(TRUE);

					// We have a dilemma here, since we have now found a <META> tag
					// defining which encoding we should use, but we may already
					// be decoding the document as if it was another encoding.
					//
					// There are six cases to consider:
					//
					//  0. We can read the document and the tag says that it is
					//     an incompatible encoding (such as utf-16 instead of
					//     iso-8859-1) [see Bug#49311]
					//     => return TRUE (=ok)
					//
					//  1. The HTTP header contained a charset identifier, or we saw
					//     a meta http-equiv defining charset the last run, or one
					//     was already seen in this document
					//     => return TRUE (=ok)
					//
					//  2. The user has defined an override using View|Encoding
					//     => Set KMIME_CharSet and return TRUE (=ok)
					//
					//  3. The current character decoder has the same character
					//     set that is defined here
					//     => Set KMIME_CharSet and return TRUE (=ok)
					//
					//  4. The current character decoder has a different character
					//     set than is defined here, and the defined one is known
					//     => Set KMIME_CharSet and return FALSE (=refresh)
					//
					//  5. The current character decoder has a different character
					//     set than is defined here, but we do not know about it
					//     => Set MIME_CharSet and return TRUE (=ok)

					// Character set from HTTP header (or last <meta> found)
					const char *http_charset =
						hld_profile->GetURL()->GetAttribute(URL::KMIME_CharSet, TRUE).CStr();

					// User override from View|Encoding
					const char *override = g_pcdisplay->GetForceEncoding();
					if (override && strni_eq(override, "AUTODETECT-", 11))
						override = NULL;

					// The name that GetCharacterSet would return if
					// this character set is known. If it was not found,
					// we get a NULL Pointer.
					OpString8 found;

					// Workaround for PGO crash on Windows - see DSK-279424
					if (charset.HasContent() &&
						OpStatus::IsMemoryError(found.Set(g_charsetManager->GetCanonicalCharsetName(charset.CStr()))))
						hld_profile->SetIsOutOfMemory(TRUE);

					// Currently used decoder's character set
					URL_DataDescriptor *my_dd = NULL;
					FramesDocument* tFrm = hld_profile->GetFramesDocument();
					if (tFrm)
						my_dd = tFrm->Get_URL_DataDescriptor();

					const char *current = NULL;
					if (my_dd)
						current = my_dd->GetCharacterSet();
					else // temp crashfix. will look into what is wrong, when the testpage is up again. (bug 50811) (emil)
						current = "utf-16";

					if ((current && (0 == op_strncmp(current, "utf-16", 6))) ||
					    (found.HasContent() && (0 == found.Compare("utf-16", 6))))
					{
						// CASE 0: META tag might not be telling the truth
						found.Empty();
						charset.Empty();
						rc = TRUE;
					}
					else if (http_charset)
					{
						// CASE 1: HTTP header have charset
						rc = TRUE;
					}
					else if (override && *override)
					{
						// CASE 2: User defined override
						rc = TRUE;
						if (charset.HasContent())
						{
							// Remember this anyway, since the next access to
							// the URL might not be overridden
							if (OpStatus::IsMemoryError(hld_profile->GetURL()->SetAttribute(URL::KMIME_CharSet, charset)))
								hld_profile->SetIsOutOfMemory(TRUE);
							if (hld_profile->SetCharacterSet(charset.CStr()) == OpStatus::ERR_NO_MEMORY)
								hld_profile->SetIsOutOfMemory(TRUE);
						}
					}
					else
					{
						// CASE 3-5
						if(my_dd)
							my_dd->StopGuessingCharset();

						if (current)
						{
							if (found.HasContent())
							{
								// CASE 3-4

								if (found.CompareI(current) == 0)
								{
									// CASE 3: Using the correct decoder already
									rc = TRUE;
									if (OpStatus::IsMemoryError(hld_profile->GetURL()->SetAttribute(URL::KMIME_CharSet, found)))
										hld_profile->SetIsOutOfMemory(TRUE);
									if (hld_profile->SetCharacterSet(found.CStr()) == OpStatus::ERR_NO_MEMORY)
										hld_profile->SetIsOutOfMemory(TRUE);
								}
								else
								{
									// CASE 4: Switching character set
									rc = FALSE;
									if (OpStatus::IsMemoryError(hld_profile->GetURL()->SetAttribute(URL::KMIME_CharSet, found)))
										hld_profile->SetIsOutOfMemory(TRUE);
									if (hld_profile->SetCharacterSet(found.CStr())	== OpStatus::ERR_NO_MEMORY)
										hld_profile->SetIsOutOfMemory(TRUE);
								}
							}
							else
							{
								// CASE 5: Wrong charset, but unknown
								rc = TRUE;
								if (OpStatus::IsMemoryError(hld_profile->GetURL()->SetAttribute(URL::KMIME_CharSet, charset)))
									hld_profile->SetIsOutOfMemory(TRUE);

								if (hld_profile->SetCharacterSet(NULL) == OpStatus::ERR_NO_MEMORY)
									hld_profile->SetIsOutOfMemory(TRUE);
							}
						}
					}
				}
			}
		}
		else
		if (uni_stri_eq(http_equiv, "LINK") && parent_elm)
		{
			const uni_char* val_tmp = content;
			while (*val_tmp && !hld_profile->GetIsOutOfMemory())
			{
				while (uni_isspace(*val_tmp))
					val_tmp++;

				if (*val_tmp != '<')
					return TRUE;
				val_tmp++;

				const uni_char* tmp = uni_strchr(val_tmp, '>');
				if (!tmp)
					return TRUE;

				HtmlAttrEntry ha_list[5];
				int i = 0;
				ha_list[i].ns_idx = NS_IDX_DEFAULT;
				ha_list[i].attr = ATTR_HREF;
				ha_list[i].value = val_tmp;
				ha_list[i++].value_len = tmp - val_tmp;

				tmp++;

				if (*tmp == ';')
				{
					BOOL type_done = FALSE;
					BOOL rel_done = FALSE;
					BOOL media_done = FALSE;

					tmp++;
					while (*tmp)
					{
						BOOL is_type = FALSE;
						BOOL is_rel = FALSE;
						BOOL is_media = FALSE;

						tmp++;
						while (uni_isspace(*tmp))
							tmp++;

						if (uni_strni_eq(tmp, "TYPE", 4))
						{
							tmp += 4;
							is_type = TRUE;
						}
						else if (uni_strni_eq(tmp, "REL", 3))
						{
							tmp += 3;
							is_rel = TRUE;
						}
						else if (uni_strni_eq(tmp, "MEDIA", 5))
						{
							tmp += 5;
							is_media = TRUE;
						}
						else
						{
							break;
						}

						BOOL end_found;
						UINT tval_len = 0;
						const uni_char* tval = GetMetaValue(tmp, tval_len, end_found);

						if (is_type)
						{
							if (type_done)
								break;
							else
							{
								ha_list[i].ns_idx = NS_IDX_DEFAULT;
								ha_list[i].attr = ATTR_TYPE;
								ha_list[i].value = tval;
								ha_list[i++].value_len = tval_len;
							}
						}
						else if (is_media)
						{
							if (media_done)
								break;
							else
							{
								ha_list[i].ns_idx = NS_IDX_DEFAULT;
								ha_list[i].attr = ATTR_MEDIA;
								ha_list[i].value = tval;
								ha_list[i++].value_len = tval_len;
							}
						}
						else if (is_rel)
						{
							if (rel_done)
								break;
							else
							{
								ha_list[i].ns_idx = NS_IDX_DEFAULT;
								ha_list[i].attr = ATTR_REL;
								ha_list[i].value = tval;
								ha_list[i++].value_len = tval_len;
							}
						}

						if (end_found)
							break;
					}
				}

				ha_list[i].attr = ATTR_NULL;

				HTML_Element* new_elm = NEW_HTML_Element();
				if( new_elm && OpStatus::ERR_NO_MEMORY != new_elm->Construct(hld_profile, NS_IDX_HTML, HE_LINK, ha_list, HE_INSERTED_BY_CSS_IMPORT) )
				{
					new_elm->Under(parent_elm);
					if (OpStatus::IsMemoryError(hld_profile->HandleLink(new_elm)))
						hld_profile->SetIsOutOfMemory(TRUE);
				}
				else
				{
					DELETE_HTML_Element(new_elm);
					hld_profile->SetIsOutOfMemory(TRUE);
				}
				val_tmp = tmp;
			}
		}
		else if (uni_stri_eq(http_equiv, "EXPIRES"))
		{
			URL *url = hld_profile->GetURL();

			if(url && uni_strlen(content) > 0)
			{
				uni_char *uni_content = NULL;
				if (UniSetStr(uni_content, content) == OpStatus::ERR_NO_MEMORY)
					hld_profile->SetIsOutOfMemory(TRUE);
				else if (uni_content)
				{
					char *temp_content=(char*) uni_content;
					make_singlebyte_in_place(temp_content);

					url->SetAttribute(URL::KHTTPExpires,temp_content);
					OP_DELETEA(uni_content);
				}
			}
		}
		else if (uni_stri_eq(http_equiv, "PRAGMA"))
		{
			URL *url = hld_profile->GetURL();

			if(url && uni_strlen(content) > 0)
			{
				uni_char *uni_content = NULL;
				if (UniSetStr(uni_content, content) == OpStatus::ERR_NO_MEMORY)
					hld_profile->SetIsOutOfMemory(TRUE);
				else if (uni_content)
				{
					char *temp_content=(char*) uni_content;
					make_singlebyte_in_place(temp_content);

					url->SetAttribute(URL::KHTTPPragma,temp_content);

					OP_DELETEA(uni_content);
				}
			}
		}
		else if (uni_stri_eq(http_equiv, "CACHE-CONTROL"))
		{
			URL *url = hld_profile->GetURL();

			if(url && *content)
			{
				uni_char *uni_content = NULL;
				if (UniSetStr(uni_content, content) == OpStatus::ERR_NO_MEMORY)
					hld_profile->SetIsOutOfMemory(TRUE);
				else if (uni_content)
				{
					char *temp_content=(char*) uni_content;
					make_singlebyte_in_place(temp_content);

					url->SetAttribute(URL::KHTTPCacheControl, temp_content);

					OP_DELETEA(uni_content);
				}
			}
		}
		else if (uni_stri_eq(http_equiv, "REFRESH")
				&& (!hld_profile->GetFramesDocument()->GetWindow()->IsMailOrNewsfeedWindow()
					&& hld_profile->GetURL()->Type() != URL_ATTACHMENT))
		{
			if (GetInserted() != HE_INSERTED_BY_PARSE_AHEAD)
				HandleMetaRefresh(hld_profile, content);
		}
#ifdef DNS_PREFETCHING
		else if (uni_stri_eq(http_equiv, "x-dns-prefetch-control"))
		{
			if (uni_stri_eq(content, "on"))
				hld_profile->GetLogicalDocument()->SetDNSPrefetchControl(TRUE);
			else if (uni_stri_eq(content, "off"))
				hld_profile->GetLogicalDocument()->SetDNSPrefetchControl(FALSE);
		}
#endif // DNS_PREFETCHING
#if 0 // It seems as if MSIE8 didn't include support for this in META and since supporting this in META has problems (it's too late to prevent parsing and execution when we come here) I'll disable it for now but leave it in to make it easy to enable again.
		else if (uni_stri_eq(http_equiv, "x-frame-options")) // Click-jacking protection
		{
			// From the spec:
			// * The META tag SHOULD appear in the document <HEAD>, but MAY appear
			//   anywhere in the document due to liberal HTML5 rules.
			// * The name and value are NOT case-sensitive.
			// * Only the first valid X-FRAME-OPTIONS directive is respected

			// The third item will be ignored for now which means that a same-origin
			// followed by a deny might be blocked when it should not.

			FramesDocument* doc = hld_profile->GetFramesDocument();
			if (doc->GetParentDoc() != NULL)
			{
				// In a frame. Is that bad? Time to check what the document has
				// stipulated

				// The HTTP header takes precedence
				OpString8 frame_options;
				doc->GetURL().GetAttribute(URL::KHTTPFrameOptions, frame_options);
				if (frame_options.IsEmpty())
				{
					// Note that very similar code also exists in DocumantManager::HandleByOpera
					// where the HTTP header is taken care of

					BOOL allow = TRUE;
					if (uni_stri_eq(content, "deny"))
						allow = FALSE;
					else if (uni_stri_eq(content, "sameorigin"))
					{
						URL top_doc_url = doc->GetWindow()->DocManager()->GetCurrentDoc()->GetURL();
						if (!doc->GetURL().SameServer(top_doc_url))
							allow = FALSE;
					}

					if (!allow)
					{
						// Go away!
						// We should replace ourselves with a warning page immediately somehow
						doc->GetDocManager()->StopLoading(FALSE);
						rc = FALSE;
						doc->GetDocManager()->GenerateAndShowClickJackingBlock(doc->GetURL());
					}
				}
			}
		}
#endif // 0 - Removed support for anti-clickjacking in META
	}
	return rc;
}

#ifdef LOGDOC_EXPENSIVE_ELEMENT_DEBUGGING
int html_elm_balance = 0;

struct HTML_Exo
{
	HTML_Element *elt;
	HTML_Exo *next;
};

HTML_Exo *all_html_elements = NULL;
#endif // LOGDOC_EXPENSIVE_ELEMENT_DEBUGGING

HTML_Element::~HTML_Element()
{
	OP_ASSERT(!GetESElement());
#ifdef SVG_SUPPORT
	DestroySVGContext();
#endif

	if (IsFormElement())
	  DestroyFormObjectBackup();

	DetachChildren();
	LocalClean();

#ifdef DISPLAY_CLICKINFO_SUPPORT
	if (MouseListener::GetClickInfo().GetImageElement() == this)
		MouseListener::GetClickInfo().Reset();
#endif // DISPLAY_CLICKINFO_SUPPORT

#ifdef LOGDOC_EXPENSIVE_ELEMENT_DEBUGGING
	html_elm_balance--;
	HTML_Exo *p, *q;
	for ( p=all_html_elements, q=0 ; p != 0 && p->elt != this ; q=p, p=p->next )
		;
	OP_ASSERT( p != 0 );
	if (q == 0)
		all_html_elements = p->next;
	else
		q->next = p->next;
	OP_DELETE(p);
#endif // LOGDOC_EXPENSIVE_ELEMENT_DEBUGGING

	OP_ASSERT(!Parent());
	OP_ASSERT(!m_first_ref);
}

void HTML_Element::LocalClean()
{
	g_ns_manager->GetElementAt(GetNsIdx())->DecRefCount();

	if(Type() == HE_TEXT) // HE_TEXT is the same in all namespacse
	{
		DELETE_TextData(data.text);
		data.text = NULL;
	}
	else
	{
		if (data.attrs)
		{
			REPORT_MEMMAN_DEC(GetAttrSize() * sizeof(AttrItem));
			DELETE_HTML_Attributes(data.attrs);
			data.attrs = NULL;
		}
	}
}

int GetAlignValue(BYTE align_val, BOOL allow_middle, BOOL allow_justify)
{
	switch (align_val)
	{
	case ATTR_VALUE_center:
		return CSS_VALUE_center;
	case ATTR_VALUE_left:
		return CSS_VALUE_left;
	case ATTR_VALUE_right:
		return CSS_VALUE_right;
	case ATTR_VALUE_bottom:
		return CSS_VALUE_bottom;
	case ATTR_VALUE_justify:
		if (allow_justify)
			return CSS_VALUE_justify;
		else
			return 0;
	case ATTR_VALUE_middle:
		if (allow_middle)
			return CSS_VALUE_center;
		else
			return 0;
	default:
		return 0;
	}
}

void SetNumAttrVal(HTML_ElementType type, short attr, BYTE val, void* &value)
{
	value = 0;
	switch (attr)
	{
#ifdef SUPPORT_TEXT_DIRECTION
	case ATTR_DIR:
		if (val == ATTR_VALUE_ltr)
			value = (void*)CSS_VALUE_ltr;
		else if (val == ATTR_VALUE_rtl)
			value = (void*)CSS_VALUE_rtl;
		break;
#endif // SUPPORT_TEXT_DIRECTION

	case ATTR_SHAPE:
		if (val == ATTR_VALUE_circle || val == ATTR_VALUE_circ)
			value = (void*)AREA_SHAPE_CIRCLE;
		else if (val == ATTR_VALUE_polygon || val == ATTR_VALUE_poly)
			value = (void*)AREA_SHAPE_POLYGON;
		else if (val == ATTR_VALUE_default)
			value = (void*)AREA_SHAPE_DEFAULT;
		else
			value = (void*)AREA_SHAPE_RECT;
		break;

	case ATTR_ALIGN:
		switch (type)
		{
		case HE_IMG:
		case HE_EMBED:
		case HE_APPLET:
		case HE_OBJECT:
#ifdef MEDIA_HTML_SUPPORT
		case HE_AUDIO:
		case HE_VIDEO:
#endif // MEDIA_HTML_SUPPORT
#ifdef CANVAS_SUPPORT
		case HE_CANVAS:
#endif
		case HE_METER:
		case HE_PROGRESS:
		case HE_IFRAME:
		case HE_INPUT:
		case HE_CAPTION:
		case HE_LEGEND:
			if (val == ATTR_VALUE_top)
				value = (void*) CSS_VALUE_top;
			else if (val == ATTR_VALUE_left)
				value = (void*) CSS_VALUE_left;
			else if (val == ATTR_VALUE_right)
				value = (void*) CSS_VALUE_right;
			else if (val == ATTR_VALUE_center)
				value = (void*) CSS_VALUE_center;
			else if (val == ATTR_VALUE_middle || val == ATTR_VALUE_absmiddle)
			{
				if (type != HE_CAPTION)
					value = (void*) CSS_VALUE_middle;
			}
			else if (val == ATTR_VALUE_absbottom)
				value = (void*) CSS_VALUE_text_bottom;
			else
				value = (void*) CSS_VALUE_bottom;
			break;

		case HE_HR:
			if (val == ATTR_VALUE_right)
				value = (void*) CSS_VALUE_right;
			else if (val == ATTR_VALUE_left)
				value = (void*) CSS_VALUE_left;
			else if (val == ATTR_VALUE_center)
				value = (void*) CSS_VALUE_center;
			break;

		default:
			BOOL is_row_or_cell_or_group = (type == HE_TR || type == HE_TD || type == HE_TH || type == HE_TBODY || type == HE_TFOOT || type == HE_THEAD); //TBODY etc see bug 83880
			BOOL is_h_or_p_div = (type == HE_P || type == HE_DIV || (type >= HE_H1 && type <= HE_H6));
			value = INT_TO_PTR(GetAlignValue(val, is_row_or_cell_or_group, is_row_or_cell_or_group || is_h_or_p_div || type == HE_COL || type == HE_COLGROUP));
			if (is_h_or_p_div && value == 0)
				value = (void*)CSS_VALUE_left;
			break;
		}

		break;

	case ATTR_VALIGN:
		if (val == ATTR_VALUE_top)
			value = (void*) CSS_VALUE_top;
		else if (val == ATTR_VALUE_middle || val == ATTR_VALUE_center)
			value = (void*) CSS_VALUE_middle;
		else if (val == ATTR_VALUE_bottom)
			value = (void*) CSS_VALUE_bottom;
		else if (val == ATTR_VALUE_baseline)
			value = (void*) CSS_VALUE_baseline;
		else
			value = (void*) 0;

		break;

	case ATTR_CLEAR:
		if (val == ATTR_VALUE_left)
			value = (void*) CLEAR_LEFT;
		else if (val == ATTR_VALUE_right)
			value = (void*) CLEAR_RIGHT;
		else if (val == ATTR_VALUE_all || val == ATTR_VALUE_both)
			value = (void*) CLEAR_ALL;
		else
			value = (void*) CLEAR_NONE;

		break;

	case ATTR_TYPE:
		if (type == HE_INPUT || type == HE_BUTTON)
		{
			if (ATTR_is_inputtype_val(val))
			{
				// Magic. The enum InputType and ATTR_VALUE_... table must be in sync
				InputType inp_type = (enum InputType) (val - ATTR_VALUE_text + INPUT_TEXT);
				if (type == HE_BUTTON)
				{
					// Set value to default value if it's a bad or unknown value
					if (inp_type != INPUT_BUTTON && inp_type != INPUT_RESET)
						inp_type = INPUT_SUBMIT;
				}
				value = (void*) inp_type;
			}
			else // different defaults for HE_INPUT and HE_BUTTON
				value = (void*) (type == HE_INPUT ? INPUT_TEXT : INPUT_SUBMIT);
		}
		else
		{
			if (val == ATTR_VALUE_disc)
				value = (void*) LIST_TYPE_DISC;
			else if (val == ATTR_VALUE_square)
				value = (void*) LIST_TYPE_SQUARE;
			else if (val == ATTR_VALUE_circle)
				value = (void*) LIST_TYPE_CIRCLE;
			else
				value = (void*) LIST_TYPE_NULL;
		}

		break;

	case ATTR_SCROLLING:
		if (val == ATTR_VALUE_yes)
			value = (void*) SCROLLING_YES;
		else if (val == ATTR_VALUE_no)
			value = (void*) SCROLLING_NO;
		else
			value = (void*) SCROLLING_AUTO;

		break;

	case ATTR_METHOD:
	case ATTR_FORMMETHOD:
		if (val == ATTR_VALUE_post)
			value = (void*) HTTP_METHOD_POST;
		else
			value = (void*) HTTP_METHOD_GET;

		break;

	case ATTR_DIRECTION:
		switch (val)
		{
		case ATTR_VALUE_up:
		case ATTR_VALUE_down:
		case ATTR_VALUE_left:
		case ATTR_VALUE_right:
			value = INT_TO_PTR(val);
			break;
		}
		break;


	case ATTR_BEHAVIOR:
		switch (val)
		{
		case ATTR_VALUE_slide:
		case ATTR_VALUE_scroll:
		case ATTR_VALUE_alternate:
			value = INT_TO_PTR(val);
			break;
		}
		break;

	default:
		break;
	}
}

PrivateAttrs*
AddPrivateAttr(HLDocProfile* hld_profile, HTML_ElementType type, int plen, HtmlAttrEntry* ha_list)
{
#if defined(_APPLET_2_EMBED_)
	plen += 2; // RL 2000-01-09: Adds 2 to the length to make space for java_DOCBASE and java_CODEBASE
#endif

	PrivateAttrs* pa = NEW_PrivateAttrs((plen)); // FIXME:REPORT_MEMMAN-KILSMO

	// OOM check
	if (!pa)
	{
		hld_profile->SetIsOutOfMemory(TRUE);
		return NULL;
	}

	if (OpStatus::IsMemoryError(pa->ProcessAttributes(hld_profile, type, ha_list)))
		hld_profile->SetIsOutOfMemory(TRUE);

	return pa;
}

void
HTML_Element::ReplaceAttrLocal(int i, short attr, ItemType item_type, void* value, int ns_idx/*=NS_IDX_HTML*/, BOOL need_free/*=FALSE*/, BOOL is_special/*=FALSE*/, BOOL is_id/*=FALSE*/, BOOL is_specified/*=TRUE*/, BOOL is_event/*=FALSE*/)
{
	if (data.attrs[i].GetAttr() != ATTR_NULL)
		data.attrs[i].Clean();

	data.attrs[i].Set(attr, item_type, value, ns_idx, need_free, is_special, is_id, is_specified, is_event);

	if (!is_special && attr == Markup::HA_CLASS)
	{
		NS_Type ns = g_ns_manager->GetNsTypeAt(ResolveNsIdx(ns_idx));
		if (ns == NS_HTML || ns == NS_SVG)
			SetHasClass();
	}
}

void
HTML_Element::SetAttrLocal(int i, short attr, ItemType item_type, void* value, int ns_idx/*=NS_IDX_HTML*/, BOOL need_free/*=FALSE*/, BOOL is_special/*=FALSE*/, BOOL is_id/*=FALSE*/, BOOL is_specified/*=TRUE*/, BOOL is_event/*=FALSE*/)
{
	data.attrs[i].Set(attr, item_type, value, ns_idx, need_free, is_special, is_id, is_specified, is_event);
	if (!is_special && attr == Markup::HA_CLASS)
	{
		NS_Type ns = g_ns_manager->GetNsTypeAt(ResolveNsIdx(ns_idx));
		if (ns == NS_HTML || ns == NS_SVG)
			SetHasClass();
	}
}

int
HTML_Element::SetAttrCommon(int i, short attr, ItemType item_type, void* val, BOOL need_free, int ns_idx, BOOL is_special, BOOL is_id, BOOL is_specified, BOOL is_event)
{
	OP_ASSERT(Type() != Markup::HTE_TEXT);
	if (i >= 0)
	{
		ReplaceAttrLocal(i, attr, item_type, val, ns_idx, need_free, is_special, is_id, is_specified, is_event);
		return i;
	}

	int attr_size = GetAttrSize();
	i = attr_size;
	// check if there is empty entries - always at the end
	while (i > 0 && GetAttrItem(i - 1) == ATTR_NULL)
		i--;

	if (i >= 0 && i < attr_size)
	{
		ReplaceAttrLocal(i, attr, item_type, val, ns_idx, need_free, is_special, is_id, is_specified, is_event);
		return i;
	}

	// create new entry
	int new_len = attr_size + 2; // one extra
	AttrItem* new_attrs = NEW_HTML_Attributes((new_len));

	// OOM check
	if (new_attrs)
	{

		// need to decrease ns reference count on all attributes overwritten
		// by the memcpy below
		for (int j = 0; j < attr_size; j++)
			if (!new_attrs[j].IsSpecial())
				g_ns_manager->GetElementAt(new_attrs[j].GetNsIdx())->DecRefCountA();

		op_memcpy(new_attrs, data.attrs, attr_size * sizeof(AttrItem));

		// need to set NeedFree to false so that the destructor called from DELETE_HTML_Attributes
		// a few lines down, doesn't delete the contents of the attributes
		for (int j = 0; j < attr_size; j++)
		{
			if (!new_attrs[j].IsSpecial())
				g_ns_manager->GetElementAt(new_attrs[j].GetNsIdx())->IncRefCountA();
			data.attrs[j].SetNeedFree(FALSE);
		}

		SetAttrSize(new_len);

		DELETE_HTML_Attributes(data.attrs);
		data.attrs = new_attrs;

		// set new attr value
		SetAttrLocal(attr_size, attr, item_type, val, ns_idx, need_free, is_special, is_id, is_specified, is_event);

		// reset extra entry
		SetAttrLocal(attr_size + 1, ATTR_NULL, ITEM_TYPE_BOOL, NULL, NS_IDX_DEFAULT, FALSE, TRUE);

		REPORT_MEMMAN_INC((new_len - attr_size) * sizeof(AttrItem));

		return attr_size;
	}

	return -1;
}

/*inline*/ int
HTML_Element::GetResolvedAttrNs(int i) const
{
	int idx = data.attrs[i].GetNsIdx();
	if (idx != NS_IDX_DEFAULT)
		return idx;
	else
		return GetNsIdx();
}

BOOL
HTML_Element::IsStyleElement()
{
	NS_Type ns = GetNsType();
	return Type() == Markup::HTE_STYLE && (ns == NS_HTML || ns == NS_SVG);
}

BOOL
HTML_Element::IsScriptElement()
{
	NS_Type ns = GetNsType();
	return Type() == Markup::HTE_SCRIPT && (ns == NS_HTML || ns == NS_SVG);
}

BOOL
HTML_Element::IsLinkElement()
{
	if (IsMatchingType(HE_LINK, NS_HTML))
		return TRUE;

	if (Type() == HE_PROCINST)
	{
		const uni_char* target = GetStringAttr(ATTR_TARGET);
		if(target && uni_str_eq(target, "xml-stylesheet"))
			return TRUE;
	}

	return FALSE;
}

BOOL
HTML_Element::IsCssImport()
{
	HTML_Element* parent_elm = Parent();
	return (GetInserted() == HE_INSERTED_BY_CSS_IMPORT && parent_elm && (parent_elm->IsStyleElement() || parent_elm->IsLinkElement()));
}

BOOL
HTML_Element::IsFocusable(FramesDocument *document)
{
	// This method determines what is focusable on a
	// page. Elements that this returns TRUE for can
	// be focused with mousedown, can have the :focus
	// pseudo class and has a working focus() method
	// in DOM.

	if (IsText())
		return FALSE;

	if (GetAnchor_HRef(document) || IsFormElement())
		return TRUE;

	if (IsMatchingType(HE_AREA, NS_HTML) && HasAttr(ATTR_HREF))
		return TRUE;

#ifdef DOCUMENT_EDIT_SUPPORT
	// We don't allow any elements outside of body to be focusable.
	if (IsContentEditable(FALSE))
		return TRUE;
#endif // DOCUMENT_EDIT_SUPPORT

	// If the author explicitly has said that he wants a
	// certain element to be involved in keyboard
	// navigation, then we should enable :focus on it.
	if (GetNsType() == NS_HTML && HasNumAttr(ATTR_TABINDEX))
		return TRUE;

#ifdef SVG_SUPPORT
	if(g_svg_manager->IsFocusableElement(document, this))
		return TRUE;
#endif // SVG_SUPPORT

	return FALSE;
}

HTML_Element::HTML_Element() : DocTree()
	, exo_object(NULL)
	, m_first_ref(NULL)
	, css_properties(NULL)
{
	packed1_init = 0;
	packed2_init = 0;
	data.attrs = NULL;
	layout_box = NULL;
#ifdef SVG_SUPPORT
	svg_context = NULL;
#endif // SVG_SUPPORT
	g_ns_manager->GetElementAt(GetNsIdx())->IncRefCount();

#ifdef LOGDOC_EXPENSIVE_ELEMENT_DEBUGGING
	html_elm_balance++;
	HTML_Exo *p = OP_NEW(HTML_Exo, ());
	if (p)
	{
		p->elt = this;
		p->next = all_html_elements;

		all_html_elements = p;
	}
#endif // LOGDOC_EXPENSIVE_ELEMENT_DEBUGGING
}

OP_STATUS HTML_Element::Construct(HLDocProfile* hld_profile, HTML_Element* he, BOOL skip_events, BOOL force_html)
{
	css_properties = NULL;
	layout_box = NULL;

	packed1_init = 0;
	packed2_init = 0;

	int elmtype = he->Type();
	int new_ns = he->GetNsIdx();

	if (force_html && new_ns != NS_IDX_HTML && he->GetTagName())
	{
		if (he->GetNsType() == NS_HTML)
			new_ns = NS_IDX_HTML;
		else
		{
			elmtype = HTM_Lex::GetElementType(he->GetTagName(), NS_HTML, FALSE);

			HtmlAttrEntry *attrs = htmLex->GetAttrArray();
			int attr_index = 0, attr_count = he->GetAttrSize();

			if (elmtype == HE_UNKNOWN)
			{
				attrs[0].attr = ATTR_XML_NAME;
				attrs[0].is_id = FALSE;
				attrs[0].is_special = TRUE;
				attrs[0].is_specified = FALSE;
				LogdocXmlName *old_name = (LogdocXmlName*)he->GetSpecialAttr(ATTR_XML_NAME, ITEM_TYPE_COMPLEX, (void*)NULL, SpecialNs::NS_LOGDOC);

				attrs[0].value = (uni_char*)old_name;
				attrs[0].value_len = 0;
				++attr_index;
			}

			attrs[attr_index].attr = ATTR_NULL;

			for (int i = 0; i < attr_count && attr_index < HtmlAttrEntriesMax; i++)
				if (!he->GetAttrIsSpecial(i) && he->GetAttrItem(i) == ATTR_XML)
				{
					const uni_char *value = (const uni_char *) he->GetValueItem(i);

					attrs[attr_index].attr = HTM_Lex::GetAttrType(value, NS_HTML, FALSE);
					attrs[attr_index].ns_idx = NS_IDX_HTML;
					attrs[attr_index].is_id = attrs[attr_index].attr == ATTR_ID;
					attrs[attr_index].is_specified = he->GetAttrIsSpecified(i);
					attrs[attr_index].is_special = FALSE;
					attrs[attr_index].value = value + uni_strlen(value) + 1;
					attrs[attr_index].value_len = uni_strlen(attrs[attr_index].value);
					attrs[attr_index].name = value;
					attrs[attr_index].name_len = uni_strlen(value);

					attrs[++attr_index].attr = ATTR_NULL;
				}

			return Construct(hld_profile, NS_IDX_HTML, (HTML_ElementType) elmtype, attrs);
		}
	}

	SetType((HTML_ElementType) elmtype);

	g_ns_manager->GetElementAt(GetNsIdx())->DecRefCount();
	SetNsIdx(new_ns);
	g_ns_manager->GetElementAt(new_ns)->IncRefCount();

	if (Type() == HE_TEXT)
	{
		if (he->Content())
		{
			data.text = NEW_TextData(());
			if (! data.text || data.text->Construct(he->GetTextData()) == OpStatus::ERR_NO_MEMORY)
				return OpStatus::ERR_NO_MEMORY;
		}
	}
	else
	{
		int attr_count = he->GetAttrSize();
		int insert_count = 0;

		packed1.inserted = he->GetInserted();

		if (attr_count)
		{
			data.attrs = NEW_HTML_Attributes((attr_count));

			// OOM check
			if (!data.attrs)
				return OpStatus::ERR_NO_MEMORY;

			REPORT_MEMMAN_INC(attr_count * sizeof(AttrItem));
		}
		else
			data.attrs = NULL;

		SetAttrSize(attr_count);

		for (OP_MEMORY_VAR int i = 0; i < attr_count; i++)
		{
			ItemType item_type = he->GetItemType(i);
			void* OP_MEMORY_VAR value = he->GetValueItem(i);
			short attr = he->GetAttrItem(i);
			int attr_ns = he->GetAttrNs(i);
			BOOL attr_is_special = he->GetAttrIsSpecial(i);
			BOOL attr_is_id = he->GetAttrIsId(i);
			BOOL attr_is_specified = he->GetAttrIsSpecified(i);
			BOOL attr_is_event = he->GetAttrIsEvent(i);

			if (attr)
			{
				switch (item_type)
				{
				case ITEM_TYPE_BOOL:
				case ITEM_TYPE_NUM:
					if (attr_is_special)
					{

						// Must not copy numbers that are really pointers
						if (attr_ns == SpecialNs::NS_LOGDOC && attr == ATTR_FORMOBJECT_BACKUP)
							continue;

						// Must not reuse other forms' form nr or we'll get really confused
						if (attr_ns == SpecialNs::NS_LOGDOC && attr == ATTR_JS_ELM_IDX)
							continue;

#if defined SVG_SUPPORT
						// This is a meta attribute that would lie if it was copied
						if(attr_ns == SpecialNs::NS_SVG && attr == Markup::SVGA_SHADOW_TREE_BUILT)
							continue;
#endif // SVG_SUPPORT
					}
					break;

				case ITEM_TYPE_NUM_AND_STRING:
					{
						OP_ASSERT(value);
						uni_char* string_part = reinterpret_cast<uni_char*>(static_cast<char*>(value)+sizeof(INTPTR));
						size_t len = sizeof(INTPTR)+sizeof(uni_char)*(uni_strlen(string_part)+1);
						char* new_value = OP_NEWA(char, len);
						if (!new_value)
						{
							value = NULL;
							item_type = ITEM_TYPE_STRING;
							hld_profile->SetIsOutOfMemory(TRUE);
						}
						else
						{
							op_memcpy(new_value, value, len);
							value = new_value;
						}
					}
					break;
				case ITEM_TYPE_STRING:
					if (value)
					{
						const uni_char *xml_value = NULL;
						int xml_value_len = 0;
						const uni_char *str = (const uni_char *) value;
						int str_len = uni_strlen(str);

						if (attr == ATTR_XML)
						{
							xml_value = (const uni_char *) str + str_len + 1;

							if (xml_value)
								xml_value_len = uni_strlen(xml_value);
						}

						if (attr == ATTR_XML)
						{
							value = OP_NEWA(uni_char, str_len + xml_value_len + 2); // FIXME:REPORT_MEMMAN-KILSMO

							if (value)
							{
								uni_strncpy((uni_char*)value, str, str_len);
								((uni_char*)value)[str_len] = '\0';
								if (xml_value_len)
									uni_strncpy((uni_char*)value + str_len + 1, xml_value, xml_value_len);
								((uni_char*)value)[str_len + xml_value_len + 1] = '\0';
							}
						}
						else
							value = (void*) UniSetNewStrN(str, str_len); // FIXME:REPORT_MEMMAN-KILSMO

						if (!value)
							hld_profile->SetIsOutOfMemory(TRUE);
					}
					break;

				case ITEM_TYPE_URL:
					if (value)
					{
						value = OP_NEW(URL, (*((URL*) value)));

						if (!value)
							hld_profile->SetIsOutOfMemory(TRUE);
					}
					break;

				case ITEM_TYPE_URL_AND_STRING:
					{
						OP_ASSERT(value);
						UrlAndStringAttr *url_attr = static_cast<UrlAndStringAttr*>(value);

						UrlAndStringAttr *new_attr;
						const uni_char *url_string = url_attr->GetString();
						OP_STATUS oom = UrlAndStringAttr::Create(url_string, url_attr->GetStringLength(), new_attr);

						if (OpStatus::IsSuccess(oom))
						{
							if (url_attr->GetResolvedUrl())
							{
								oom = new_attr->SetResolvedUrl(url_attr->GetResolvedUrl());
								if (OpStatus::IsMemoryError(oom))
								{
									UrlAndStringAttr::Destroy(new_attr);
									value = NULL;
									item_type = ITEM_TYPE_URL_AND_STRING;
									hld_profile->SetIsOutOfMemory(TRUE);
								}
								else
									value = static_cast<void*>(new_attr);
							}
							else
								value = static_cast<void*>(new_attr);
						}
						else
						{
							value = NULL;
							item_type = ITEM_TYPE_URL_AND_STRING;
							hld_profile->SetIsOutOfMemory(TRUE);
						}
					}
					break;

				case ITEM_TYPE_COORDS_ATTR:
					if (value)
					{
						CoordsAttr* new_value;
						OP_STATUS status = ((CoordsAttr*)value)->CreateCopy(&new_value);
						if (OpStatus::IsError(status))
							hld_profile->SetIsOutOfMemory(TRUE);
						else
							value = (void*)new_value;
					}
					break;

				case ITEM_TYPE_PRIVATE_ATTRS:
					if (value)
					{
						PrivateAttrs* attrs = (PrivateAttrs*)value;
						value = attrs->GetCopy(attrs->GetLength());
						if (!value)
							hld_profile->SetIsOutOfMemory(TRUE);
					}
					break;

				case ITEM_TYPE_COMPLEX:
					if (value)
					{
						ComplexAttr *new_value;
						OP_STATUS status = static_cast<ComplexAttr*>(value)->CreateCopy(&new_value);
						if (status == OpStatus::ERR_NOT_SUPPORTED)
						{
							// This attribute shouldn't be copied.
							continue;
						}
						// The attribute might be required and the failure to create it might
						// leave the element in a inconsistent state causing crashes
						// later. Better to abort immediately.
						RETURN_IF_ERROR(status);
						OP_ASSERT(new_value);
						value = (void*)new_value;
					}
					break;

				default:
					continue;
				}
			}

			if (hld_profile->GetIsOutOfMemory())
				return OpStatus::ERR_NO_MEMORY;

			SetAttrLocal(insert_count++, attr, item_type, value, attr_ns, item_type != ITEM_TYPE_BOOL && item_type != ITEM_TYPE_NUM, attr_is_special, attr_is_id, attr_is_specified, attr_is_event);

			// When cloning form elements we need to clone the form value
			if (attr_is_special && attr_ns == SpecialNs::NS_LOGDOC && attr == ATTR_FORM_VALUE)
			{
				FormValue* newformvalue = (FormValue*) value;
				if (he->GetFormValue()->GetType() == FormValue::VALUE_RADIOCHECK)
				{
					FormValueRadioCheck* new_radiovalue = FormValueRadioCheck::GetAs(newformvalue);
					FormValueRadioCheck* old_radiovalue = FormValueRadioCheck::GetAs(he->GetFormValue());
					new_radiovalue->SetIsChecked(this, old_radiovalue->IsChecked(he), NULL, FALSE);
				}
				else
				{
					// Clone the value unless it's already stored in an attribute or somewhere else
					FormValue* oldformvalue = he->GetFormValue();
					if (oldformvalue->HasMeaningfulTextRepresentation() &&
						oldformvalue->GetType() != FormValue::VALUE_OUTPUT &&
						oldformvalue->GetType() != FormValue::VALUE_NO_OWN_VALUE)
					{
						OpString text;
						oldformvalue->GetValueAsText(he, text);
						newformvalue->SetValueFromText(this, text.CStr());
					}
				}
			}
		}

		for (int j = insert_count; j < attr_count; j++)
		{
			SetAttrLocal(j, ATTR_NULL, ITEM_TYPE_BOOL, NULL, NS_IDX_DEFAULT, FALSE, TRUE);
		}
	}

#if defined(SVG_SUPPORT)
	if (IsMatchingType(Markup::SVGE_SVG, NS_SVG))
	{
		SVGContext* ctx = g_svg_manager->CreateDocumentContext(this, hld_profile->GetLogicalDocument());
		if (!ctx)
			return OpStatus::ERR_NO_MEMORY;
	}
#endif // SVG_SUPPORT

	if (IsFormElement())
	{
		FormValue* form_value = GetFormValue();
		form_value->SetMarkedPseudoClasses(form_value->CalculateFormPseudoClasses(hld_profile->GetFramesDocument(), this));
	}

	return OpStatus::OK;
}

static int GetNumberOfExtraAttributesForType(HTML_ElementType type, NS_Type ns, HLDocProfile *hld_profile, HtmlAttrEntry* ha_list)
{
	if (ns == NS_HTML)
	{
		switch (type)
		{
		case HE_FORM: // one extra for ATTR_JS_ELM_IDX
		case HE_LABEL: // 1 extra for ATTR_JS_FORM_IDX
		case HE_LEGEND: // 1 extra for ATTR_JS_FORM_IDX
		case HE_FIELDSET: // 1 extra for ATTR_JS_FORM_IDX
		case HE_PROGRESS:
		case HE_METER:
#ifdef CANVAS_SUPPORT
		case HE_CANVAS:
#endif // CANVAS_SUPPORT
#ifdef MEDIA_HTML_SUPPORT
		case Markup::HTE_TRACK:
#endif // MEDIA_HTML_SUPPORT
			return 1;

		case HE_IMG:
			if (hld_profile->GetCurrentForm())
				return 1;
			break;

		case HE_OBJECT:
			{
				int attr_count = 2; // one extra for ATTR_PRIVATE/ATTR_VALUE
#ifdef PICTOGRAM_SUPPORT
				if (ha_list)
				{
					while (ha_list->attr != Markup::HA_NULL)
					{
						if (ha_list->attr == Markup::HA_DATA)
						{
							if (ha_list->value_len > 7 && uni_strni_eq(ha_list->value, "PICT://", 7))
								attr_count++; // one extra for ATTR_LOCALSRC_URL
							break;
						}

						ha_list++;
					}
				}
#endif // PICTOGRAM_SUPPORT
				return attr_count;
			}

		case HE_EMBED: // one extra for ATTR_PRIVATE/ATTR_VALUE
		case HE_APPLET: // one extra for ATTR_PRIVATE/ATTR_VALUE
		case HE_SCRIPT:
		case HE_OUTPUT:
#ifdef MEDIA_HTML_SUPPORT
		case HE_AUDIO:
		case HE_VIDEO:
#endif //MEDIA_HTML_SUPPORT
			return 2;

		case HE_BUTTON:
		case HE_INPUT:
		case HE_TEXTAREA:
		case HE_SELECT:
#if defined(_SSL_SUPPORT_) && !defined(_EXTERNAL_SSL_SUPPORT_)
		case HE_KEYGEN:
#endif
		case HE_STYLE:
			return 3; // three extra for ATTR_JS_ELM_IDX, ATTR_FORM_VALUE and ATTR_JS_FORM_IDX (the JS attrs are set later in InsertElement)

		case HE_PROCINST:
		case HE_LINK:
			return 4;
		}
	}
#ifdef SVG_SUPPORT
	else if (ns == NS_SVG)
	{
		switch (type)
		{
			case Markup::SVGE_SCRIPT:
				return 2;
			case Markup::SVGE_STYLE:
				return 3;
		}
	}
#endif // SVG_SUPPORT
#if defined PICTOGRAM_SUPPORT && defined _WML_SUPPORT_
	else if (ns == NS_WML)
	{
		int attr_count = 0;
		if (ha_list)
		{
			while (ha_list->attr != Markup::HA_NULL)
			{
				if (ha_list->attr == Markup::WMLA_LOCALSRC)
				{
					if (ha_list->value_len > 7 && uni_strni_eq(ha_list->value, "PICT://", 7))
						attr_count++; // one extra for ATTR_LOCALSRC_URL
					break;
				}

				ha_list++;
			}
		}

		return attr_count;
	}
#endif // PICTOGRAM_SUPPORT && _WML_SUPPORT_

	return 0;
}

OP_STATUS HTML_Element::SetExtraAttributesForType(unsigned &attr_i, NS_Type ns_type, HtmlAttrEntry *ha_list, int attr_count, HLDocProfile *hld_profile)
{
	HTML_ElementType type = Type();
	OP_STATUS ret_stat = OpStatus::OK;

	if (ns_type == NS_HTML)
	{
		switch (type)
		{
		case HE_PROCINST:
			{
				const uni_char* target = GetStringAttr(ATTR_TARGET, NS_IDX_HTML);
				if (!target || !uni_str_eq(target, "xml-stylesheet"))
					break;
			}
			// fall through

		case HE_STYLE:
		case HE_LINK:
			SetAttrLocal(attr_i++, ATTR_CSS, ITEM_TYPE_CSS, (void*)0, SpecialNs::NS_LOGDOC, TRUE, TRUE);
			// fall through

		case HE_SCRIPT:
			// link, script and style tag need some special attributes
			ret_stat = SetSrcListAttr(attr_i++, NULL);
			break;

		case HE_EMBED:
		case HE_APPLET:
		case HE_OBJECT:
			if (ha_list)
			{
				// add private attrs
				int plen = attr_count - 1; // not including this

				PrivateAttrs* pa = AddPrivateAttr(hld_profile, type, plen, ha_list);

				// OOM check
				if (!pa)
					return OpStatus::ERR_NO_MEMORY;

				SetAttrLocal(attr_i++, ATTR_PRIVATE, ITEM_TYPE_PRIVATE_ATTRS, pa, SpecialNs::NS_LOGDOC, TRUE, TRUE);
				SetAttrLocal(attr_i++, ATTR_JS_ELM_IDX, ITEM_TYPE_NUM, INT_TO_PTR(hld_profile->GetNewEmbedElmNumber()), SpecialNs::NS_LOGDOC, FALSE, TRUE);
			}
			break;

#ifdef MEDIA_HTML_SUPPORT
		case HE_AUDIO:
		case HE_VIDEO:
			if (GetInserted() < HE_INSERTED_FIRST_HIDDEN_BY_ACTUAL)
			{
				RETURN_IF_ERROR(ConstructMediaElement(attr_i++));
				SetAttrLocal(attr_i++, Markup::MEDA_MEDIA_ATTR_IDX, ITEM_TYPE_NUM, INT_TO_PTR(Markup::MEDA_COMPLEX_MEDIA_ELEMENT), SpecialNs::NS_MEDIA, FALSE, TRUE);
			}
			break;

		case Markup::HTE_TRACK:
			if (GetInserted() < HE_INSERTED_FIRST_HIDDEN_BY_ACTUAL)
			{
				TrackElement* track_elm = OP_NEW(TrackElement, ());
				if (!track_elm)
					return OpStatus::ERR_NO_MEMORY;

				SetAttrLocal(attr_i++, Markup::MEDA_COMPLEX_TRACK_ELEMENT, ITEM_TYPE_COMPLEX, track_elm, SpecialNs::NS_MEDIA, TRUE, TRUE);
			}
			break;
#endif // MEDIA_HTML_SUPPORT

#ifdef CANVAS_SUPPORT
		case HE_CANVAS:
			if (GetInserted() < HE_INSERTED_FIRST_HIDDEN_BY_ACTUAL &&
				hld_profile->GetESEnabled()
				&& g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::CanvasEnabled,
				*hld_profile->GetURL()))
			{
				Canvas* value = OP_NEW(Canvas, ());
				if (!value)
					return OpStatus::ERR_NO_MEMORY;
				SetAttrLocal(attr_i++, Markup::VEGAA_CANVAS, ITEM_TYPE_COMPLEX,
					value, SpecialNs::NS_OGP, TRUE, TRUE);
			}
			break;
#endif // CANVAS_SUPPORT

		// Insert form values if that kind of element
		case HE_TEXTAREA:
		case HE_OUTPUT:
#if defined(_SSL_SUPPORT_) && !defined(_EXTERNAL_SSL_SUPPORT_)
		case HE_KEYGEN:
#endif
		case HE_INPUT:
		case HE_SELECT:
		case HE_BUTTON:
			CHECK_IS_FORM_VALUE_TYPE(this);
			{
				FormValue* value;
				RETURN_IF_ERROR(ConstructFormValue(hld_profile, value));
				SetAttrLocal(attr_i++, ATTR_FORM_VALUE, ITEM_TYPE_COMPLEX,
					value, SpecialNs::NS_LOGDOC, TRUE, TRUE);
			}
			break;
		}
	} // end if (ns == NS_HTML)
#if defined(SVG_SUPPORT)
	else if (ns_type == NS_SVG)
	{
		switch (type)
		{
		case Markup::SVGE_STYLE:
			SetAttrLocal(attr_i++, ATTR_CSS, ITEM_TYPE_CSS, 0, SpecialNs::NS_LOGDOC, TRUE, TRUE);
			// fall through
		case Markup::SVGE_SCRIPT:

			// link, script and style tag need some special attributes
			ret_stat = SetSrcListAttr(attr_i++, 0);
			break;
		}
	}
#endif // SVG_SUPPORT

	return ret_stat;
}

OP_STATUS
HTML_Element::Construct(HLDocProfile* hld_profile, int ns_idx,
						HTML_ElementType type, HtmlAttrEntry* ha_list,
						HE_InsertType inserted/*=HE_NOT_INSERTED*/,
						BOOL decode_entities_in_attributes /*= FALSE*/)
{
	css_properties = NULL;
	layout_box = NULL;

	packed1_init = 0;
	packed2_init = 0;
	SetType(type);

	g_ns_manager->GetElementAt(GetNsIdx())->DecRefCount();
	SetNsIdx(ns_idx);
	g_ns_manager->GetElementAt(ns_idx)->IncRefCount();

	NS_Type ns_type = g_ns_manager->GetNsTypeAt(ns_idx);

	unsigned attr_count = 0;
	unsigned attr_i = 0;

	if (inserted != HE_NOT_INSERTED)
	{
		SetInserted(inserted);
		attr_count++;
	}

	if (ha_list)
	{
#if defined XML_EVENTS_SUPPORT
		BOOL has_xml_events_attributes = FALSE;
#endif // XML_EVENTS_SUPPORT

		while (ha_list[attr_i].attr != ATTR_NULL)
		{
			if (!ha_list[attr_i].is_special)
			{
				switch (g_ns_manager->GetNsTypeAt(ResolveNsIdx(ha_list[attr_i].ns_idx)))
				{
				case NS_HTML:
					{
						if (ha_list[attr_i].attr == ATTR_FACE) // one extra for font number
							attr_count++;
#ifdef PICTOGRAM_SUPPORT
						else if (type == HE_OBJECT && ha_list[attr_i].attr == ATTR_DATA
							&& ha_list[attr_i].value_len > 7 && uni_strni_eq(ha_list[attr_i].value, "PICT://", 7))
						{
							attr_count++;
						}
#endif // PICTOGRAM_SUPPORT
					}
					break;

#ifdef PICTOGRAM_SUPPORT
				case NS_WML:
					if (ha_list[attr_i].attr == WA_LOCALSRC)
						attr_count++;
					break;
#endif // PICTOGRAM_SUPPORT

#ifdef XML_EVENTS_SUPPORT
				case NS_EVENT:
					has_xml_events_attributes = TRUE;
					hld_profile->GetFramesDocument()->SetHasXMLEvents();
					break;
#endif // XML_EVENTS_SUPPORT
				}
			}

			attr_i++;
			attr_count++;
		}

#if defined XML_EVENTS_SUPPORT
		if (has_xml_events_attributes)
			attr_count++; // one extra for ATTR_XML_EVENTS_REGISTRATION
#endif // XML_EVENTS_SUPPORT
	}

	attr_count += GetNumberOfExtraAttributesForType(type, ns_type, hld_profile, ha_list);

	if (attr_count)
	{
		data.attrs = NEW_HTML_Attributes((attr_count));

		if (!data.attrs)
		{
			hld_profile->SetIsOutOfMemory(TRUE);
			return OpStatus::ERR_NO_MEMORY;
		}

		SetAttrSize(attr_count);
		REPORT_MEMMAN_INC(attr_count * sizeof(AttrItem));
	}
	else
		data.attrs = NULL;

	void* value;
	ItemType item_type;
	attr_i = 0;

	if (ha_list)
	{
		BOOL replace_escapes = Markup::IsRealElement(Type()) && decode_entities_in_attributes;

		for (int i=0; ha_list[i].attr != ATTR_NULL; i++)
		{
			item_type = ITEM_TYPE_UNDEFINED;
			value = 0;
			HtmlAttrEntry* hae = &ha_list[i];
			BOOL need_free = TRUE;
			BOOL is_event = FALSE;

			const uni_char* orig_hae_value = hae->value;
			UINT orig_hae_value_len = hae->value_len;

			// If we allocate a new buffer for
			BOOL may_steal_hae_value = FALSE;
			if (replace_escapes && hae->value_len > 0)
			{
				OP_BOOLEAN replaced = ReplaceAttributeEntities(hae);
				if (OpStatus::IsError(replaced))
					goto oom_err;
				if (replaced == OpBoolean::IS_TRUE)
					may_steal_hae_value = TRUE; // Now we own hae->value
			}
			OP_STATUS status = ConstructAttrVal(hld_profile, hae, may_steal_hae_value, value, item_type, need_free, is_event, ha_list, &attr_i);
			if (may_steal_hae_value)
			{
				OP_DELETEA(const_cast<uni_char*>(hae->value));
				hae->value = orig_hae_value;
				hae->value_len = orig_hae_value_len;
			}
			if (OpStatus::IsMemoryError(status))
				goto oom_err;

			if (item_type != ITEM_TYPE_UNDEFINED)
				SetAttrLocal(attr_i++, hae->attr, item_type, value, hae->ns_idx, need_free, hae->is_special, hae->is_id, hae->is_specified, is_event);
		}
	}

	if (OpStatus::IsMemoryError(SetExtraAttributesForType(attr_i, ns_type, ha_list, attr_count, hld_profile)))
		goto oom_err;

	OP_ASSERT(attr_i <= attr_count);

	// reset unidentified attrs
	while (attr_i < attr_count)
	{
		SetAttrLocal(attr_i, ATTR_NULL, ITEM_TYPE_BOOL, NULL, NS_IDX_DEFAULT, FALSE, TRUE);
		attr_i++;
	}

#if defined(JS_PLUGIN_SUPPORT)
	if (type == HE_OBJECT)
		RETURN_IF_MEMORY_ERROR(SetupJsPluginIfRequested(GetStringAttr(ATTR_TYPE), hld_profile));
#endif // JS_PLUGIN_SUPPORT

#if defined(SVG_SUPPORT)
	if (ns_type == NS_SVG && type == Markup::SVGE_SVG)
	{
		SVGContext* ctx = g_svg_manager->CreateDocumentContext(this, hld_profile->GetLogicalDocument());
		if (!ctx)
			goto oom_err;
	}
#endif // SVG_SUPPORT

	return OpStatus::OK;

oom_err:
	hld_profile->SetIsOutOfMemory(TRUE);
	return OpStatus::ERR_NO_MEMORY;
}

#if defined(JS_PLUGIN_SUPPORT)
OP_STATUS HTML_Element::SetupJsPluginIfRequested(const uni_char* type_attr, HLDocProfile* hld_profile)
{
	if (type_attr)
	{
		FramesDocument *frames_doc = hld_profile->GetFramesDocument();

		RETURN_IF_ERROR(frames_doc->ConstructDOMEnvironment());

		DOM_Environment *environment = frames_doc->GetDOMEnvironment();

		if (environment && environment->IsEnabled())
			if (JS_Plugin_Context *ctx = environment->GetJSPluginContext())
				if (ctx->HasObjectHandler(type_attr, NULL))
				{
					ES_Runtime *runtime = environment->GetRuntime();
					DOM_Object *node;
					RETURN_IF_ERROR(environment->ConstructNode(node, this));
					return ctx->HandleObject(type_attr, this, runtime, ES_Runtime::GetHostObject(runtime->GetGlobalObjectAsPlainObject()));
				}
	}

	return OpStatus::ERR;
}
#endif

OP_STATUS HTML_Element::Construct(HLDocProfile* hld_profile, const uni_char* ttxt, unsigned int text_len)
{
	return Construct(ttxt, text_len, FALSE, FALSE);
}

OP_STATUS
HTML_Element::Construct(const uni_char* ttxt, unsigned int text_len, BOOL resolve_entities, BOOL is_cdata)
{
	OP_ASSERT(text_len <= SplitTextLen || !"Use AppendText/SetText if you are going to insert long text chunks, or rather use them anyway");

	css_properties = NULL;
	layout_box = NULL;

	packed1_init = 0;
	packed2_init = 0;
	packed2.props_clean = 1;

	g_ns_manager->GetElementAt(GetNsIdx())->DecRefCount();
	SetNsIdx(NS_IDX_DEFAULT);
	g_ns_manager->GetElementAt(NS_IDX_DEFAULT)->IncRefCount();

	SetType(HE_TEXT);

	// Avoid allocating a TextData if we're going to work more on it anyway,
	// which is signaled by a NULL ttxt.
	if (ttxt)
	{
		data.text = NEW_TextData(());
		if( ! data.text || data.text->Construct(ttxt, text_len, resolve_entities, is_cdata, FALSE) == OpStatus::ERR_NO_MEMORY )
		{
			DELETE_TextData(data.text);
			data.text = NULL;
			return OpStatus::ERR_NO_MEMORY;
		}
	}
	else
		data.text = NULL;

	return OpStatus::OK;
}

static OP_STATUS CreateStyleAttribute(const uni_char* str, size_t str_len, URL* base_url, HLDocProfile* hld_profile, void*& value, ItemType& item_type)
{
	CSS_property_list* list = NULL;

	if (str_len > 0 && hld_profile)
	{
		CSS_PARSE_STATUS stat;
		list = CSS::LoadHtmlStyleAttr(str, str_len, base_url ? *base_url : URL(), hld_profile, stat);
		if (OpStatus::IsMemoryError(stat))
			return stat;
	}

	value = (void*) NULL;
	if (str)
	{
		uni_char* string = UniSetNewStrN(str, str_len);
		if (list)
		{
			if (string)
			{
				StyleAttribute* style_attr = OP_NEW(StyleAttribute, (string, list));
				value = static_cast<void*>(style_attr);
			}

			if (!value)
			{
				OP_DELETEA(string);
				list->Unref();
				return OpStatus::ERR_NO_MEMORY;
			}

			item_type = ITEM_TYPE_COMPLEX;
		}
		else
		{
			value = static_cast<void*>(string);
			item_type = ITEM_TYPE_STRING;
		}
	}

	return OpStatus::OK;
}

OP_STATUS
ClassAttribute::Construct(const OpVector<ReferencedHTMLClass>& classes)
{
	UINT32 count = classes.GetCount();

	OP_ASSERT(count > 0);

	ReferencedHTMLClass** class_list = OP_NEWA(ReferencedHTMLClass*, count+1);

	if (class_list)
	{
		class_list[count] = NULL;

		while (count--)
			class_list[count] = classes.Get(count);

		SetClassList(class_list);
		return OpStatus::OK;
	}
	else
		return OpStatus::ERR_NO_MEMORY;
}

/* virtual */
ClassAttribute::~ClassAttribute()
{
	if (IsSingleClass())
	{
		if (m_class)
			m_class->UnRef();
	}
	else
	{
		ReferencedHTMLClass** cur = GetClassList();
		while (*cur)
		{
			(*cur)->UnRef();
			cur++;
		}
		OP_DELETEA(GetClassList());
	}

	OP_DELETEA(m_string);
}

/* virtual */ OP_STATUS
ClassAttribute::CreateCopy(ComplexAttr **copy_to)
{
	ReferencedHTMLClass* single_class = NULL;
	ReferencedHTMLClass** new_class_list = NULL;

	uni_char* string = UniSetNewStr(m_string);
	if (!string)
		return OpStatus::ERR_NO_MEMORY;

	if (IsSingleClass())
		single_class = m_class;
	else
	{
		ReferencedHTMLClass** class_list = GetClassList();
		unsigned int i = 0;
		while (*class_list++)
			i++;

		new_class_list = OP_NEWA(ReferencedHTMLClass*, i+1);
		if (!new_class_list)
		{
			OP_DELETEA(string);
			return OpStatus::ERR_NO_MEMORY;
		}
	}

	ClassAttribute* copy = OP_NEW(ClassAttribute, (string, m_class));

	if (!copy)
	{
		OP_DELETEA(string);
		OP_DELETEA(new_class_list);
		return OpStatus::ERR_NO_MEMORY;
	}

	if (single_class)
		single_class->Ref();
	else
		if (new_class_list)
		{
			copy->SetClassList(new_class_list);
			ReferencedHTMLClass** class_list = GetClassList();
			while ((*new_class_list = *class_list++))
				(*new_class_list++)->Ref();
		}

	*copy_to = copy;
	return OpStatus::OK;
}

/* virtual */ BOOL
ClassAttribute::MoveToOtherDocument(FramesDocument *old_document, FramesDocument *new_document)
{
	if (!new_document)
		return FALSE;

	LogicalDocument* logdoc = new_document->GetLogicalDocument();

	if (!logdoc)
		return FALSE;

	if (IsSingleClass())
	{
		if (m_class)
		{
			uni_char* new_str = UniSetNewStr(m_class->GetString());
			if (!new_str)
				return FALSE;

			ReferencedHTMLClass* new_ref = logdoc->GetClassReference(new_str);
			if (new_ref)
			{
				m_class->UnRef();
				m_class = new_ref;
			}
			else
				return FALSE;
		}
	}
	else
	{
		ReferencedHTMLClass** class_list = GetClassList();

		while (*class_list)
		{
			uni_char* new_str = UniSetNewStr((*class_list)->GetString());
			if (!new_str)
				return FALSE;

			ReferencedHTMLClass* new_ref = logdoc->GetClassReference(new_str);
			if (new_ref)
			{
				(*class_list)->UnRef();
				*class_list++ = new_ref;
			}
			else
				return FALSE;
		}
	}

	return TRUE;
}

OP_STATUS
StringTokenListAttribute::SetValue(const uni_char* attr_str_in, size_t length)
{
	OP_ASSERT(attr_str_in);
	RETURN_IF_ERROR(m_string.Set(attr_str_in, length));

	m_list.DeleteAll();
	// Decompose string into tokens.
	const uni_char* attr_str = m_string.CStr();
	while (*attr_str)
	{
		// Skip whitespace.
		attr_str += uni_strspn(attr_str, WHITESPACE_L);
		// Find size of next token.
		if (size_t token_len = uni_strcspn(attr_str, WHITESPACE_L))
		{
			OpString* new_token = OP_NEW(OpString, ());
			if (!new_token || OpStatus::IsMemoryError(new_token->Set(attr_str, token_len)))
			{
				OP_DELETE(new_token);
				return OpStatus::ERR_NO_MEMORY;
			}
			RETURN_IF_ERROR(m_list.Add(new_token));

			// Skip this token.
			attr_str += token_len;
			// Continue populating collection from next token.
		}
	}
	return OpStatus::OK;
}

/* virtual */ OP_STATUS
StringTokenListAttribute::CreateCopy(ComplexAttr **copy_to)
{
	OpAutoPtr<StringTokenListAttribute> ap_copy(OP_NEW(StringTokenListAttribute, ()));
	RETURN_OOM_IF_NULL(ap_copy.get());

	RETURN_IF_ERROR(ap_copy->m_string.Set(m_string));
	for (UINT32 i_token = 0; i_token < m_list.GetCount(); i_token++)
	{
		OpAutoPtr<OpString> new_token(OP_NEW(OpString, ()));
		RETURN_OOM_IF_NULL(new_token.get());
		RETURN_IF_ERROR(new_token->Set(m_list.Get(i_token)->CStr()));
		RETURN_IF_ERROR(ap_copy->m_list.Add(new_token.release()));
	}

	*copy_to = ap_copy.release();
	return OpStatus::OK;
}

OP_STATUS
StringTokenListAttribute::Add(const uni_char* token_to_add)
{
	OP_ASSERT(token_to_add);
	OP_ASSERT(token_to_add[0] != ' ');
	if (Contains(token_to_add))
		// No need to add, it's already there.
		return OpStatus::OK;

	// Add to list.
	OpAutoPtr<OpString> str_token_to_add(OP_NEW(OpString, ()));
	RETURN_IF_ERROR(str_token_to_add->Set(token_to_add));
	RETURN_IF_ERROR(m_list.Add(str_token_to_add.release()));

	// Add to underlying string.
	TempBuffer *buffer = g_opera->logdoc_module.GetTempBuffer();
	OP_STATUS status = buffer->Append(m_string.CStr());
	if (OpStatus::IsSuccess(status))
		status = StringTokenListAttribute::AppendWithSpace(buffer, token_to_add);
	if (OpStatus::IsSuccess(status))
		status = m_string.Set(buffer->GetStorage());
	g_opera->logdoc_module.ReleaseTempBuffer(buffer);
	return status;
}

OP_STATUS
StringTokenListAttribute::Remove(const uni_char* token_to_remove)
{
	OP_ASSERT(token_to_remove);
	OP_ASSERT(token_to_remove[0] != ' ');
	BOOL did_remove = FALSE;

	// Remove from list.
	for (int i = m_list.GetCount() - 1; i >= 0; i--)
		if (uni_str_eq(m_list.Get(i)->CStr(), token_to_remove))
		{
			m_list.Delete(i);
			did_remove = TRUE;
		}

	// Remove from underlying string.
	if (did_remove)
	{
		OP_STATUS status = OpStatus::OK;

		const uni_char* attr_str = m_string.CStr();
		const uni_char* copy_start = attr_str;
		const uni_char* copy_end = attr_str;
		unsigned int to_remove_len = uni_strlen(token_to_remove);
		TempBuffer* buffer = g_opera->logdoc_module.GetTempBuffer();
		while (*attr_str)
		{
			// Skip whitespace.
			attr_str += uni_strspn(attr_str, WHITESPACE_L);
			// Find size of next token.
			if (size_t token_len = uni_strcspn(attr_str, WHITESPACE_L))
			{
				if (uni_strncmp(attr_str, token_to_remove, MAX(token_len, to_remove_len)) == 0)
				{
					// Copy what we have so far.
					if (OpStatus::IsError(status = StringTokenListAttribute::AppendWithSpace(buffer, copy_start, copy_end - copy_start)))
						break;
					// Skip this occurence of to_remove.
					attr_str += token_len;
					// skip whitespace
					attr_str += uni_strspn(attr_str, WHITESPACE_L);
					// Continue copying from next token.
					copy_start = copy_end = attr_str;
				}
				else
				{
					// Keep this token, will copy later.
					attr_str += token_len;
					copy_end = attr_str;
				}
			}
		}
		// Now append what remains.
		if (status == OpStatus::OK)
			status = StringTokenListAttribute::AppendWithSpace(buffer, copy_start, attr_str - copy_start);
		if (status == OpStatus::OK)
			status = m_string.Set(buffer->GetStorage() ? buffer->GetStorage() : UNI_L(""));
		g_opera->logdoc_module.ReleaseTempBuffer(buffer);
		return status;
	}
	else
		// There's nothing to remove.
		return OpStatus::OK;
}

static StringTokenListAttribute*
CreateStringTokenListAttribute(const uni_char* attr_str, size_t str_len)
{
	if (!attr_str)
	{
		attr_str = UNI_L("");
		str_len = 0;
	}

	// Create complex attribute.
	StringTokenListAttribute* stl = OP_NEW(StringTokenListAttribute, ());
	if (!stl || OpStatus::IsError(stl->SetValue(attr_str, str_len)))
	{
		OP_DELETE(stl);
		return NULL;
	}
	return stl;
}

static OP_STATUS CreateBackgroundAttribute(const uni_char* str, size_t str_len, void*& value, ItemType& item_type)
{
	/* Fake background style by rewriting it as a css property.  The reason for this is that layout
	   need a stable CSS_decl, tied to the element.

	   When background images is stored as a list in the cascade (as it must be for multiple
	   background images to work), the only current viable choice is to keep a pointer to the css
	   declaration directly (CSS_decl).  When the declaration comes from a HTML attribute, the
	   declarations are usually shared (allocated statically) and only stable during property
	   reload, i.e. pointers to them can not be stored in the cascade.  Storing the background
	   attribute as a StyleAttribute lets us have a pointer to a stable CSS_decl in the cascade. */

	CSS_property_list* prop_list = OP_NEW(CSS_property_list, ());
	if (!prop_list)
		return OpStatus::ERR_NO_MEMORY;
	else
	{
		prop_list->Ref();

		CSS_generic_value gen_arr;
		gen_arr.value_type = CSS_FUNCTION_URL;
		gen_arr.value.string = UniSetNewStrN(str_len ? str : UNI_L(""), str_len);

		if (!gen_arr.value.string)
		{
			prop_list->Unref();
			return OpStatus::ERR_NO_MEMORY;
		}

		/* AddDeclL will make a copy of the string. We'll re-use the allocated string for the
		   ToString representation of StyleAttribute. */
		TRAPD(status, prop_list->AddDeclL(CSS_PROPERTY_background_image, &gen_arr, 1, 1, FALSE, CSS_decl::ORIGIN_AUTHOR));
		if (OpStatus::IsError(status))
		{
			OP_DELETEA(gen_arr.value.string);
			prop_list->Unref();
			return status;
		}
		else
		{
			value = OP_NEW(StyleAttribute, (gen_arr.value.string, prop_list));
			if (!value)
			{
				OP_DELETEA(gen_arr.value.string);
				prop_list->Unref();
				return OpStatus::ERR_NO_MEMORY;
			}
		}
	}

	item_type = ITEM_TYPE_COMPLEX;
	return OpStatus::OK;
}

#if defined IMODE_EXTENSIONS
void
AddTelUrlAttributes(const uni_char *&value_str, UINT &value_len, HtmlAttrEntry* ha_list)
{
//	tel/mail-urls:
//	initially this iMode thingie resides here
//	Should maybe be moved to HandleEvent. But then we need to know and
//	all attributes that we should parse (which, according to spec, we
//	shall not assume)

	if( !ha_list )
		return;

	if ((value_len > 4 && uni_strni_eq(value_str, "TEL:", 4))
			|| (value_len > 5 && uni_strni_eq(value_str, "MAIL:", 5))
			|| (value_len > 5 && uni_strni_eq(value_str, "VTEL:", 5)))
	{
		uni_char* tmp_buf = (uni_char*)g_memory_manager->GetTempBuf();
		UINT buf_len = g_memory_manager->GetTempBufLen();

		if (value_len + 1 <= buf_len)
		{
			if (value_len)
				uni_strncpy(tmp_buf, value_str, value_len);

			BOOL first_attr = TRUE;
			for (int i = 0; ha_list[i].attr != ATTR_NULL; i++)
			{
				HtmlAttrEntry* current_hae = &ha_list[i];

				if (ha_list[i].attr != ATTR_HREF
# ifdef TEL_EXCLUDE_CORE_ATTRS
					&& ha_list[i].attr != ATTR_CLASS
					&& ha_list[i].attr != ATTR_ID
					&& ha_list[i].attr != ATTR_STYLE
					&& ha_list[i].attr != ATTR_TITLE
					&& ha_list[i].attr != ATTR_LANG
					&& ha_list[i].attr != ATTR_DIR
# endif // TEL_EXCLUDE_CORE_ATTRS
					&& !ha_list[i].is_special)
				{
					// avoid buffer overrun
					if (value_len + current_hae->value_len + current_hae->name_len + 4 > buf_len)
						break;

					if (first_attr)
						tmp_buf[value_len] = '?';
					else
						tmp_buf[value_len] = '&';

					value_len++;

					first_attr = FALSE;

					if (current_hae->name_len)
					{
						uni_strncpy(tmp_buf + value_len, current_hae->name, current_hae->name_len);
						value_len += current_hae->name_len;

						tmp_buf[value_len++] = '=';

						if (current_hae->value_len)
						{
							uni_strncpy(tmp_buf + value_len, current_hae->value, current_hae->value_len);
							value_len += current_hae->value_len;
						}
					}
				}
			}
			tmp_buf[value_len] = '\0';

			value_str = tmp_buf;
		}
	}
}
#endif // IMODE_EXTENSIONS


BOOL HTML_Element::IsSameAttrValue(int index, const uni_char* name, const uni_char* value)
{
	void *old_attr_value = GetValueItem(index);
	BOOL is_same_value = FALSE;

	switch (GetItemType(index))
	{
	case ITEM_TYPE_BOOL:
	case ITEM_TYPE_NUM:
		{
			uni_char buf[25]; /* ARRAY OK 2009-05-07 stighal */
#ifdef DEBUG_ENABLE_OPASSERT
			buf[24] = 0xabcd; // To catch too long keywords
#endif // DEBUG_ENABLE_OPASSERT
			int ns_idx = GetAttrNs(index);
			int resolved_ns_idx = ResolveNsIdx(ns_idx);
			NS_Type attr_ns = g_ns_manager->GetNsTypeAt(resolved_ns_idx);
			const uni_char* generated_value = GenerateStringValueFromNumAttr(GetAttrItem(index), attr_ns, old_attr_value, buf, HE_UNKNOWN);
			OP_ASSERT(buf[24] == 0xabcd);
			is_same_value = generated_value && uni_str_eq(generated_value, value);
		}
		break;

	case ITEM_TYPE_STRING:
		if (GetAttrItem(index) == ATTR_XML)
		{
			const uni_char* old_name_value = static_cast<const uni_char *>(old_attr_value);
			// Verify same attribute name
			OP_ASSERT(GetNsIdx() != NS_IDX_HTML ? uni_str_eq(name, old_name_value) : uni_stri_eq(name, old_name_value));
			int value_offset = uni_strlen(name) + 1;
			is_same_value = uni_str_eq(value, old_name_value+value_offset);
		}
		else
			is_same_value = uni_str_eq(static_cast<const uni_char *>(old_attr_value), value);
		break;

	case ITEM_TYPE_URL_AND_STRING:
		if (Type() == HE_SCRIPT || Type() == HE_OBJECT)
		{
			UrlAndStringAttr *old_attr = static_cast<UrlAndStringAttr*>(old_attr_value);
			is_same_value = old_attr->GetString() && uni_str_eq(old_attr->GetString(), value);
		}
		break;

	case ITEM_TYPE_COMPLEX:
		ComplexAttr *old_complex_attr = static_cast<ComplexAttr*>(old_attr_value);
		if (old_complex_attr)
		{
			TempBuffer old_attr_buf;
			if (OpStatus::IsSuccess(old_complex_attr->ToString(&old_attr_buf)))
			{
				const uni_char *old_attr_string = old_attr_buf.GetStorage();
				is_same_value = old_attr_string && uni_str_eq(old_attr_string, value);
			}
		}
		break;
	}
	return is_same_value;
}

OP_STATUS HTML_Element::GetInnerTextOrImgAlt(TempBuffer& title_buffer, const uni_char*& alt_text)
{
	for (HTML_Element *iter = this, *stop = this->NextSiblingActual(); iter != stop;)
	{
		NS_Type helm_ns_type = iter->GetNsType();

		if (iter->Type() == HE_TEXT)
		{
			RETURN_IF_ERROR(title_buffer.Append(iter->TextContent()));
		}
		else if (helm_ns_type == NS_HTML)
		{
			switch (iter->Type())
			{
			case HE_IMG:
				if (iter->GetIMG_Alt())
				{
					alt_text = iter->GetIMG_Alt();
				}
				break;
			case HE_SELECT:
			case HE_TEXTAREA:
			case HE_SCRIPT:
			case HE_STYLE:
				iter = iter->NextSiblingActual();
				continue;
			}
		}

		iter = iter->NextActual();
	}

	return OpStatus::OK;
}

/**
 * This either makes a copy of hae->value or sets hae->value
 * to NULL and then returns the original hae->value or,
 * if hae->value_len is 0, returns empty_string.
 *
 * @returns The string as void* to make it easier to use in ConstructAttrVal or NULL if there was an OOM.
 */
static void* StealOrCopyString(HtmlAttrEntry* hae, BOOL may_steal_hae_value, const uni_char* empty_string)
{
	OP_ASSERT(empty_string && !*empty_string);
	UINT hae_value_len = hae->value_len;
	void* value;
	if (hae_value_len)
	{
		if (may_steal_hae_value)
		{
			value = (void*)hae->value;
			hae->value = NULL; // Signal that it has been stolen
		}
		else
			value = (void*) UniSetNewStrN(hae->value, hae_value_len);
	}
	else
		value = (void*)empty_string;

	return value;
}

OP_STATUS HTML_Element::ConstructAttrVal(HLDocProfile* hld_profile,
										 HtmlAttrEntry* hae,
										 BOOL may_steal_hae_value,
										 void*& value,
										 ItemType& item_type,
										 BOOL& need_free,
										 BOOL& is_event,
										 HtmlAttrEntry* ha_list,
										 unsigned *attr_index)
{
	OP_PROBE8(OP_PROBE_HTML_ELEMENT_CONSTRUCTATTRVAL);

	OP_ASSERT(!may_steal_hae_value || hae->value && uni_strlen(hae->value) == hae->value_len);
	OP_STATUS ret_stat = OpStatus::OK;
	short attr = hae->attr;
	URL* base_url = hld_profile ? hld_profile->BaseURL() : NULL;

	item_type = ITEM_TYPE_UNDEFINED;
	value = NULL;
	is_event = FALSE;

	const uni_char* empty_string = UNI_L("");

	if (hae->is_special)
	{
		if (hae->ns_idx == SpecialNs::NS_LOGDOC)
		{
			if (hae->attr == ATTR_XML_NAME)
			{
				if (hae->value)
				{
					LogdocXmlName *new_name = OP_NEW(LogdocXmlName, ());
					if (new_name)
					{
						ret_stat = new_name->SetName((LogdocXmlName*)hae->value);
						item_type = ITEM_TYPE_COMPLEX;
						need_free = TRUE;
						value = (void*)new_name;
					}
					else
						ret_stat = OpStatus::ERR_NO_MEMORY;
				}
			}
			else if (hae->attr == ATTR_TEXT_CONTENT ||
					 hae->attr == ATTR_PUB_ID ||
					 hae->attr == ATTR_SYS_ID)
			{
				if (hae->value_len > 0)
				{
					value = (void*) UniSetNewStrN(hae->value, hae->value_len); // FIXME:REPORT_MEMMAN-KILSMO
					need_free = TRUE;
				}
				else
				{
					value = (void*)empty_string;
					need_free = FALSE;
				}

				if (value)
					item_type = ITEM_TYPE_STRING;
				else if (hae->value)
					ret_stat = OpStatus::ERR_NO_MEMORY;
			}
			else if (hae->attr == ATTR_LOGDOC)
			{
				item_type = ITEM_TYPE_COMPLEX;
				need_free = FALSE;
				value = const_cast<uni_char*>(hae->value);
			}
			else if (hae->attr == ATTR_ALTERNATE)
			{
				if (hae->value_len == 3 && uni_strncmp(hae->value, "yes", 3) == 0)
				{
					value = INT_TO_PTR(TRUE);
					item_type = ITEM_TYPE_BOOL;
				}
			}
			else
			{
				OP_ASSERT(!"Unknown special attribute in the logdoc/lexer namespace");
			}
		}

		return ret_stat;
	}

	NS_Type attr_ns = g_ns_manager->GetNsTypeAt(ResolveNsIdx(hae->ns_idx));

	switch (attr_ns)
	{
	case NS_HTML:
		{
			switch (hae->attr)
			{
			case ATTR_FACE:
				if (hae->value && hld_profile)
				{
#ifdef FONTSWITCHING
					short font_num = GetFirstFontNumber(hae->value, hae->value_len, hld_profile->GetPreferredScript());
#else //FONTSWITCHING
					short font_num = GetFirstFontNumber(hae->value, hae->value_len, WritingSystem::Unknown);
#endif // FONTSWITCHING
					value = INT_TO_PTR(font_num);
					item_type = ITEM_TYPE_NUM;
				}
				break;

			case ATTR_NOWRAP:
				// 06/11/97: Removed since it is not supported on <TABLE> by HTML 3.2 or Netscape
				if (Type() == HE_TABLE)
					break;
			case ATTR_ISMAP:
			case ATTR_NOHREF:
			case ATTR_HIDDEN:
			case ATTR_ITEMSCOPE:
			case ATTR_NOSHADE:
			case ATTR_NORESIZE:
			case ATTR_CHECKED:
			case ATTR_SELECTED:
			case ATTR_READONLY:
			case ATTR_MULTIPLE:
			case ATTR_COMPACT:
			case ATTR_DISABLED:
			case ATTR_DEFER:
			case ATTR_DECLARE:
			case ATTR_AUTOPLAY:
			case ATTR_CONTROLS:
			case ATTR_MUTED:
			case ATTR_PUBDATE:
			case Markup::HA_DEFAULT:
			case ATTR_REQUIRED:
			case ATTR_AUTOFOCUS:
			case ATTR_NOVALIDATE:
			case ATTR_FORMNOVALIDATE:
			case Markup::HA_REVERSED:
				{
					// Normally the presence of the hidden attribute means that it's hidden, but it seems
					// we have supported hidden=false on plugins in the past so we might need to continue
					// supporting it.
					item_type = ITEM_TYPE_BOOL;
					if (attr == ATTR_MULTIPLE ||
					    (attr == ATTR_HIDDEN && (IsMatchingType(HE_EMBED, NS_HTML) || IsMatchingType(HE_OBJECT, NS_HTML))))
						value = INT_TO_PTR(ATTR_GetKeyword(hae->value, hae->value_len) != ATTR_VALUE_false);
					else
						value = INT_TO_PTR(TRUE);
				}
				break;

			case ATTR_FRAMEBORDER:
				value = INT_TO_PTR(hae->value_len==1 && *hae->value == '1');
				item_type = ITEM_TYPE_BOOL;
				break;

			case ATTR_COLOR:
				value = INT_TO_PTR(GetColorVal(hae->value, hae->value_len, TRUE));
				item_type = ITEM_TYPE_NUM;
				break;

			case ATTR_LOOP:
				value = INT_TO_PTR(SetLoop(hae->value, hae->value_len));
				item_type = ITEM_TYPE_NUM;
				break;

			case ATTR_WIDTH:
			case ATTR_HEIGHT:
			case ATTR_HSPACE:
			case ATTR_VSPACE:
			case ATTR_BORDER:
			case ATTR_SPAN:
			case ATTR_COLSPAN:
			case ATTR_ROWSPAN:
			case ATTR_CELLPADDING:
			case ATTR_CELLSPACING:
			case ATTR_CHAROFF:
			case ATTR_TOPMARGIN:
			case ATTR_LEFTMARGIN:
			case ATTR_RIGHTMARGIN:
			case ATTR_BOTTOMMARGIN:
			case ATTR_MARGINWIDTH:
			case ATTR_MARGINHEIGHT:
			case ATTR_FRAMESPACING:
			case ATTR_MAXLENGTH:
			case ATTR_SCROLLAMOUNT:
			case ATTR_SCROLLDELAY:
				{
					// Need a null terminated string for uni_atoi. Hadn't it been for that we could
					// have removed this string copy
					uni_char *tmp = (uni_char*)g_memory_manager->GetTempBuf();
					unsigned int tlen = (UNICODE_DOWNSIZE(g_memory_manager->GetTempBufLen()) <= hae->value_len) ? UNICODE_DOWNSIZE(g_memory_manager->GetTempBufLen())-1 : hae->value_len;
					if (tlen)
						uni_strncpy(tmp, hae->value, tlen);
					tmp[tlen] = '\0';

					int val = 0;

					while (*tmp && uni_isspace(*tmp))
						tmp++;

					BOOL store_as_string = FALSE;
					if (*tmp && (uni_isdigit(*tmp) || *tmp == '-' || *tmp == '+'))
						val = uni_atoi(tmp);
					else if (attr == ATTR_BORDER)
						val = Type() == HE_TABLE ? 1 : 0;
					else
						store_as_string = TRUE;

#ifdef CANVAS_SUPPORT
					if (val <= 0 && Type() == HE_CANVAS)
						store_as_string = TRUE;
#endif // CANVAS_SUPPORT

					if (store_as_string)
					{
						value = StealOrCopyString(hae, may_steal_hae_value, empty_string);
						item_type = ITEM_TYPE_STRING;
						if (!value)
							ret_stat = OpStatus::ERR_NO_MEMORY;
					}
					else
					{
						if (val < 0)
							val = 0;

						switch (attr)
						{
						case ATTR_WIDTH:
						case ATTR_HEIGHT:
						case ATTR_CHAROFF:
							{
								for (UINT k=0; k<tlen; k++)
								{
									if (tmp[k] == '%')
									{
										val = -val; // mark as percent
										break;
									}
								}
							}
							break;

						case ATTR_VSPACE:
						case ATTR_HSPACE:
							if (val > 253)
								val = 253;

							break;

						case ATTR_SPAN:
						case ATTR_COLSPAN:
							if (val > 1000) // MSIE limit!
								val = 1;

							break;

						case ATTR_ROWSPAN:
							if (val > SHRT_MAX)
								val = SHRT_MAX; // Opera layout engine limit!

							break;
						}

						value = INT_TO_PTR(val);
						item_type = ITEM_TYPE_NUM;
					}
				}
				break;

			case ATTR_SIZE:
				if (hae->value && hae->value_len)
				{
					item_type = ITEM_TYPE_NUM;
					if (Type() == HE_FONT)
					{
						int len = hae->value_len;
						const uni_char* tmp = hae->value;
						BOOL pos = *tmp == '+';
						BOOL neg = *tmp == '-';
						if (pos || neg)
						{
							tmp++;
							len--;
						}
						if (len)
						{
							int fsize = uni_atoi(tmp);
							if (pos)
								fsize += 3;
							else if (neg)
								fsize = 3 - fsize;

							if (fsize > 7)
								fsize = 7;
							else if (fsize < 1)
								fsize = 1;
							value = INT_TO_PTR(fsize);
						}
					}
					else
					{
						uni_char* end_ptr;
						int parsed_number = uni_strtol(hae->value, &end_ptr, 10);
						// We only accept numbers at the start of the value
						// and only if they're followed by something that isn't a
						// letter. This is compatible with MSIE. Mozilla is weird.
						if (end_ptr != hae->value && (end_ptr == hae->value + hae->value_len || !uni_isalpha(*end_ptr)))
							value = INT_TO_PTR(parsed_number);
						else
						{
							// Store as a string
							value = StealOrCopyString(hae, may_steal_hae_value, empty_string);
							item_type = ITEM_TYPE_STRING;
							if (!value)
								ret_stat = OpStatus::ERR_NO_MEMORY;
						}
					}
				}
				else
				{
					value = (void*) empty_string;
					item_type = ITEM_TYPE_STRING;
				}
				break;

			case ATTR_TABINDEX:
				{
					long tabindex;
					if (SetTabIndexAttr(hae->value, hae->value_len, this, tabindex))
					{
						item_type = ITEM_TYPE_NUM;
						value = (void*)tabindex;
					}
					else
					{
						// Store as a string
						value = StealOrCopyString(hae, may_steal_hae_value, empty_string);
						item_type = ITEM_TYPE_STRING;
						if (!value)
							ret_stat = OpStatus::ERR_NO_MEMORY;
					}

				}
				break;

			case ATTR_SHAPE:
			case ATTR_ALIGN:
			case ATTR_VALIGN:
			case ATTR_CLEAR:
			case ATTR_METHOD:
			case ATTR_FORMMETHOD:
			case ATTR_SCROLLING:
			case ATTR_DIRECTION:
			case ATTR_BEHAVIOR:
			case ATTR_DIR:
#ifdef _WML_SUPPORT_
				if (hld_profile && hld_profile->IsWml() && Type() == HE_TABLE)
				{
					item_type = ITEM_TYPE_STRING;
					if (hae->value_len)
					{
						value = (void*) SetStringAttr(hae->value, hae->value_len, FALSE);
						if (!value)
							ret_stat = OpStatus::ERR_NO_MEMORY;
					}
					else
						value = (void*)empty_string;
				}
				else
#endif // _WML_SUPPORT_
				{
					item_type = ITEM_TYPE_NUM;
					if (hae->value && hae->value_len)
					{
						BYTE val = ATTR_GetKeyword(hae->value, hae->value_len);
						SetNumAttrVal(Type(), attr, val, value);
					}
					else
					{
						value = (void*) empty_string;
						item_type = ITEM_TYPE_STRING;
					}
				}
				break;

			case ATTR_SPELLCHECK:
				{
					SPC_ATTR_STATE spellcheck_state = IsMatchingType(HE_INPUT, NS_HTML) ? SPC_DISABLE_DEFAULT : SPC_ENABLE_DEFAULT;
					if (hae->value && hae->value_len)
					{
						if (uni_strni_eq(hae->value, "true", hae->value_len))
							spellcheck_state = SPC_ENABLE;
						else if (uni_strni_eq(hae->value, "false", hae->value_len))
							spellcheck_state = SPC_DISABLE;
					}
					else
						spellcheck_state = SPC_ENABLE;

					value = INT_TO_PTR(spellcheck_state);
					item_type = ITEM_TYPE_NUM;
				}
				break;

			case ATTR_FRAME:
			case ATTR_RULES:
				if (hae->value && hae->value_len)
				{
					value = INT_TO_PTR(ATTR_GetKeyword(hae->value, hae->value_len));
					if (value != NULL)
						item_type = ITEM_TYPE_NUM;
				}
				if (item_type != ITEM_TYPE_NUM)
				{
					value = StealOrCopyString(hae, may_steal_hae_value, empty_string);
					item_type = ITEM_TYPE_STRING;
					if (!value)
						ret_stat = OpStatus::ERR_NO_MEMORY;
				}
				break;

			case ATTR_COORDS:
				if (hae->value_len)
				{
					// Parse the numbers in the string into an array of ints.

					int coords_len = 0;
					int *tmp_coords = (int*) g_memory_manager->GetTempBuf2k();
					int max_coords_len = g_memory_manager->GetTempBuf2kLen() / sizeof(int);

					// This is an approximation of the HTML5 algorithm for parsing lists of numbers:
					// http://www.whatwg.org/specs/web-apps/current-work/multipage/infrastructure.html#rules-for-parsing-a-list-of-integers
					const uni_char *tmp = hae->value;
					const uni_char* end = hae->value + hae->value_len;
					while (tmp < end)
					{
						// Skip initial trash
						const uni_char* start_number_p = tmp;
						while (tmp < end && !uni_isdigit(*tmp))
							tmp++;

						BOOL negated = FALSE;
						if (tmp > start_number_p && *(tmp-1) == '-')
							negated = TRUE;

						int current_number = 0;
						while (tmp < end && uni_isdigit(*tmp))
						{
							current_number = current_number * 10 + *tmp - '0';
							tmp++;
						}

						// Spec says:
						//BOOL is_acceptable_number_terminator =
						//	*tmp <= 0x1f || // control codes
						//	*tmp >= 0x21 && *tmp <= 0x2b || // the special chars you get from shift+digit on a us keyboard
						//	*tmp >= 0x2d && *tmp <= 0x2f || // - . and /
						//	*tmp == 0x3a || // colon
						//	*tmp >= 0x3c && *tmp <= 0x40 || // <, >, =, ?, @
						//	*tmp >= 0x5b && *tmp <= 0x60 || // [ ] ^ _
						//	*tmp >= 0x7b && *tmp <= 0x7f;
						BOOL is_acceptable_number_terminator = *tmp <= 0x7f && !op_isalnum(*tmp);
						if (tmp < end && is_acceptable_number_terminator)
						{
							// Skip all to the next seperator (yes, this is weird, but see HTML5, i.e. MSIE)
							while (tmp < end && *tmp != ' ' && *tmp != ',' && *tmp != ';')
								tmp++;
						}

						if (tmp < end && *tmp != ' ' && *tmp != ',' && *tmp != ';')
						{
							// Trailing garbage on the number. Abort the parsing
							tmp = end;
						}
						else
						{
							if (negated)
								current_number = -current_number; // These will be set to 0 below but I tried to keep this block generic in case we want to reuse it.

							tmp_coords[coords_len++] = current_number;
							if (tmp < end)
								tmp++;

							if (coords_len >= max_coords_len)
								break;
						}
					}
					// end generic number list parser

					if (coords_len)
					{
						CoordsAttr* ca = NEW_CoordsAttr((coords_len)); // FIXME:REPORT_MEMMAN-KILSMO

						// OOM check
						if (ca && OpStatus::IsSuccess(ca->SetOriginalString(hae->value, hae->value_len)))
						{
							int* coords = ca->GetCoords();
							for (int i = 0; i < coords_len; i++)
							{
								if (tmp_coords[i] < 0)
									coords[i] = 0;
								else
									coords[i] = tmp_coords[i];
							}
							value = (void*)ca;
							item_type = ITEM_TYPE_COORDS_ATTR;
						}
						else
						{
							OP_DELETE(ca);
							ret_stat = OpStatus::ERR_NO_MEMORY;
						}
					}
				}
				else
				{
					value = (void*)empty_string;
					item_type = ITEM_TYPE_STRING;
				}
			break;

		case ATTR_START:
			if (hae->value && hae->value_len)
				switch (Type())
				{
				case HE_OL:
					{
						const uni_char* start_str = hae->value;
						while (uni_isspace(*start_str) && ((UINT)(start_str - hae->value)) < hae->value_len)
							start_str++;
						long start = uni_strtol(start_str, NULL, 10, NULL);

						/* Check if start wasn't a valid number. */
						if (((UINT)(start_str - hae->value)) == hae->value_len || start == 0 && *start_str != '0')
						{
							value = StealOrCopyString(hae, may_steal_hae_value, empty_string);
							item_type = ITEM_TYPE_STRING;
							if (!value)
								ret_stat = OpStatus::ERR_NO_MEMORY;
						}
						else
						{
							value = INT_TO_PTR(start);
							item_type = ITEM_TYPE_NUM;
						}
					}
					break;

				default:
					value = StealOrCopyString(hae, may_steal_hae_value, empty_string);
					item_type = ITEM_TYPE_STRING;
					if (!value)
						ret_stat = OpStatus::ERR_NO_MEMORY;
				}
			else
			{
				value = (void*) empty_string;
				item_type = ITEM_TYPE_STRING;
			}
			break;

		case ATTR_BACKGROUND:
			ret_stat = CreateBackgroundAttribute(hae->value, hae->value_len, value, item_type);
			break;

		case ATTR_CODEBASE:
		case ATTR_CITE:
		case ATTR_DATA:
		case ATTR_SRC:
		case ATTR_LONGDESC:
		case ATTR_HREF:
		case ATTR_DYNSRC:
		case ATTR_USEMAP:
		case ATTR_ACTION:
		case ATTR_FORMACTION:
		case ATTR_POSTER:
		case ATTR_MANIFEST:
			if (hae->value_len)
			{
				const uni_char *value_str = hae->value;
				UINT value_len = hae->value_len;

#ifdef PICTOGRAM_SUPPORT
				if (hae->attr == ATTR_DATA && Type() == HE_OBJECT)
				{
					if (value_len > 7 && uni_strni_eq(value_str, "PICT://", 7))
					{
						OpString local_url_str;

						ret_stat = PictUrlConverter::GetLocalUrl(value_str, value_len, local_url_str);
						if (OpStatus::IsSuccess(ret_stat) && !local_url_str.IsEmpty())
						{
							value = (void*) SetUrlAttr(local_url_str.CStr(), local_url_str.Length(), base_url, hld_profile, FALSE, TRUE);
							if (value)
							{
								if (attr_index)
									SetAttrLocal((*attr_index)++, ATTR_LOCALSRC_URL, ITEM_TYPE_URL, value, SpecialNs::NS_LOGDOC, TRUE, TRUE);
								else
									SetSpecialAttr(ATTR_LOCALSRC_URL, ITEM_TYPE_URL, value, TRUE, SpecialNs::NS_LOGDOC);
							}
							else
								ret_stat = OpStatus::ERR_NO_MEMORY;

							if (OpStatus::IsMemoryError(ret_stat))
								break;
						}
					}
				}
#endif // PICTOGRAM_SUPPORT

#if defined IMODE_EXTENSIONS
				if (Type() == HE_A)
					AddTelUrlAttributes(value_str, value_len, ha_list);
#endif // IMODE_EXTENSIONS

				// FIXME: Don't modify attributes here, do it when they are used (relates to the ATTR_DATA case that strips whitespace)
				if (attr == ATTR_DATA)
				{
					value_str = SetStringAttr(value_str, value_len, TRUE);
					if (!value_str)
					{
						ret_stat = OpStatus::ERR_NO_MEMORY;
						break;
					}
				}

				UrlAndStringAttr *url_attr;

				ret_stat = UrlAndStringAttr::Create(value_str, value_len, url_attr);

				if (attr == ATTR_DATA)
					OP_DELETEA(value_str);

				if (OpStatus::IsSuccess(ret_stat))
				{
					value = static_cast<void*>(url_attr);
					item_type = ITEM_TYPE_URL_AND_STRING;
				}
			}
			else
			{
				UrlAndStringAttr *url_attr;
				ret_stat = UrlAndStringAttr::Create(UNI_L(""), 0, url_attr);
				if (OpStatus::IsSuccess(ret_stat))
				{
					value = static_cast<void*>(url_attr);
					item_type = ITEM_TYPE_URL_AND_STRING;
				}
			}
			break;

		case ATTR_TYPE:
			if (hae->value && hae->value_len)
			{
				HTML_ElementType type = Type();
				if (GetNsType() == NS_HTML &&
					(type == HE_INPUT ||
					type == HE_BUTTON ||
					type == HE_LI ||
					type == HE_UL ||
					type == HE_OL ||
					type == HE_MENU || // Kyocera extension
					type == HE_DIR)) // Kyocera extension
				{
					item_type = ITEM_TYPE_NUM;
					if (type != HE_INPUT && type != HE_BUTTON && hae->value_len == 1)
					{
						value = (void*) LIST_TYPE_NULL;
						if (*(hae->value) == 'a')
							value = (void*) LIST_TYPE_LOWER_ALPHA;
						else if (*(hae->value) == 'A')
							value = (void*) LIST_TYPE_UPPER_ALPHA;
						else if (*(hae->value) == 'i')
							value = (void*) LIST_TYPE_LOWER_ROMAN;
						else if (*(hae->value) == 'I')
							value = (void*) LIST_TYPE_UPPER_ROMAN;
						else if (*(hae->value) == '1') // || *(hae->value) == 'n' || *(hae->value) == 'N')
							value = (void*) LIST_TYPE_NUMBER;
					}
					else
					{
						BYTE val = ATTR_GetKeyword(hae->value, hae->value_len);
						SetNumAttrVal(type, attr, val, value);
					}
				}
				else
				{
					value = StealOrCopyString(hae, may_steal_hae_value, empty_string);
					item_type = ITEM_TYPE_STRING;
					if (!value)
						ret_stat = OpStatus::ERR_NO_MEMORY;
				}
			}
			else
			{
				// Empty type attributes for stylesheets is not the same as none
				value = (void*) empty_string;
				item_type = ITEM_TYPE_STRING;
			}
			break;

		case ATTR_BGPROPERTIES:
			if (hae->value_len == 5 && uni_strni_eq(hae->value, "FIXED", 5))
			{
				value = INT_TO_PTR(TRUE);
				item_type = ITEM_TYPE_BOOL;
			}
			break;

		case ATTR_BGCOLOR:
		case ATTR_TEXT:
		case ATTR_LINK:
		case ATTR_VLINK:
		case ATTR_ALINK:
			if (hae->value_len)
			{
				value = (void*)GetColorVal(hae->value, hae->value_len, TRUE);
				item_type = ITEM_TYPE_NUM;
			}
			else
			{
				value = (void*) empty_string;
				item_type = ITEM_TYPE_STRING;
			}
			break;

		case ATTR_ID:
		case ATTR_VALUE:
		case ATTR_ROWS:
		case ATTR_COLS:
		case ATTR_CONTENT:
#ifdef CORS_SUPPORT
		case Markup::HA_CROSSORIGIN:
#endif // CORS_SUPPORT
			if (hae->value)
			{
				HTML_ElementType type = Type();
				if ((
#ifdef _WML_SUPPORT_
					GetNsType() != NS_WML &&
#endif //_WML_SUPPORT
					attr == ATTR_VALUE && type != HE_PARAM
					&& type != HE_INPUT && type != HE_SELECT
					&& type != HE_TEXTAREA && type != HE_OPTION
					&& type != HE_BUTTON && type != HE_UNKNOWN
					&& type != HE_A) ||
					((attr == ATTR_ROWS || attr == ATTR_COLS) && (type == HE_INPUT || type == HE_TEXTAREA)))
				{
					if (IsNumericAttributeFloat(attr))
					{
						value = StealOrCopyString(hae, may_steal_hae_value, empty_string);
						item_type = ITEM_TYPE_STRING;
						if (!value)
							ret_stat = OpStatus::ERR_NO_MEMORY;
					}
					else
					{
						if (hae->value_len)
							value = INT_TO_PTR(uni_atoi(hae->value));
						item_type = ITEM_TYPE_NUM;
					}
				}
				else if (attr == ATTR_CONTENT && type == HE_COMMENT)
				{
					uni_char *val_buffer = OP_NEWA(uni_char, hae->value_len + 1);

					if (val_buffer)
					{
						op_memcpy(val_buffer, hae->value, UNICODE_SIZE(hae->value_len));
						val_buffer[hae->value_len] = 0;

						value = (void*) val_buffer;
						item_type = ITEM_TYPE_STRING;
					}
					else
						ret_stat = OpStatus::ERR_NO_MEMORY;
				}
				else if (!(type == HE_FRAMESET && hae->value_len == 0 && (attr == ATTR_COLS || attr == ATTR_ROWS)))
				{
					// FIXME: Remove special cases that are historic with no obvious reason and probably fixed the right way long ago
					// and use StealOrCopy string like the rest.
					value = (void*) SetStringAttr(hae->value, hae->value_len, type == HE_EMBED && attr == ATTR_PLUGINSPACE);
					if (value)
						item_type = ITEM_TYPE_STRING;
					else
						ret_stat = OpStatus::ERR_NO_MEMORY;

					if (attr == ATTR_ID)
						hae->is_id = TRUE;
				}
			}
			else
			{
				value = (void*)empty_string;
				item_type = ITEM_TYPE_STRING;
			}
			break;

		case ATTR_MEDIA:
			{
				MediaAttr* media_attr = MediaAttr::Create(hae->value, hae->value_len);
				if (media_attr)
				{
					item_type = ITEM_TYPE_COMPLEX;
					value = media_attr;
				}
				else
					ret_stat = OpStatus::ERR_NO_MEMORY;
			}
			break;

		case ATTR_TITLE:
			value = StealOrCopyString(hae, may_steal_hae_value, empty_string);
			item_type = ITEM_TYPE_STRING;
			if (!value)
				ret_stat = OpStatus::ERR_NO_MEMORY;
			break;

		case ATTR_ALT:
			{
				value = StealOrCopyString(hae, may_steal_hae_value, empty_string);
				item_type = ITEM_TYPE_STRING;
				if (!value)
					ret_stat = OpStatus::ERR_NO_MEMORY;
				break;
			}

		case ATTR_STYLE:
			ret_stat = CreateStyleAttribute(hae->value, hae->value_len, base_url, hld_profile, value, item_type);
			break;

		case ATTR_CLASS:
			if (hld_profile)
			{
				ClassAttribute* class_attr = hld_profile->GetLogicalDocument()->CreateClassAttribute(hae->value, hae->value_len);
				if (class_attr)
				{
					value = static_cast<void*>(class_attr);
					item_type = ITEM_TYPE_COMPLEX;
				}
				else
					ret_stat = OpStatus::ERR_NO_MEMORY;
			}
			break;
		case ATTR_ITEMPROP:
		case ATTR_ITEMREF:
			{
				StringTokenListAttribute* stl = CreateStringTokenListAttribute(hae->value, hae->value_len);
				if (stl)
				{
					value = static_cast<void*>(stl);
					item_type = ITEM_TYPE_COMPLEX;
				}
				else
					ret_stat = OpStatus::ERR_NO_MEMORY;
			}
			break;

#ifdef DRAG_SUPPORT
		case Markup::HA_DROPZONE:
			{
				DropzoneAttribute* dropzone = DropzoneAttribute::Create(hae->value, hae->value_len);
				if (dropzone)
				{
					value = static_cast<void*>(dropzone);
					item_type = ITEM_TYPE_COMPLEX;
				}
				else
					ret_stat = OpStatus::ERR_NO_MEMORY;
			}
			break;
#endif // DRAG_SUPPORT

		case ATTR_ONLOAD:
		case ATTR_ONUNLOAD:
		case ATTR_ONBLUR:
		case ATTR_ONFOCUS:
		case ATTR_ONFOCUSIN:
		case ATTR_ONFOCUSOUT:
		case ATTR_ONERROR:
		case ATTR_ONSUBMIT:
		case ATTR_ONCHANGE:
		case ATTR_ONCLICK:
		case ATTR_ONDBLCLICK:
		case ATTR_ONKEYDOWN:
		case ATTR_ONKEYPRESS:
		case ATTR_ONKEYUP:
		case ATTR_ONMOUSEOVER:
		case ATTR_ONMOUSEENTER:
		case ATTR_ONMOUSEOUT:
		case ATTR_ONMOUSELEAVE:
		case ATTR_ONMOUSEMOVE:
		case ATTR_ONMOUSEUP:
		case ATTR_ONMOUSEDOWN:
		case ATTR_ONMOUSEWHEEL:
		case ATTR_ONRESET:
		case ATTR_ONSELECT:
		case ATTR_ONRESIZE:
		case ATTR_ONSCROLL:
		case ATTR_ONHASHCHANGE:
		case ATTR_ONINPUT:
		case ATTR_ONFORMINPUT:
		case ATTR_ONINVALID:
		case ATTR_ONFORMCHANGE:
# ifdef MEDIA_HTML_SUPPORT
		case ATTR_ONLOADSTART:
		case ATTR_ONPROGRESS:
		case ATTR_ONSUSPEND:
		case ATTR_ONSTALLED:
		case ATTR_ONLOADEND:
		case ATTR_ONEMPTIED:
		case ATTR_ONPLAY:
		case ATTR_ONPAUSE:
		case ATTR_ONLOADEDMETADATA:
		case ATTR_ONLOADEDDATA:
		case ATTR_ONWAITING:
		case ATTR_ONPLAYING:
		case ATTR_ONSEEKING:
		case ATTR_ONSEEKED:
		case ATTR_ONTIMEUPDATE:
		case ATTR_ONENDED:
		case ATTR_ONCANPLAY:
		case ATTR_ONCANPLAYTHROUGH:
		case ATTR_ONRATECHANGE:
		case ATTR_ONDURATIONCHANGE:
		case ATTR_ONVOLUMECHANGE:
		case Markup::HA_ONCUECHANGE:
# endif
#ifdef CLIENTSIDE_STORAGE_SUPPORT
		case ATTR_ONSTORAGE:
#endif // CLIENTSIDE_STORAGE_SUPPORT
# ifdef TOUCH_EVENTS_SUPPORT
		case ATTR_ONTOUCHSTART:
		case ATTR_ONTOUCHMOVE:
		case ATTR_ONTOUCHEND:
		case ATTR_ONTOUCHCANCEL:
# endif // TOUCH_EVENTS_SUPPORT
		case ATTR_ONCONTEXTMENU:
		case ATTR_ONPOPSTATE:
#ifdef PAGED_MEDIA_SUPPORT
		case Markup::HA_ONPAGECHANGE:
#endif // PAGED_MEDIA_SUPPORT
#ifdef DRAG_SUPPORT
		case Markup::HA_ONDRAG:
		case Markup::HA_ONDRAGOVER:
		case Markup::HA_ONDRAGENTER:
		case Markup::HA_ONDRAGLEAVE:
		case Markup::HA_ONDRAGSTART:
		case Markup::HA_ONDRAGEND:
		case Markup::HA_ONDROP:
#endif // DRAG_SUPPORT
#ifdef USE_OP_CLIPBOARD
		case Markup::HA_ONCOPY:
		case Markup::HA_ONCUT:
		case Markup::HA_ONPASTE:
#endif // USE_OP_CLIPBOARD
			// If we come from the parser, only add the event handler if this is the
			// first attribute for that event type.  Using "!attr_index" to check if
			// we come from somewhere other than the parser is probably not perfect.
			if (!HasAttr(hae->attr) || !attr_index)
			{
				DOM_EventType event = GetEventType(hae->attr, hae->ns_idx);

				is_event = TRUE;

				if (hld_profile)
				{
					hld_profile->GetLogicalDocument()->AddEventHandler(event);

					if (exo_object)
					{
						FramesDocument *frames_doc = hld_profile->GetFramesDocument();

						if (OpStatus::IsMemoryError(SetEventHandler(frames_doc, event, hae->value, hae->value_len)))
						{
							ret_stat = OpStatus::ERR_NO_MEMORY;
							break;
						}
					}
				}

				item_type = ITEM_TYPE_STRING;
				if (hae->value_len)
				{
					value = (void*) UniSetNewStrN(hae->value, hae->value_len); // FIXME:REPORT_MEMMAN-KILSMO

					if (!value)
						ret_stat = OpStatus::ERR_NO_MEMORY;
				}
				else
					value = (void*) empty_string;
			}

			break;

		default:
			// Unknown attribute, store as name/value string pair. If we end up here
			// for a known attribute it's a bug that will cause the attribute to
			// become semi-invisible.
			if (attr < Markup::HA_LAST) // name can be looked up, store value as string
			{
				value = StealOrCopyString(hae, may_steal_hae_value, empty_string);
				if (value)
					item_type = ITEM_TYPE_STRING;
				else
					ret_stat = OpStatus::ERR_NO_MEMORY;
				break;
			}
			else
			{
				OP_ASSERT(hae->attr == ATTR_XML);
				hae->attr = ATTR_XML;
			}
			break;
		}
	}
	break;

#ifdef _WML_SUPPORT_
	case NS_WML:
		{
			if (hld_profile)
			{
				hld_profile->WMLInit();
			}

			switch(hae->attr)
			{
			case WA_EMPTYOK:
			case WA_ORDERED:
			case WA_OPTIONAL:
			case WA_NEWCONTEXT:
			case WA_SENDREFERER:
				// values "1" and "true" (case insensitive) are interpreted as TRUE, otherwise FALSE
				if ((hae->value_len == 1 && *hae->value == '1')
					|| (hae->value_len == 4 && uni_strni_eq(hae->value, "TRUE", 4)))
					value = (void*)1;
				else
					value = (void*)0;
				item_type = ITEM_TYPE_BOOL;
				break;

			case WA_METHOD:
				if (hae->value_len == 4 && uni_strni_eq(hae->value, "POST", 4))
					value = (void*)HTTP_METHOD_POST;
				else
					value = (void*)HTTP_METHOD_GET;
				item_type = ITEM_TYPE_NUM;
				break;

			case WA_MODE:
				if (hae->value_len == 6 && uni_strni_eq(hae->value, "NOWRAP", 6))
					value = (void*)WATTR_VALUE_nowrap;
				else
					value = (void*)WATTR_VALUE_wrap;
				item_type = ITEM_TYPE_NUM;
				break;

			case WA_TABINDEX:
				{
					long tabindex;
					if (SetTabIndexAttr(hae->value, hae->value_len, this, tabindex))
					{
						item_type = ITEM_TYPE_NUM;
						value = (void*)tabindex;
					}
					else
					{
						value = StealOrCopyString(hae, may_steal_hae_value, empty_string);
						item_type = ITEM_TYPE_STRING;
						if (!value)
							ret_stat = OpStatus::ERR_NO_MEMORY;
					}
				}
				break;

			case WA_CACHE_CONTROL:
				if (hae->value_len == 8 && uni_strni_eq(hae->value, "NO-CACHE", 8))
					value = (void*)WATTR_VALUE_nocache;
				else
					value = (void*)WATTR_VALUE_cache;
				item_type = ITEM_TYPE_NUM;
				break;

			case WA_LOCALSRC:
				if (hae->value)
				{
#ifdef PICTOGRAM_SUPPORT
					if (hae->value_len > 7 && uni_strni_eq(hae->value, "PICT://", 7))
					{
						OpString local_url_str;
						ret_stat = PictUrlConverter::GetLocalUrl(hae->value, hae->value_len, local_url_str);
						if (OpStatus::IsSuccess(ret_stat) && !local_url_str.IsEmpty())
						{
							value = (void*) SetUrlAttr(local_url_str.CStr(), local_url_str.Length(), base_url, hld_profile, FALSE, TRUE);
							if (value)
							{
								if (attr_index)
									SetAttrLocal((*attr_index)++, ATTR_LOCALSRC_URL, ITEM_TYPE_URL, value, SpecialNs::NS_LOGDOC, TRUE, TRUE);
								else
									SetSpecialAttr(ATTR_LOCALSRC_URL, ITEM_TYPE_URL, value, TRUE, SpecialNs::NS_LOGDOC);
							}
							else
								ret_stat = OpStatus::ERR_NO_MEMORY;
						}
					}
#endif // PICTOGRAM_SUPPORT
					value = (void*) SetStringAttr(hae->value, hae->value_len, FALSE);
					if (value)
						item_type = ITEM_TYPE_STRING;
					else
						ret_stat = OpStatus::ERR_NO_MEMORY;
				}
				else
				{
					value = (void*) empty_string;
					item_type = ITEM_TYPE_STRING;
				}
				break;

			case WA_ID:
			case WA_HREF:
			case WA_TYPE:
			case WA_NAME:
			case WA_PATH:
			case WA_CLASS:
			case WA_TITLE:
			case WA_VALUE:
			case WA_LABEL:
			case WA_INAME:
			case WA_FORUA:
			case WA_DOMAIN:
			case WA_FORMAT:
			case WA_IVALUE:
			case WA_ENCTYPE:
			case WA_ACCESSKEY:
			case WA_ACCEPT_CHARSET:
				value = StealOrCopyString(hae, may_steal_hae_value, empty_string);
				item_type = ITEM_TYPE_STRING;
				if (!value)
					ret_stat = OpStatus::ERR_NO_MEMORY;
				else if (hae->value_len)
				{
#ifdef IMODE_EXTENSIONS
					const uni_char *value_str = (const uni_char*)value;
					UINT value_len = hae->value_len;

					if (hae->attr == WA_HREF)
						AddTelUrlAttributes(value_str, value_len, ha_list);
#endif // IMODE_EXTENSIONS

					hae->is_id = hae->attr == WA_ID;
				}
				break;

			case WA_ONPICK:
			case WA_ONTIMER:
			case WA_ONENTERFORWARD:
			case WA_ONENTERBACKWARD:
				{
					if (hld_profile)
					{
						hld_profile->WMLInit();
					}
					value = StealOrCopyString(hae, may_steal_hae_value, empty_string);
					item_type = ITEM_TYPE_STRING;
					if (!value)
						ret_stat = OpStatus::ERR_NO_MEMORY;
				}
				break;

			case ATTR_XML:
				// do nothing
				break;

			default:
				if (hae->attr < Markup::HA_LAST)
				{
					value = StealOrCopyString(hae, may_steal_hae_value, empty_string);
					item_type = ITEM_TYPE_STRING;
					if (!value)
						ret_stat = OpStatus::ERR_NO_MEMORY;
				}
				else
					hae->attr = ATTR_XML;
				break;
			}
		}
		break;
#endif // _WML_SUPPORT_

#ifdef SVG_SUPPORT
	case NS_SVG:
		// attributes for unknown elements should be stored with full name and value
		if(hae->attr != Markup::HA_XML && hld_profile)
		{
			if(Type() == HE_UNKNOWN)
			{
				value = StealOrCopyString(hae, may_steal_hae_value, empty_string);
				item_type = ITEM_TYPE_STRING;
				if (!value)
					ret_stat = OpStatus::ERR_NO_MEMORY;
			}
			else
			{
				FramesDocument *frames_doc = hld_profile->GetFramesDocument();
				DOM_EventType event = DOM_EVENT_NONE;
				switch(hae->attr)
				{
					case Markup::SVGA_CONTENTSCRIPTTYPE:
					case Markup::SVGA_CONTENTSTYLETYPE:
					case Markup::SVGA_ID:
						{
							value =
								(void*) SetStringAttrUTF8Escaped(hae->value, hae->value_len, hld_profile);

							if (value)
								item_type = ITEM_TYPE_STRING;
							else
								ret_stat = OpStatus::ERR_NO_MEMORY;

							if(hae->attr == Markup::SVGA_ID)
							{
								hae->is_id = TRUE;
							}
						}
						break;

					case Markup::SVGA_TYPE:
						{
							Markup::Type type = Type();
							if (type == Markup::SVGE_SCRIPT ||
								type == Markup::SVGE_STYLE ||
								type == Markup::SVGE_VIDEO ||
								type == Markup::SVGE_AUDIO ||
								type == Markup::SVGE_HANDLER)
							{
								value = StealOrCopyString(hae, may_steal_hae_value, empty_string);
								item_type = ITEM_TYPE_STRING;
								if (!value)
									ret_stat = OpStatus::ERR_NO_MEMORY;
							}
							else
							{
								ret_stat = g_svg_manager->ParseAttribute(this, frames_doc,
																hae->attr, NS_IDX_SVG, hae->value,
																hae->value_len, &value, item_type);
							}
						}
						break;

					case Markup::SVGA_STYLE:
						ret_stat = CreateStyleAttribute(hae->value, hae->value_len, base_url, hld_profile, value, item_type);
						break;

#ifdef DRAG_SUPPORT
					case Markup::SVGA_DRAGGABLE:
						{
							value = (void*) SetStringAttr(hae->value, hae->value_len, FALSE);
							if (value)
								item_type = ITEM_TYPE_STRING;
							else
								ret_stat = OpStatus::ERR_NO_MEMORY;
						}
						break;

					case Markup::SVGA_DROPZONE:
						{
							DropzoneAttribute* dropzone = DropzoneAttribute::Create(hae->value, hae->value_len);
							if (dropzone)
							{
								value = static_cast<void*>(dropzone);
								item_type = ITEM_TYPE_COMPLEX;
							}
							else
								ret_stat = OpStatus::ERR_NO_MEMORY;
						}
						break;
#endif // DRAG_SUPPORT

#ifdef SVG_DOM
					case Markup::SVGA_ONFOCUSIN:
						event = DOMFOCUSIN;
						break;
					case Markup::SVGA_ONFOCUSOUT:
						event = DOMFOCUSOUT;
						break;
					case Markup::SVGA_ONACTIVATE:
						event = DOMACTIVATE;
						break;
					case Markup::SVGA_ONCLICK:
						event = ONCLICK;
						break;
					case Markup::SVGA_ONMOUSEDOWN:
						event = ONMOUSEDOWN;
						break;
					case Markup::SVGA_ONMOUSEUP:
						event = ONMOUSEUP;
						break;
					case Markup::SVGA_ONMOUSEOVER:
						event = ONMOUSEOVER;
						break;
					case Markup::SVGA_ONMOUSEENTER:
						event = ONMOUSEENTER;
						break;
					case Markup::SVGA_ONMOUSEMOVE:
						event = ONMOUSEMOVE;
						break;
					case Markup::SVGA_ONMOUSEOUT:
						event = ONMOUSEOUT;
						break;
					case Markup::SVGA_ONMOUSELEAVE:
						event = ONMOUSELEAVE;
						break;
					case Markup::SVGA_ONUNLOAD:
						event = SVGUNLOAD;
						break;
					case Markup::SVGA_ONLOAD:
						event = SVGLOAD;
						break;
					case Markup::SVGA_ONABORT:
						event = SVGABORT;
						break;
					case Markup::SVGA_ONERROR:
						event = SVGERROR;
						break;
					case Markup::SVGA_ONRESIZE:
						event = SVGRESIZE;
						break;
					case Markup::SVGA_ONSCROLL:
						event = SVGSCROLL;
						break;
					case Markup::SVGA_ONZOOM:
						event = SVGZOOM;
						break;
					case Markup::SVGA_ONBEGIN:
						event = BEGINEVENT;
						break;
					case Markup::SVGA_ONEND:
						event = ENDEVENT;
						break;
					case Markup::SVGA_ONREPEAT:
						event = REPEATEVENT;
						break;
#ifdef TOUCH_EVENTS_SUPPORT
					case Markup::SVGA_ONTOUCHSTART:
						event = TOUCHSTART;
						break;
					case Markup::SVGA_ONTOUCHMOVE:
						event = TOUCHMOVE;
						break;
					case Markup::SVGA_ONTOUCHEND:
						event = TOUCHEND;
						break;
					case Markup::SVGA_ONTOUCHCANCEL:
						event = TOUCHCANCEL;
						break;
#endif // TOUCH_EVENTS_SUPPORT
#ifdef DRAG_SUPPORT
					case Markup::SVGA_ONDRAG:
						event = ONDRAG;
						break;
					case Markup::SVGA_ONDRAGOVER:
						event = ONDRAGOVER;
						break;
					case Markup::SVGA_ONDRAGENTER:
						event = ONDRAGENTER;
						break;
					case Markup::SVGA_ONDRAGLEAVE:
						event = ONDRAGLEAVE;
						break;
					case Markup::SVGA_ONDRAGSTART:
						event = ONDRAGSTART;
						break;
					case Markup::SVGA_ONDRAGEND:
						event = ONDRAGEND;
						break;
					case Markup::SVGA_ONDROP:
						event = ONDROP;
						break;
#endif // DRAG_SUPPORT
#ifdef USE_OP_CLIPBOARD
					case Markup::SVGA_ONCOPY:
						event = ONCOPY;
						break;
					case Markup::SVGA_ONCUT:
						event = ONCUT;
						break;
					case Markup::SVGA_ONPASTE:
						event = ONPASTE;
						break;
#endif // USE_OP_CLIPBOARD
#endif // SVG_DOM
					default:
						ret_stat = g_svg_manager->ParseAttribute(this, frames_doc,
																 hae->attr, NS_IDX_SVG, hae->value,
																 hae->value_len, &value, item_type);
						break;
				}

				if(event != DOM_EVENT_NONE)
				{
					is_event = TRUE;

					hld_profile->GetLogicalDocument()->AddEventHandler(event);

					if (exo_object)
						ret_stat = SetEventHandler(frames_doc, event, hae->value, hae->value_len);

					value = (void*) UniSetNewStrN(hae->value, hae->value_len); // FIXME:REPORT_MEMMAN-KILSMO

					if (value)
						item_type = ITEM_TYPE_STRING;
					else if( hae->value ) // only out of memory if a value was supposed to be copied
						ret_stat = OpStatus::ERR_NO_MEMORY;
				}
			}
		}
		else
			hae->attr = ATTR_XML;
		break;
#endif // SVG_SUPPORT

#ifdef XLINK_SUPPORT
	case NS_XLINK:
	{
		if (hae->attr == Markup::XLINKA_HREF)
		{
			if (hld_profile && hae->value)
			{
				FramesDocument *frames_doc = hld_profile->GetFramesDocument();
				ret_stat = g_svg_manager->ParseAttribute(this, frames_doc,
														 (Markup::AttrType)hae->attr, NS_IDX_XLINK,
														 hae->value, hae->value_len, &value, item_type);
			}
			else
			{
				value = StealOrCopyString(hae, may_steal_hae_value, empty_string);
				item_type = ITEM_TYPE_STRING;
				if (!value)
					ret_stat = OpStatus::ERR_NO_MEMORY;
			}
		}
		else if (hae->attr < Markup::HA_LAST)
		{
			value = StealOrCopyString(hae, may_steal_hae_value, empty_string);
			item_type = ITEM_TYPE_STRING;
			if (!value)
				ret_stat = OpStatus::ERR_NO_MEMORY;
		}
	}
	break;
#endif // XLINK_SUPPORT

#ifdef XML_EVENTS_SUPPORT
	case NS_EVENT:
		{
			if (hld_profile)
			{
				// Replace escapes, except for XML documents where that's already been done
				value = StealOrCopyString(hae, may_steal_hae_value, empty_string);
				item_type = ITEM_TYPE_STRING;
				if (!value)
					ret_stat = OpStatus::ERR_NO_MEMORY;
			}
		}
		break;
#endif // XML_EVENTS_SUPPORT

	case NS_XML:
		switch (hae->attr)
		{
		case XMLA_ID:
		case XMLA_LANG:
		case XMLA_BASE:
			{
				value =
					(void*) SetStringAttrUTF8Escaped(hae->value, hae->value_len, hld_profile);

				if (value)
					item_type = ITEM_TYPE_STRING;
				else
					ret_stat = OpStatus::ERR_NO_MEMORY;

				hae->is_id = hae->attr == XMLA_ID;
			}
			break;

		case XMLA_SPACE:
			{
				item_type = ITEM_TYPE_BOOL;
				if (hae->value_len == 8 && uni_strni_eq(hae->value, "PRESERVE", hae->value_len))
					value = INT_TO_PTR(TRUE);
				else
					value = INT_TO_PTR(FALSE);
			}
			break;

		default:
			if (hae->attr < Markup::HA_LAST)
			{
				value = StealOrCopyString(hae, may_steal_hae_value, empty_string);
				item_type = ITEM_TYPE_STRING;
				if (!value)
					ret_stat = OpStatus::ERR_NO_MEMORY;
			}
			else
				hae->attr = ATTR_XML;
			break;
		}
		break;

#ifdef ARIA_ATTRIBUTES_SUPPORT
	case NS_ARIA:
		if (hae->attr < Markup::HA_LAST)
		{
			value = StealOrCopyString(hae, may_steal_hae_value, empty_string);
			item_type = ITEM_TYPE_STRING;
			if (!value)
				ret_stat = OpStatus::ERR_NO_MEMORY;
		}
		else
			hae->attr = ATTR_XML;
		break;
#endif // ARIA_ATTRIBUTES_SUPPORT

#ifdef MATHML_ELEMENT_CODES
	case NS_MATHML:
		if (hae->attr < Markup::HA_LAST)
		{
			value = StealOrCopyString(hae, may_steal_hae_value, empty_string);
			item_type = ITEM_TYPE_STRING;
			if (!value)
				ret_stat = OpStatus::ERR_NO_MEMORY;
		}
		else
			hae->attr = ATTR_XML;
		break;
#endif // MATHML_ELEMENT_CODES

	default:
		if (hae->attr < Markup::HA_LAST)
		{
			value = StealOrCopyString(hae, may_steal_hae_value, empty_string);
			item_type = ITEM_TYPE_STRING;
			if (!value)
				ret_stat = OpStatus::ERR_NO_MEMORY;
		}
		else
			hae->attr = ATTR_XML;
		break;
	}

	// The ATTR_XML code must be the same in all namespace
	if (hae->attr == ATTR_XML)
	{
		value = OP_NEWA(uni_char, hae->name_len + hae->value_len + 2); // FIXME:REPORT_MEMMAN-KILSMO

		if (value)
		{
			// Copy the attribute name to the value string
			if (hae->name_len)
				uni_strncpy((uni_char*)value, hae->name, hae->name_len);
			((uni_char*)value)[hae->name_len] = '\0';

			// Copy the attribute value to the value string
			uni_char* val_start = (uni_char*)value + hae->name_len + 1;
			if (hae->value_len > 0)
				uni_strncpy(val_start, hae->value, hae->value_len);
			val_start[hae->value_len] = '\0';

			item_type = ITEM_TYPE_STRING;
		}
		else
			ret_stat = OpStatus::ERR_NO_MEMORY;
	}

	if (item_type == ITEM_TYPE_NUM || item_type == ITEM_TYPE_BOOL)
	{
		BOOL store_string = TRUE;
#if 1 // optimization (save memory by spending some CPU here)
		if (item_type == ITEM_TYPE_NUM)
		{
			uni_char buf[25]; /* ARRAY OK 2009-05-07 stighal */
#ifdef DEBUG_ENABLE_OPASSERT
			buf[24] = 0xabcd; // To catch too long keywords
#endif // DEBUG_ENABLE_OPASSERT
			const uni_char* generated_value = GenerateStringValueFromNumAttr(attr, attr_ns, value, buf, HE_UNKNOWN);
			OP_ASSERT(buf[24] == 0xabcd);
			if (generated_value && hae->value_len == uni_strlen(generated_value) && uni_strncmp(hae->value, generated_value, hae->value_len) == 0)
			{
				// We can recreate this string exactly with op_ltoa so we'll save some memory and an allocation.
				store_string = FALSE;
			}
		}
#endif // 1
		if (store_string)
		{
			char* value_with_string = OP_NEWA(char, sizeof(INTPTR) + sizeof(uni_char)*(hae->value_len+1));
			if (!value_with_string)
			{
				// This is not critical so we could go on with just the number, but we'll not do that.
				ret_stat = OpStatus::ERR_NO_MEMORY;
			}
			else
			{
				uni_char* string_part = reinterpret_cast<uni_char*>(value_with_string + sizeof(INTPTR));
				if (hae->value_len)
					uni_strncpy(string_part, hae->value, hae->value_len);
				string_part[hae->value_len] = '\0';
				INTPTR* num_part = reinterpret_cast<INTPTR*>(value_with_string);
				*num_part = reinterpret_cast<INTPTR>(value);
				item_type = ITEM_TYPE_NUM_AND_STRING;
				value = value_with_string;
			}
		}
	}

	need_free = item_type != ITEM_TYPE_NUM && item_type != ITEM_TYPE_BOOL
		&& !(item_type == ITEM_TYPE_STRING
			&& static_cast<uni_char*>(value) == empty_string);

	return ret_stat;
}

BOOL HTML_Element::GetIsCDATA() const
{
	if (Type() == HE_TEXT)
		return data.text->GetIsCDATA();
	else if (Type() == HE_TEXTGROUP)
		return GetSpecialBoolAttr(ATTR_CDATA, SpecialNs::NS_LOGDOC);
	else
		return FALSE;
}

short HTML_Element::GetAttrCount() const
{
	int count = 0, size = GetAttrSize();

	for (int i = 0; i < size; i++)
	{
		if (!GetAttrIsSpecial(i))
			count++;
	}
	return count;
}

const uni_char*	HTML_Element::GetXmlAttr(const uni_char* attr_name, BOOL case_sensitive)
{
	int idx = FindAttrIndex(ATTR_XML, attr_name, NS_IDX_ANY_NAMESPACE, case_sensitive);

	if ((idx != ATTR_NOT_FOUND) && (GetItemType(idx) == ITEM_TYPE_STRING))
	{
		const uni_char* str_value = (const uni_char*)GetValueItem(idx);
		if (str_value)
		{
			if (uni_stricmp(str_value, attr_name) == 0)
				return str_value + uni_strlen(str_value) + 1;
		}
	}

	return NULL;
}

BOOL HTML_Element::HasNumAttr(short attr, int ns_idx/* = NS_IDX_HTML*/) const
{
	OP_ASSERT(attr != ATTR_XML);
	int idx = FindAttrIndex(attr, NULL, ns_idx, FALSE);

	if (idx != ATTR_NOT_FOUND)
	{
		ItemType item_type = GetItemType(idx);
		return item_type == ITEM_TYPE_BOOL ||
			item_type == ITEM_TYPE_NUM ||
			item_type == ITEM_TYPE_NUM_AND_STRING;
	}
	return FALSE;
}

BOOL HTML_Element::HasAttr(short attr, int ns_idx/*=NS_IDX_HTML*/) const
{
	if (attr != ATTR_XML && FindAttrIndex(attr, NULL, ns_idx, FALSE) != ATTR_NOT_FOUND)
		return TRUE;
	return FALSE;
}

BOOL HTML_Element::HasSpecialAttr(short attr, SpecialNs::Ns ns) const
{
	if (FindSpecialAttrIndex(attr, ns) != ATTR_NOT_FOUND)
		return TRUE;
	return FALSE;
}

void* HTML_Element::GetAttrByIndex(int idx, ItemType item_type, void* def_value) const
{
	if (idx > ATTR_NOT_FOUND)
	{
		if (GetItemType(idx) == item_type)
		{
			if (GetAttrItem(idx) == ATTR_XML)
			{
				uni_char* str_value = (uni_char*)GetValueItem(idx);
				if (str_value)
					return str_value + uni_strlen(str_value) + 1;
			}

			return GetValueItem(idx);
		}
		else if (GetItemType(idx) == ITEM_TYPE_NUM_AND_STRING)
		{
			if (item_type == ITEM_TYPE_NUM || item_type == ITEM_TYPE_BOOL)
				return INT_TO_PTR(*static_cast<INTPTR*>(GetValueItem(idx)));
			else if (item_type == ITEM_TYPE_STRING)
				return static_cast<char*>(GetValueItem(idx))+sizeof(INTPTR);
		}
		else if (GetItemType(idx) == ITEM_TYPE_URL_AND_STRING)
		{
			if (item_type == ITEM_TYPE_URL)
				return static_cast<UrlAndStringAttr*>(GetValueItem(idx))->GetResolvedUrl();
			else if (item_type == ITEM_TYPE_STRING)
				return static_cast<UrlAndStringAttr*>(GetValueItem(idx))->GetString();
		}
	}

	return def_value;
}

const uni_char*
HTML_Element::GetMicrodataItemValue(TempBuffer& buffer, FramesDocument* frm_doc)
{
	int attr = Markup::HA_NULL;
	switch (Type())
	{
	case Markup::HTE_AUDIO:
	case Markup::HTE_EMBED:
	case Markup::HTE_IFRAME:
	case Markup::HTE_IMG:
	case Markup::HTE_SOURCE:
	//case Markup::HTE_TRACK:
	case Markup::HTE_VIDEO:
		attr = Markup::HA_SRC;
		break;
	case Markup::HTE_A:
	case Markup::HTE_AREA:
	case Markup::HTE_LINK:
		attr = Markup::HA_HREF;
		break;
	case Markup::HTE_OBJECT:
		attr = Markup::HA_DATA;
		break;

	case Markup::HTE_META:
		return GetStringAttr(Markup::HA_CONTENT);
	case Markup::HTE_TIME:
		if (HasAttr(Markup::HA_DATETIME))
			return GetStringAttr(Markup::HA_DATETIME);
		// else fall through
	default:
		OP_ASSERT(frm_doc->GetDOMEnvironment());
		return DOMGetContentsString(frm_doc->GetDOMEnvironment(), &buffer);
	}
	if (attr != Markup::HA_NULL)
		if (URL* url = GetUrlAttr(attr, NS_IDX_HTML, frm_doc->GetLogicalDocument()))
			return url->GetAttribute(URL::KUniName_With_Fragment_Escaped).CStr();
	return NULL;
}

BOOL
HTML_Element::IsToplevelMicrodataItem() const
{
	if (GetBoolAttr(Markup::HA_ITEMSCOPE) && (!GetItemPropAttribute() || GetItemPropAttribute()->GetTokenCount() == 0))
		return TRUE;
	return FALSE;
}


void* HTML_Element::GetAttr(short attr, ItemType item_type, void* def_value, int ns_idx/*=NS_IDX_HTML*/) const
{
	OP_PROBE4(OP_PROBE_HTML_ELEMENT_GETATTR);

	OP_ASSERT(item_type != ITEM_TYPE_NUM_AND_STRING); // Internal type, please don't expose past this level

	int idx = FindAttrIndex(attr, NULL, ns_idx, FALSE);
	return GetAttrByIndex(idx, item_type, def_value);
}

int HTML_Element::SetAttr(short attr, ItemType item_type, void* val, BOOL need_free/*=FALSE*/, int ns_idx/*=NS_IDX_HTML*/, BOOL strict_ns/*=FALSE*/, BOOL is_id/*=FALSE*/, BOOL is_specified/*=TRUE*/, BOOL is_event/*=FALSE*/, int at_known_index /*= -1*/, BOOL case_sensitive/*=FALSE*/)
{
	OP_ASSERT(need_free == FALSE || need_free == TRUE || !"Are the arguments messed up again, and the namespace used at this position?");

	int i;
	if (at_known_index >= 0)
		i = at_known_index;
	else
		i = FindAttrIndex(attr, attr == ATTR_XML ? (const uni_char *) val : NULL, ns_idx, case_sensitive, strict_ns);

	return SetAttrCommon(i, attr, item_type, val, need_free, ns_idx, FALSE, is_id, is_specified, is_event);
}

void* HTML_Element::GetSpecialAttr(Markup::AttrType attr, ItemType item_type, void* def_value, SpecialNs::Ns ns) const
{
	OP_PROBE7(OP_PROBE_HTML_ELEMENT_GETSPECIALATTR);

	OP_ASSERT(item_type != ITEM_TYPE_NUM_AND_STRING); // Internal type, please don't expose past this level
	OP_ASSERT(item_type != ITEM_TYPE_URL_AND_STRING && item_type != ITEM_TYPE_NUM_AND_STRING);

	int idx = FindSpecialAttrIndex(attr, ns);
	if (idx > ATTR_NOT_FOUND && GetItemType(idx) == item_type)
		return GetValueItem(idx);

	return def_value;
}

int HTML_Element::SetSpecialAttr(Markup::AttrType attr, ItemType item_type, void* val, BOOL need_free, SpecialNs::Ns ns)
{
	OP_ASSERT(need_free == FALSE || need_free == TRUE || !"Are the arguments messed up again, and the namespace used at this position?");

	int i = FindSpecialAttrIndex(attr, ns);

	return SetAttrCommon(i, attr, item_type, val, need_free, ns, TRUE, FALSE, FALSE, FALSE);
}

ElementRef* HTML_Element::GetFirstReferenceOfType(ElementRef::ElementRefType type)
{
	ElementRef *iter = m_first_ref;
	while (iter && !iter->IsA(type))
		iter = iter->NextRef();

	return iter;
}

BOOL HTML_Element::SetAttrValue(const uni_char* attr, const uni_char* value, BOOL create)
{
	// must check for attr == ATTR_XML and search using string-functions
	if (!create && !GetAttrValue(attr))
		return FALSE;

	int attr_len = uni_strlen(attr);
	int value_len = uni_strlen(value);
	uni_char* new_value = OP_NEWA(uni_char, attr_len+value_len+2); // FIXME:REPORT_MEMMAN-KILSMO

	if (new_value)
	{
		uni_strcpy(new_value, attr);
		uni_strcpy(new_value + attr_len + 1, value);

		SetAttr(ATTR_XML, ITEM_TYPE_STRING, (void*)new_value, TRUE);
		return TRUE;
	}
	else
		return FALSE;
}

int HTML_Element::ResolveNameNs(const uni_char *prefix, unsigned prefix_length, const uni_char *name) const
{
	for (int index = 0, length = GetAttrSize(); index < length; ++index)
	{
		Markup::AttrType attr_item = GetAttrItem(index);

		if (GetAttrIsSpecial(index))
			continue;

		int resolved_attr_ns = GetResolvedAttrNs(index);

		const uni_char *attr_name;

		NS_Type ns_type = g_ns_manager->GetNsTypeAt(resolved_attr_ns);
		if (attr_item == ATTR_XML)
			attr_name = static_cast<const uni_char *>(GetValueItem(index));
		else
		{
			attr_name = HTM_Lex::GetAttributeString(attr_item, ns_type);
			if (!attr_name)
				continue;
		}

		const uni_char *ns_prefix = g_ns_manager->GetPrefixAt(resolved_attr_ns);
		if (prefix_length > 0 && ns_prefix && uni_strlen(ns_prefix) == prefix_length && uni_strncmp(ns_prefix, prefix, prefix_length) == 0 && uni_str_eq(name, attr_name))
			return resolved_attr_ns;
		else if (prefix_length > 0 && uni_strncmp(attr_name, prefix, prefix_length) == 0 && attr_name[prefix_length] == ':' && uni_str_eq(attr_name + prefix_length + 1, name))
			return NS_IDX_DEFAULT;
		else if (ns_type == NS_XMLNS && prefix_length == 0 && !*ns_prefix && uni_str_eq(attr_name, name))
			return resolved_attr_ns;
	}

	return NS_IDX_DEFAULT;
}


int HTML_Element::FindAttrIndexNs(int attr, const uni_char *name, int ns_idx, BOOL case_sensitive, BOOL strict_ns) const
{
	OP_PROBE4(OP_PROBE_HTML_ELEMENT_FINDATTR);

	OP_ASSERT(!(attr != ATTR_XML && ns_idx == NS_IDX_ANY_NAMESPACE));
	OP_ASSERT(attr != ATTR_XML || name);

	if (!strict_ns)
		ns_idx = ResolveNsIdx(ns_idx);

	BOOL any_namespace;
	NS_Type ns_type;
	int name_length;

	if (attr == ATTR_XML)
	{
		any_namespace = ns_idx == NS_IDX_ANY_NAMESPACE;
		name_length = uni_strlen(name);

		if (!any_namespace)
		{
			ns_type = g_ns_manager->GetNsTypeAt(ResolveNsIdx(ns_idx));
			if (name_length)
				attr = htmLex->GetAttrType(name, name_length, ns_type, case_sensitive);
		}
		else
			ns_type = NS_NONE;
	}
	else
	{
		any_namespace = FALSE;
		ns_type = g_ns_manager->GetNsTypeAt(ResolveNsIdx(ns_idx));
		name_length = 0; // Won't be used, but the compiler might warn about it being used initialized anyway.
	}

	const uni_char *uri;

	if (!any_namespace && (strict_ns || ns_type == NS_USER))
		uri = g_ns_manager->GetElementAt(ns_idx)->GetUri();
	else
		uri = NULL;

	for (int index = 0, length = GetAttrSize(); index < length; ++index)
	{
		/* This function is called a lot, so we want to do the comparison
		   most likely to fail first, to make the loop as tight as possible. */

		Markup::AttrType attr_item = GetAttrItem(index);

		if (attr != attr_item)
		{
			// Textual comparison instead?
			if (attr != ATTR_XML) // no
				continue;
		}

		if (GetAttrIsSpecial(index))
			continue;

		int resolved_attr_ns = GetResolvedAttrNs(index);

		if (attr == ATTR_XML)
		{
			OP_ASSERT(name);
			const uni_char *attr_name;

			if (attr_item == ATTR_XML)
				attr_name = (const uni_char *) GetValueItem(index);
			else
			{
				attr_name = HTM_Lex::GetAttributeString(attr_item, g_ns_manager->GetNsTypeAt(resolved_attr_ns));
				if (!attr_name)
					continue;
			}

			if (case_sensitive ? uni_strcmp(name, attr_name) : uni_stricmp(name, attr_name))
				continue;
		}

		int attr_ns = GetAttrNs(index);

		if (!any_namespace && (strict_ns ? attr_ns != ns_idx : resolved_attr_ns != ns_idx))
		{
			if (g_ns_manager->GetNsTypeAt(resolved_attr_ns) != ns_type)
				continue;

			if (strict_ns || ns_type == NS_USER)
				if (uni_strcmp(uri, g_ns_manager->GetElementAt(attr_ns)->GetUri()))
					continue;
		}

		return index;
	}

	return -1;
}

int HTML_Element::FindHtmlAttrIndex(int attr, const uni_char *name) const
{
	OP_ASSERT(attr != ATTR_XML || name);

	if (attr == ATTR_XML)
		attr = htmLex->GetAttrType(name, NS_HTML, FALSE);

	for (int index = 0, length = GetAttrSize(); index < length; ++index)
	{
		/* This function is called a lot, so we want to do the comparison
		   most likely to fail first, to make the loop as tight as possible. */

		int attr_item = GetAttrItem(index);
		if (attr != attr_item)
			continue;

		if (GetAttrIsSpecial(index))
			continue;

		int resolved_attr_ns = GetResolvedAttrNs(index);

		if (attr == ATTR_XML)
		{
			OP_ASSERT(name);
			const uni_char *attr_name = (const uni_char *) GetValueItem(index);

			if (uni_stricmp(name, attr_name))
				continue;
		}

		if (resolved_attr_ns != NS_IDX_HTML && g_ns_manager->GetNsTypeAt(resolved_attr_ns) != NS_HTML)
			continue;

		return index;
	}

	return -1;
}

int HTML_Element::FindSpecialAttrIndex(int attr, SpecialNs::Ns ns_idx) const
{
	OP_PROBE4(OP_PROBE_HTML_ELEMENT_FINDSPECIALATTRINDEX);

	for (int index = 0, length = GetAttrSize(); index < length; ++index)
	{
		if (attr != GetAttrItem(index) || !GetAttrIsSpecial(index) || GetAttrNs(index) != ns_idx)
			continue;

		return index;
	}

	return -1;
}

void HTML_Element::LocalCleanAttr(int i)
{
	data.attrs[i].Clean();
}

extern const uni_char* CSS_GetKeywordString(short val);

#include "modules/logdoc/src/htm_elm_maps.inl"

const uni_char * GetColorString(COLORREF value)
{
	/* When a named color arrives in Opera (either via a
	   HTML-attribute or CSS) an index is stored in the color-table.
	   To check if your color value is such an index, and it with
	   CSS_COLOR_KEYWORD_TYPE_index */

	if (value & CSS_COLOR_KEYWORD_TYPE_x_color)
		return HTM_Lex::GetColNameByIndex(value & CSS_COLOR_KEYWORD_TYPE_index);
	else
	{
		uni_char* number = (uni_char*)g_memory_manager->GetTempBuf();
		HTM_Lex::GetRGBStringFromVal(value, number, TRUE);
		return number;
	}
}

static BOOL IsLengthAttribute(HTML_ElementType for_element_type, short attr);

const uni_char* HTML_Element::GenerateStringValueFromNumAttr(int attr, NS_Type ns_type, void* value, uni_char* buf, HTML_ElementType for_element_type) const
{
	short num_value = (short) (INTPTR) value;
	if (ns_type == NS_HTML)
		switch (attr)
		{
		case ATTR_ALIGN:
		case ATTR_VALIGN:
			return CSS_GetKeywordString(num_value);

		case ATTR_METHOD:
		case ATTR_FORMMETHOD:
			return (num_value >= HTTP_METHOD_GET && num_value <= HTTP_METHOD_PUT) ? MethodNameMap[num_value - HTTP_METHOD_GET] : NULL;

		case ATTR_TYPE:
			if ((Type() == HE_INPUT || Type() == HE_BUTTON) && GetNsType() == NS_HTML)
				return GetInputTypeString((InputType)(INTPTR)value);
			else if (((Type() == HE_LI && Parent() && Parent()->Type() == HE_OL) || (Type() == HE_OL)) && GetNsType() == NS_HTML)
				return GetLiTypeString((short)(INTPTR)value);
			else
				return CSS_GetKeywordString(num_value);

		case ATTR_SHAPE:
			return (num_value >= AREA_SHAPE_DEFAULT && num_value <= AREA_SHAPE_POLYGON) ? ShapeNameMap[num_value - AREA_SHAPE_DEFAULT] : NULL;

		case ATTR_DIRECTION:
			return GetDirectionString(num_value);

		case ATTR_BEHAVIOR:
			return (num_value >= ATTR_VALUE_slide && num_value <= ATTR_VALUE_alternate) ? BehaviorNameMap[num_value - ATTR_VALUE_slide] : NULL;

		case ATTR_CLEAR:
			return (num_value >= CLEAR_NONE && num_value <= CLEAR_ALL) ? ClearNameMap[num_value - CLEAR_NONE] : NULL;

		case ATTR_SCROLLING:
			return (num_value >= SCROLLING_NO && num_value <= SCROLLING_AUTO) ? ScrollNameMap[num_value - SCROLLING_NO] : ScrollNameMap[SCROLLING_AUTO];

		case ATTR_FRAME:
			return (num_value >= ATTR_VALUE_box && num_value <= ATTR_VALUE_border) ? FrameValueNameMap[num_value - ATTR_VALUE_box] : NULL;

		case ATTR_RULES:
			if (num_value == ATTR_VALUE_all)
				return RulesNameMap[ATTR_VALUE_groups - ATTR_VALUE_none + 1];
			else if (num_value >= ATTR_VALUE_none && num_value <= ATTR_VALUE_groups)
				return RulesNameMap[num_value - ATTR_VALUE_none];
			else
				return NULL;

#ifdef SUPPORT_TEXT_DIRECTION
		case ATTR_DIR:
			return (num_value == CSS_VALUE_ltr || num_value == CSS_VALUE_rtl) ? DirNameMap[num_value - CSS_VALUE_ltr] : NULL;
#endif
		case ATTR_ALINK:
		case ATTR_LINK:
		case ATTR_TEXT:
		case ATTR_VLINK:
		case ATTR_COLOR:
		case ATTR_BGCOLOR:
			return GetColorString((COLORREF) (INTPTR) value);

		case ATTR_WIDTH:
		case ATTR_HEIGHT:
		case ATTR_CHAROFF:
			{
				uni_char* number = buf;
				BOOL convPercent = for_element_type == HE_ANY || for_element_type != HE_UNKNOWN && IsLengthAttribute(for_element_type, attr);

				uni_ltoa((convPercent ? (int)op_abs((INTPTR)value) : (INTPTR)value), number, 10);
				if (convPercent && ((INTPTR)value) < 0)
					uni_strcat(number, UNI_L("%"));
				return number;
			}

		case ATTR_SPELLCHECK:
			if (num_value == SPC_DISABLE)
				uni_strcpy(buf, UNI_L("false"));
			else if (num_value == SPC_ENABLE)
				uni_strcpy(buf, UNI_L("true"));
			else
				return NULL;
			return buf;
		}

	return uni_ltoa(reinterpret_cast<INTPTR>(value), buf, 10);
}

const uni_char * HTML_Element::GetAttrValueValue(int i, short attr, HTML_ElementType for_element_type, TempBuffer *buffer/*=NULL*/) const
{
	void* value = GetValueItem(i);
	NS_Type ns = g_ns_manager->GetNsTypeAt(GetResolvedAttrNs(i));

	switch (GetItemType(i))
	{
	case ITEM_TYPE_BOOL:
		// FIXME: All the code below might be useless after the addition of ITEM_TYPE_NUM_AND_STRING
		if (ns == NS_HTML && attr == ATTR_FRAMEBORDER)
			return (value) ? UNI_L("1") : UNI_L("0");
		else if (ns == NS_XML && attr == XMLA_SPACE)
			return (value) ? UNI_L("preserve") : UNI_L("default");
		else
			return (value) ? UNI_L("true") : UNI_L("false");

	case ITEM_TYPE_NUM:
		return GenerateStringValueFromNumAttr(attr, ns, value, (uni_char*)g_memory_manager->GetTempBuf(), for_element_type);

	case ITEM_TYPE_NUM_AND_STRING:
		{
			OP_ASSERT(value);
			uni_char* string_part = reinterpret_cast<uni_char*>(static_cast<char*>(value)+sizeof(INTPTR));
			return string_part;
		}

	case ITEM_TYPE_STRING:
		if (attr == ATTR_XML)
			return (const uni_char*) value + uni_strlen((const uni_char*)value) + 1;
		else
			return (const uni_char*) value;

	case ITEM_TYPE_URL:
		if (value)
			return ((URL*)value)->GetAttribute(URL::KUniName_With_Fragment_Username_Password_NOT_FOR_UI).CStr();
		break;

	case ITEM_TYPE_URL_AND_STRING:
		{
			OP_ASSERT(value);
			return static_cast<UrlAndStringAttr*>(value)->GetString();
		}

	case ITEM_TYPE_COORDS_ATTR:
		if(value)
		{
			CoordsAttr *ca = (CoordsAttr *)value;
			return ca->GetOriginalString();
		}
		break;

	case ITEM_TYPE_COMPLEX:
		if (value && buffer)
		{
			buffer->Clear();
			if(OpStatus::IsSuccess(((ComplexAttr*)value)->ToString(buffer)))
			{
				return buffer->GetStorage();
			}
		}
		break;

	default:
		return NULL;
	}

	return NULL;
}

const uni_char* HTML_Element::GetAttrValue(short attr, int ns_idx, HTML_ElementType for_element_type, BOOL strict_ns, TempBuffer *buffer, int known_index)
{
	int idx = known_index >= 0 ? known_index : FindAttrIndex(attr, NULL, ns_idx, FALSE, strict_ns);
	if (idx > ATTR_NOT_FOUND)
		return GetAttrValueValue(idx, attr, for_element_type, buffer);

	return NULL;
}

const uni_char* HTML_Element::GetAttrValue(const uni_char* attr_name, int ns_idx, HTML_ElementType for_element_type, TempBuffer *buffer, int known_index)
{
	short attr = ATTR_XML;
	NS_Type ns_type = ns_idx == NS_IDX_ANY_NAMESPACE ? NS_USER : g_ns_manager->GetNsTypeAt(ResolveNsIdx(ns_idx));

	if (ns_type != NS_USER && Type() != HE_UNKNOWN)
		attr = (short)htmLex->GetAttrType(attr_name, ns_type);

	int idx = known_index >= 0 ? known_index : FindAttrIndex(attr, attr_name, ns_idx, FALSE);

	if (idx != ATTR_NOT_FOUND)
	{
		if (GetAttrItem(idx) == ATTR_XML)
		{
			const uni_char* str_value = (const uni_char*)GetValueItem(idx);
			if (str_value)
				return str_value + uni_strlen(str_value) + 1;
		}
		else
			return GetAttrValueValue(idx, GetAttrItem(idx), for_element_type, buffer);
	}

	return NULL;
}

BOOL HTML_Element::IsInsideUnclosedFormElement()
{
	HTML_Element* elm = this;
	while (elm)
	{
		if (elm->IsMatchingType(HE_FORM, NS_HTML))
		{
			if (elm->GetEndTagFound())
				return FALSE; // Inside _closed_ form element

			return TRUE;
		}

		// This is where MSIE and Mozilla differs. MSIE doesn't search
		// up through tables and other "special" elements.
		if (elm->GetNsType() == NS_HTML)
		{
			HTML_ElementType parent_type = elm->Type();
			if (parent_type == HE_TABLE ||
				parent_type == HE_BUTTON ||
				parent_type == HE_SELECT ||
				parent_type == HE_DATALIST ||
				parent_type == HE_APPLET ||
				parent_type == HE_OBJECT)
			{
				return FALSE;
			}
		}
		elm = elm->Parent();
	}

	return FALSE;
}

BOOL HTML_Element::InsideLIList() const
{
	for (HTML_Element *p = Parent(); p; p = p->Parent())
		switch (p->Type())
		{
			case HE_OL:
			case HE_UL:
			case HE_MENU:
			case HE_DIR:
				return TRUE;
		}

	return FALSE;
}

BOOL HTML_Element::InsideDLList() const
{
	for (HTML_Element *p = Parent(); p; p = p->Parent())
		if (p->Type() == HE_DL)
			return TRUE;

	return FALSE;
}

const uni_char*
HTML_Element::LocalContent() const
{
	return (Type() == HE_TEXT && data.text) ? data.text->GetText() : NULL;
}

short
HTML_Element::LocalContentLength() const
{
	return (Type() == HE_TEXT && data.text) ? data.text->GetTextLen() : 0;
}

void HTML_Element::MakeIsindex(HLDocProfile* hld_profile, const uni_char* prompt)
{
	HTML_Element* he = NEW_HTML_Element();

	// OOM check
    if (!he || he->Construct(hld_profile, NS_IDX_HTML, HE_HR, NULL) == OpStatus::ERR_NO_MEMORY)
	{
		DELETE_HTML_Element(he);
		hld_profile->SetIsOutOfMemory(TRUE);
		return;
	}

	he->Under(this);

	OP_STATUS err;
	OpString use_prompt;
	if (prompt)
	{
		err = use_prompt.Set(prompt);
	}
	else
	{
		TRAP(err, g_languageManager->GetStringL(Str::SI_ISINDEX_TEXT, use_prompt));
	}

	if (OpStatus::IsMemoryError(err))
		hld_profile->SetIsOutOfMemory(TRUE);

	he = HTML_Element::CreateText(use_prompt.CStr(), use_prompt.Length(), TRUE, FALSE, FALSE);
	if (!he)
	{
		hld_profile->SetIsOutOfMemory(TRUE);
		return;
	}

	he->Under(this);

	he = NEW_HTML_Element();

	// OOM check
    if (!he || OpStatus::ERR_NO_MEMORY == he->Construct(hld_profile, NS_IDX_HTML, HE_BR, NULL))
	{
		DELETE_HTML_Element(he);
		hld_profile->SetIsOutOfMemory(TRUE);
		return;
	}

	he->Under(this);

	const uni_char* const inp_str1 = UNI_L("text");
	const uni_char* const inp_str2 = UNI_L("20");

	HtmlAttrEntry attr_array[3]; /* ARRAY OK 2010-09-17 fs */
	attr_array[0].attr = ATTR_TYPE;
	attr_array[0].value = inp_str1;
	attr_array[0].value_len = uni_strlen(inp_str1);
	attr_array[1].attr = ATTR_SIZE;
	attr_array[1].value = inp_str2;
	attr_array[1].value_len = uni_strlen(inp_str2);
	attr_array[2].attr = ATTR_NULL;

	he = NEW_HTML_Element();

	// OOM check
	if (!he || OpStatus::ERR_NO_MEMORY == he->Construct(hld_profile, NS_IDX_HTML, HE_INPUT, attr_array))
	{
		DELETE_HTML_Element(he);
		hld_profile->SetIsOutOfMemory(TRUE);
		return;
	}

	he->Under(this);

	he = NEW_HTML_Element();

	// OOM check
    if (he && OpStatus::ERR_NO_MEMORY != he->Construct(hld_profile, NS_IDX_HTML, HE_HR, NULL))
	    he->Under(this);
	else
	{
		DELETE_HTML_Element(he);
		hld_profile->SetIsOutOfMemory(TRUE);
	}
}

void
HTML_Element::DeleteTextData()
{
	OP_DELETE(data.text);
	data.text = NULL;
}

void HTML_Element::EmptySrcListAttr()
{
	if (DataSrc* head = GetDataSrc())
		head->DeleteAll();
}


OP_STATUS HTML_Element::SetTextInternal(FramesDocument* doc, const uni_char *txt, unsigned int txt_len)
{
	OP_ASSERT(IsText());
	// First remove the current text
	TextData* fallback_textdata = NULL;
	if (Type() == HE_TEXTGROUP)
	{
		if (doc && doc->GetDocRoot() && doc->GetDocRoot()->IsAncestorOf(this))
			MarkDirty(doc);

		while (HTML_Element* child = FirstChild())
		{
			 child->OutSafe(doc, TRUE);
			 if (child->Clean(doc))
				 child->Free(doc);
		}
	}
	else
	{
		fallback_textdata = data.text;
		data.text = NULL;
	}

	// Add the new text
	HLDocProfile* hld_profile = doc ? doc->GetHLDocProfile() : NULL;
	OP_STATUS status = AppendText(hld_profile, txt, txt_len, FALSE, FALSE, FALSE);

	// Make sure we don't have a NULL data.text in case of oom
	if (OpStatus::IsError(status) && Type() == HE_TEXT && !data.text)
	{
		data.text = fallback_textdata;
	}
	else
	{
		DELETE_TextData(fallback_textdata);
	}

	return status;
}

OP_STATUS HTML_Element::SetSrcListAttr(int idx, DataSrc* dupl_head)
{
	DataSrc* head = OP_NEW(DataSrc, ());
	if (!head)
		return OpStatus::ERR_NO_MEMORY;

	SetAttrLocal(idx, ATTR_SRC_LIST, ITEM_TYPE_COMPLEX, static_cast<void*>(head), SpecialNs::NS_LOGDOC, TRUE, TRUE);

	if (dupl_head && head)
	{
		DataSrcElm* sse = dupl_head->First();
		URL origin = dupl_head->GetOrigin();
		while (sse)
		{
			if (sse->GetSrc())
				RETURN_IF_ERROR(head->AddSrc(sse->GetSrc(), sse->GetSrcLen(), origin));
			sse = sse->Suc();
		}
	}

	return OpStatus::OK;
}

OP_STATUS HTML_Element::AddToSrcListAttr(const uni_char* src, int src_len, URL origin)
{
	if (DataSrc* head = GetDataSrc())
		return head->AddSrc(src, src_len, origin);
	else
		return OpStatus::ERR_NO_MEMORY;
}

void HTML_Element::RemoveCSS(const DocumentContext& context)
{
	RemoveImportedStyleElements(context);

	if (context.hld_profile)
		context.hld_profile->GetCSSCollection()->RemoveCollectionElement(this);

	RemoveSpecialAttribute(ATTR_CSS, SpecialNs::NS_LOGDOC);
}

OP_STATUS HTML_Element::LoadStyle(const DocumentContext& context, BOOL user_defined)
{
	OP_PROBE4(OP_PROBE_HTML_ELEMENT_LOADSTYLE);

	if (IsStyleElement() || IsLinkElement())
	{
		// Remove old stylesheet.
		RemoveCSS(context);

		// Create DataSrc from inline style elements (HE_STYLE, Markup::SVGE_STYLE)
		if (IsStyleElement())
			RETURN_IF_ERROR(CreateSrcListFromChildren());

#ifdef USER_JAVASCRIPT
		// BeforeCSS and AfterCSS events are generated for linked stylesheets because
		// their contents might not be accessible due to cross origin restrictions,
		// parse errors or proprietary will be parsed out and therefore not represented
		// by the CSSOM. The style loading logic already expects stylesheets to be loaded
		// asynchronously so waiting a bit more to run an event listener is trivial.
		// <style> elements are not supported because their contents are easily accessible
		// in the DOM, the raw contents are just the text nodes, and because supporting
		// them would require the styles' loading logic for inline <style> elements to
		// be async, which would add some small lag due to event queueing and running
		// the ecmascript chunk before handling the CSS.
		if (IsLinkElement() && context.hld_profile && DOM_Environment::IsEnabled(context.frames_doc, TRUE))
		{
			// Creation of the DOM environment will fail if scripts are off. Deal with it gracefully.
			RETURN_IF_MEMORY_ERROR(context.frames_doc->ConstructDOMEnvironment());
			if (DOM_Environment *environment = context.frames_doc->GetDOMEnvironment())
			{
				// Run the script immediately since any other thread can be blocked waiting for the style.
				ES_Thread* current_thread = context.frames_doc->GetESScheduler()->GetCurrentThread();
				OP_BOOLEAN handled_by_userjs = environment->HandleCSS(this, current_thread);

				if (handled_by_userjs != OpBoolean::IS_FALSE)
					return OpStatus::IsError(handled_by_userjs) ? handled_by_userjs : OpStatus::OK;
			}
		}
#endif // USER_JAVASCRIPT

		DataSrc* src_head = GetDataSrc();
		if (!src_head)
			return OpStatus::ERR_NO_MEMORY;

		URL* base_url_ptr = 0, base_url;

		if (IsLinkElement())
			base_url_ptr = GetLinkURL(context.logdoc);
		else
			if (context.hld_profile)
				base_url_ptr = context.hld_profile->BaseURL();

		if (base_url_ptr)
			base_url = *base_url_ptr;

		BOOL suspicious_origin = FALSE;
		// HE_STYLE/Markup::SVGE_STYLE are local so they are ok, and loads
		// outside the document (hld_profile == NULL) is for user.css
		// and they are always ok as well.
		if (!IsStyleElement() && context.hld_profile)
		{
			suspicious_origin = TRUE;
			URL css_origin = src_head->GetOrigin();
			OP_ASSERT(!css_origin.IsEmpty());
			if (!css_origin.IsEmpty())
			{
				URLContentType content_type = css_origin.ContentType();
				if (content_type == URL_CSS_CONTENT) // If the server says it's CSS, then it's always ok.
					suspicious_origin = FALSE;
				else
				{
					// Try to mark CSS from suspicious sources to make stealing
					// content by loading as CSS harder.
					if (css_origin.Type() == URL_DATA ||
						css_origin.Type() == URL_JAVASCRIPT ||
						g_secman_instance->OriginCheck(context.frames_doc, css_origin))
						suspicious_origin = FALSE;
				}

				base_url = css_origin;
			}
		}

		CSS* css = OP_NEW(CSS, (this, base_url, user_defined));
		if (!css)
			return OpStatus::ERR_NO_MEMORY;

		if (!context.hld_profile || context.hld_profile->IsXml())
			css->SetXml();

		BOOL is_ssr = context.frames_doc && context.frames_doc->GetWindow()->GetLayoutMode() == LAYOUT_SSR;
		BOOL store_css = !is_ssr || (context.hld_profile->GetCSSMode() == CSS_FULL && context.hld_profile->HasMediaStyle(CSS_MEDIA_TYPE_HANDHELD));

		CSS_PARSE_STATUS stat = OpStatus::OK;

		// If the source is not empty, parse into a CSS stylesheet object.
		if (!src_head->IsEmpty())
		{
			unsigned line_no_start = 1, line_character_start = 0;
			if (SourcePositionAttr* pos_attr =  static_cast<SourcePositionAttr*>(GetSpecialAttr(ATTR_SOURCE_POSITION, ITEM_TYPE_COMPLEX, NULL, SpecialNs::NS_LOGDOC)))
			{
				line_no_start = pos_attr->GetLineNumber();
				line_character_start = pos_attr->GetLinePosition();
			}

			if (!suspicious_origin)
			{
				stat = css->Load(context.hld_profile, src_head, line_no_start, line_character_start);

				if (OpStatus::IsMemoryError(stat))
				{
					RemoveImportedStyleElements(context);
					store_css = FALSE;
				}
			}
			else
				store_css = FALSE;
		}

		if (store_css && SetSpecialAttr(ATTR_CSS, ITEM_TYPE_CSS, css, TRUE, SpecialNs::NS_LOGDOC) >= 0)
		{
			if (context.hld_profile)
			{
				context.hld_profile->AddCSS(css);
#ifdef USER_JAVASCRIPT
				if (IsLinkElement())
					if (DOM_Environment *environment = context.frames_doc->GetDOMEnvironment())
						stat = environment->HandleCSSFinished(this, context.hld_profile->GetESLoadManager()->GetInterruptedThread(this));
#endif // USER_JAVASCRIPT
			}
		}
		else
			OP_DELETE(css);

		// For inline style elements, discard the DataSrc we created in the beginning of this method.
		if (IsStyleElement())
			src_head->DeleteAll();

		if (stat == OpStatus::ERR_NO_MEMORY)
			return stat;
	}

	return OpStatus::OK;
}

void HTML_Element::RemoveImportedStyleElements(const DocumentContext& context)
{
	HTML_Element* child = FirstChild();
	while (child)
	{
		HTML_Element* next_child = child->Suc();

		if (child->GetInserted() == HE_INSERTED_BY_CSS_IMPORT)
		{
			child->OutSafe(context);
			if (child->Clean(context))
				child->Free(context);
		}
		child = next_child;
	}
}

#ifdef USE_HTML_PARSER_FOR_XML
inline void StripCDATA(uni_char*& content, int& content_len)
{
	// Remove the CDATA stuff before and after
	while (content_len > 0 && uni_isspace(*content))
		++content, --content_len;

	BOOL is_cdata = FALSE;
	if (content_len > 9 && uni_strni_eq(content, "<![CDATA[", 9))
	{
		is_cdata = TRUE;
		content += 9, content_len -= 9;
	}

	while (content_len > 0 && uni_isspace(content[content_len - 1]))
		content_len--;

	if (is_cdata && uni_strni_eq(content + content_len - 3, "]]>", 3))
		content_len -= 3;
}
#endif // USE_HTML_PARSER_FOR_XML

OP_STATUS HTML_Element::CreateSrcListFromChildren()
{
	EmptySrcListAttr();

	HTML_Element *iter = FirstChild(), *stop = (HTML_Element *) NextSibling();
	if (iter == stop)
		RETURN_IF_ERROR(AddToSrcListAttr(UNI_L(""), 0, URL()));
	else
	{
		while (iter && iter != stop)
		{
			switch (iter->Type())
			{
			case HE_TEXT:
			{
				uni_char* content = iter->GetTextData()->GetText();
				int content_len = iter->GetTextData()->GetTextLen();
#ifdef USE_HTML_PARSER_FOR_XML
				StripCDATA(content, content_len);
#endif // USE_HTML_PARSER_FOR_XML
				RETURN_IF_ERROR(AddToSrcListAttr(content, content_len, URL()));
			}
			/* fall through */

			case HE_TEXTGROUP:
				iter = (HTML_Element *) iter->Next();
				break;

			default:
				iter = (HTML_Element *) iter->NextSibling();
			}
		}
	}

	return OpStatus::OK;
}


BOOL HTML_Element::ShowAltText()
{
	OP_ASSERT(Type() == HE_IMG || Type() == HE_INPUT || Type() == HE_OBJECT || Type() == HE_EMBED);

	HEListElm* hle = GetHEListElm(FALSE);

	if (hle)
	{
		Image img = hle->GetImage();

		if (img.Width() && img.Height())
			// Size can be determined; reflow

			return FALSE;
	}

	return TRUE;
}

# if defined(SVG_SUPPORT)
static const uni_char* GetStringAttrFromSVGRoot(HTML_Element* elm, Markup::AttrType attr)
{
	HTML_Element* parent = elm->Parent();
	while(parent)
	{
		if(parent->IsMatchingType(Markup::SVGE_SVG, NS_SVG))
		{
			break;
		}
		else if(Markup::IsRealElement(parent->Type()))
		{
			// crossing namespace border not allowed
			parent = NULL;
			break;
		}
		parent = parent->Parent();
	}

	if(parent)
	{
		return parent->GetStringAttr(attr, NS_IDX_SVG);
	}

	return NULL;
}
#endif // SVG_SUPPORT

OP_STATUS HTML_Element::ProcessCSS(HLDocProfile *hld_profile)
{
	LogicalDocument* logdoc = hld_profile->GetLogicalDocument();

	OP_STATUS status = OpStatus::OK;
	OpStatus::Ignore(status);

	LayoutMode layout_mode = hld_profile->GetFramesDocument()->GetWindow()->GetLayoutMode();
	BOOL is_smallscreen = layout_mode == LAYOUT_SSR || layout_mode == LAYOUT_CSSR;
	CSS_MediaObject* media_obj = GetLinkStyleMediaObject();
	short link_style_media = media_obj ? media_obj->GetMediaTypes() : 0;

#if defined(SVG_SUPPORT)
	const uni_char *type = NULL;
	if(GetNsType() != NS_SVG)
	{
		type = GetStringAttr(ATTR_TYPE, NS_IDX_HTML);
	}
	else
	{
		type = GetStringAttr(Markup::SVGA_TYPE, NS_IDX_SVG);
		if(!type)
		{
			type = GetStringAttrFromSVGRoot(this, Markup::SVGA_CONTENTSTYLETYPE);
		}
	}
#else
    const uni_char *type = GetStringAttr(ATTR_TYPE, NS_IDX_HTML);
#endif // SVG_SUPPORT
	// Only read stylesheets that match text/css or has no explicit type.
	// HTML 5 also specifies that charset and unknown parameters causes it to
	// be an unsupported style sheet, hence only a type of exactly "text/css"
	// is supported
	if (!type || (uni_stri_eq(type, "text/css")))
	{
		HTML_Element::DocumentContext context(logdoc);

		if ((is_smallscreen
			&& ((hld_profile->GetCSSMode() == CSS_FULL && hld_profile->HasMediaStyle(CSS_MEDIA_TYPE_HANDHELD))))
			|| (layout_mode != LAYOUT_SSR
				&& g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(hld_profile->GetCSSMode(),
					PrefsCollectionDisplay::EnableAuthorCSS), *hld_profile->GetURL()))
			|| hld_profile->GetFramesDocument()->GetWindow()->IsCustomWindow())
		{
			status = LoadStyle(context, FALSE);
		}
		else if (is_smallscreen && hld_profile->GetCSSMode() == CSS_FULL)
		{
			status = LoadStyle(context, FALSE);
			if (status == OpStatus::OK)
			{
				if (hld_profile->HasMediaStyle(CSS_MEDIA_TYPE_HANDHELD) || (link_style_media & CSS_MEDIA_TYPE_HANDHELD))
				{
					hld_profile->SetHasMediaStyle(CSS_MEDIA_TYPE_HANDHELD);
					// Q: Is this thing really needed? Shouldn't CSS::Added be called in LoadAllCSS(), also for this element?
					// A: I don't know, but not necessarily. Perhaps this is what makes reloading default props for elements
					//    whose default style is different in SSR and handheld mode, but are not affected by any stylesheets, work?
					// Let's keep CHANGED_PROPS for now, at least.
					hld_profile->GetCSSCollection()->StyleChanged(CSSCollection::CHANGED_PROPS);
					status = hld_profile->LoadAllCSS();
				}
				else
					hld_profile->SetUnloadedCSS(TRUE);
			}
		}

		hld_profile->SetHasMediaStyle(link_style_media);
	}

	return status;
}

HTML_Element::DocumentContext::DocumentContext(FramesDocument *frames_doc)
	: frames_doc(frames_doc)
	, logdoc(frames_doc ? frames_doc->GetLogicalDocument() : NULL)
	, hld_profile(logdoc ? logdoc->GetHLDocProfile() : NULL)
	, environment(frames_doc ? frames_doc->GetDOMEnvironment() : NULL)
{
}

HTML_Element::DocumentContext::DocumentContext(LogicalDocument *logdoc)
	: frames_doc(logdoc ? logdoc->GetFramesDocument() : NULL)
	, logdoc(logdoc)
	, hld_profile(logdoc ? logdoc->GetHLDocProfile() : NULL)
	, environment(frames_doc ? frames_doc->GetDOMEnvironment() : NULL)
{
}

HTML_Element::DocumentContext::DocumentContext(DOM_Environment *environment)
	: frames_doc(environment ? environment->GetFramesDocument() : NULL)
	, logdoc(frames_doc ? frames_doc->GetLogicalDocument() : NULL)
	, hld_profile(logdoc ? logdoc->GetHLDocProfile() : NULL)
	, environment(environment)
{
}

#if defined SEARCH_MATCHES_ALL && !defined HAS_NO_SEARCHTEXT
void HTML_Element::CleanSearchHit(FramesDocument *frm_doc)
{
	if (frm_doc)
	{
		if (HTML_Document* html_doc = frm_doc->GetHtmlDocument())
		{
			HTML_Element* stop_elm = static_cast<HTML_Element*>(NextSibling());
			for (HTML_Element* iter = this; iter && iter != stop_elm; iter = iter->Next())
			{
				html_doc->RemoveElementFromSearchHit(iter);
			}
		}
	}
}
#endif // SEARCH_MATCHES_ALL && !HAS_NO_SEARCHTEXT

BOOL HTML_Element::Clean(const DocumentContext &context, BOOL going_to_delete/*=TRUE*/)
{
	OP_PROBE4(OP_PROBE_HTML_ELEMENT_CLEAN);

	BOOL keep = FALSE;

	CleanLocal(context);

	if (m_first_ref)
	{
		// Head of list for the processed refs. The refs are taken out of the
		// element list as they are processed to avoid problems with finding
		// the next ref if other refs are removed from the list as a
		// side-effect of the processing.
		AutoNullElementRef placeholder;

		while (m_first_ref)
		{
			// The first ref left will be the next to process.
			ElementRef *current_ref = m_first_ref;
			m_first_ref = current_ref->NextRef();
			// Move the current ref out of the active list.
			current_ref->DetachAndMoveAfter(&placeholder);

			if (going_to_delete)
				current_ref->OnDelete(context.frames_doc);
			else
				current_ref->OnRemove(context.frames_doc);
		}

		// Put the references that has not been reset back.
		m_first_ref = placeholder.NextRef();
	}

	LogicalDocument *logdoc = context.logdoc;

	if (!Parent() && logdoc && logdoc->GetRoot() == this)
		if (context.environment)
			if (context.environment->GetDocument())
				keep = TRUE;

	HTML_Element* c = FirstChild();
	while (c)
	{
		if (!c->Clean(context, going_to_delete))
			keep = TRUE;
		c = c->Suc();
	}

	return exo_object == NULL && !keep;
}

void HTML_Element::FreeLayout(const DocumentContext &context)
{
	HTML_Element* elm = this;
	HTML_Element* stop = static_cast<HTML_Element*>(NextSibling());
	while (elm != stop)
	{
		FreeLayoutLocal(context.frames_doc);
		elm = elm->Next();
	}
}

void HTML_Element::FreeLayoutLocal(FramesDocument* doc)
{
	OP_PROBE7(OP_PROBE_HTML_ELEMENT_FREELAYOUTLOCAL);
	if (css_properties)
	{
		DELETE_CSS_PROPERTIES(this);
	}

	if (layout_box)
		RemoveLayoutBox(doc, TRUE);

	if (doc)
	{
		if (!doc->IsBeingDeleted())
		{
			SetCurrentDynamicPseudoClass(0);
		}
#ifdef SVG_SUPPORT
		else
			DestroySVGContext();
#endif // SVG_SUPPORT

#ifdef _SPAT_NAV_SUPPORT_
		Window* window = doc->GetWindow();
		if (window && window->GetSnHandler())
			window->GetSnHandler()->ElementDestroyed(this);
#endif // _SPAT_NAV_SUPPORT_
	}
}

void HTML_Element::CleanLocal(const DocumentContext &context)
{
	OP_PROBE4(OP_PROBE_HTML_ELEMENT_CLEANLOCAL);

	FramesDocument* doc = context.frames_doc;

	FreeLayoutLocal(doc);
	/* FreeLayoutLocal needs to be called before FlushInlineElement, because
	   FlushInlineElement will remove the FramesDocElm that belongs to an iframe
	   and IFrameContent::Disable uses the FramesDocElm object. */

	if (doc)
	{
#ifdef DOCUMENT_EDIT_SUPPORT
		if (doc->GetDocumentEdit())
		{
			OpDocumentEdit* de = doc->GetDocumentEdit();
			de->OnElementDeleted(this);
		}
#endif // DOCUMENT_EDIT_SUPPORT

		if (!doc->IsBeingDeleted())
		{
			if (IsReferenced())
				CleanReferences(doc);

			if (!IsText())
				doc->FlushAsyncInlineElements(this);
		}

#ifdef DRAG_SUPPORT
		g_drag_manager->OnElementRemove(this);
#endif // DRAG_SUPPORT

		if (context.hld_profile)
		{
			if ((IsStyleElement() || IsLinkElement()) && GetCSS())
				RemoveCSS(context);
#ifdef SVG_SUPPORT
			else if (IsMatchingType(Markup::SVGE_FONT, NS_SVG) || IsMatchingType(Markup::SVGE_FONT_FACE, NS_SVG))
			{
				CSS_SvgFont* svg_font = static_cast<CSS_SvgFont*>(context.hld_profile->GetCSSCollection()->RemoveCollectionElement(this));
				OP_DELETE(svg_font);
			}
#endif // SVG_SUPPORT
			else if (IsMatchingType(HE_META, NS_HTML))
				CheckMetaElement(context, ParentActual(), FALSE);
		}
	}

	// Must make sure that the formobject is removed because if the
	// document is deleted it will keep a dangling pointer to it
	if (IsFormElement())
		DestroyFormObjectBackup();

#ifdef XML_EVENTS_SUPPORT
	if (!doc || doc->HasXMLEvents())
	{
		// This attribute contains pointers to the document so it needs
		// to be removed when the element is disconnected from the document
		RemoveSpecialAttribute(ATTR_XML_EVENTS_REGISTRATION, SpecialNs::NS_LOGDOC);
	}
#endif // XML_EVENTS_SUPPORT

	if (Type() == HE_DOC_ROOT)
	{
		// Remove cached pointer to logdoc. It might disappear while we stay alive
		RemoveSpecialAttribute(ATTR_LOGDOC, SpecialNs::NS_LOGDOC);
	}
}

void HTML_Element::Free(const DocumentContext &context)
{
	OP_PROBE4(OP_PROBE_HTML_ELEMENT_FREE);

	HTML_Element* c = FirstChild();
	while (c)
	{
		HTML_Element *tmp_elm = c->Suc();
		c->Out();
		c->Free(context);
		c = tmp_elm;
	}

#ifdef DEBUG
	// Only KeepAliveElementRefs should continue to reference an element which has been called Free() on
	for (ElementRef* ref = m_first_ref; ref; ref = ref->NextRef())
		OP_ASSERT(ref->IsA(ElementRef::KEEP_ALIVE));
#endif

	if (!m_first_ref)  // KeepAliveElementRef keeps the element alive as long as the ref exist
		DELETE_HTML_Element(this);
}

BOOL HTML_Element::Clean(int *ptr)
{
	OP_ASSERT(ptr == NULL);
	return Clean(static_cast<FramesDocument *>(NULL));
}

void HTML_Element::Free(int *ptr)
{
	OP_ASSERT(ptr == NULL);
	Free(static_cast<FramesDocument *>(NULL));
}

OP_STATUS HTML_Element::DeepClone(HLDocProfile *hld_profile, HTML_Element *src_elm, BOOL force_html)
{
	OP_ASSERT(src_elm->GetInserted() < HE_INSERTED_FIRST_HIDDEN_BY_ACTUAL || src_elm->GetInserted() == HE_INSERTED_BY_IME);
	HTML_Element *child = src_elm->FirstChildActual();
	while( child )
	{
		HTML_Element *new_elm = NEW_HTML_Element();
		if( ! new_elm || new_elm->Construct(hld_profile, child, FALSE, force_html) == OpStatus::ERR_NO_MEMORY )
		{
			DELETE_HTML_Element(new_elm);
			return OpStatus::ERR_NO_MEMORY;
		}

		if( new_elm->DeepClone(hld_profile, child, force_html) == OpStatus::ERR_NO_MEMORY )
		{
			new_elm->Free(hld_profile->GetFramesDocument());
			return OpStatus::ERR_NO_MEMORY;
		}

		new_elm->SetInserted(HE_INSERTED_BY_DOM);
		new_elm->SetEndTagFound(child->GetEndTagFound());
		new_elm->Under(this);
		child = child->SucActual();
	}

	return OpStatus::OK;
}

OP_STATUS
HTML_Element::AppendEntireContentAsString(TempBuffer *tmp_buf, BOOL text_only, BOOL include_this, BOOL is_xml/*=FALSE*/)
{
	return HTML5Parser::SerializeTree(this, tmp_buf, HTML5Parser::SerializeTreeOptions().IsXml(is_xml).TextOnly(text_only).IncludeRoot(include_this));
}

void
HTML_Element::AppendEntireContentAsStringL(TempBuffer *tmp_buf, BOOL text_only, BOOL include_this, HTML_Element *root/*=NULL*/, BOOL is_xml/*=FALSE*/)
{
	HTML5Parser::SerializeTreeL(this, tmp_buf, HTML5Parser::SerializeTreeOptions().IsXml(is_xml).TextOnly(text_only).IncludeRoot(include_this));
}

void HTML_Element::CleanReferences(FramesDocument *frm_doc)
{
	OP_PROBE7(OP_PROBE_HTML_ELEMENT_CLEANREFS);

	OP_ASSERT(IsReferenced()); // Expensive function, don't call if not needed

	if (frm_doc)
		frm_doc->CleanReferencesToElement(this);

	SetReferenced(FALSE);
}

HTML_Element* HTML_Element::GetNamedElm(const uni_char* name)
{
	if (!name)
		return NULL;

	const uni_char* element_name = NULL;

	switch (Type())
	{
	case HE_A:
		element_name = GetA_Name();
		break;

	case HE_MAP:
		element_name = GetMAP_Name();
		break;

	default:
		/* element_name = GetStringAttr(ATTR_NAME); */
		break;
	}

	if (element_name && (uni_strcmp(element_name, name) == 0))
		return this;

	const uni_char* id = GetId();

	if (id && (uni_strcmp(id, name) == 0))
		return this;

	// 05/11/97: changed search direction. This routine is used to find "map" with given name.
	//			 Netscape always use the last of two with identical names.

	for (HTML_Element* he = LastChildActual(); he; he = he->PredActual())
	{
		HTML_Element* a_elm = he->GetNamedElm(name);

		if (a_elm)
			return a_elm;
	}

	return NULL;
}

HTML_Element* HTML_Element::GetElmById(const uni_char* name)
{
	HTML_Element* elm = this;
	do
	{
		const uni_char* id = elm->GetId();

		if (id && (uni_strcmp(id, name) == 0))
			return elm;
		elm = elm->NextActual();
	} while (elm);

	return NULL;
}

HTML_Element* HTML_Element::FindFirstContainedFormElm()
{
	if( (Type() == HE_INPUT || Type() == HE_SELECT || Type() == HE_BUTTON || Type() == HE_ISINDEX || Type() == HE_TEXTAREA || Type() == HE_OPTGROUP) && g_ns_manager->GetNsTypeAt(GetNsIdx()) == NS_HTML)
		return this;

	for (HTML_Element* he = FirstChildActual(); he; he = he->SucActual())
	{
		HTML_Element* a_elm = he->FindFirstContainedFormElm();

		if (a_elm)
			return a_elm;
	}

	return NULL;
}

HTML_Element* HTML_Element::GetMAP_Elm(const uni_char* name)
{
	if (Type() == HE_MAP)
	{
		const uni_char *elm_name = GetMAP_Name();
		// changed to case insensitive because of bug 83236
		if (elm_name && (uni_stricmp(elm_name, name) == 0))
			return this;

		const uni_char *id = GetId();

		if (id && (uni_stricmp(id, name) == 0))
			return this;
	}

	for (HTML_Element *he = FirstChildActual(); he; he = he->SucActual())
	{
		HTML_Element *map_elm = he->GetMAP_Elm(name);

		if (map_elm)
			return map_elm;
	}

	return NULL;
}

OP_STATUS HTML_Element::InitESObject(ES_Runtime* runtime, BOOL statically_allocated)
{
	if (FramesDocument *frames_doc = runtime->GetFramesDocument())
		if (DOM_Environment *environment = frames_doc->GetDOMEnvironment())
		{
			DOM_Object *node;
			RETURN_IF_ERROR(environment->ConstructNode(node, this));
			return OpStatus::OK;
		}

	return OpStatus::ERR;
}

const uni_char* HTML_Element::GetAnchor_HRef(FramesDocument *doc)
{
	if (IsMatchingType(HE_A, NS_HTML))
		return GetA_HRef(doc);

#ifdef _WML_SUPPORT_
	if (doc && doc->GetDocManager()->WMLHasWML())
	{
		WML_Context* wc = doc->GetDocManager()->WMLGetContext();
		if (wc)
		{
			URL ret_url = wc->GetWmlUrl(this);
			return ret_url.IsEmpty() ? NULL : ret_url.GetAttribute(URL::KUniName).CStr();
		}
	}
#endif //_WML_SUPPORT_

#ifdef SVG_SUPPORT
	if (IsMatchingType(Markup::SVGE_A, NS_SVG))
	{
		URL* url = g_svg_manager->GetXLinkURL(this, &doc->GetURL());
		if (url)
		{
			return url->GetAttribute(URL::KUniName).CStr();
		}
	}
#endif // SVG_SUPPORT

	if (URL *css_link = (URL*)GetSpecialAttr(ATTR_CSS_LINK, ITEM_TYPE_URL, (void*)NULL, SpecialNs::NS_LOGDOC))
		if (css_link->Type() != URL_JAVASCRIPT)
			return css_link->GetAttribute(URL::KUniName_Username_Password_Hidden).CStr();

	return NULL;
}


const uni_char*	HTML_Element::GetA_HRef(FramesDocument *doc, BOOL insert_password/*= FALSE*/)
{
	if (Type() == HE_A)
	{
#ifdef _WML_SUPPORT_
		if (doc && doc->GetDocManager()->WMLHasWML())
		{
			WML_Context* wc = doc->GetDocManager()->WMLGetContext();
			if (wc)
			{
				URL ret_url = wc->GetWmlUrl((HTML_Element*)this);
				return ret_url.GetAttribute(insert_password ? URL::KUniName_Username_Password_NOT_FOR_UI : URL::KUniName).CStr();
			}
		}
#endif // _WML_SUPPORT_

		return GetStringAttr(ATTR_HREF);
	}

	return NULL;
}

const char*	HTML_Element::GetA_HRefWithRel(FramesDocument *doc)
{
	if (Type() == HE_A)
	{
#ifdef _WML_SUPPORT_
		if (doc && doc->GetDocManager()->WMLHasWML())
		{
			WML_Context* wc = doc->GetDocManager()->WMLGetContext();
			if (wc)
			{
				URL ret_url = wc->GetWmlUrl((HTML_Element*)this);
				return ret_url.GetAttribute(URL::KName_With_Fragment, TRUE).CStr();
			}
		}
#endif // _WML_SUPPORT_

		URL* url = GetUrlAttr(ATTR_HREF, NS_IDX_HTML, doc ? doc->GetLogicalDocument() : NULL);

		if (url)
			return url->GetAttribute(URL::KName_With_Fragment, TRUE).CStr();
	}

	return NULL;
}

const char*	HTML_Element::GetA_HRefRel(LogicalDocument *logdoc/*=NULL*/)
{
	if (Type() == HE_A)
	{
		URL* url = GetUrlAttr(ATTR_HREF, NS_IDX_HTML, logdoc);

		if (url)
			return url->RelName();
	}

	return 0;
}

URL HTML_Element::GetAnchor_URL(HTML_Document *doc)
{
	OP_ASSERT(doc);
	return GetAnchor_URL(doc->GetFramesDocument());
}

URL HTML_Element::GetAnchor_URL(FramesDocument *doc)
{
	OP_ASSERT(doc);

#ifdef _WML_SUPPORT_
	if (doc->GetDocManager()->WMLHasWML())
	{
		WML_Context* wc = doc->GetDocManager()->WMLGetContext();
		if (wc)
		{
			URL ret_url = wc->GetWmlUrl(this);
			if (!ret_url.IsEmpty())
				return ret_url;
		}
	}
#endif //_WML_SUPPORT_

	if (URL *css_link = (URL*)GetSpecialAttr(ATTR_CSS_LINK, ITEM_TYPE_URL, (void*)NULL, SpecialNs::NS_LOGDOC))
	{
		if (css_link->Type() != URL_JAVASCRIPT)
			return *css_link;
	}
	else if (GetNsType() == NS_HTML &&
		(Type() == HE_A || Type() == HE_LINK || Type() == HE_AREA))
	{
		URL* tmp_url = GetUrlAttr(ATTR_HREF, NS_IDX_HTML, doc->GetLogicalDocument());
		if (tmp_url)
			return *tmp_url;
	}
#ifdef SVG_SUPPORT
	else if (IsMatchingType(Markup::SVGE_A, NS_SVG))
	{
		URL root_url = doc->GetURL();
		URL* retval = g_svg_manager->GetXLinkURL(this, &root_url);
		if(retval)
			return *retval;
	}
#endif // SVG_SUPPORT

	return URL();
}

OP_STATUS HTML_Element::GetAnchor_URL(FramesDocument *doc, OpString& url)
{
	URL target = GetAnchor_URL(doc);

	const uni_char* url_local = target.GetAttribute(URL::KUniName_With_Fragment_Username_Password_Hidden).CStr();
	if (!url_local)
		url_local = UNI_L("");

	return url.Set(url_local);
}

OP_STATUS HTML_Element::GetURLAndTitle(FramesDocument *doc, OpString& url, OpString& title)
{
	RETURN_IF_ERROR(GetAnchor_URL(doc, url));

	TempBuffer title_buffer;
	const uni_char* title_local;

	if (IsMatchingType(HE_AREA, NS_HTML))
	{
		RETURN_IF_ERROR(title_buffer.Append(GetStringAttr(ATTR_TITLE)));
		title_local = title_buffer.GetStorage();
	}
	else
	{
		const uni_char* alt_text = NULL;

		RETURN_IF_ERROR(GetInnerTextOrImgAlt(title_buffer, alt_text));

		uni_char* ptr = title_buffer.GetStorage();

		if (ptr)
			while (uni_isspace(*ptr)) ++ptr;

		if ((!ptr || uni_strlen(ptr) == 0) && alt_text)
			title_local = alt_text;
		else
			title_local = ptr;
	}

	if (!title_local)
		title_local = UNI_L("");

	return title.Set(title_local);
}

CSSValue HTML_Element::GetListType() const
{
	HTML_ElementType type = Type();
	CSSValue ltype = (CSSValue)GetNumAttr(ATTR_TYPE, NS_IDX_HTML, LIST_TYPE_NULL);
	switch (type)
	{
	case HE_LI:
		return (ltype == LIST_TYPE_NULL) && (NULL != Parent()) ? (CSSValue)Parent()->GetListType() : ltype;

	case HE_UL:
	case HE_MENU:
		return ltype == LIST_TYPE_NULL ? LIST_TYPE_DISC : ltype;

	case HE_OL:
		return ltype == LIST_TYPE_NULL ? LIST_TYPE_NUMBER : ltype;
	}

	return ltype;
}

int	HTML_Element::GetEMBED_PrivateAttrs(uni_char** &argn, uni_char** &argv) const
{
	argn = 0;
	argv = 0;
	PrivateAttrs* pa = (PrivateAttrs*)GetSpecialAttr(ATTR_PRIVATE, ITEM_TYPE_PRIVATE_ATTRS, (void*)0, SpecialNs::NS_LOGDOC);
	if (pa)
	{
		argn = pa->GetNames();
		argv = pa->GetValues();
		return pa->GetLength();
	}
	return 0;
}

BOOL HTML_Element::GetMapUrl(VisualDevice* vd, int x, int y, const HTML_Element* img_element, URL* murl, LogicalDocument *logdoc/*=NULL*/)
{
	//if (Type() == HE_AREA)
	{
		BOOL hit = FALSE;
		int shape = GetAREA_Shape();
		CoordsAttr* ca = (CoordsAttr*)GetAttr(ATTR_COORDS, ITEM_TYPE_COORDS_ATTR, (void*)0);
		if (ca)
		{
			int coords_len = ca->GetLength();
			int* coords = ca->GetCoords();

			int width_scale = static_cast<int>(img_element->GetSpecialNumAttr(Markup::LAYOUTA_IMAGEMAP_WIDTH_SCALE, SpecialNs::NS_LAYOUT, 1000));
			int height_scale = static_cast<int>(img_element->GetSpecialNumAttr(Markup::LAYOUTA_IMAGEMAP_HEIGHT_SCALE, SpecialNs::NS_LAYOUT, 1000));

			POINT pt;
			pt.x = x*1000/width_scale;
			pt.y = y*1000/height_scale;

			switch (shape)
			{
			case AREA_SHAPE_RECT:
				if (coords_len >= 4)
				{
					int minx, maxx, miny, maxy;
					if(coords[0] > coords[2])
					{
						minx = coords[2];
						maxx = coords[0];
					}
					else
					{
						minx = coords[0];
						maxx = coords[2];
					}
					if(coords[1] > coords[3])
					{
						miny = coords[3];
						maxy = coords[1];
					}
					else
					{
						miny = coords[1];
						maxy = coords[3];
					}
					hit = (pt.x >= minx &&
						   pt.x <= maxx &&
						   pt.y >= miny &&
						   pt.y <= maxy);
				}
				break;

			case AREA_SHAPE_CIRCLE:
				if (coords_len >= 3)
					hit = vd->InEllipse(coords, pt.x, pt.y);
				break;

			case AREA_SHAPE_POLYGON:
				if (coords_len >= 2)
					hit = vd->InPolygon(coords,  coords_len / 2, pt.x, pt.y);
				break;
			}
		}

		if (hit)
		{
			if (murl)
				if (!GetAREA_NoHRef())
				{
					URL* url = NULL;

					url = GetUrlAttr(ATTR_HREF, NS_IDX_HTML, logdoc);

					if (url)
						*murl = *url;
				}

			return TRUE;
		}
	}

	return FALSE;
}

void HTML_Element::InvertRegionBorder(VisualDevice* vd, int xpos, int ypos, int width, int height, int bsize, RECT* region_rect, BOOL clear, HTML_Element* img_element)
{
	int shape = GetAREA_Shape();
	int coords_len = 0;
	int* coords = NULL;
	int tmpcoords[4];

	xpos -= vd->GetRenderingViewX();
	ypos -= vd->GetRenderingViewY();

	if (shape == AREA_SHAPE_DEFAULT)
	{
		//default shape is 'rect', with the full image as coordinates.
		coords_len = 4;
		coords = tmpcoords;
		coords[0] = 0;
		coords[1] = 0;
		coords[2] = width;
		coords[3] = height;
		shape = AREA_SHAPE_RECT;
	}
	else if (CoordsAttr* ca = (CoordsAttr*)GetAttr(ATTR_COORDS, ITEM_TYPE_COORDS_ATTR, (void*)0))
	{
		coords_len = ca->GetLength();
		coords = ca->GetCoords();
	}

	if (coords_len)
	{
		int width_scale = static_cast<int>(img_element->GetSpecialNumAttr(Markup::LAYOUTA_IMAGEMAP_WIDTH_SCALE, SpecialNs::NS_LAYOUT, 1000));
		int height_scale = static_cast<int>(img_element->GetSpecialNumAttr(Markup::LAYOUTA_IMAGEMAP_HEIGHT_SCALE, SpecialNs::NS_LAYOUT, 1000));

		switch (shape)
		{
		case AREA_SHAPE_RECT:
			if (coords_len >= 4)
			{
				int x, y, w, h;

				x = xpos + MIN(coords[0]*width_scale/1000, width);
				y = ypos + MIN(coords[1]*height_scale/1000, height);
				w = MIN((coords[2] - coords[0])*width_scale/1000, MAX(width - coords[0]*width_scale/1000, 0));
				h = MIN((coords[3] - coords[1])*height_scale/1000, MAX(height - coords[1]*height_scale/1000, 0));

				if (region_rect)
				{
					region_rect->left = x;
					region_rect->top = y;
					region_rect->right = x + w;
					region_rect->bottom = y + h;
				}
				else
				{
					OpRect drawRect(x, y, w, h);
					vd->painter->InvertBorderRect(vd->OffsetToContainer(vd->ScaleToScreen(drawRect)), bsize);
				}
			}
			break;

		case AREA_SHAPE_CIRCLE:
			if (coords_len >= 3)
			{
				int radius_scale = MIN(width_scale, height_scale);
				int radius = coords[2]*radius_scale/1000;

				if (region_rect)
				{
					region_rect->left = xpos + coords[0]*width_scale/1000 - radius + 1;
					region_rect->top = ypos + coords[1]*height_scale/1000 - radius + 1;
					region_rect->right = region_rect->left + 2*radius;
					region_rect->bottom = region_rect->top + 2*radius;
				}
				else
				{
					OpRect drawRect(xpos + coords[0]*width_scale/1000 - radius + 1,
									ypos + coords[1]*height_scale/1000 - radius + 1,
									2*radius,
									2*radius);
					vd->painter->InvertBorderEllipse(vd->OffsetToContainer(vd->ScaleToScreen(drawRect)), bsize);
				}
			}
			break;

		case AREA_SHAPE_POLYGON:
			if (coords_len >= 2)
			{
				if (region_rect)
				{
					int max_x = 0;
					int max_y = 0;
					int numpoints = coords_len / 2;
					for (int i = 0; i < numpoints; i++)
					{
						max_x = MAX(max_x, coords[i * 2]*width_scale/1000);
						max_y = MAX(max_y, coords[i * 2 + 1]*height_scale/1000);
					}
					region_rect->left = xpos;
					region_rect->top = ypos;
					region_rect->right = xpos + max_x;
					region_rect->bottom = ypos + max_y;
				}
				else
				{
					int numpoints = coords_len / 2;
					OpPoint* points = OP_NEWA(OpPoint, numpoints);
					if (!points)
						return; //FIXME:OOM7 - Can't propagate the OOM error
					for(int i=0; i < numpoints; i++)
					{
						points[i].Set(xpos + coords[i*2]*width_scale/1000,
									  ypos + coords[i*2 + 1]*height_scale/1000);
						points[i] = vd->OffsetToContainer(vd->ScaleToScreen(points[i]));
					}
					vd->painter->InvertBorderPolygon(points, numpoints, bsize);
					OP_DELETEA(points);
				}
			}
			break;
		}
	}
}

#ifdef SKINNABLE_AREA_ELEMENT
void HTML_Element::DrawRegionBorder(VisualDevice* vd, int xpos, int ypos, int width, int height, int bsize, RECT* region_rect, BOOL clear, HTML_Element* img_element)
{
	int shape = GetAREA_Shape();
	int coords_len = 0;
	int* coords = NULL;
	int tmpcoords[4];

	xpos -= vd->GetRenderingViewX();
	ypos -= vd->GetRenderingViewY();

	if (shape == AREA_SHAPE_DEFAULT)
	{
		//default shape is 'rect', with the full image as coordinates.
		coords_len = 4;
		coords = tmpcoords;
		coords[0] = 0;
		coords[1] = 0;
		coords[2] = width;
		coords[3] = height;
		shape = AREA_SHAPE_RECT;
	}
	else if (CoordsAttr* ca = (CoordsAttr*)GetAttr(ATTR_COORDS, ITEM_TYPE_COORDS_ATTR, (void*)0))
	{
		coords_len = ca->GetLength();
		coords = ca->GetCoords();
	}

	if (coords_len)
	{
		int width_scale = static_cast<int>(img_element->GetSpecialNumAttr(Markup::LAYOUTA_IMAGEMAP_WIDTH_SCALE, 1000, SpecialNs::NS_LAYOUT));
		int height_scale = static_cast<int>(img_element->GetSpecialNumAttr(Markup::LAYOUTA_IMAGEMAP_HEIGHT_SCALE, 1000, SpecialNs::NS_LAYOUT));

		switch (shape)
		{
		case AREA_SHAPE_RECT:
			if (coords_len >= 4)
			{
				int x, y, w, h;

				x = xpos + MIN(coords[0]*width_scale/1000, width);
				y = ypos + MIN(coords[1]*height_scale/1000, height);
				w = MIN((coords[2] - coords[0])*width_scale/1000, MAX(width - coords[0]*width_scale/1000, 0));
				h = MIN((coords[3] - coords[1])*height_scale/1000, MAX(height - coords[1]*height_scale/1000, 0));

				if (region_rect)
				{
					region_rect->left = x;
					region_rect->top = y;
					region_rect->right = x + w;
					region_rect->bottom = y + h;
				}
				else
				{
					OpRect drawRect(x, y, w, h);

					OpSkinElement* skin_elm_area = g_skin_manager->GetSkinElement(UNI_L("Active Area Element"));

					UINT32 outline_color = 0;
					UINT32 outline_width = 2;
					if (skin_elm_area)
					{
						skin_elm_area->GetOutlineColor(&outline_color, 0);
						skin_elm_area->GetOutlineWidth(&outline_width, 0);
					}

					vd->painter->SetColor(outline_color);
					vd->painter->DrawRect(vd->OffsetToContainer(vd->ScaleToScreen(drawRect)), outline_width);
				}
			}
			break;

		case AREA_SHAPE_CIRCLE:
			if (coords_len >= 3)
			{
				int radius_scale = MIN(width_scale, height_scale);
				int radius = coords[2]*radius_scale/1000;

				if (region_rect)
				{
					region_rect->left = xpos + coords[0]*width_scale/1000 - radius + 1;
					region_rect->top = ypos + coords[1]*height_scale/1000 - radius + 1;
					region_rect->right = region_rect->left + 2*radius;
					region_rect->bottom = region_rect->top + 2*radius;
				}
				else
				{
					OpRect drawRect(xpos + coords[0]*width_scale/1000 - radius + 1,
									ypos + coords[1]*height_scale/1000 - radius + 1,
									2*radius,
									2*radius);

					OpSkinElement* skin_elm_area = g_skin_manager->GetSkinElement(UNI_L("Active Area Element"));

					UINT32 outline_color = 0;
					UINT32 outline_width = 2;
					if (skin_elm_area)
					{
						skin_elm_area->GetOutlineColor(&outline_color, 0);
						skin_elm_area->GetOutlineWidth(&outline_width, 0);
					}

					vd->painter->SetColor(outline_color);
					vd->painter->DrawEllipse(vd->OffsetToContainer(vd->ScaleToScreen(drawRect)), outline_width);
				}
			}
			break;

		case AREA_SHAPE_POLYGON:
			if (coords_len >= 2)
			{
				if (region_rect)
				{
					int max_x = 0;
					int max_y = 0;
					int numpoints = coords_len / 2;
					for (int i = 0; i < numpoints; i++)
					{
						max_x = MAX(max_x, coords[i * 2]*width_scale/1000);
						max_y = MAX(max_y, coords[i * 2 + 1]*height_scale/1000);
					}
					region_rect->left = xpos;
					region_rect->top = ypos;
					region_rect->right = xpos + max_x;
					region_rect->bottom = ypos + max_y;
				}
				else
				{
					int numpoints = coords_len / 2;
					OpPoint* points = OP_NEWA(OpPoint, numpoints);
					if (!points)
						return; //FIXME:OOM7 - Can't propagate the OOM error
					for(int i=0; i < numpoints; i++)
					{
						points[i].Set(xpos + coords[i*2]*width_scale/1000,
									  ypos + coords[i*2 + 1]*height_scale/1000);
						points[i] = vd->OffsetToContainer(vd->ScaleToScreen(points[i]));
					}

					OpSkinElement* skin_elm_area = g_skin_manager->GetSkinElement(UNI_L("Active Area Element"));

					UINT32 outline_color = 0;
					UINT32 outline_width = 2;
					if (skin_elm_area)
					{
						skin_elm_area->GetOutlineColor(&outline_color, 0);
						skin_elm_area->GetOutlineWidth(&outline_width, 0);
					}

					vd->painter->SetColor(outline_color);
					vd->painter->DrawPolygon(points, numpoints, outline_width);
					OP_DELETEA(points);
				}
			}
			break;
		}
	}
}
#endif //SKINNABLE_AREA_ELEMENT

URL* HTML_Element::GetEMBED_URL(LogicalDocument *logdoc/*=NULL*/)
{
	if (Type() == HE_EMBED)
		return GetUrlAttr(ATTR_SRC, NS_IDX_HTML, logdoc);
	else
		return NULL;
}

URL HTML_Element::GetImageURL(BOOL follow_redirect/*=TRUE*/, LogicalDocument *logdoc/*=NULL*/)
{
	short attr = ATTR_NULL;
	if (GetNsType() == NS_HTML)
	{
		HTML_ElementType type = Type();
		switch (type)
		{
			case HE_IMG:
#ifdef PICTOGRAM_SUPPORT
				{
					URL *local_url = (URL*)GetSpecialAttr(ATTR_LOCALSRC_URL, ITEM_TYPE_URL, NULL, SpecialNs::NS_LOGDOC);
					if (local_url)
					{
						local_url->SetAttribute(URL::KIsGeneratedByOpera, TRUE);
						return *local_url;
					}
				}
#endif // PICTOGRAM_SUPPORT
				// fall through
			case Markup::HTE_INPUT:
			case Markup::HTE_EMBED:
				attr = Markup::HA_SRC;
				break;

#ifdef MEDIA_HTML_SUPPORT
			case HE_VIDEO:
				attr = ATTR_POSTER;
				break;
#endif // MEDIA_HTML_SUPPORT

			case HE_OBJECT:
				{
#ifdef PICTOGRAM_SUPPORT
					URL *local_url = (URL*)GetSpecialAttr(ATTR_LOCALSRC_URL, ITEM_TYPE_URL, NULL, SpecialNs::NS_LOGDOC);
					if (local_url)
					{
						local_url->SetAttribute(URL::KIsGeneratedByOpera, TRUE);
						return *local_url;
					}
#endif // PICTOGRAM_SUPPORT
#ifdef JS_PLUGIN_ATVEF_VISUAL
					JS_Plugin_Object *obj_p = NULL;
					OP_BOOLEAN is_jsplugin = IsJSPluginObject(logdoc->GetHLDocProfile(), &obj_p);
					if (is_jsplugin == OpBoolean::IS_TRUE)
					{
						// Does this jsplugin want an ATVEF visual representation?
						ES_Object *esobj = DOM_Utils::GetJSPluginObject(GetESElement());
						if (obj_p->IsAtvefVisualPlugin() && esobj != NULL)
						{
							// Get the JS_Plugin_HTMLObjectElement_Object that correspond to
							// the OBJECT tag. This is not the same as the obj_p we just
							// retrieved...
							EcmaScript_Object *ecmascriptobject =
								ES_Runtime::GetHostObject(esobj);
							OP_ASSERT(ecmascriptobject->IsA(ES_HOSTOBJECT_JSPLUGIN));
							JS_Plugin_Object *jsobject =
								static_cast<JS_Plugin_Object *>(ecmascriptobject);

							// Check that we actually did request visualization when
							// instantiating this object.
							if (jsobject->IsAtvefVisualPlugin())
							{
								// Create an internal tv: URL so that we can set up a listener
								// that can filter on this particular object only (this is done
								// in GetResolvedObjectType()).
								uni_char tv_url_string[sizeof (void *) * 2 + 9]; /* ARRAY OK 2010-01-18 peter */
								uni_snprintf(tv_url_string, ARRAY_SIZE(tv_url_string), UNI_L("tv:///%p"),
									reinterpret_cast<void *>(jsobject));
								return g_url_api->GetURL(tv_url_string);
							}
						}
					}
					else if (OpStatus::IsMemoryError(is_jsplugin))
					{
						g_memory_manager->RaiseCondition(is_jsplugin);
						return URL();
					}
#endif

					attr = ATTR_DATA;
				}
				break;

			case HE_TABLE:
			case HE_TD:
			case HE_TH:
			case HE_BODY:
				attr = ATTR_BACKGROUND;
				break;

			default:
				break;
		}
	}

	if (attr == ATTR_NULL)
		return URL();
	else if (attr == ATTR_BACKGROUND)
	{
		StyleAttribute* style_attr = (StyleAttribute *)GetAttr(ATTR_BACKGROUND, ITEM_TYPE_COMPLEX, (void *)0);
		if (style_attr && style_attr->GetProperties()->GetFirstDecl())
		{
			CSS_decl *bg_images_cp = style_attr->GetProperties()->GetFirstDecl();
			OP_ASSERT(bg_images_cp->GenArrayValue() && bg_images_cp->ArrayValueLen() == 1);

			const uni_char *url_str = bg_images_cp->GenArrayValue()[0].GetString();
			return url_str ? g_url_api->GetURL(url_str) : URL();
		}
		else
			return URL();
	}

	if (URL *tmp_url = GetUrlAttr(attr, NS_IDX_HTML, logdoc))
	{
		if (!follow_redirect)
			return *tmp_url;
		URL target_url = tmp_url->GetAttribute(URL::KMovedToURL, TRUE);
		return !target_url.IsEmpty() ? target_url : *tmp_url;
	}

	return URL();
}

OP_STATUS HTML_Element::SetCSS_Style(CSS_property_list* prop_list)
{
	StyleAttribute* style = OP_NEW(StyleAttribute, (NULL, prop_list));
	if (!style)
	{
		return OpStatus::ERR_NO_MEMORY;
	}
	int res;
#ifdef SVG_SUPPORT
	if(GetNsType() == NS_SVG)
	{
		res = SetAttr(Markup::SVGA_STYLE, ITEM_TYPE_COMPLEX, static_cast<void*>(style), TRUE, NS_IDX_SVG);
	}
	else
#endif // SVG_SUPPORT
	{
		res = SetAttr(ATTR_STYLE, ITEM_TYPE_COMPLEX, static_cast<void*>(style), TRUE);
	}
	if (res == -1)
	{
		prop_list->Ref(); // Take back the reference we gave to |style|
		OP_DELETE(style);
		return OpStatus::ERR_NO_MEMORY;
	}
	return OpStatus::OK;
}

StyleAttribute*
HTML_Element::GetStyleAttribute()
{
# if defined(SVG_SUPPORT)
	if (GetNsType() == NS_SVG)
		return static_cast<StyleAttribute*>(GetAttr(Markup::SVGA_STYLE, ITEM_TYPE_COMPLEX, (void*)0, NS_IDX_SVG));
	else
# endif // SVG_SUPPORT
		return static_cast<StyleAttribute*>(GetAttr(ATTR_STYLE, ITEM_TYPE_COMPLEX, (void*)0));
}

CSS_property_list*
HTML_Element::GetCSS_Style()
{
	StyleAttribute* style_attr = GetStyleAttribute();
	return style_attr ? style_attr->GetProperties() : NULL;
}

OP_STATUS
HTML_Element::EnsureCSS_Style()
{
	if (!GetCSS_Style())
	{
		CSS_property_list *prop_list = OP_NEW(CSS_property_list, ());
		RETURN_OOM_IF_NULL(prop_list);
		prop_list->Ref();

		if (OpStatus::IsMemoryError(SetCSS_Style(prop_list)))
		{
			prop_list->Unref();
			return OpStatus::ERR_NO_MEMORY;
		}
	}

	return OpStatus::OK;
}

OP_STATUS
HTML_Element::SetStyleDecl(CSSProperty property, float value, CSSValueType val_type)
{
	if (StyleAttribute *style_attr = GetStyleAttribute())
		style_attr->SetIsModified();

	RETURN_IF_ERROR(EnsureCSS_Style());
	CSS_property_list *prop_list = GetCSS_Style();

	CSS_number_decl *decl = OP_NEW(CSS_number_decl, (property, value, val_type));
	RETURN_OOM_IF_NULL(decl);

	prop_list->ReplaceDecl(decl, TRUE, CSS_decl::ORIGIN_AUTHOR);

	return OpStatus::OK;
}

OP_STATUS
HTML_Element::ApplyStyle(HLDocProfile* hld_profile)
{
	return hld_profile->GetLayoutWorkplace()->ApplyPropertyChanges(this, 0, FALSE, Markup::HA_STYLE);
}

const uni_char*	HTML_Element::GetId() const
{
	BOOL is_svg = (GetNsType() == NS_SVG);
	const uni_char* id = NULL;
	for (int index = 0, length = GetAttrSize(); index < length; ++index)
	{
		if (GetAttrIsId(index) && GetItemType(index) == ITEM_TYPE_STRING)
		{
			id = GetAttrValueString(index);
			if(!is_svg || (is_svg && GetAttrNs(index) == NS_IDX_XML))
				break;
		}
	}
	return id;
}

OP_STATUS HTML_Element::SetText(const DocumentContext &context, const uni_char *txt, unsigned int txt_len,
								HTML_Element::ValueModificationType modification_type /* = MODIFICATION_REPLACING_ALL */,
								unsigned offset /* = 0 */, unsigned length1 /* = 0 */, unsigned length2 /* = 0 */)
{
	FramesDocument *doc = context.frames_doc;

#if defined(DOCUMENT_EDIT_SUPPORT)
	if(doc && doc->GetDocumentEdit())
		doc->GetDocumentEdit()->OnTextChange(this);
#endif

	if (doc)
		doc->BeforeTextDataChange(this);

	OP_STATUS status = SetTextInternal(doc, txt, txt_len);
	if (OpStatus::IsError(status))
	{
		if (doc)
			doc->AbortedTextDataChange(this);
		return status;
	}

	if (doc && doc->GetDocRoot() && doc->GetDocRoot()->IsAncestorOf(this))
	{
#ifdef DOCUMENT_EDIT_SUPPORT
		if(doc->GetDocumentEdit())
			doc->GetDocumentEdit()->OnTextChanged(this);
#endif
		if (layout_box)
		{
			layout_box->RemoveCachedInfo();
#ifndef HAS_NOTEXTSELECTION
			doc->RemoveLayoutCacheFromSelection(this);
#endif // !HAS_NOTEXTSELECTION
		}

		MarkDirty(doc);

		DOM_Environment *environment = context.environment;
		ES_Thread* thread = environment ? environment->GetCurrentScriptThread() : NULL;

		if (HTML_Element *parent = ParentActual())
			status = parent->HandleCharacterDataChange(doc, thread, TRUE);
	}

	if (doc)
		doc->TextDataChange(this, modification_type, offset, length1, length2);

	if (context.environment)
	{
		OP_STATUS status2 = context.environment->ElementCharacterDataChanged(this, modification_type, offset, length1, length2);
		if (OpStatus::IsError(status2))
			status = status2;
	}

	return status;
}

OP_STATUS
HTML_Element::SetTextOfNumerationListItemMarker(FramesDocument* doc, const uni_char *txt, unsigned int txt_len)
{
	// Two asserts ensuring that this is a text element of a numeration list item marker.
	OP_ASSERT(GetInserted() == HE_INSERTED_BY_LAYOUT);
	OP_ASSERT(Parent() && Parent()->GetIsListMarkerPseudoElement());

	RETURN_IF_ERROR(SetTextInternal(doc, txt, txt_len));

	/* Numeration string length is currently strictly limited by the layout engine (a lot below 32k).
	   So any conversion to HE_TEXTGROUP cannot occur after setting the new text data. */
	OP_ASSERT(Type() == HE_TEXT);

	if (layout_box)
		layout_box->RemoveCachedInfo();

	return OpStatus::OK;
}

BOOL GetFramesetRowColSpecVal(const uni_char* spec, int i, int& val, int& type)
{
	const uni_char* str = spec;
	for (int j=i; j>0 && str; j--)
	{
		str = uni_strchr(str, ',');
		if (str)
			str++;
	}

	if (!str)
		return FALSE;

	while (*str && uni_isspace(*str))
		str++;

	if (*str == '*' || *str == ',')
	{
		// Empty values should be treated as 1*
		val = 1;
		type = FRAMESET_RELATIVE_SIZED;
		return TRUE;
	}
	else if (uni_isdigit(*str))
	{
		val = uni_atoi(str);
		while (uni_isdigit(*str))
			str++;
		if (*str == '.')
		{
			str++;
			while (uni_isdigit(*str))
				str++;
		}
		while (uni_isspace(*str))
			str++;
		if (*str == '*')
			type = FRAMESET_RELATIVE_SIZED;
		else if (*str == '%')
			type = FRAMESET_PERCENTAGE_SIZED;
		else
			type = FRAMESET_ABSOLUTE_SIZED;
		return TRUE;
	}
	else if (uni_strchr(str, ','))
	{
		// garbage, but not garbage at the end. Treat as 0
		val = 0;
		type = FRAMESET_ABSOLUTE_SIZED;
		return TRUE;
	}
	else
		return FALSE;
}

BOOL HTML_Element::GetFramesetRow(int i, int& val, int& tpe) const
{
	if (Type() == HE_FRAMESET)
	{
		const uni_char* spec = GetStringAttr(ATTR_ROWS);
		if (spec)
			return GetFramesetRowColSpecVal(spec, i, val, tpe);
	}
	return FALSE;
}

BOOL HTML_Element::GetFramesetCol(int i, int& val, int& tpe) const
{
	if (Type() == HE_FRAMESET)
	{
		const uni_char* spec = GetStringAttr(ATTR_COLS);
		if (spec)
			return GetFramesetRowColSpecVal(spec, i, val, tpe);
	}
	return FALSE;
}

int HTML_Element::GetFramesetRowCount() const
{
	int rows = 0;
	if (Type() == HE_FRAMESET)
	{
		int j, type;
		for (;GetFramesetRow(rows, j, type);rows++){}
	}
	return rows;
}

int HTML_Element::GetFramesetColCount() const
{
	int cols = 0;
	if (Type() == HE_FRAMESET)
	{
		int j, type;
		for (;GetFramesetCol(cols, j, type);cols++){}
	}
	return cols;
}

OP_STATUS HTML_Element::GetOptionText(TempBuffer *buffer)
{
	OP_ASSERT(IsMatchingType(HE_OPTION, NS_HTML) || IsMatchingType(HE_OPTGROUP, NS_HTML));
	int len = GetTextContentLength();

	RETURN_IF_ERROR(buffer->Expand(len + 1));

	uni_char *storage = buffer->GetStorage();

	GetTextContent(storage, len + 1);
	buffer->SetCachedLengthPolicy(TempBuffer::UNTRUSTED);

	uni_char *source = storage, *target = storage;
	BOOL insert_space = FALSE;

	while (*source)
	{
		if (uni_isspace(*source))
		{
			if (uni_isnbsp(*source))
				*target++ = ' ';
			else
				insert_space = TRUE;
		}
		else
		{
			if (insert_space && target != storage)
				*target++ = ' ';

			*target++ = *source;
			insert_space = FALSE;
		}

		++source;
	}

	*target = 0;

	return OpStatus::OK;
}

BOOL HTML_Element::IsIncludedInSelect(HTML_Element* select)
{
	OP_ASSERT(select && select->IsMatchingType(HE_SELECT, NS_HTML));
	OP_ASSERT(select->IsAncestorOf(this));

	if (!IsMatchingType(HE_OPTION, NS_HTML) && !IsMatchingType(HE_OPTGROUP, NS_HTML))
		return FALSE;

	HTML_Element* parent = Parent();
	while (parent != select)
	{
		if (!parent->IsMatchingType(HE_OPTGROUP, NS_HTML))
			return FALSE;
		parent = parent->Parent();
	}

	return TRUE;
}

int	HTML_Element::GetFormNr(FramesDocument* frames_doc /* = NULL */) const
{
	if (Type() == HE_FORM)
		return static_cast<int>(GetSpecialNumAttr(ATTR_JS_ELM_IDX, SpecialNs::NS_LOGDOC, -1));
	else if (Type() == HE_ISINDEX || (Parent() && Parent()->Type() == HE_ISINDEX))
		return 0; // quick hack to make ISINDEX work (this is no problem if the page is normal and only has one isindex and no other form elements)
	else
	{
		// The form element may be disassociated with the form
		const uni_char* form_specified = FormManager::GetFormIdString(const_cast<HTML_Element*>(this));
		if (form_specified)
		{
			HTML_Element* form_elm = FormManager::FindElementFromIdString(frames_doc, const_cast<HTML_Element*>(this), form_specified);
			if (form_elm)
				return form_elm->GetFormNr();
			return -1;
		}
		int formnr = static_cast<int>(GetSpecialNumAttr(ATTR_JS_FORM_IDX, SpecialNs::NS_LOGDOC, -1));

		if (GetInserted() == HE_INSERTED_BY_DOM)
		{
			// Elements inserted by DOM don't know which form they belong to so we need to check.
			formnr = -1;
			HTML_Element* he = ParentActual();
			while (he)
			{
				if (he->GetNsType() == NS_HTML && he->Type() == HE_FORM)
				{
					formnr = he->GetFormNr();
					break;
				}
				he = he->ParentActual();
			}
		}
		return formnr;
	}
}


long HTML_Element::GetFormElmNr() const
{
	int elm_nr = GetElmNr();
	int form_nr = GetFormNr();
	return MAKELONG((int)elm_nr, (int)form_nr);
}

int	HTML_Element::GetSize() const
{
	int size = (int)GetNumAttr(ATTR_SIZE);
	if (Type() == HE_INPUT)
	{
		if (size <= 0)
		{
			switch (GetInputType())
		{
			case INPUT_TEL:
			case INPUT_SEARCH:
			case INPUT_URI:
			case INPUT_EMAIL:
			case INPUT_TEXT:
			case INPUT_PASSWORD:
				size = 20; //15;
				break;
			case INPUT_DATE:
				size = 20; // XXX MAX_SIZE;
				break;
			case INPUT_WEEK:
				size = 8;
				break;
			case INPUT_TIME:
				size = 21;
				break;
			case INPUT_NUMBER:
			case INPUT_RANGE:
				size = 12;
				break;
			case INPUT_MONTH:
				size = 5;
				break;
			case INPUT_DATETIME:
				size = 50;
				break;
			case INPUT_DATETIME_LOCAL:
				size = 48;
				break;
			case INPUT_COLOR:
				size = 10;
				break;
			}
		}
	}
	else if (Type() == HE_SELECT)
	{
		if (size < 1 && GetMultiple())
			size = 4;
		else
			if (size < 1)
				size = 1;
	}
	return size;
}

HTML_Element* HTML_Element::GetFirstElm(HTML_ElementType tpe, NS_Type ns/*=NS_HTML*/)
{
	if (IsMatchingType(tpe, ns))
		return this;

	HTML_Element* the = 0;
	HTML_Element* he = FirstChildActual();
	while (he && !the)
	{
		the = he->GetFirstElm(tpe, ns);
		he = he->SucActual();
	}
	return the;
}

int HTML_Element::GetElmNumber(HTML_Element* he, BOOL& found)
{
	if (he == this)
	{
		found = TRUE;
		return 1;
	}
	int count = 1;
	HTML_Element* che = FirstChildActual();
	found = FALSE;
	while (che && !found)
	{
		count += che->GetElmNumber(he, found);
		che = che->SucActual();
	}
	return count;
}

HTML_Element* HTML_Element::GetElmNumber(int elm_nr, int &i)
{
	i++;
	if (elm_nr == i)
		return this;

	HTML_Element* he = 0;
	HTML_Element* che = FirstChildActual();
	while (che && !he)
	{
		he = che->GetElmNumber(elm_nr, i);
		che = che->SucActual();
	}
	return he;
}


BOOL HTML_Element::IsDisabled(FramesDocument* frames_doc)
{
	// In WF2 fieldset may disable anything within it
	return FormValidator::IsInheritedDisabled(this);
}

BOOL HTML_Element::HasContent()
{
	HTML_ElementType type = Type();
	if (type == HE_IMG || type == HE_HR || type == HE_INPUT || type == HE_SELECT ||
#if defined(_SSL_SUPPORT_) && !defined(_EXTERNAL_SSL_SUPPORT_)
		type == HE_KEYGEN ||
#endif
#ifdef _PLUGIN_SUPPORT_
		type == HE_EMBED ||
#endif
		type == HE_TEXTAREA)
		return TRUE;
	else if (type == HE_TEXT)
	{
		const uni_char* txt = LocalContent();
		if (!txt)
			return FALSE;
		while (*txt && uni_isspace(*txt))
			txt++;
		return (*txt != '\0');
	}

	HTML_Element* he = FirstChild();
	BOOL ret = FALSE;
	while (he && !ret)
	{
		ret = he->HasContent();
		he = he->Suc();
	}
	return ret;
}

BOOL HTML_Element::IsPartOf(HTML_ElementType tpe) const
{
	HTML_Element* p = ParentActual();
	while (p)
	{
		if (p->Type() == tpe)
			return TRUE;
		p = p->ParentActual();
	}
	return FALSE;
}

HTML_Element* HTML_Element::GetTitle()
{
	if (IsMatchingType(HE_TITLE, NS_HTML))
		return this;
#ifdef SVG_SUPPORT
	else if(IsMatchingType(Markup::SVGE_SVG, NS_SVG))
	{
		HTML_Element* title = FirstChildActual();
		while(title)
		{
			if(title->IsMatchingType(Markup::SVGE_TITLE, NS_SVG))
				return title;
			title = title->NextSiblingActual();
		}
		return 0;
	}
#endif // SVG_SUPPORT
	else
	{
		HTML_Element* elm = FirstChildActual();
		HTML_Element* title;
		while (elm)
		{
			title = elm->GetTitle();
			if (title)
				return title;
			elm = elm->SucActual();
		}
		return 0;
	}
}

const uni_char*
HTML_Element::GetCurrentLanguage()
{
	const uni_char *ret_val;

	if ((ret_val = GetStringAttr(XMLA_LANG, NS_IDX_XML)) != NULL)
		return ret_val;

	if ((ret_val = GetStringAttr(ATTR_LANG)) != NULL)
		return ret_val;

	if (Parent())
		return Parent()->GetCurrentLanguage();
	else
		return NULL;
}

URL* HTML_Element::GetScriptURL(URL& root_url, LogicalDocument *logdoc/*=NULL*/)
{
	URL *original_src_url = static_cast<URL*>(GetSpecialAttr(Markup::LOGA_ORIGINAL_SRC, ITEM_TYPE_URL, NULL, SpecialNs::NS_LOGDOC));
	if (original_src_url)
		return original_src_url;

#ifdef SVG_DOM
	if (IsMatchingType(Markup::SVGE_SCRIPT, NS_SVG))
		return g_svg_manager->GetXLinkURL(this, &root_url);
#endif // SVG_DOM
	return GetUrlAttr(ATTR_SRC, NS_IDX_HTML, logdoc);
}

OP_BOOLEAN HTML_Element::HandleScriptElement(HLDocProfile* hld_profile, ES_Thread *thread, BOOL register_first, BOOL is_inline_script, unsigned stream_position)
{
	/* Script parsed in innerHTML/outerHTML.  Will be handled when inserted into the
	   document instead. */
	if (!hld_profile->GetHandleScript())
		return OpBoolean::IS_FALSE;

	URL* url = GetScriptURL(*hld_profile->GetURL(), hld_profile->GetLogicalDocument());
	BOOL is_external = url && url->Type() != URL_NULL_TYPE;

	// Don't execute completely empty scripts. They might execute later if the content is changed.
	if (!is_external && !FirstChildActualStyle())
		return OpBoolean::IS_FALSE;

#ifdef DELAYED_SCRIPT_EXECUTION
	if (!hld_profile->ESDelayScripts())
#endif // DELAYED_SCRIPT_EXECUTION
	{
		RETURN_IF_ERROR(hld_profile->GetFramesDocument()->ConstructDOMEnvironment());

		if (register_first)
			RETURN_IF_ERROR(hld_profile->GetESLoadManager()->RegisterScript(this, is_external, is_inline_script, thread));
	}

#ifdef DELAYED_SCRIPT_EXECUTION
	if (hld_profile->ESDelayScripts() && !HasAttr(ATTR_DECLARE))
		RETURN_IF_ERROR(hld_profile->ESAddScriptElement(this, stream_position, !is_external));
#endif // DELAYED_SCRIPT_EXECUTION

	if (is_external)
	{
		int handled = GetSpecialNumAttr(ATTR_JS_SCRIPT_HANDLED, SpecialNs::NS_LOGDOC);

		if ((handled & SCRIPT_HANDLED_LOADED) == 0)
		{
			BOOL already_loaded = FALSE;

#ifdef DELAYED_SCRIPT_EXECUTION
			OP_BOOLEAN result = FetchExternalScript(hld_profile, thread, &already_loaded, !hld_profile->ESIsExecutingDelayedScript() || !hld_profile->ESIsFirstDelayedScript(this));

			if (hld_profile->ESDelayScripts())
				if (result == OpBoolean::IS_TRUE)
				{
					if (already_loaded)
						RETURN_IF_ERROR(hld_profile->ESSetScriptElementReady(this));
					return OpBoolean::IS_FALSE;
				}
				else
				{
					OP_STATUS status = hld_profile->ESCancelScriptElement(this);
					if (OpStatus::IsMemoryError(status))
						return status;
				}
#else // DELAYED_SCRIPT_EXECUTION
			OP_BOOLEAN result = FetchExternalScript(hld_profile, thread, &already_loaded);
#endif // DELAYED_SCRIPT_EXECUTION

			if (!already_loaded)
				return result;
			else if ((GetSpecialNumAttr(ATTR_JS_SCRIPT_HANDLED, SpecialNs::NS_LOGDOC) & SCRIPT_HANDLED_LOADED) == 0)
				return LoadExternalScript(hld_profile);
			else
				return OpBoolean::IS_TRUE;
		}
	}

#ifdef DELAYED_SCRIPT_EXECUTION
	if (hld_profile->ESDelayScripts())
		return OpBoolean::IS_FALSE;
	else
#endif // DELAYED_SCRIPT_EXECUTION
		return LoadScript(hld_profile);
}

void HTML_Element::CancelScriptElement(HLDocProfile* hld_profile)
{
	hld_profile->GetESLoadManager()->CancelInlineScript(this);
}

#if defined(USER_JAVASCRIPT) && defined(_PLUGIN_SUPPORT_)
OP_BOOLEAN HTML_Element::PluginInitializedElement(HLDocProfile* hld_profile)
{
	OP_ASSERT(hld_profile->GetFramesDocument());
	RETURN_IF_ERROR(hld_profile->GetFramesDocument()->ConstructDOMEnvironment());
	return hld_profile->GetFramesDocument()->GetDOMEnvironment()->PluginInitializedElement(this);
}
#endif // USER_JAVASCRIPT && _PLUGIN_SUPPORT_

BOOL HTML_Element::IsScriptSupported(FramesDocument* frames_doc)
{
	BOOL script_supported = frames_doc->GetDOMEnvironment()->IsEnabled();

	if (script_supported)
	{
		const uni_char* type = NULL;
		const uni_char* language = NULL;

#if defined(SVG_SUPPORT)
		if(GetNsType() == NS_SVG)
		{
			type = GetStringAttr(Markup::SVGA_TYPE, NS_IDX_SVG);
			if(!type)
			{
				type = GetStringAttrFromSVGRoot(this, Markup::SVGA_CONTENTSCRIPTTYPE);
			}
		}
		else
#endif // SVG_SUPPORT
		{
			type = GetStringAttr(ATTR_TYPE);
			language = GetStringAttr(ATTR_LANGUAGE);
		}

		script_supported = g_ecmaManager->IsScriptSupported(type, language);
	}

	return script_supported;
}

OP_BOOLEAN HTML_Element::LoadScript(HLDocProfile* hld_profile)
{
	OP_PROBE4(OP_PROBE_HTML_ELEMENT_LOADSCRIPT1);

	FramesDocument* frames_doc = hld_profile->GetFramesDocument();
	ES_LoadManager *load_manager = hld_profile->GetESLoadManager();

	OP_STATUS status = frames_doc->ConstructDOMEnvironment();
	if (OpStatus::IsError(status))
	{
		if (status == OpStatus::ERR)
			OpStatus::Ignore(load_manager->CancelInlineScript(this));
		return status;
	}

	BOOL script_supported = IsScriptSupported(frames_doc), script_skipped = FALSE;

	/* <script declare="declare"> means that this script should only be executed
	   when activated by an event. */
	if (HasAttr(ATTR_DECLARE))
		script_skipped = TRUE;

#ifdef USER_JAVASCRIPT
	if (script_supported && !script_skipped)
	{
		OP_BOOLEAN handled_by_userjs = frames_doc->GetDOMEnvironment()->HandleScriptElement(this, hld_profile->GetESLoadManager()->GetInterruptedThread(this));

		if (handled_by_userjs != OpBoolean::IS_FALSE)
			return handled_by_userjs;
	}
#endif // USER_JAVASCRIPT

	if (script_supported)
	{
		/* If the script tag comes from an innerHTML, outerHTML or insertAdjacentHTML,
		   it needs to be supported or there will be crashes if the script calls
		   document.write.  It's unclear exactly when and how such scripts would be
		   executed anyway. */
		if (!hld_profile->GetHandleScript())
			script_supported = FALSE;
	}

	if (!script_supported)
	{
		hld_profile->ESSetScriptNotSupported(TRUE);
		script_skipped = TRUE;
	}

	if (script_skipped)
	{
		RETURN_IF_ERROR(load_manager->CancelInlineScript(this));
		return OpBoolean::IS_FALSE;
	}

	hld_profile->ESSetScriptNotSupported(FALSE);

	ES_ProgramText *program_array = NULL;
	int program_array_length = 0;

	status = ConstructESProgramText(program_array, program_array_length, *hld_profile->GetURL(), hld_profile->GetLogicalDocument());

	if (OpStatus::IsError(status))
	{
		OpStatus::Ignore(load_manager->CancelInlineScript(this));
		if (OpStatus::IsMemoryError(status))
			hld_profile->SetIsOutOfMemory(TRUE);
		return status;
	}

	OP_BOOLEAN result;
	long handled = GetSpecialNumAttr(ATTR_JS_SCRIPT_HANDLED, SpecialNs::NS_LOGDOC);
	if (program_array_length > 0)
	{
		result = load_manager->SetScript(this, program_array, program_array_length, (handled & SCRIPT_HANDLED_CROSS_ORIGIN) != 0);

		if (result == OpBoolean::IS_TRUE)
		{
			handled = GetSpecialNumAttr(ATTR_JS_SCRIPT_HANDLED, SpecialNs::NS_LOGDOC);
			SetSpecialNumAttr(ATTR_JS_SCRIPT_HANDLED, handled | SCRIPT_HANDLED_EXECUTED, SpecialNs::NS_LOGDOC);
		}
		else
			OpStatus::Ignore(frames_doc->HandleEvent(ONLOAD, NULL, this, SHIFTKEY_NONE, 0, NULL));
	}
	else
	{
		OpStatus::Ignore(frames_doc->HandleEvent(ONLOAD, NULL, this, SHIFTKEY_NONE, 0, NULL));
		OP_STATUS status = load_manager->CancelInlineScript(this);

		if (OpStatus::IsSuccess(status))
			result = OpBoolean::IS_FALSE;
		else
			result = status;
	}

	OP_DELETEA(program_array);
	return result;
}

OP_BOOLEAN
HTML_Element::FetchExternalScript(HLDocProfile* hld_profile, ES_Thread *thread, BOOL *already_loaded, BOOL load_inline)
{
	FramesDocument *frames_doc = hld_profile->GetFramesDocument();

#ifdef USER_JAVASCRIPT
#ifdef DELAYED_SCRIPT_EXECUTION
	if (!hld_profile->ESDelayScripts())
#endif // DELAYED_SCRIPT_EXECUTION
	{
		OP_STATUS status = frames_doc->ConstructDOMEnvironment();
		RETURN_IF_MEMORY_ERROR(status);

		if (OpStatus::IsError(status))
		{
			hld_profile->GetESLoadManager()->CancelInlineScript(this);
			return status;
		}

		OP_BOOLEAN handled_by_userjs = frames_doc->GetDOMEnvironment()->HandleExternalScriptElement(this, hld_profile->GetESLoadManager()->GetInterruptedThread(this));

		if (handled_by_userjs != OpBoolean::IS_FALSE)
			return handled_by_userjs;
	}
#endif // USER_JAVASCRIPT

#ifdef LOGDOC_GETSCRIPTLIST
	RETURN_IF_ERROR(hld_profile->AddScript(this));
#endif // LOGDOC_GETSCRIPTLIST

	URL* url = GetScriptURL(*hld_profile->GetURL(), hld_profile->GetLogicalDocument());

	OP_ASSERT(url && url->Type() != URL_NULL_TYPE);

	OP_LOAD_INLINE_STATUS load_stat;

	if (load_inline)
	{
		LoadInlineOptions options;
		if (frames_doc->GetDocManager()->MustReloadScripts()
#ifdef DELAYED_SCRIPT_EXECUTION
			/* With DSE we may get here twice: the first time when parsing ahead and
			   the second when executing the script. Force reload only for the first case
			   otherwise we'll get NSL.
			 */
			&& !hld_profile->ESIsExecutingDelayedScript()
#endif // DELAYED_SCRIPT_EXECUTION
			)
			options.ReloadConditionally();
		load_stat = frames_doc->LoadInline(url, this, SCRIPT_INLINE, options);
	}
	else
		load_stat = LoadInlineStatus::USE_LOADED;

	if (load_stat == OpStatus::ERR_NO_MEMORY)
		return OpStatus::ERR_NO_MEMORY;

	if (HasAttr(ATTR_DECLARE))
		// The script should not be executed during loading.
		return OpBoolean::IS_FALSE;
	else if ((load_stat == LoadInlineStatus::LOADING_CANCELLED || load_stat == LoadInlineStatus::USE_LOADED) && url->Status(TRUE) == URL_LOADED)
	{
		if (already_loaded)
		{
			*already_loaded = TRUE;
			return OpBoolean::IS_TRUE;
		}
		else if ((GetSpecialNumAttr(ATTR_JS_SCRIPT_HANDLED, SpecialNs::NS_LOGDOC) & SCRIPT_HANDLED_LOADED) == 0)
			return LoadExternalScript(hld_profile);
		else
			return OpBoolean::IS_TRUE;
	}
	else if (load_stat == LoadInlineStatus::LOADING_REFUSED || load_stat == LoadInlineStatus::LOADING_CANCELLED)
	{
#ifdef OPERA_CONSOLE
		RETURN_IF_MEMORY_ERROR(LogicalDocument::PostConsoleMsg(url,
			Str::S_ES_LOADING_LINKED_SCRIPT_FAILED,
			OpConsoleEngine::EcmaScript, OpConsoleEngine::Error, GetLogicalDocument()));
#endif // OPERA_CONSOLE

		// The script will not be loaded, for whatever reason, so continue parsing.
		hld_profile->GetESLoadManager()->CancelInlineScript(this);
		return OpBoolean::IS_FALSE;
	}
	else if (!load_inline)
	{
		// We expected the inline to be loaded already; if it wasn't, we
		// can't block here.
		hld_profile->GetESLoadManager()->CancelInlineScript(this);
		return OpBoolean::IS_FALSE;
	}
	else
		// Default: block parsing progress waiting for the script to load.
		return OpBoolean::IS_TRUE;
}

OP_BOOLEAN
HTML_Element::LoadExternalScript(HLDocProfile* hld_profile)
{
	OP_PROBE4(OP_PROBE_HTML_ELEMENT_LOADEXTERNALSCRIPT1);

	FramesDocument* frames_doc = hld_profile->GetFramesDocument();
	LogicalDocument *logdoc = hld_profile->GetLogicalDocument();

	RETURN_IF_ERROR(frames_doc->ConstructDOMEnvironment());

	if (!frames_doc->GetDOMEnvironment()->IsEnabled())
	{
		RETURN_IF_ERROR(hld_profile->GetESLoadManager()->CancelInlineScript(this));
		return OpBoolean::IS_FALSE;
	}

	URL* url = GetScriptURL(*hld_profile->GetURL(), logdoc);

#ifdef _DEBUG
	OP_NEW_DBG("LoadExternalScript", "html5");
	if (Debug::DoDebugging("html5"))
	{
		OpString dbg_url_str;
		url->GetAttribute(URL::KUniName_With_Fragment_Username_Password_NOT_FOR_UI, dbg_url_str, TRUE);
		OP_DBG((UNI_L("Loaded external (%s)"), dbg_url_str.CStr()));
	}
#endif // _DEBUG

	DataSrc *src_head = GetDataSrc();
	OP_BOOLEAN result = OpBoolean::IS_FALSE; OpStatus::Ignore(result);

	HEListElm* helm = GetHEListElmForInline(SCRIPT_INLINE);
	if (helm)
		helm->SetHandled();

	URL target_url = url->GetAttribute(URL::KMovedToURL, TRUE);
	if (target_url.IsEmpty())
		target_url = *url;

	OP_MEMORY_VAR uint32 http_response = HTTP_OK;
	if (target_url.Type() == URL_HTTP || target_url.Type() == URL_HTTPS)
		http_response = target_url.GetAttribute(URL::KHTTP_Response_Code, TRUE);

	src_head->DeleteAll();

#ifdef CORS_SUPPORT
	if (HasAttr(Markup::HA_CROSSORIGIN))
	{
		if (!helm->IsCrossOriginAllowed())
		{
			hld_profile->GetESLoadManager()->ReportScriptError(this, UNI_L("Error loading script."));
			CancelScriptElement(hld_profile);
#ifdef DELAYED_SCRIPT_EXECUTION
			hld_profile->ESCancelScriptElement(this);
#endif // DELAYED_SCRIPT_EXECUTION
			SendEvent(ONERROR, frames_doc);
			return result;
		}
		else
		{
			long handled = GetSpecialNumAttr(ATTR_JS_SCRIPT_HANDLED, SpecialNs::NS_LOGDOC);
			SetSpecialNumAttr(ATTR_JS_SCRIPT_HANDLED, handled | SCRIPT_HANDLED_CROSS_ORIGIN, SpecialNs::NS_LOGDOC);
		}
	}
#endif // CORS_SUPPORT
	BOOL is_cancelled = FALSE;
	if (http_response == HTTP_OK || http_response == HTTP_NOT_MODIFIED)
	{
		URL_DataDescriptor* url_data_desc;

		unsigned short int parent_charset_id = helm ? GetSuggestedCharsetId(this, hld_profile, helm->GetLoadInlineElm()) : 0;
		g_charsetManager->IncrementCharsetIDReference(parent_charset_id);

		if (helm && OpStatus::IsSuccess(result = helm->CreateURLDataDescriptor(url_data_desc, NULL, URL::KFollowRedirect, FALSE, TRUE, frames_doc->GetWindow(), URL_X_JAVASCRIPT, 0, FALSE, parent_charset_id)))
		{
			BOOL more = TRUE;
			unsigned long data_size;

			while (more)
			{
#ifdef OOM_SAFE_API
				TRAP(result, data_size = url_data_desc->RetrieveDataL(more));
				if (OpStatus::IsError(result))
					break;
#else // OOM_SAFE_API
				data_size = url_data_desc->RetrieveData(more);
#endif // OOM_SAFE_API

				uni_char* data_buf = (uni_char *) url_data_desc->GetBuffer();

				if (!data_buf || !data_size)
					break;

				if (OpStatus::IsError(result = src_head->AddSrc(data_buf, UNICODE_DOWNSIZE(data_size), *url)))
					break;

				url_data_desc->ConsumeData(data_size);
			}

			OP_DELETE(url_data_desc);
		}
		else
		{
			CancelScriptElement(hld_profile);
#ifdef DELAYED_SCRIPT_EXECUTION
			hld_profile->ESCancelScriptElement(this);
#endif // DELAYED_SCRIPT_EXECUTION
			is_cancelled = TRUE;
		}
		g_charsetManager->DecrementCharsetIDReference(parent_charset_id);
	}
	else if (http_response >= 400)
	{
		hld_profile->GetESLoadManager()->CancelInlineScript(this);

#ifdef DELAYED_SCRIPT_EXECUTION
		hld_profile->ESCancelScriptElement(this);
#endif // DELAYED_SCRIPT_EXECUTION

		is_cancelled = TRUE;

		/* Note: load error not reported at the toplevel error handler, but to the
		   console only. Do not want to expose that result beyond the script element's
		   onerror handler, but as a compatibility note, Gecko always reports it. */
#ifdef OPERA_CONSOLE
		RETURN_IF_MEMORY_ERROR(LogicalDocument::PostConsoleMsg(url,
			Str::S_ES_LOADING_LINKED_SCRIPT_FAILED,
			OpConsoleEngine::EcmaScript, OpConsoleEngine::Error, GetLogicalDocument()));
#endif // OPERA_CONSOLE

		/* To prevent duplicate error events, report 'error' via the inline's element, if possible. */
		if (helm)
			helm->SendOnError();
		else
			SendEvent(ONERROR, frames_doc);
	}

	long handled = GetSpecialNumAttr(ATTR_JS_SCRIPT_HANDLED, SpecialNs::NS_LOGDOC);
	SetSpecialNumAttr(ATTR_JS_SCRIPT_HANDLED, handled | SCRIPT_HANDLED_LOADED, SpecialNs::NS_LOGDOC);

	if (!OpStatus::IsMemoryError(result) && http_response < 400)
	{

		BOOL parser_blocking = !is_cancelled && logdoc->GetParser();
		if (logdoc->IsXml())
			parser_blocking = FALSE;

		if (parser_blocking)
		{
			result = logdoc->GetParser()->GetTreeBuilder()->ExternalScriptReady(this);
		}

		if (!OpStatus::IsMemoryError(result))
			result = HandleScriptElement(hld_profile);
	}

	if (OpStatus::IsMemoryError(result))
	{
		hld_profile->SetIsOutOfMemory(TRUE);
		src_head->DeleteAll();
	}

	return result;
}

OP_STATUS
HTML_Element::ConstructESProgramText(ES_ProgramText *&program_array, int &program_array_length, URL& root_url, LogicalDocument *logdoc/*=NULL*/)
{
	BOOL is_external = FALSE;

	if (URL* url = GetScriptURL(root_url, logdoc))
		if (!url->IsEmpty())
			is_external = TRUE;

	if (is_external)
	{
		DataSrc* src_head = GetDataSrc();

		program_array_length = src_head->GetDataSrcElmCount();

		if (program_array_length > 0)
		{
			program_array = OP_NEWA(ES_ProgramText, program_array_length);

			if (program_array)
			{
				ES_ProgramText *program_array_ptr = program_array;

				DataSrcElm *src_elm = src_head->First();
				while (src_elm)
				{
					program_array_ptr->program_text = src_elm->GetSrc();
					program_array_ptr->program_text_length = src_elm->GetSrcLen();

					++program_array_ptr;
					src_elm = src_elm->Suc();
				}
			}
			else
				return OpStatus::ERR_NO_MEMORY;
		}
	}
	else
	{
		HTML_Element *elm;

		program_array = 0;
		program_array_length = 0;

		elm = (HTML_Element *) NextActual();
		while (elm && IsAncestorOf(elm))
		{
			if (elm->Type() == HE_TEXT && elm->LocalContent())
				++program_array_length;
			elm = (HTML_Element *) elm->NextActual();
		}

		if (program_array_length)
		{
			ES_ProgramText *program_array_ptr = program_array = OP_NEWA(ES_ProgramText, program_array_length); // FIXME:REPORT_MEMMAN-KILSMO
			if (!program_array)
				return OpStatus::ERR_NO_MEMORY;

			elm = (HTML_Element *) NextActual();
			while (elm && IsAncestorOf(elm))
			{
				if (elm->Type() == HE_TEXT && elm->LocalContent())
				{
					program_array_ptr->program_text = elm->LocalContent();
					program_array_ptr->program_text_length = elm->LocalContentLength();
					++program_array_ptr;
				}
				elm = (HTML_Element *) elm->NextActual();
			}
		}
	}

	/* Strip trailing HTML comment stuff (-->).

	   A comment end is stripped by shortening the script text.  It is stripped
	   back to the first line break unless it's commented out by a '//' or '<--', in
	   which case nothing is changed and we let the ecmascript engine ignore it
	   the ordinary way. */

	if (program_array_length > 0)
	{
		int text_length = program_array[program_array_length - 1].program_text_length;
		const uni_char* last = program_array[program_array_length - 1].program_text + text_length - 1;

		while (text_length > 0 && uni_isspace(*last))
			--last, --text_length;

		if (text_length >= 3 && uni_strni_eq(last - 2, "-->", 3))
		{
			last -= 3;
			text_length -= 3;

			// Look for a // or <!-- on the line to see if this is already commented out
			// the ordinary way. Else strip it completely
			while (TRUE)
			{
				if (text_length == 0 || *last == '\n' || *last == '\r')
				{
					// Strip the line (MSIE compatibility, WebKit and Mozilla doesn't do this)
					program_array[program_array_length - 1].program_text_length = text_length;
					break;
				}

				if ((text_length >= 2 && last[0] == '/' && last[-1] == '/') ||
					(text_length >= 4 && last[0] == '-' && last[-1] == '-' && last[-2] == '!' && last[-3] == '<'))
				{
					// the end comment is already commented out so don't strip the line
					break;
				}

				--last, --text_length;
			}
		}
	}

	return OpStatus::OK;
}

void HTML_Element::CleanupScriptSource()
{
	if (DataSrc* src = GetDataSrc())
		src->DeleteAll();
}

void HTML_Element::StealScriptSource(DataSrc* other)
{
	if (DataSrc* src = GetDataSrc())
		src->MoveTo(other);
}

OP_STATUS HTML_Element::SetEventHandler(FramesDocument *frames_doc, DOM_EventType event, const uni_char *value, int value_length)
{
	RETURN_IF_ERROR(frames_doc->ConstructDOMEnvironment());

	DOM_Environment *environment = frames_doc->GetDOMEnvironment();

	if (environment && environment->IsEnabled())
	{
		if (!exo_object)
		{
			/* Calling ConstructNode when there is already a node is typically
			   safe, but this function is typically indirectly called from
			   ConstructNode itself, and there is an OP_ASSERT there that gets
			   confused if we it gets called recursively like that. */

			DOM_Object *node;
			RETURN_IF_ERROR(environment->ConstructNode(node, this));
		}

		OP_STATUS status = environment->SetEventHandler(exo_object, event, value, value_length);

		return status;
	}

	return OpStatus::OK;
}

OP_STATUS HTML_Element::ClearEventHandler(DOM_Environment *environment, DOM_EventType event)
{
	OP_ASSERT(environment);

	if (exo_object)
	{
		if (environment->IsEnabled())
		{
			return environment->ClearEventHandler(exo_object, event);
		}
	}

	return OpStatus::OK;
}

OP_STATUS
HTML_Element::EnableEventHandlers(FramesDocument *doc)
{
	return OpStatus::OK;
}

BOOL
HTML_Element::HasEventHandler(FramesDocument *frames_doc, DOM_EventType event, HTML_Element* referencing_elm, HTML_Element** handling_elm, ES_EventPhase phase)
{
	if (frames_doc)
	{
		if (!GetESElement() && HasEventHandlerAttribute(frames_doc, event))
		{
			if (handling_elm)
				*handling_elm = this;
			return TRUE;
		}
		if (DOM_Environment *dom_environment = frames_doc->GetDOMEnvironment())
			return dom_environment->HasEventHandler(this, event, handling_elm);
		else if (Parent()) // Assume that the event will bubble so ask parent
			return Parent()->HasEventHandler(frames_doc, event, referencing_elm, handling_elm, phase);
	}
	return FALSE;
}

#ifdef _WML_SUPPORT_
void
DoWmlSelection(HTML_Element *select_elm, HTML_Element *option_elm, FramesDocument *frames_doc, BOOL user_initiated)
{
	if (!select_elm)
	{
		if (option_elm)
		{
			select_elm = option_elm->Parent();
			while (select_elm)
			{
				if (select_elm->IsMatchingType(HE_SELECT, NS_HTML))
					break;
				select_elm = select_elm->Parent();
			}
		}

		if (!select_elm)
			return;
	}

	// Need to update the WML Variables.
	WML_Context *wc = frames_doc->GetDocManager()->WMLGetContext();
	wc->UpdateWmlSelection(select_elm, FALSE);
	FormValueList* form_value_list = FormValueList::GetAs(select_elm->GetFormValue());
	if (HTML_Element* option_he = form_value_list->GetFirstSelectedOption(select_elm))
	{
		// checks if we should navigate somewhere
		BOOL noop = TRUE;
		WMLNewTaskElm *tmp_task = wc->GetTaskByElement(option_he);
		if (tmp_task && wc->PerformTask(tmp_task, noop, user_initiated, WEVT_ONPICK) == OpStatus::ERR_NO_MEMORY)
			frames_doc->GetWindow()->RaiseCondition(OpStatus::ERR_NO_MEMORY);
	}
}
#endif // _WML_SUPPORT_

/**
 * Some elements expect focus and blur elements when interacted with.
 * This identifies those and returns that element, or NULL
 * if there is no such element involved.
 */
static HTML_Element* GetFocusBlurElement(FramesDocument* frames_doc, HTML_Element* elm)
{
	HTML_Element* focusable_element = elm;

	// Using the same definition of focusable as the script method HTMLElement::focus and the
	// CSS pseudo class :focused (or is it :focus ?)
	while (focusable_element && !focusable_element->IsFocusable(frames_doc))
	{
		focusable_element = focusable_element->Parent();
	}

	return focusable_element;
}

void
HTML_Element::GetCaretCoordinate(FramesDocument* frames_doc, int& caret_doc_x, int& caret_doc_y,
								 int& caret_offset_x, int& caret_offset_y)
{
	BOOL found_doc_coords = FALSE;

#ifdef DOCUMENT_EDIT_SUPPORT
	if (OpDocumentEdit *de = frames_doc->GetDocumentEdit())
	{
		if (de->GetEditableContainer(this))
		{
			OpPoint doc_caret_point = frames_doc->GetCaretPainter()->GetCaretPosInDocument();

			caret_doc_x = doc_caret_point.x;
			caret_doc_y = doc_caret_point.y;
			found_doc_coords = TRUE;
		}
	}
#endif // DOCUMENT_EDIT_SUPPORT

	if (!found_doc_coords)
	{
		if (FormObject* form_obj = GetFormObject())
		{
			if (!form_obj->GetCaretPosInDocument(&caret_doc_x, &caret_doc_y))
			{
				// Not the caret, but closer than (0,0)
				AffinePos doc_pos;
				form_obj->GetPosInDocument(&doc_pos);

				OpPoint doc_pt = doc_pos.GetTranslation();
				caret_doc_x = doc_pt.x;
				caret_doc_y = doc_pt.y;
			}
			found_doc_coords = TRUE;
		}
	}

	RECT rect;
	BOOL status = frames_doc->GetLogicalDocument()->GetBoxRect(this, BOUNDING_BOX, rect);
	if (status)
	{
		if (!found_doc_coords)
		{
			// In the middle of the box for no specific reason except symmetry.
			caret_doc_x = (rect.left + rect.right) / 2;
			caret_doc_y = (rect.top + rect.bottom) / 2;
		}

		caret_offset_x = caret_doc_x - rect.left;
		caret_offset_y = caret_doc_y - rect.top;
	}
	else
	{
		if (!found_doc_coords)
		{
			// So, no idea where we are. Upper left corner will have to do
			caret_doc_x = 0;
			caret_doc_y = 0;
		}
		caret_offset_x = 0;
		caret_offset_y = 0;
	}
}

#ifndef NO_SAVE_SUPPORT
/**
 * Trigger "Save image as" depending on modifiers.
 *
 * Ctrl+Alt+Click on img should result in triggering action. As legacy
 * feature, we also support Ctrl+Click on <img> that is not an anchor.
 *
 * SVG inside <img> and <object> is also supported.
 *
 * @param modifiers   key modifiers pressed
 * @param frames_doc  FramesDocument to which this event was dispatched
 * @param target      HTML_Element which received the event
 *
 * @returns TRUE if download was triggered
 *          FALSE if there was nothing saved
 */
static BOOL HandleSaveImage(ShiftKeyState modifiers, FramesDocument *frames_doc, HTML_Element *target)
{
# ifdef _KIOSK_MANAGER_
	if (KioskManager::GetInstance()->GetNoSave())
		return FALSE;
# endif

	OP_ASSERT(frames_doc);
	OP_ASSERT(target);

# ifdef SVG_SUPPORT
# define IS_MATCHING_SVG_ELEMENT target->IsMatchingType(Markup::SVGE_SVG, NS_SVG)
# else
# define IS_MATCHING_SVG_ELEMENT FALSE
#endif // SVG_SUPPORT

	OP_ASSERT(target->IsMatchingType(HE_IMG, NS_HTML) ||
	          target->IsMatchingType(HE_EMBED, NS_HTML) ||
	          target->IsMatchingType(HE_OBJECT, NS_HTML) ||
	          target->IsMatchingType(HE_APPLET, NS_HTML) ||
	          IS_MATCHING_SVG_ELEMENT);

#undef IS_MATCHING_SVG_ELEMENT

	if (!(modifiers & SHIFTKEY_CTRL))
		return FALSE;

	URL image_url;

# ifdef SVG_SUPPORT
	if (target->IsMatchingType(Markup::SVGE_SVG, NS_SVG))
	{
		// SVG inside iframe ?
		if (frames_doc->GetParentDoc() &&
			target->ParentActual()->Type() == HE_DOC_ROOT)
		{
			image_url = frames_doc->GetURL();
		}
		else
			return FALSE;
	}
	else
# endif // SVG_SUPPORT
	{
		// If <img> is wrapped by link, expect also alt modifier.
		HTML_Element *parent = target;
		while ((parent = parent->ParentActual()) != NULL)
			if (parent->IsAnchorElement(frames_doc))
			{
				if (!(modifiers & SHIFTKEY_ALT))
					return FALSE;
				break;
			}

		image_url = target->GetImageURL(TRUE, frames_doc->GetLogicalDocument());

		HEListElm *hle = target->GetHEListElm(FALSE);
		BOOL is_valid_image = (hle && !hle->GetImage().IsFailed()) ||
# ifdef SVG_SUPPORT
			(!image_url.IsEmpty() && image_url.ContentType() == URL_SVG_CONTENT) ||
# endif
			(!image_url.IsEmpty() && image_url.GetAttribute(URL::KIsImage, TRUE));
		if (!is_valid_image)
			return FALSE;
	}

	WindowCommander *wic = frames_doc->GetWindow()->GetWindowCommander();

# ifdef WIC_USE_DOWNLOAD_CALLBACK
	ViewActionFlag view_action_flag;
	view_action_flag.Set(ViewActionFlag::SAVE_AS);
	TransferManagerDownloadCallback *download_callback = OP_NEW(TransferManagerDownloadCallback, (frames_doc->GetDocManager(), image_url, NULL, view_action_flag));
	if (download_callback)
	{
		wic->GetDocumentListener()->OnDownloadRequest(wic, download_callback);
		download_callback->Execute();
	}
	else
		frames_doc->GetWindow()->RaiseCondition(OpStatus::ERR_NO_MEMORY);
# else
	OpString str_image_url, suggested_name;
	OpString8 mime_type;
	OP_STATUS status;
	TRAP(status, image_url.GetAttributeL(URL::KSuggestedFileName_L, suggested_name));
	if (OpStatus::IsSuccess(status) &&
		OpStatus::IsSuccess(status = image_url.GetAttribute(URL::KUniName, str_image_url)) &&
		OpStatus::IsSuccess(status = image_url.GetAttribute(URL::KHTTP_ContentType, mime_type)))
	{
		wic->GetDocumentListener()->OnDownloadRequest(wic, str_image_url.CStr(), suggested_name.CStr(), mime_type.CStr(), image_url.GetContentSize(), OpDocumentListener::SAVE, NULL);
	}
	else if (OpStatus::IsMemoryError(status))
		frames_doc->GetWindow()->RaiseCondition(status);
# endif // WIC_USE_DOWNLOAD_CALLBACK

	return TRUE;
}
#endif // NO_SAVE_SUPPORT

BOOL
HTML_Element::IsWidgetWithCaret()
{
	if (IsMatchingType(Markup::HTE_INPUT, NS_HTML))
	{
		InputType input_type = GetInputType();
		return input_type == INPUT_TEXT ||
#ifdef WEBFORMS2_SUPPORT
		input_type == INPUT_NUMBER || input_type == INPUT_URI ||
		input_type == INPUT_TEL || input_type == INPUT_EMAIL ||
		input_type == INPUT_SEARCH || input_type == INPUT_TIME ||
#endif // WEBFORMS2_SUPPORT
		input_type == INPUT_PASSWORD;
	}

	return IsMatchingType(Markup::HTE_TEXTAREA, NS_HTML);
}

#ifdef DRAG_SUPPORT
static BOOL IsDateTimeWidget(HTML_Element* elm)
{
	if (elm->IsMatchingType(Markup::HTE_INPUT, NS_HTML))
	{
		InputType input_type = elm->GetInputType();
		return input_type == INPUT_DATETIME || input_type == INPUT_DATETIME_LOCAL;
	}

	return FALSE;
}

BOOL
HTML_Element::IsWidgetWithDraggableSelection()
{
	BOOL is_widget_with_caret = IsWidgetWithCaret();
	BOOL is_passwd_widget = (IsMatchingType(Markup::HTE_INPUT, NS_HTML) &&
	                        GetInputType() == INPUT_PASSWORD);

	return (is_widget_with_caret || IsDateTimeWidget(this)) && !is_passwd_widget;
}

BOOL
HTML_Element::IsWidgetAcceptingDrop()
{
	return IsWidgetWithCaret() || IsDateTimeWidget(this);
}
#endif // DRAG_SUPPORT

#if defined MEDIA_SUPPORT || defined MEDIA_HTML_SUPPORT
/**
 * Handles several events on a Media element.
 * @returns TRUE if the event produced side effects and therefore should be considered handled.
 */
static BOOL HandleMediaElementEvent(DOM_EventType event, FramesDocument* frames_doc,HTML_Element *current_target, const HTML_Element::HandleEventOptions& options)
{
	switch(event)
	{
	case ONMOUSEDOWN:
	case ONMOUSEUP:
	case ONMOUSEOVER:
	case ONMOUSEMOVE:
	case ONMOUSEOUT:
	{
		switch(current_target->Type())
		{
#ifdef MEDIA_SUPPORT
		case HE_EMBED:
		case HE_OBJECT:
		case HE_APPLET:
		{
			if (Media* media = current_target->GetMedia())
				media->OnMouseEvent(event, EXTRACT_MOUSE_BUTTON(options.sequence_count_and_button_or_key_or_delta), options.offset_x, options.offset_y);
			break;
		}
#endif // MEDIA_SUPPORT
#ifdef MEDIA_HTML_SUPPORT
		case HE_VIDEO:
		case HE_AUDIO:
		{
			if (MediaElement* media_element = current_target->GetMediaElement())
				media_element->OnMouseEvent(event, EXTRACT_MOUSE_BUTTON(options.sequence_count_and_button_or_key_or_delta), options.offset_x, options.offset_y);
			break;
		}
#endif // MEDIA_HTML_SUPPORT
		}
		break;
	}
#ifdef DOM_FULLSCREEN_MODE
	case ONDBLCLICK:
		if (options.cancelled || current_target->Type() != HE_VIDEO)
			break;
		// Fancy: make double click switch between fullscreen and non-fullscreen just like media players.
		if (frames_doc->GetFullscreenElement() == current_target)
		{
			frames_doc->DOMExitFullscreen(NULL);
			return TRUE;
		}
		else if (frames_doc->GetFullscreenElement() == NULL)
			return OpStatus::IsSuccess(frames_doc->DOMRequestFullscreen(current_target, NULL, TRUE));
		break;
#endif //DOM_FULLSCREEN_MODE
	}
	return FALSE;
}
#endif // defined MEDIA_SUPPORT || defined MEDIA_HTML_SUPPORT

BOOL
HTML_Element::HandleEvent(DOM_EventType event, FramesDocument* frames_doc, HTML_Element* target,
                          const HandleEventOptions& options, HTML_Element* imagemap /* = NULL */, HTML_Element** event_bubbling_path /* = NULL */, int event_bubbling_path_len /* = -1 */)
{
	HTML_Element* related_target = options.related_target;
	int offset_x = options.offset_x;
	int offset_y = options.offset_y;
	int document_x = options.document_x;
	int document_y = options.document_y;
	int sequence_count_and_button_or_key_or_delta = options.sequence_count_and_button_or_key_or_delta;
	ShiftKeyState modifiers = options.modifiers;
	BOOL cancelled = options.cancelled;
	BOOL synthetic = options.synthetic;
	ES_Thread* thread = options.thread;
#ifdef TOUCH_EVENTS_SUPPORT
	int radius = options.radius;
# ifdef DOC_RETURN_TOUCH_EVENT_TO_SENDER
	void* user_data = options.user_data;
# endif // DOC_RETURN_TOUCH_EVENT_TO_SENDER
#endif // TOUCH_EVENTS_SUPPORT
	unsigned int id = options.id;

	if (!frames_doc->GetHLDocProfile())
	{
		// This happens when an event is delayed for script processing
		// and the document is removed in the meantime. When the
		// script is removed, it will send the events that should have
		// been sent earlier.
		return FALSE;
	}

#ifdef DRAG_SUPPORT
	if ((event == ONDRAGSTART || event == ONDRAG || event == ONDRAGENTER || event == ONDRAGOVER ||
		event == ONDRAGLEAVE || event == ONDROP || event == ONDRAGEND) && !g_drag_manager->IsDragging())
	{
		// This happens when an event thread is blocked by e.g. an alert
		// and d'n'd has to be finished in a meanwhile.
		return FALSE;
	}
#endif // DRAG_SUPPORT

	if (synthetic &&
		(event == ONKEYDOWN || event == ONKEYUP || event == ONKEYPRESS ||
		 event == ONMOUSEDOWN || event == ONMOUSEUP || event == ONMOUSEMOVE ||
		 event == ONDBLCLICK ||
		 event == ONMOUSEOVER || event == ONMOUSEOUT ||
		 event == ONMOUSEENTER || event == ONMOUSELEAVE ||
		 event == ONMOUSEWHEELH || event == ONMOUSEWHEELV ||
#ifdef TOUCH_EVENTS_SUPPORT
		 event == TOUCHSTART || event == TOUCHMOVE || event == TOUCHEND || event == TOUCHCANCEL ||
#endif // TOUCH_EVENTS_SUPPORT
#ifdef DRAG_SUPPORT
		 event == ONDRAGSTART || event == ONDRAG || event == ONDRAGENTER || event == ONDRAGOVER ||
		 event == ONDRAGLEAVE || event == ONDROP || event == ONDRAGEND ||
#endif // DRAG_SUPPORT
#ifdef USE_OP_CLIPBOARD
		 event == ONCOPY || event == ONCUT || event == ONPASTE ||
#endif // USE_OP_CLIPBOARD
		 event == ONCONTEXTMENU))
	{
		return FALSE;
	}

	BOOL handled = FALSE;

	if (target == this)
	{
#ifdef DRAG_SUPPORT
		/* Let the drag manager know about mouse down/up because
		 cancelling ONMOUSEDOWN must prevent the drag from starting. */
		if (event == ONMOUSEDOWN)
			g_drag_manager->OnMouseDownDefaultAction(frames_doc, target, cancelled);
#endif // DRAG_SUPPORT

		frames_doc->HandleEventFinished(event, target);

		/* We may want to ignore 'cancelled'. */
		if (event == ONKEYDOWN || event == ONKEYUP || event == ONKEYPRESS)
		{
			OpKey::Code key = static_cast<OpKey::Code>(sequence_count_and_button_or_key_or_delta);

#ifdef OP_KEY_CONTEXT_MENU_ENABLED
			// Generate a context menu if the user pressed the context menu
			// key regardless of if scripts tried to concel the key event
			// or not.
			if (event == ONKEYPRESS && key == OP_KEY_CONTEXT_MENU)
			{
				int caret_doc_x;
				int caret_doc_y;
				int caret_offset_x;
				int caret_offset_y;

				GetCaretCoordinate(frames_doc, caret_doc_x, caret_doc_y, caret_offset_x, caret_offset_y);
				frames_doc->HandleMouseEvent(ONCONTEXTMENU,
					related_target, target, imagemap,
					caret_offset_x, caret_offset_y,
					caret_doc_x, caret_doc_y,
					modifiers,
					sequence_count_and_button_or_key_or_delta,
					NULL, FALSE, FALSE,
					NULL, NULL,
					TRUE);

				return TRUE;
			}
#endif // OP_KEY_CONTEXT_MENU_ENABLED

			// Reset the "block keypress event" flag if keypress/keyup is handled.
			if ((event == ONKEYPRESS || event == ONKEYUP) && frames_doc->GetWindow()->HasRecentlyCancelledKeyDown(key))
				frames_doc->GetWindow()->SetRecentlyCancelledKeyDown(OP_KEY_INVALID);

			if (!cancelled)
			{
				if (event == ONKEYDOWN || event == ONKEYUP || event == ONKEYPRESS)
				{
					// While scripts handled the key events, the keyboard focus might have moved
					// so we have to be careful sending the events to the right receiver.
					// This is both a security issue (redirecting key events to file input fields)
					// and a correctness issue (having enter ending up in a wand
					// dialog, closing it; see bug 278731)
					OpInputContext* document_context = frames_doc->GetVisualDevice();
					OpInputContext* current_context = g_input_manager->GetKeyboardInputContext();

					if (!current_context)
						return FALSE; // The focused element has disappeared so let us ignore the event.

					if (current_context != document_context &&
						!current_context->IsChildInputContextOf(document_context))
					{
						// Focus seemed to have moved far away, just ignore the event (not optimal)
						return FALSE;
					}
					const uni_char *key_value = reinterpret_cast<const uni_char *>(reinterpret_cast<INTPTR>(options.user_data));

					switch (event)
					{
					case ONKEYDOWN:
						g_input_manager->InvokeKeyDown(key, key_value, options.key_event_data, modifiers, FALSE, OpKey::LOCATION_STANDARD, FALSE);
						return TRUE;

					case ONKEYUP:
						g_input_manager->InvokeKeyUp(key, key_value, options.key_event_data, modifiers, FALSE, OpKey::LOCATION_STANDARD, FALSE);
						return TRUE;

					case ONKEYPRESS:
						g_input_manager->InvokeKeyPressed(key, key_value, modifiers, FALSE, FALSE);
						return TRUE;

					default:
						break;
					}
				}
			}
		}

#ifndef MOUSELESS
		if (!cancelled && (event == ONMOUSEWHEELH || event == ONMOUSEWHEELV))
		{
			BOOL vertical = event == ONMOUSEWHEELV;
			int& delta = sequence_count_and_button_or_key_or_delta;

			// The scroll command should go to the scrollable
			// thing that is under the mouse cursor
			OpInputContext* target_input_context = frames_doc->GetVisualDevice();

			// Se if we have a scrollable container as parent of the inner_box
			HTML_Element* h_elm = this;
			BOOL invoke_action = TRUE;

			while(h_elm && invoke_action)
			{
				if (h_elm->GetLayoutBox())
				{
					FormObject *form_object = h_elm->GetFormObject();
					if( form_object )
					{
						// Scrollable forms should stop bubbling the wheel up in the hierarchy, even if it didn't scroll now (might have hit the bottom/top).

						if (form_object->OnMouseWheel(OpPoint(document_x, document_y), delta, vertical))
							invoke_action = FALSE;
					}

					if (ScrollableArea* sc = h_elm->GetLayoutBox()->GetScrollable())
					{
						target_input_context = sc;
						break;
					}
				}
				h_elm = h_elm->ParentActual();
			}

			if (invoke_action)
			{
				g_input_manager->InvokeAction(delta < 0 ?
				                              (vertical ? OpInputAction::ACTION_SCROLL_UP : OpInputAction::ACTION_SCROLL_LEFT) :
				                              (vertical ? OpInputAction::ACTION_SCROLL_DOWN : OpInputAction::ACTION_SCROLL_RIGHT),
				                              op_abs(delta), 0, target_input_context, NULL, TRUE, OpInputAction::METHOD_MOUSE);
			}
		}
#endif // MOUSELESS

#ifdef TOUCH_EVENTS_SUPPORT
		if (event == TOUCHSTART || event == TOUCHMOVE || event == TOUCHEND || event == TOUCHCANCEL)
		{
#ifdef DOC_TOUCH_SMARTPHONE_COMPATIBILITY
			// CORE-37279: Safari/iPhone ignores preventdefault on text area and text input elements.
			if (FormManager::IsFormElement(this) && GetFormValue()->IsUserEditableText(this))
				cancelled = FALSE;
#endif // DOC_TOUCH_SMARTPHONE_COMPATIBILITY

			BOOL primary_touch = EXTRACT_MOUSE_BUTTON(sequence_count_and_button_or_key_or_delta) == 0;
			BOOL mouse_simulation_handled = false;
			BOOL want_click = EXTRACT_CLICK_INDICATOR(sequence_count_and_button_or_key_or_delta);

#ifdef DOC_RETURN_TOUCH_EVENT_TO_SENDER
			 mouse_simulation_handled = frames_doc->SignalTouchEventProcessed(user_data, cancelled);
#endif // DOC_RETURN_TOUCH_EVENT_TO_SENDER

			if (want_click && !cancelled && primary_touch && !mouse_simulation_handled && frames_doc->GetHtmlDocument())
				SimulateMouse(frames_doc->GetHtmlDocument(), event, document_x, document_y, radius, modifiers);
		}
#endif // TOUCH_EVENTS_SUPPORT

#ifdef DOCUMENT_EDIT_SUPPORT
		if (frames_doc->GetDocumentEdit())
		{
			OpDocumentEdit *doc_edit = frames_doc->GetDocumentEdit();
			if(event == ONMOUSEDOWN && doc_edit->m_layout_modifier.IsLayoutModifiable(this))
			{
				doc_edit->HandleMouseEvent(this, event, document_x, document_y, EXTRACT_MOUSE_BUTTON(sequence_count_and_button_or_key_or_delta));
			}
		}
#endif // DOCUMENT_EDIT_SUPPORT
	}
#ifdef DOCUMENT_EDIT_SUPPORT
	BOOL target_at_layout_modifier = frames_doc->GetDocumentEdit() && frames_doc->GetDocumentEdit()->m_layout_modifier.m_helm == target;
	if(target_at_layout_modifier
#ifdef DRAG_SUPPORT
	   && event != ONDRAGSTART && event != ONDRAG && event != ONDRAGENTER && event != ONDRAGOVER && event != ONDROP
#endif // DRAG_SUPPORT
	   )
		cancelled = TRUE;
#endif // DOCUMENT_EDIT_SUPPORT

	BOOL shift_pressed = (modifiers & SHIFTKEY_SHIFT) == SHIFTKEY_SHIFT;
	BOOL control_pressed = (modifiers & SHIFTKEY_CTRL) == SHIFTKEY_CTRL;

	BOOL is_link = FALSE;
	NS_Type ns = GetNsType();

#define MOUSEOVERURL_THREAD , thread
#define HANDLEEVENT_THREAD , thread

#ifdef ON_DEMAND_PLUGIN
	// Allow enabling of on-demand plugins even if event is cancelled
	if (!synthetic && layout_box && GetNsType() == NS_HTML
		&& (Type() == HE_OBJECT || Type() == HE_EMBED || Type() == HE_APPLET)
		&& IsPluginPlaceholder() && !GetNS4Plugin())
	{
		static_cast<PluginPlaceholderContent*>(layout_box->GetContent())->HandleMouseEvent(frames_doc, event);
	}
#endif // ON_DEMAND_PLUGIN

	/* It would be possible to do more sophisticated things with the 'cancelled'
	   and 'synthetic' flags.  Synthetic events might be a security risk. */
	switch (event)
	{
	case ONMOUSEDOWN:
		break;

	case ONMOUSEUP:
		/* Our only default action is to arm an element (mousedown) or fire a
		   click event (mouseup on the active element).  MSIE does nothing
		   special when mousedown/mouseup is cancelled, so if we do, click will
		   not work on some pages that mindlessly cancel all mousedown/mouseup
		   events. */
		if (target == this && !synthetic)
		{
			switch(EXTRACT_MOUSE_BUTTON(sequence_count_and_button_or_key_or_delta))
			{
#ifndef HAS_NOTEXTSELECTION
			case MOUSE_BUTTON_1: // left mouse button
				// Move the end point of the selection here, cancelled events should end the
				// selection anyway, but synthetic events shouldn't.
				{
#ifdef DRAG_SUPPORT
					BOOL is_selecting = frames_doc->GetSelectingText();
#endif // DRAG_SUPPORT
					frames_doc->EndSelectionIfSelecting(document_x, document_y);
#ifdef DRAG_SUPPORT
					// Don't clear the selection if we're clicking on something
					// that are likely to start a script (either via a javascript
					// url or via an event listener) that might ask for current
					// selection.
					HTML_Element* hovered = this;

					while (hovered && hovered->IsText())
					{
						hovered = hovered->ParentActual();
					}

					BOOL may_clear = !hovered ||
						!(hovered->Type() == Markup::HTE_A || hovered->Type() == Markup::HTE_IMG ||
						  FormManager::IsButton(hovered) ||
						  hovered->Type() == Markup::HTE_INPUT && hovered->GetInputType() == INPUT_IMAGE);
					int sequence_count = EXTRACT_SEQUENCE_COUNT(sequence_count_and_button_or_key_or_delta);
					HTML_Document* html_doc = frames_doc->GetHtmlDocument();
					if (may_clear && html_doc && html_doc->HasSelectedText() && this == frames_doc->GetActiveHTMLElement() &&
						frames_doc->GetMouseUpMayBeClick() && !frames_doc->GetWasSelectingText() &&
						sequence_count == 1 && !shift_pressed && !is_selecting)
						frames_doc->ClearDocumentSelection(FALSE, TRUE);
#endif // DRAG_SUPPORT
				}
				break;
#endif // HAS_NOTEXTSELECTION

			case MOUSE_BUTTON_2: // right mouse button
				{
					BOOL might_be_interpreted_as_click = EXTRACT_CLICK_INDICATOR(sequence_count_and_button_or_key_or_delta);

					if (might_be_interpreted_as_click)
					{
						if (frames_doc->HandleMouseEvent(ONCONTEXTMENU,
							related_target, target, imagemap,
							offset_x, offset_y,
							document_x, document_y,
							modifiers,
							sequence_count_and_button_or_key_or_delta) == OpStatus::ERR_NO_MEMORY)
						{
							frames_doc->GetWindow()->RaiseCondition(OpStatus::ERR_NO_MEMORY);
						}
					}
					break;
				}
			default:
				break;
			}
		}
		break;

	case ONCLICK:
		if (IsMatchingType(HE_INPUT, NS_HTML))
		{
			FormValue* value = GetFormValue();
			if (value->GetType() == FormValue::VALUE_RADIOCHECK)
			{
				FormValueRadioCheck* radiocheck = FormValueRadioCheck::GetAs(value);
				BOOL is_changed = radiocheck->RestoreStateAfterOnClick(this, cancelled);
				if (is_changed)
				{
					OP_ASSERT(!cancelled);
					OP_STATUS status = FormValueListener::HandleValueChanged(frames_doc, this, TRUE, !synthetic, thread);
					if (OpStatus::IsMemoryError(status))
						frames_doc->GetWindow()->RaiseCondition(OpStatus::ERR_NO_MEMORY);
				}
			}

			FormObject *form_object = GetFormObject();
			if (form_object && form_object->GetInputType() == INPUT_FILE && !cancelled)
				if ((thread && thread->IsUserRequested() && !thread->HasOpenedFileUploadDialog()) ||
					(!thread && !synthetic))
				{
					static_cast<FileUploadObject*>(form_object)->Click(thread);
					if (thread)
						thread->SetHasOpenedFileUploadDialog();
				}
		}
		if (cancelled)
			goto replay_mouse_actions;
		break;

	/* HandleOnDrag*() functions below take care of everything what is needed to be done on a drag'n'drop event e.g.
	the cursor change or moving the the next event (replaying next event). This is why we may return right after calling them. */
#ifdef DRAG_SUPPORT
	case ONDRAGSTART:
		g_drag_manager->HandleOnDragStartEvent(cancelled);
		return TRUE;

	case ONDRAGOVER:
		g_drag_manager->HandleOnDragOverEvent(frames_doc->GetHtmlDocument(), document_x, document_y, offset_x, offset_y, cancelled);
		return TRUE;

	case ONDRAGENTER:
		g_drag_manager->HandleOnDragEnterEvent(this, frames_doc->GetHtmlDocument(), document_x, document_y, offset_x, offset_y, modifiers, cancelled);
		return TRUE;

	case ONDRAG:
		g_drag_manager->HandleOnDragEvent(frames_doc->GetHtmlDocument(), document_x, document_y, offset_x, offset_y, modifiers, cancelled);
		return TRUE;

	case ONDROP:
		g_drag_manager->HandleOnDropEvent(frames_doc->GetHtmlDocument(), document_x, document_y, offset_x, offset_y, modifiers, cancelled);
		return TRUE;

	case ONDRAGLEAVE:
		g_drag_manager->HandleOnDragLeaveEvent(this, frames_doc->GetHtmlDocument());
		return TRUE;

	case ONDRAGEND:
		g_drag_manager->HandleOnDragEndEvent(frames_doc->GetHtmlDocument());
		return TRUE;
#endif // DRAG_SUPPORT
#ifdef USE_OP_CLIPBOARD
	case ONCUT:
	case ONCOPY:
	case ONPASTE:
		g_clipboard_manager->HandleClipboardEventDefaultAction(event, cancelled, id);
		return TRUE;
#endif // USE_OP_CLIPBOARD

	default:
		if (cancelled)
			goto replay_mouse_actions;
	}

	if (event == ONMOUSEOVER || event == ONCLICK || event == ONMOUSEMOVE)
	{
		URL* url = (URL *)GetSpecialAttr(ATTR_CSS_LINK, ITEM_TYPE_URL, (void*)NULL, SpecialNs::NS_LOGDOC);

		if (url && url->Type() != URL_JAVASCRIPT)
		{
			if (frames_doc->MouseOverURL(*url, NULL, event, shift_pressed, control_pressed MOUSEOVERURL_THREAD, this) == OpStatus::ERR_NO_MEMORY)
				frames_doc->GetWindow()->RaiseCondition(OpStatus::ERR_NO_MEMORY);

			is_link = TRUE;
			goto cursor_check;
		}
	}
#ifndef MOUSELESS
	else if (event == ONMOUSEUP)
	{
		HTML_Document* html_doc = frames_doc->GetHtmlDocument();
		FormObject* form_object = GetFormObject();
		MouseButton button = EXTRACT_MOUSE_BUTTON(sequence_count_and_button_or_key_or_delta);
		UINT8 nclicks = EXTRACT_SEQUENCE_COUNT(sequence_count_and_button_or_key_or_delta);
		if (html_doc->GetCapturedWidgetElement())
		{
			if (form_object)
			{
				// This will position cursors and activate checkboxes and radio buttons.
				form_object->OnMouseUp(OpPoint(document_x, document_y), button, nclicks);
			}
			html_doc->SetCapturedWidgetElement(NULL);
		}
		if (g_widget_globals->captured_widget)
		{
			OP_ASSERT(button == current_mouse_button);
			AffinePos widget_pos = g_widget_globals->captured_widget->GetPosInDocument();

			OpPoint point(document_x, document_y);
			widget_pos.ApplyInverse(point);

			g_widget_globals->captured_widget->GenerateOnMouseUp(point, button, nclicks);
		}
	}
#endif // MOUSELESS

	if (GetIsPseudoElement())
	{
		if (Parent())
			handled = Parent()->HandleEvent(event, frames_doc, target, options, imagemap, event_bubbling_path, event_bubbling_path_len);

		goto cursor_check;
	}

	if (target == this)
	{
		BOOL is_capturing_element = FALSE;
#ifdef SVG_SUPPORT
		BOOL is_svg_element = FALSE;
#endif // SVG_SUPPORT

		HTML_Element *form_element = this;

		/* Check if this element or any of its ancestors is a form
		   element.  The only relevant descendant of a form element is
		   OPTION and OPTGROUP. */
		while (form_element)
		{
#ifdef SVG_SUPPORT
			if (form_element->IsMatchingType(Markup::SVGE_SVG, NS_SVG))
			{
				// SVG rootcontainers are OpInputContexts, the focus has already been set on it
				is_svg_element = TRUE;
				break;
			}
			else
#endif // SVG_SUPPORT
				if (form_element->GetFormObject() &&
				(form_element->GetNsType() == NS_HTML &&
				 (form_element->Type() == HE_INPUT ||
				  form_element->Type() == HE_TEXTAREA ||
				  form_element->Type() == HE_SELECT
#ifdef _SSL_SUPPORT_
				  || form_element->Type() == HE_KEYGEN
#endif
					 )))
			{
				is_capturing_element = TRUE;
				break;
			}
			else
				form_element = form_element->Parent();
		}

		if (event == ONMOUSEDOWN)
		{
			HTML_Document *html_document = frames_doc->GetHtmlDocument();

			if (!cancelled)
			{
				BOOL set_focus_on_visdev = !is_capturing_element
#ifdef SVG_SUPPORT
										&& !is_svg_element
#endif // SVG_SUPPORT
#ifdef _PLUGIN_SUPPORT_
										&& GetNS4Plugin() == 0
#endif // _PLUGIN_SUPPORT_
										;

				if (!target->IsMatchingType(HE_BUTTON, NS_HTML))
					frames_doc->GetVisualDevice()->SetDrawFocusRect(FALSE);

				BOOL is_editable_root = FALSE;
#ifdef DOCUMENT_EDIT_SUPPORT
				if (frames_doc->GetDocumentEdit() && frames_doc->GetDocumentEdit()->GetEditableContainer(target))
					is_editable_root = TRUE;
#endif // DOCUMENT_EDIT_SUPPORT
				if (target->IsMatchingType(HE_BUTTON, NS_HTML) && !target->IsDisabled(frames_doc) && !target->GetUnselectable() && !is_editable_root)
				{
					if (html_document && frames_doc->GetDocRoot()->IsAncestorOf(target))
						html_document->FocusElement(target, HTML_Document::FOCUS_ORIGIN_MOUSE, FALSE);
				}
				else if (is_capturing_element && !is_editable_root)
				{
					// These handle their own focus
				}
				else if (target->IsMatchingType(HE_OPTION, NS_HTML) && !is_editable_root)
				{
					 // HE_OPTION mousedowns are generated by the form code and we must not interfere with dropdown focus
				}
				else
				{
					// Set the real inputmanager focus to the body, but also send a
					// fake focus event to certain elements that expect such and
					// record them in HTML_Document as the focused element. This
					// includes HE_A, HE_AREA and HE_IMG.
					HTML_Element* focus_blur_element = GetFocusBlurElement(frames_doc, this);
					if (html_document && html_document->GetFocusedElement() != focus_blur_element)
					{
						html_document->SetFocusedElement(focus_blur_element, FALSE);
						if (focus_blur_element)
						{
							frames_doc->HandleEvent(ONFOCUS,
													html_document->GetHoverReferencingHTMLElement(),
													focus_blur_element,
													modifiers);
						}
					}

					FOCUS_REASON focus_reason = synthetic ? FOCUS_REASON_OTHER : FOCUS_REASON_MOUSE;
#ifdef DOCUMENT_EDIT_SUPPORT
					HTML_Element* docedit_elm_to_focus = NULL;
					if (OpDocumentEdit* docedit = frames_doc->GetDocumentEdit())
					{
						if (docedit->GetEditableContainer(target))
							docedit_elm_to_focus = target;
						else if (LogicalDocument* logdoc = frames_doc->GetLogicalDocument())
						{
							// Let clicks outside the <body> also focus <body> if <body> is editable since
							// anything else would be confusing for the user.
							HTML_Element* body = logdoc->GetBodyElm();
							if (body && target->IsAncestorOf(body) && docedit->GetEditableContainer(body))
								docedit_elm_to_focus = body;
						}
					}

					if (docedit_elm_to_focus)
						frames_doc->GetDocumentEdit()->FocusEditableRoot(docedit_elm_to_focus, focus_reason);
					else
#endif // DOCUMENT_EDIT_SUPPORT
					if (set_focus_on_visdev)
					{
						if (OpInputContext* input_context = Content::FindInputContext(target))
							input_context->SetFocus(focus_reason);
						else
							frames_doc->GetVisualDevice()->SetFocus(focus_reason);
					}
				}

				// Remove the default-look from the defaultbutton.
				FormObject::ResetDefaultButton(frames_doc);
			} // !cancelled

			if (is_capturing_element)
			{
				FormObject* form_object = form_element->GetFormObject();
				// The mouse down event used to be sent from the
				// widget to the HE_OPTION for listboxes and
				// this was to prevent infinite event loops. Now
				// that only happens for listboxes inside dropdowns
				// where the mouse event never went through
				// the document initially.
				if (!(form_element->IsMatchingType(HE_SELECT, NS_HTML) &&
					  form_object &&
					  !static_cast<SelectionObject*>(form_object)->IsListbox() &&
					  target->IsMatchingType(HE_OPTION, NS_HTML)))
				{
					html_document->SetCapturedWidgetElement(form_element);
#ifndef MOUSELESS
					unsigned sequence_count = EXTRACT_SEQUENCE_COUNT(sequence_count_and_button_or_key_or_delta);
					OP_ASSERT(sequence_count >= 1);
					MouseButton button = EXTRACT_MOUSE_BUTTON(sequence_count_and_button_or_key_or_delta);
					form_object->OnMouseDown(OpPoint(document_x, document_y), button, sequence_count, cancelled);
#endif // !MOUSELESS
				}
			}
		}
		else if (event == ONCONTEXTMENU)
			frames_doc->InitiateContextMenu(this, offset_x, offset_y, document_x, document_y, options.has_keyboard_origin);

		if (layout_box)
			layout_box->HandleEvent(frames_doc, event, offset_x, offset_y);
	}

	if (ns == NS_HTML)
	{
		HTML_ElementType type = Type();

        switch (type)
        {
#ifdef _PLUGIN_SUPPORT_
		case HE_EMBED:
		case HE_OBJECT:
		case HE_APPLET:
		{
			OpNS4Plugin* plugin = GetNS4Plugin();

			if (!plugin)
			{
				// No plug-in object; divert to layout module for handling of alternate content.
				if (layout_box && layout_box->IsContentEmbed())
					((EmbedContent*) layout_box->GetContent())->HandleMouseEvent(frames_doc->GetWindow(), event);

#ifdef MEDIA_SUPPORT
				handled = HandleMediaElementEvent(event, frames_doc, this, options);
				if (handled)
					goto cursor_check;
#endif //MEDIA_SUPPORT
#ifndef NO_SAVE_SUPPORT
				if (!cancelled && event == ONCLICK)
				{
					handled = HandleSaveImage(modifiers, frames_doc, this);
					if (handled)
						goto cursor_check;
				}
#endif // NO_SAVE_SUPPORT

				break;
			}
# ifdef _WINDOWLESS_PLUGIN_SUPPORT_
			VisualDevice* vd = frames_doc->GetVisualDevice();

			if (plugin->IsWindowless())
			{
				// The coordinate for handleevent should be in raw pixels relative to the OpView.
				OpPoint point(document_x - vd->GetRenderingViewX(), document_y - vd->GetRenderingViewY());

				point = vd->ScaleToScreen(point);
				int button_or_key_or_delta = sequence_count_and_button_or_key_or_delta;
				if (event == ONMOUSEUP || event == ONMOUSEDOWN || event == ONCLICK || event == ONDBLCLICK)
				{
					button_or_key_or_delta = EXTRACT_MOUSE_BUTTON(sequence_count_and_button_or_key_or_delta);
				}
				plugin->HandleEvent(event, point, button_or_key_or_delta, modifiers);

				/* Document might be collapsing under our feet if it was deleted before
				   event returned from plugin (when context menu blocked it for example).
				   We will try to escape immediately to prevent further code from running. */
				if (frames_doc->IsBeingDeleted())
					return TRUE;
			}

#ifdef VEGA_OPPAINTER_SUPPORT
			if (vd->GetOpView())
				((MDE_OpView*)vd->GetOpView())->GetMDEWidget()->m_screen->ReleaseMouseCapture();
#endif // VEGA_OPPAINTER_SUPPORT
# endif // _WINDOWLESS_PLUGIN_SUPPORT_
			break;
		}
#endif // _PLUGIN_SUPPORT_

		case HE_AREA:
			switch (event)
			{
			case ONMOUSEOVER:
			case ONMOUSEMOVE:
			case ONCLICK:
				{
					/* Pretty ugly hack to not load a new page if the current document
					   has been replaced using document.write. (jl@opera.com) */
					if (event == ONCLICK && frames_doc->GetESScheduler() && !frames_doc->GetESScheduler()->TestTerminatingAction(ES_OpenURLAction::FINAL, ES_OpenURLAction::CONDITIONAL))
						goto cursor_check;

					URL* url = GetAREA_URL(frames_doc->GetLogicalDocument());

					if (!url)
						break;
					else
					{
						const uni_char* win_name = GetAREA_Target();

						if (!win_name || !*win_name)
							win_name = GetCurrentBaseTarget(this);

						if (frames_doc->MouseOverURL(*url, win_name, event, shift_pressed, control_pressed MOUSEOVERURL_THREAD, this) == OpStatus::ERR_NO_MEMORY)
							frames_doc->GetWindow()->RaiseCondition(OpStatus::ERR_NO_MEMORY);

						is_link = TRUE;
					}

					goto cursor_check;
				}

#ifdef WIC_MIDCLICK_SUPPORT
			case ONMOUSEDOWN:
			case ONMOUSEUP:
				{
					MouseButton button = EXTRACT_MOUSE_BUTTON(sequence_count_and_button_or_key_or_delta);
					if (button == MOUSE_BUTTON_3)
					{
						if (URL *url = GetAREA_URL(frames_doc->GetLogicalDocument()))
							frames_doc->HandleMidClick(this, offset_x, offset_y, document_x, document_y, modifiers, event == ONMOUSEDOWN);

						goto cursor_check;
					}
				}
				break;
#endif // WIC_MIDCLICK_SUPPORT

			default:
				break;
			}
			break;

		case HE_A:
			switch (event)
			{
			case ONMOUSEOVER:
			case ONMOUSEMOVE:
			case ONCLICK:
				{
					/* Pretty ugly hack to not load a new page if the current document
					   has been replaced using document.write. (jl@opera.com) */
					if (event == ONCLICK && frames_doc->GetESScheduler() && !frames_doc->GetESScheduler()->TestTerminatingAction(ES_OpenURLAction::FINAL, ES_OpenURLAction::CONDITIONAL))
						goto cursor_check;

					URL url = GetAnchor_URL(frames_doc); // will substitute if it's a WML docuemnt

					if (url.IsEmpty())
						break;

#ifdef DOCUMENT_EDIT_SUPPORT
					if (event == ONCLICK && frames_doc->GetDocumentEdit() && frames_doc->GetDocumentEdit()->GetEditableContainer(this))
						break;
#endif // DOCUMENT_EDIT_SUPPORT

					is_link = TRUE;
#ifdef WIC_USE_ANCHOR_SPECIAL
					// Let the platform code abort user link clicks, but don't tell them
					// about synthetic clicks since that would allow scripts to trigger
					// ui actions.
					if (!synthetic && event == ONCLICK)
					{
						const uni_char *rel_str = GetStringAttr(ATTR_REL);
						const uni_char *ttl_str = GetStringAttr(ATTR_TITLE);
						OpString urlnamerel_str;
						OpString urlname_str;
						OP_STATUS set_stat;

						do
						{
							set_stat = url.GetAttribute(URL::KUniName_With_Fragment, urlnamerel_str, TRUE);
							if (OpStatus::IsMemoryError(set_stat))
								break;

							set_stat = url.GetAttribute(URL::KUniName, urlname_str, TRUE);
							if (OpStatus::IsMemoryError(set_stat))
								break;
						}
						while (0);

						if (set_stat != OpStatus::OK)
						{
							frames_doc->GetWindow()->RaiseCondition(set_stat);
							goto cursor_check;
						}

						const OpDocumentListener::AnchorSpecialInfo anchor_info(rel_str, ttl_str, urlnamerel_str.CStr(), urlname_str.CStr());
						if (WindowCommander *wc = frames_doc->GetWindow()->GetWindowCommander())
						{
							if (wc->GetDocumentListener()->OnAnchorSpecial(wc, anchor_info))
								goto cursor_check;
						}
					}
#endif // !WIC_USE_ANCHOR_SPECIAL
#ifdef IMODE_EXTENSIONS
					// utn support
					const uni_char *attr_value = GetAttrValue(UNI_L("UTN"));
					if( attr_value && uni_stri_eq(attr_value, "TRUE"))
						url.SetUseUtn(TRUE);
#endif //IMODE_EXTENSIONS

					const uni_char* win_name = GetA_Target();
					if (!win_name || !*win_name)
						win_name = GetCurrentBaseTarget(this);

#ifdef _WML_SUPPORT_
					if (event == ONCLICK && frames_doc->GetDocManager())
					{
						if (WML_Context *wc = frames_doc->GetDocManager()->WMLGetContext())
							wc->SetStatusOn(WS_GO);
					}
#endif // _WML_SUPPORT_

					if (frames_doc->MouseOverURL(url, win_name, event, shift_pressed, control_pressed MOUSEOVERURL_THREAD, this) == OpStatus::ERR_NO_MEMORY)
						frames_doc->GetWindow()->RaiseCondition(OpStatus::ERR_NO_MEMORY);

					goto cursor_check;
				}

#ifdef WIC_MIDCLICK_SUPPORT
			case ONMOUSEDOWN:
			case ONMOUSEUP:
				{
					MouseButton button = EXTRACT_MOUSE_BUTTON(sequence_count_and_button_or_key_or_delta);
					if (button == MOUSE_BUTTON_3)
					{
						URL url(GetAnchor_URL(frames_doc));

						if (!url.IsEmpty())
							frames_doc->HandleMidClick(this, offset_x, offset_y, document_x, document_y, modifiers, event == ONMOUSEDOWN);

						goto cursor_check;
					}
				}
				break;
#endif // WIC_MIDCLICK_SUPPORT

			default:
				break;
			}
			break;

		case HE_IMG:
#ifndef MOUSELESS
			if (IsMap())
			{
				switch (event)
				{
				case ONMOUSEOVER:
				case ONCLICK:
				case ONMOUSEMOVE:
#ifdef WIC_MIDCLICK_SUPPORT
				case ONMOUSEDOWN:
				case ONMOUSEUP:
#endif // WIC_MIDCLICK_SUPPORT
					{
						HTML_Element* a_elm = GetA_Tag();

						if (a_elm)
						{
							/* Pretty ugly hack to not load a new page if the current document
							   has been replaced using document.write. (jl@opera.com) */
							if (event == ONCLICK && frames_doc->GetESScheduler() && !frames_doc->GetESScheduler()->TestTerminatingAction(ES_OpenURLAction::FINAL, ES_OpenURLAction::CONDITIONAL))
								goto cursor_check;

							const uni_char* win_name = a_elm->GetA_Target();

							if (!win_name || !*win_name)
								win_name = GetCurrentBaseTarget(a_elm);

							const uni_char* href = a_elm->GetA_HRef(frames_doc, TRUE);

							if (!href)
								goto cursor_check;

							int count = uni_strlen(href);

							uni_char *buffer = OP_NEWA(uni_char, count + sizeof(long) * 3 + sizeof(int) * 3 + 5);
							if (!buffer)
							{
								frames_doc->GetWindow()->RaiseCondition(OpStatus::ERR_NO_MEMORY);
								goto cursor_check;
							}

							uni_sprintf(buffer, UNI_L("%s?%d,%ld"), href, offset_x, offset_y);

							URL url(ResolveUrl(buffer, frames_doc->GetLogicalDocument(), ATTR_HREF));

							OP_DELETEA(buffer);

#ifdef WIC_MIDCLICK_SUPPORT
							MouseButton button = EXTRACT_MOUSE_BUTTON(sequence_count_and_button_or_key_or_delta);
							if (button == MOUSE_BUTTON_3
								&& (event == ONMOUSEDOWN || event == ONMOUSEUP)
								&& !url.IsEmpty())
							{
								frames_doc->HandleMidClick(this, offset_x, offset_y, document_x, document_y, modifiers, event == ONMOUSEDOWN);
							}
							else
#endif // WIC_MIDCLICK_SUPPORT

							if (frames_doc->MouseOverURL(url, win_name, event == ONMOUSEMOVE ? ONMOUSEOVER : event, shift_pressed, control_pressed MOUSEOVERURL_THREAD, this) == OpStatus::ERR_NO_MEMORY)
								frames_doc->GetWindow()->RaiseCondition(OpStatus::ERR_NO_MEMORY);

							is_link = TRUE;
							if (event == ONCLICK)
								goto cursor_check;
						}
					}
					break;

				default:
					break;
				}
			}
#ifndef NO_SAVE_SUPPORT
			else // HE_IMG
			{
				if (!cancelled && event == ONCLICK)
				{
					handled = HandleSaveImage(modifiers, frames_doc, this);
					if (handled)
						goto cursor_check;
				}
			}
#endif // NO_SAVE_SUPPORT
#endif // !MOUSELESS

			break;

		case HE_BUTTON:
		case HE_INPUT:
		case HE_TEXTAREA:
			{
#ifdef _X11_SELECTION_POLICY_
				if (event == ONMOUSEDOWN || event == ONMOUSEUP)
				{
					MouseButton button = EXTRACT_MOUSE_BUTTON(sequence_count_and_button_or_key_or_delta);
					if (button == MOUSE_BUTTON_3)
						goto cursor_check;
				}
#endif // _X11_SELECTION_POLICY_

				if (event == ONFORMCHANGE || event == ONFORMINPUT)
				{
					// This is an event that is sent to all elements in the form and it
					// has no default action so we don't want to do anything at all when it
					// happens. Not even call FindFormElm (that is O(n) which makes
					// OnFormChange O(n^2) if we call it).
					break;
				}
				if (event == ONCLICK && IsDisabled(frames_doc))
				{
					break;
				}
#ifdef DOCUMENT_EDIT_SUPPORT
				if (event == ONCLICK && frames_doc->GetDocumentEdit() && frames_doc->GetDocumentEdit()->GetEditableContainer(this))
					break;
#endif // DOCUMENT_EDIT_SUPPORT

				HTML_Element* form_element = FormManager::FindFormElm(frames_doc, this);
				if (form_element)
				{
					InputType inp_type = GetInputType();

					switch (inp_type)
					{
					case INPUT_SUBMIT:
					case INPUT_IMAGE:
						switch (event)
						{
						case ONCLICK:
							{
								OP_STATUS status = FormManager::HandleSubmitButtonClick(frames_doc, form_element,
									this, offset_x, offset_y, document_x, document_y,
									sequence_count_and_button_or_key_or_delta, modifiers, thread);
								if (OpStatus::IsMemoryError(status))
									frames_doc->GetWindow()->RaiseCondition(OpStatus::ERR_NO_MEMORY);
							}
							goto cursor_check;

#ifndef MOUSELESS
						case ONMOUSEOVER:
						case ONMOUSEMOVE:
							{
								URL& url = frames_doc->GetURL();
								URL moved_url = url.GetAttribute(URL::KMovedToURL, TRUE);

								Form form(!moved_url.IsEmpty() ? moved_url : url, form_element, target, offset_x, offset_y);

								URL form_url;
								OP_STATUS status = form.GetDisplayURL(form_url, frames_doc);
								if (status == OpStatus::ERR_NO_MEMORY)
									frames_doc->GetWindow()->RaiseCondition(OpStatus::ERR_NO_MEMORY);
								else
								{
									if (frames_doc->MouseOverURL(form_url, NULL, event, shift_pressed, control_pressed MOUSEOVERURL_THREAD, this) == OpStatus::ERR_NO_MEMORY)
										frames_doc->GetHLDocProfile()->SetIsOutOfMemory(TRUE);
									if (GetInputType() != INPUT_SUBMIT)
										is_link = TRUE;
								}

								goto cursor_check;
							}
#endif // !MOUSELESS
						default:
							break;
						}
						break;

					case INPUT_RESET:
						if (event == ONCLICK)
						{
							if (frames_doc->HandleEvent(ONRESET, this, form_element, modifiers) == OpStatus::ERR_NO_MEMORY)
							{
								frames_doc->GetWindow()->RaiseCondition(OpStatus::ERR_NO_MEMORY);
							}
						}
						break;

						// XXX Shouldn't INPUT_FILE be included in this?
					case INPUT_PASSWORD:
					case INPUT_TEXT:
					case INPUT_URI:
					case INPUT_DATE:
					case INPUT_WEEK:
					case INPUT_TIME:
					case INPUT_EMAIL:
					case INPUT_NUMBER:
					case INPUT_RANGE:
					case INPUT_MONTH:
					case INPUT_DATETIME:
					case INPUT_DATETIME_LOCAL:
					case INPUT_COLOR:
					case INPUT_TEL:
					case INPUT_SEARCH:
						break;

					default:
						break;
					}
					if (event == ONCHANGE || event == ONINPUT)
					{
						// The default action of a change event should be to
						// send the formchange event to all elements in the form

						// The default action of a input event should be to
						// send the forminput event to all elements in the form
						// XXX The OnINPUT event?
						if (form_element->Type() == HE_FORM) // Not for HE_ISINDEX
						{
							// Don't send formchange/forminput for changes in an output element
							if (!target || target->Type() != HE_OUTPUT)
							{
								FormValidator form_validator(frames_doc); // XXX Split the FormValidator class
								if (event == ONCHANGE)
								{
									form_validator.DispatchFormChange(form_element);
								}
								else
								{
									OP_ASSERT(event == ONINPUT);
									form_validator.DispatchFormInput(form_element);
								}
							}
						}
					}
				}

				if (event == ONINVALID)
				{
					// The first oninvalid message for a form submission
					// causes an error message to be displayed
					FormValidator form_validator(frames_doc);
					form_validator.MaybeDisplayErrorMessage(this);
				}
			}

			break;

#ifdef _WML_SUPPORT_

		case HE_SELECT:
		case HE_OPTION:

#ifdef _X11_SELECTION_POLICY_
			if (type == HE_SELECT && (event == ONMOUSEDOWN || event == ONMOUSEUP))
			{
				MouseButton button = EXTRACT_MOUSE_BUTTON(sequence_count_and_button_or_key_or_delta);
				if (button == MOUSE_BUTTON_3)
					goto cursor_check;
			}
#endif // _X11_SELECTION_POLICY_
			if (type == HE_SELECT && event == ONCHANGE ||
				type == HE_OPTION && event == ONCLICK)
			{
				if (frames_doc && frames_doc->GetDocManager() && frames_doc->GetDocManager()->WMLHasWML())
				{
					BOOL user_initiated = !synthetic;
					if (thread)
						user_initiated = thread->GetInfo().is_user_requested;
					if (type == HE_SELECT)
						DoWmlSelection(this, NULL, frames_doc, user_initiated);
					else
					{
						OP_ASSERT(type == HE_OPTION);
						DoWmlSelection(NULL, this, frames_doc, user_initiated);
					}
				}
			}
#endif // _WML_SUPPORT_
			if (event == ONINVALID)
			{
				// The first oninvalid message for a form submission
				// causes an error message to be displayed
				FormValidator form_validator(frames_doc);
				form_validator.MaybeDisplayErrorMessage(this);
			}
			break;

		case HE_ISINDEX:
		case HE_FORM:
			if (event == ONSUBMIT || event == ONRESET)
			{
				OP_STATUS status;
				if (event == ONRESET)
				{
					status = FormManager::ResetForm(frames_doc, this);
				}
				else // event == ONSUBMIT
				{
					ES_Thread* submit_thread = synthetic ? thread : NULL;
					status = FormManager::SubmitForm(frames_doc, this, related_target,
						offset_x, offset_y, submit_thread, synthetic, modifiers);
					if (OpStatus::IsSuccess(status))
					{
						is_link = TRUE;
					}
				}
				if (OpStatus::IsMemoryError(status))
					frames_doc->GetWindow()->RaiseCondition(OpStatus::ERR_NO_MEMORY);
				goto cursor_check;
			}
			break;

		case HE_LABEL:
			// Clicks on labels should simulate a click on the form element for simple form controls.
			if (event == ONCLICK &&
				!target->IsMatchingType(HE_OPTION, NS_HTML) && !target->IsFormElement())
			{
				HTML_Element *form_elm;
				const uni_char *for_id = GetAttrValue(ATTR_FOR);
				if (for_id)
				{
					// find the form element this label belongs to (or the root node if not inside a form)
					form_elm = this;
					HTML_Element* parent = ParentActual();
					while (parent && !form_elm->IsMatchingType(HE_FORM, NS_HTML))
					{
						form_elm = parent;
						parent = parent->ParentActual();
					}

					form_elm = form_elm->GetElmById(for_id);
				}
				else
					form_elm = FindFirstContainedFormElm();

				if (form_elm && form_elm->IsFormElement())
				{
					if (form_elm->Type() == HE_INPUT || form_elm->Type() == HE_BUTTON || form_elm->Type() == HE_TEXTAREA || form_elm->Type() == HE_SELECT)
					{
						// A click on a label will act as an alternate way to click in the form element
						if (frames_doc->GetHtmlDocument())
							frames_doc->GetHtmlDocument()->ScrollToElement(form_elm, SCROLL_ALIGN_CENTER, FALSE, VIEWPORT_CHANGE_REASON_FORM_FOCUS);

						FormObject* form_object = form_elm->GetFormObject();
						if (form_object)
						{
							form_object->SetFocus(synthetic ? FOCUS_REASON_OTHER : FOCUS_REASON_MOUSE);
							form_object->Click(thread);
						}
						else if (!form_elm->IsDisabled(frames_doc))
						{
							FormValue* form_value = form_elm->GetFormValue();
							FormValueRadioCheck* radiocheck = NULL;
							if (form_value->GetType() == FormValue::VALUE_RADIOCHECK)
							{
								// A hidden checkbox/radio button.
								radiocheck = FormValueRadioCheck::GetAs(form_value);
								radiocheck->SaveStateBeforeOnClick(form_elm);
							}

							// These have no FormObjects (could be because of no layout - display: none style).
							frames_doc->HandleMouseEvent(ONCLICK,
							                             NULL, form_elm, NULL,
							                             0, 0,       // x and y relative to the element
							                             0, 0, // x and y relative to the document
							                             0, // keystate
							                             MAKE_SEQUENCE_COUNT_AND_BUTTON(1, MOUSE_BUTTON_1),
							                             thread);

							// Generated click event can't toggle checkboxes that have no layout.
							if (radiocheck)
							{
								BOOL new_state;
								if (form_elm->GetInputType() == INPUT_CHECKBOX)
									new_state = !radiocheck->IsChecked(form_elm);
								else
									new_state = TRUE;

								radiocheck->SetIsChecked(form_elm, new_state, frames_doc, TRUE, thread);

								FormValueListener::HandleValueChanged(frames_doc, form_elm, !synthetic, !synthetic, thread);
							}
						}
					}
				}
			}
			break;

#ifdef MEDIA_HTML_SUPPORT
		case HE_AUDIO:
		case HE_VIDEO:
			handled = HandleMediaElementEvent(event, frames_doc, this, options);
			if (handled)
				goto cursor_check;
			break;
#endif // MEDIA_HTML_SUPPORT

		default:
			break;
		}
	}
#ifdef M2_SUPPORT
	else if (ns == NS_OMF)
	{
		HTML_ElementType type = Type();

		switch (type)
		{
		case OE_ITEM:
		case OE_SHOWHEADERS:
			if (event == ONMOUSEOVER || event == ONCLICK)
			{
				URL* url = (URL*)GetSpecialAttr(ATTR_CSS_LINK, ITEM_TYPE_URL, NULL, SpecialNs::NS_LOGDOC);

				if (url && url->Type() != URL_JAVASCRIPT)
				{
					if (frames_doc->MouseOverURL(*url, NULL, event, shift_pressed, control_pressed MOUSEOVERURL_THREAD, this) == OpStatus::ERR_NO_MEMORY)
						frames_doc->GetWindow()->RaiseCondition(OpStatus::ERR_NO_MEMORY);

					is_link = TRUE;
					goto cursor_check;
				}
			}
			break;
		}
	}
#endif // M2_SUPPORT

#ifdef _WML_SUPPORT_
	else if (ns == NS_WML)
	{
		WML_ElementType type = (WML_ElementType) Type();
		WML_Context *wc = frames_doc->GetDocManager()->WMLGetContext();

		OP_ASSERT( wc ); // could we get here without a context? /pw
		if (!wc)
			goto cursor_check;

		switch(type)
		{
		case WE_ANCHOR:
		case WE_DO:
			{
				WMLNewTaskElm *wml_task = wc->GetTaskByElement(this);

				switch (event)
				{
				case ONCLICK:
					{
						// setting the temporary task if a link is clicked
						if (wc->SetActiveTask(wml_task) == OpStatus::ERR_NO_MEMORY)
							frames_doc->GetWindow()->RaiseCondition(OpStatus::ERR_NO_MEMORY);
					}
					// this one should fall through
				case ONMOUSEMOVE:
				case ONMOUSEOVER:
					{
						UINT32 action = 0;
						URL url = wc->GetWmlUrl(wml_task ? wml_task->GetHElm() : this, &action);

						if (url.IsEmpty())
							break;

						is_link = TRUE;

						if (event == ONCLICK)
						{
							if ((action & WS_GO) != 0)
							{
								// Validate the form a final time before allowing
								// the submit so that we catch fields that doesn't
								// validate even before a user accesses them.
								if (!FormManager::ValidateWMLForm(frames_doc))
									break;
							}

							if (action & WS_GO)
								wc->SetStatusOn(action);
							else if (action & WS_REFRESH)
							{
								wc->SetStatusOn(action);
								frames_doc->GetDocManager()
									->GetMessageHandler()
									->PostMessage(MSG_WML_REFRESH, frames_doc->GetURL().Id(TRUE), TRUE);
								goto cursor_check;
							}
							else if (action & WS_ENTERBACK)
							{
								wc->SetStatusOn(WS_ENTERBACK);
								frames_doc->GetWindow()
									->GetMessageHandler()
									->PostMessage(MSG_HISTORY_BACK, frames_doc->GetURL().Id(TRUE), 0);
								goto cursor_check;
							}
						}

						if (frames_doc->MouseOverURL(url, NULL, event, shift_pressed, control_pressed MOUSEOVERURL_THREAD, this) == OpStatus::ERR_NO_MEMORY)
							frames_doc->GetWindow()->RaiseCondition(OpStatus::ERR_NO_MEMORY);

						goto cursor_check;
					}

				default:
					break;
				}
			}
			break;
		}
	}
#endif // _WML_SUPPORT_

#ifdef SVG_SUPPORT
	else if (ns == NS_SVG)
	{
#ifndef NO_SAVE_SUPPORT
		if (!cancelled && event == ONCLICK && IsMatchingType(Markup::SVGE_SVG, NS_SVG))
		{
			handled = HandleSaveImage(modifiers, frames_doc, this);
			if (handled)
				goto cursor_check;
		}
#endif // NO_SAVE_SUPPORT

		if (event == ONCLICK || event == ONMOUSEOVER || event == ONMOUSEMOVE)
		{
			URL *url = NULL;
			const uni_char* window_target = NULL;
			OP_BOOLEAN status = g_svg_manager->NavigateToElement(this, frames_doc, &url, event, event == ONCLICK ? &window_target : NULL);
			if (status == OpBoolean::IS_TRUE && url != NULL)
			{
				if (!url->IsEmpty())
				{
					is_link = TRUE;

					if (frames_doc->MouseOverURL(*url, window_target, event, shift_pressed, control_pressed MOUSEOVERURL_THREAD, this) == OpStatus::ERR_NO_MEMORY)
						frames_doc->GetWindow()->RaiseCondition(OpStatus::ERR_NO_MEMORY);
					goto cursor_check;
				}
			}

			if (OpStatus::IsMemoryError(status))
				frames_doc->GetWindow()->RaiseCondition(status);
		}
	}
#endif // SVG_SUPPORT

	HTML_Element* parent;

	if (IsMatchingType(HE_MAP, NS_HTML) && imagemap)
	{
		// When parent is a map and an imagemap is supplied, event
		// propagation is redirected to the imagemap.
		parent = imagemap;
		imagemap = NULL;
	}
	else
	{
		// path len == -1 - means 'find it yourself'
		if (event_bubbling_path_len == -1)
			parent = ParentActualStyle();
		else if (event_bubbling_path_len > 0) // go to the next element
		{
			parent = *event_bubbling_path;
			++event_bubbling_path;
			--event_bubbling_path_len;
		}
		else // the path ended
			parent = NULL;
	}

	if (parent)
		handled = parent->HandleEvent(event, frames_doc, target, options, imagemap, event_bubbling_path, event_bubbling_path_len);

#ifndef MOUSELESS
	else
	{
		MouseButton button = EXTRACT_MOUSE_BUTTON(sequence_count_and_button_or_key_or_delta);
		if (button == MOUSE_BUTTON_3)
		{
#ifdef WIC_MIDCLICK_SUPPORT
			if (event == ONMOUSEDOWN || event == ONMOUSEUP)
			{
				URL url;
				frames_doc->HandleMidClick(this, offset_x, offset_y, document_x, document_y, modifiers, event == ONMOUSEDOWN);
			}
#endif // WIC_MIDCLICK_SUPPORT
		}
		else switch (event)
		{
		case ONMOUSEUP:
			{

#if !defined HAS_NOTEXTSELECTION && defined _X11_SELECTION_POLICY_
				// This section has to be done outside the 'target == active' test below as they
				// may very well not be identical when finishing a selection
				if (button == MOUSE_BUTTON_1)
				{
					if (frames_doc->HasSelectedText() && frames_doc->GetWasSelectingText())
						frames_doc->CopySelectedTextToMouseSelection();
				}
#endif // !defined HAS_NOTEXTSELECTION && defined _X11_SELECTION_POLICY_

				HTML_Element* active = frames_doc->GetActiveHTMLElement();

				frames_doc->SetActiveHTMLElement(NULL);

				if (target == active &&
					!target->IsDisabled(frames_doc) && button == MOUSE_BUTTON_1)
				{
				/* Don't generate a ONCLICK if there is selected text.
				This is to make it possible to select text in a link
					without also clicking the link. */

# ifndef HAS_NOTEXTSELECTION
					BOOL selected_text = frames_doc->HasSelectedText() && frames_doc->GetWasSelectingText();

#  ifdef SVG_SUPPORT
					BOOL svg_has_selected_text = FALSE;
					if(!selected_text && active->GetNsType() == NS_SVG)
					{
						SVGWorkplace* svg_workplace = frames_doc->GetLogicalDocument()->GetSVGWorkplace();
						svg_has_selected_text = svg_workplace->HasSelectedText();
						selected_text = svg_has_selected_text;
					}
#  endif // SVG_SUPPORT
#  ifdef GRAB_AND_SCROLL
					if (frames_doc->GetWindow()->GetScrollIsPan())
						selected_text = FALSE; //ignore selected text here
#  endif // GRAB_AND_SCROLL

					BOOL might_be_interpreted_as_click = EXTRACT_CLICK_INDICATOR(sequence_count_and_button_or_key_or_delta);
					if (!might_be_interpreted_as_click && frames_doc->GetMouseUpMayBeClick())
						might_be_interpreted_as_click = TRUE;

					BOOL send_onclick_event = FALSE;
					if (might_be_interpreted_as_click)
					{
						send_onclick_event = !selected_text ||
											  (!frames_doc->HasSelectedText()
#  ifdef SVG_SUPPORT
											  && !svg_has_selected_text
#  endif // SVG_SUPPORT
											  ) ||
											  modifiers != 0;
					}

					frames_doc->SetSelectingText(FALSE);

					if (send_onclick_event)
# endif
					{
						DOM_EventType type = ONCLICK;
send_click_event:
						if (frames_doc->HandleMouseEvent(type,
							related_target, target, imagemap,
							offset_x, offset_y,
							document_x, document_y,
							modifiers,
							sequence_count_and_button_or_key_or_delta) == OpStatus::ERR_NO_MEMORY)
						{
							frames_doc->GetWindow()->RaiseCondition(OpStatus::ERR_NO_MEMORY);
						}
						else if (type == ONCLICK)
						{
							int sequence_count = EXTRACT_SEQUENCE_COUNT(sequence_count_and_button_or_key_or_delta);
							if (sequence_count == 2)
							{
								type = ONDBLCLICK;
								goto send_click_event;
							}
						}
					}
				}
			}
			break;

		case ONMOUSEDOWN:
#ifndef HAS_NOTEXTSELECTION
			if (button == MOUSE_BUTTON_1
				&& !cancelled
				&& frames_doc->GetHtmlDocument()
				&& !frames_doc->GetHtmlDocument()->GetCapturedWidgetElement()
				&& !OpWidget::hooked_widget
				&& !target->IsMatchingType(HE_OPTION, NS_HTML)
#ifdef DOCUMENT_EDIT_SUPPORT
				&& !target_at_layout_modifier
#endif // DOCUMENT_EDIT_SUPPORT
				)
			{
				int sequence_count = EXTRACT_SEQUENCE_COUNT(sequence_count_and_button_or_key_or_delta);
				if (shift_pressed)
				{
					if (sequence_count == 1 && frames_doc->GetSelectedTextLen())
					{
						frames_doc->MoveSelectionFocusPoint(document_x, document_y, FALSE);
						frames_doc->SetSelectingText(TRUE);
					}
#ifdef DOCUMENT_EDIT_SUPPORT
					else if (sequence_count == 1 && frames_doc->GetDocumentEdit())
					{
						OpDocumentEdit *de = frames_doc->GetDocumentEdit();
						if (de->GetEditableContainer(target))
						{
							OpPoint p = frames_doc->GetCaretPainter()->GetCaretPosInDocument();
							if (OpStatus::IsSuccess(frames_doc->StartSelection(p.x, p.y)))
								frames_doc->MoveSelectionFocusPoint(document_x, document_y, FALSE);
						}
					}
#endif
				}
				else
				{
					// The selection should be expanded after the second mousedown.
					if (sequence_count == 2)
					{
						frames_doc->ExpandSelection(TEXT_SELECTION_WORD);
						frames_doc->SetSelectingText(FALSE);
					}
					else if(sequence_count == 1)
					{
						// Remove all search hits
						frames_doc->GetTopDocument()->RemoveSearchHits();
						frames_doc->MaybeStartTextSelection(document_x, document_y);
					}
				}
			}
#endif // HAS_NOTEXTSELECTION
			frames_doc->SetActiveHTMLElement(target);
			break;

		case ONDBLCLICK:
#ifndef HAS_NOTEXTSELECTION
# ifdef LOGDOC_HOTCLICK

#  ifdef _X11_SELECTION_POLICY_
			frames_doc->CopySelectedTextToMouseSelection();
#  endif // _X11_SELECTION_POLICY_

#ifdef DOCUMENT_EDIT_SUPPORT
			// No hotclick menu if we clicked in a editable element
			if (!(frames_doc->GetDocumentEdit() && frames_doc->GetDocumentEdit()->GetEditableContainer(target)))
#endif // DOCUMENT_EDIT_SUPPORT
			{
				VisualDevice* vis_dev = frames_doc->GetVisualDevice();
				CoreView* view = vis_dev->GetView();
				if (view)
					ShowHotclickMenu(vis_dev, frames_doc->GetWindow(), view, frames_doc->GetDocManager(), frames_doc, TRUE);
			}
# endif // LOGDOC_HOTCLICK
#endif // HAS_NOTEXTSELECTION
			break;

		case ONMOUSEOVER:
			{
				if (g_pcdoc->GetIntegerPref(PrefsCollectionDoc::DisplayLinkTitle))
				{
					const uni_char* string = NULL;
#ifdef SVG_SUPPORT
					OpString title_owner;
#endif // SVG_SUPPORT
					HTML_Element* elm = target;
					while (elm && !string)
					{
						if (elm->GetNsType() == NS_HTML)
							string = elm->GetStringAttr(ATTR_TITLE);
#ifdef SVG_SUPPORT
						else if (elm->GetNsType() == NS_SVG)
						{
							g_svg_manager->GetToolTip(elm, title_owner);
							string = title_owner.CStr();
						}
#endif // SVG_SUPPORT
						elm = elm->Parent();
					}

					if (string)
						frames_doc->GetWindow()->DisplayLinkInformation(NULL, ST_ATITLE, string);
				}
			}
			break;

		default:
			break;
		}
	}
#endif // !MOUSELESS

cursor_check:

#ifndef MOUSELESS
	if (event == ONMOUSEOVER || event == ONMOUSEMOVE || event == ONMOUSEOUT)
	{
		Window* cur_win = frames_doc->GetWindow();
		if (Type() == Markup::HTE_DOC_ROOT &&
			!(frames_doc->GetHtmlDocument() && frames_doc->GetHtmlDocument()->GetCapturedWidgetElement()))
		{
			/* Set cursor auto when we reach doc root */
			cur_win->SetPendingCursor(CURSOR_AUTO);
		}
		else
		{
			BOOL has_cursor_settings;
			FormObject* form_object = GetFormObject();
# ifdef SVG_SUPPORT
			if (GetNsType() == NS_SVG)
				has_cursor_settings = HasCursorSettings(); // never any layout box for svg elements
			else
# endif
				has_cursor_settings = HasCursorSettings() && GetLayoutBox();

			if (has_cursor_settings)
			{
# ifdef SVG_SUPPORT
				if (GetNsType() == NS_SVG)
					cur_win->SetPendingCursor(g_svg_manager->GetCursorForElement(this));
				else
# endif // SVG
					cur_win->SetPendingCursor(GetCursorType());
			}
			else if (is_link)
			{
				cur_win->SetPendingCursor(CURSOR_CUR_POINTER);
			}
			else if (event == ONMOUSEMOVE && form_object)
			{
				form_object->OnSetCursor(OpPoint(document_x, document_y));
			}
			/* Set the default arrow cursor here so that we could avoid the text cursor on
			 * boxes inside labels or buttons.
			 */
			else if (cur_win->GetPendingCursor() == CURSOR_AUTO
					&& (IsMatchingType(HE_LABEL, NS_HTML) || IsMatchingType(HE_BUTTON, NS_HTML)))
			{
				cur_win->SetPendingCursor(CURSOR_DEFAULT_ARROW);
			}

			if (this == target)
			{
# ifdef DOCUMENT_EDIT_SUPPORT
				CursorType de_cursor = CURSOR_AUTO;
				if (OpDocumentEdit *de = frames_doc->GetDocumentEdit())
					de_cursor = de->GetCursorType(target, document_x, document_y);
				/* Note: If we want to use any specific cursor from parents
				 * check for pending cursor and then only set the cursor
				 */
				if (de_cursor != CURSOR_AUTO)
					cur_win->SetPendingCursor(de_cursor);
				else
# endif // DOCUMENT_EDIT_SUPPORT
				if (cur_win->GetPendingCursor() == CURSOR_AUTO && CanUseTextCursor())
					cur_win->SetPendingCursor(CURSOR_TEXT);

				/* Form objects would have already set a cursor, so
				 * apply the pending cursor if not a FormObject.
				 * If cursor style is specified then force that style (CORE-43297)
				 * Note: plugins can force a cursor change so in
				 * such cases we should not override it.
				 */
				if (has_cursor_settings || !form_object
#ifdef _PLUGIN_SUPPORT_
					&& !GetNS4Plugin()
#endif // _PLUGIN_SUPPORT_
					)
					cur_win->CommitPendingCursor();
			}
		}
	}
#endif // MOUSELESS

replay_mouse_actions:
	; // empty statement to silence compiler
#ifndef MOUSELESS
	if (this == target)
	{
		switch (event)
		{
		case ONMOUSEDOWN:
		case ONMOUSEUP:
		case ONMOUSEMOVE:
		case ONMOUSEOVER:
		case ONMOUSEENTER:
		case ONMOUSEOUT:
		case ONMOUSELEAVE:
		case ONMOUSEWHEELH:
		case ONMOUSEWHEELV:
#ifdef TOUCH_EVENTS_SUPPORT
		case TOUCHSTART:
		case TOUCHMOVE:
		case TOUCHEND:
		case TOUCHCANCEL:
#endif // TOUCH_EVENTS_SUPPORT
			if (HTML_Document *html_doc = frames_doc->GetHtmlDocument())
				/* This may trigger recursive calls to this function. */
				html_doc->ReplayRecordedMouseActions();
			break;
		}
	}

#endif // !MOUSELESS

	return handled;
}

#ifdef TOUCH_EVENTS_SUPPORT
void HTML_Element::SimulateMouse(HTML_Document* doc, DOM_EventType event, int x, int y, int radius, ShiftKeyState modifiers)
{
	int sequence_count_and_button = MAKE_SEQUENCE_COUNT_AND_BUTTON(1, MOUSE_BUTTON_1);

	BOOL shift = !!(modifiers & SHIFTKEY_SHIFT);
	BOOL ctrl = !!(modifiers & SHIFTKEY_CTRL);
	BOOL alt = !!(modifiers & SHIFTKEY_ALT);

#ifndef DOC_TOUCH_SMARTPHONE_COMPATIBILITY
	/*
	 * Our default behaviour is to map touch events directly to mouse events,
	 * facilitating normal desktop interactivity.
	 */
	DOM_EventType sequence[3] = { DOM_EVENT_NONE, DOM_EVENT_NONE, DOM_EVENT_NONE }; /* ARRAY OK 2010-07-26 terjes */
	switch (event)
	{
		case TOUCHSTART:
			/* Move pointer and implicitly generate ONMOUSE{OUT,OVER}. */
			sequence[0] = ONMOUSEMOVE;
			sequence[1] = ONMOUSEDOWN;
			break;

		case TOUCHMOVE:
			sequence[0] = ONMOUSEMOVE;
			break;

		case TOUCHEND:
			sequence[0] = ONMOUSEUP;
			break;
	};

	for (int i = 0; sequence[i] != DOM_EVENT_NONE; i++)
		doc->MouseAction(sequence[i], x, y, doc->GetFramesDocument()->GetVisualViewX(), doc->GetFramesDocument()->GetVisualViewY(), sequence_count_and_button, shift, ctrl, alt, FALSE, TRUE, radius);

#else // !DOC_TOUCH_SMARTPHONE_COMPATIBILITY
	/*
	 * As we are aiming for behaviour consistent with other smartphones, generate
	 * the entire sequence of mouse events on TOUCHEND.
	 */
	if (event == TOUCHEND)
	{
		/*
		 * We should consider cancelling this sequence if the mouseover/mousemove
		 * events lead to document changes. This would theoretically allow menus
		 * such as the one currently in effect on opera.com.
		 *
		 * (Cancelling the sequence is easily done by filtering all recorded events
		 * with the 'simulated' flag set.)
		 */

		DOM_EventType sequence[] = { ONMOUSEMOVE, ONMOUSEDOWN, ONMOUSEUP, DOM_EVENT_NONE };

		for (int i = 0; sequence[i] != DOM_EVENT_NONE; i++)
			doc->MouseAction(sequence[i], x, y, sequence_count_and_button, shift, ctrl, alt, FALSE, TRUE, radius);
	}
#endif // DOC_TOUCH_SMARTPHONE_COMPATIBILITY
}
#endif // TOUCH_EVENTS_SUPPORT

#ifndef MOUSELESS
void HTML_Element::UpdateCursor(FramesDocument *frames_doc)
{
	BOOL has_cursor_settings;
	HTML_Element *parent = this;
	while (parent)
	{
		if (parent->IsLink(frames_doc))
			break;

# ifdef SVG_SUPPORT
		if (GetNsType() == NS_SVG)
			has_cursor_settings = parent->HasCursorSettings(); // never any layout box for svg elements
		else
# endif
			has_cursor_settings = parent->HasCursorSettings() && GetLayoutBox();

		if (has_cursor_settings)
		{
			CursorType new_cursor;
# ifdef SVG_SUPPORT
			if (parent->GetNsType() == NS_SVG)
				new_cursor = g_svg_manager->GetCursorForElement(parent);
			else
# endif // SVG
				new_cursor = parent->GetCursorType();
			/* Only set if the current cursor and new_cursor are different */
			frames_doc->GetWindow()->SetCursor(new_cursor, TRUE);
			return;
		}
		parent = parent->Parent();
	}
}

BOOL HTML_Element::IsLink(FramesDocument *frames_doc)
{
	if (GetNsType() == NS_HTML)
	{
		if (Type() == HE_IMG && GetA_Tag())
			return TRUE;
		if (Type() == HE_A && !GetAnchor_URL(frames_doc).IsEmpty())
			return TRUE;
		if (Type() == HE_AREA && GetAREA_URL(frames_doc->GetLogicalDocument()))
			return TRUE;
		if (Type() == HE_INPUT && GetInputType() == INPUT_IMAGE && FormManager::FindFormElm(frames_doc, this))
			return TRUE;
	}

	if (URL *css_link = (URL*)GetSpecialAttr(ATTR_CSS_LINK, ITEM_TYPE_URL, (void*)NULL, SpecialNs::NS_LOGDOC))
		if (css_link->Type() != URL_JAVASCRIPT)
			return TRUE;

#ifdef SVG_SUPPORT
	if (GetNsType() == NS_SVG)
	{
		URL *url = NULL;
		OP_BOOLEAN status = g_svg_manager->NavigateToElement(this, frames_doc, &url, ONMOUSEOVER, NULL);
		if (status == OpBoolean::IS_TRUE && url != NULL && !url->IsEmpty())
			return TRUE;
	}
#endif // SVG_SUPPORT

#ifdef _WML_SUPPORT_
	if (GetNsType() == NS_WML)
	{
		WML_ElementType type = (WML_ElementType) Type();
		WML_Context *wc = frames_doc->GetDocManager()->WMLGetContext();
		WMLNewTaskElm *wml_task = wc->GetTaskByElement(this);

		if (!wc || !wml_task)
			return FALSE;

		if (type == WE_ANCHOR || type == WE_DO)
		{
			UINT32 action = 0;
			URL url = wc->GetWmlUrl(wml_task->GetHElm(), &action);
			if (!url.IsEmpty())
				return TRUE;
		}
	}
#endif //_WML_SUPPORT_

#ifdef M2_SUPPORT
	if (GetNsType() == NS_OMF)
	{
		if (Type() == OE_ITEM || Type() == OE_SHOWHEADERS)
			if (URL *css_link = (URL*)GetSpecialAttr(ATTR_CSS_LINK, ITEM_TYPE_URL, NULL, SpecialNs::NS_LOGDOC))
				if (css_link->Type() != URL_JAVASCRIPT)
					return TRUE;
	}
#endif // M2_SUPPORT

	return FALSE;
}

#endif // !MOUSELESS

HTML_Element*
HTML_Element::GetNextLinkElementInMap(BOOL forward, HTML_Element* map_element)
{
	HTML_Element* next;

	if (map_element == this)
		next = forward ? FirstChildActual() : LastChildActual();
	else
		next = forward ? NextActual() : PrevActual();

	if (next && map_element != next && map_element->IsAncestorOf(next))
	{
		if (next->IsMatchingType(HE_A, NS_HTML) || next->IsMatchingType(HE_AREA, NS_HTML))
			return next;
		else
			return next->GetNextLinkElementInMap(forward, map_element);
	}

	return NULL;
}

HTML_Element* HTML_Element::GetAncestorMapElm()
{
	HTML_Element* map_elm = ParentActual();

	while (map_elm && !map_elm->IsMatchingType(HE_MAP, NS_HTML))
		map_elm = map_elm->ParentActual();

	return map_elm;
}

HTML_Element*
HTML_Element::GetLinkElement(VisualDevice* vd, int rel_x, int rel_y,
							 const HTML_Element* img_element,
							 HTML_Element* &default_element,
							 LogicalDocument *logdoc/*=NULL*/)
{
	if (Type() == HE_A || Type() == HE_AREA)
	{
		if (GetMapUrl(vd, rel_x, rel_y, img_element, NULL, logdoc))
			return this;
		else if (GetAREA_Shape() == AREA_SHAPE_DEFAULT)
			default_element = this;
	}

	HTML_Element* link_element = NULL;

	for (HTML_Element* child = FirstChildActual(); child && !link_element; child = child->SucActual())
		link_element = child->GetLinkElement(vd, rel_x, rel_y, img_element, default_element, logdoc);

	return link_element;
}

/**
 *	Get element linked to from an usemap image based on coordinates. If the alt text is
 *	shown, the linked element is based on the href of the clicked element that was
 *	INSERTED_BY_LAYOUT to act as an alt link.
 *
 *	@param frames_doc
 *	@param rel_x
 *	@param rel_y
 *	@param clicked_element Corresponding alt text link that was clicked.
 *
 */

HTML_Element*
HTML_Element::GetLinkedElement(FramesDocument* frames_doc,
							   int rel_x, int rel_y, HTML_Element* clicked_alt_text_element)
{
	if(GetNsType() == NS_HTML)
	{
		switch (Type())
		{
		case HE_INPUT:
		case HE_IMG:
		case HE_OBJECT:
			{
				URL* usemap_url = NULL;
				if(Type() == HE_INPUT || Type() == HE_OBJECT)
					usemap_url = GetUrlAttr(ATTR_USEMAP, NS_IDX_HTML, frames_doc->GetLogicalDocument());
				else
					usemap_url = GetIMG_UsemapURL(frames_doc->GetLogicalDocument());

				if (usemap_url && !usemap_url->IsEmpty())
				{
					HTML_Element* map_elm = NULL;
					LogicalDocument* logdoc = frames_doc->GetLogicalDocument();

					if (logdoc)
					{
						const uni_char* rel_name = usemap_url->UniRelName();

						if (rel_name && logdoc->GetRoot())
							map_elm = logdoc->GetRoot()->GetMAP_Elm(rel_name);
					}

					if (map_elm)
					{
						HTML_Element* link_element = NULL;

						if (!GetLayoutBox() || !GetLayoutBox()->IsContentImage())
						{
							// We are showing the alt text.

							if (!clicked_alt_text_element)
								return NULL;

							const uni_char* elm_url = NULL;

							HTML_Element* elm_iter = clicked_alt_text_element;

							while (elm_iter)
							{
								if ((elm_url = elm_iter->GetA_HRef(frames_doc)) != NULL)
									break;

								elm_iter = elm_iter->Parent(); // Not ParentActual we want to be INSERTED_BY_LAYOUT
							}

							if (!elm_url)
								return NULL;

							HTML_Element* link_element = map_elm->GetNextLinkElementInMap(TRUE, map_elm);

							while (link_element)
							{
								const uni_char* link_element_href = link_element->GetStringAttr(ATTR_HREF);
								if (link_element_href && uni_strcmp(elm_url, link_element_href) == 0)
									return link_element;

								link_element = link_element->GetNextLinkElementInMap(TRUE, map_elm);
							}
						}
						else
						{
							HTML_Element* default_element = NULL;
							link_element = map_elm->GetLinkElement(frames_doc->GetVisualDevice(), rel_x, rel_y, this, default_element, logdoc);

							if (!link_element)
								link_element = default_element;
						}
						return link_element;
					}
				}
			}

			break;

		default:
			break;
		}
	}

	return NULL;
}

#ifdef _WML_SUPPORT_
OP_STATUS HTML_Element::WMLInit(DocumentManager *doc_man)
{
	WML_ElementType w_type = (WML_ElementType)Type();
	NS_Type ns = GetNsType();

	WML_Context *context = doc_man->WMLGetContext();

	if (!context)
	{
		doc_man->WMLInit();
		context = doc_man->WMLGetContext();

		if (!context)
			return OpStatus::ERR_NO_MEMORY;
	}

	if (ns != NS_WML)
	{
		if (ns != NS_HTML)
			return OpStatus::OK;

		switch (Type())
		{
		case HE_HTML:
		case HE_BODY:
			if (HasAttr(WA_ONTIMER, NS_IDX_WML))
			{
				if (OpStatus::IsMemoryError(context->SetEventHandler(WEVT_ONTIMER, context->NewTask(this))))
					return OpStatus::ERR_NO_MEMORY;
			}
			else if (HasAttr(WA_ONENTERFORWARD, NS_IDX_WML))
			{
				if (OpStatus::IsMemoryError(context->SetEventHandler(WEVT_ONENTERFORWARD, context->NewTask(this))))
					return OpStatus::ERR_NO_MEMORY;
			}
			else if (HasAttr(WA_ONENTERBACKWARD, NS_IDX_WML))
			{
				if (OpStatus::IsMemoryError(context->SetEventHandler(WEVT_ONENTERBACKWARD, context->NewTask(this))))
					return OpStatus::ERR_NO_MEMORY;
			}
			return OpStatus::OK;

		case HE_OPTION:
			if (HasAttr(WA_ONPICK, NS_IDX_WML))
				return context->SetTaskByElement(context->NewTask(this), this);
			break;

		default:
			return OpStatus::OK;
		}
	}

	HTML_Element *parent = Parent();
	WMLNewTaskElm  *tmp_task = NULL;

	const uni_char *value = doc_man->GetCurrentURL().UniRelName();
	const uni_char *extra_value = NULL;

	if (w_type == WE_CARD)
	{
		extra_value = GetId();

		if (!extra_value)
			extra_value = UNI_L("");

		BOOL matches_target = value ? uni_str_eq(value, extra_value) : context->IsSet(WS_FIRSTCARD);
		if (matches_target || context->IsSet(WS_FIRSTCARD))
		{
			if (matches_target)
			{
				if (context->GetPendingCurrentCard())
					context->ScrapTmpCurrentCard();

				context->SetStatusOn(WS_INRIGHTCARD);
			}
			else
			{
				RETURN_IF_MEMORY_ERROR(context->PushTmpCurrentCard());
			}

			context->SetPendingCurrentCard(this);
		}
		else
			return OpStatus::OK;
	}
	else if (!context->IsSet(WS_FIRSTCARD) && ! context->IsSet(WS_INRIGHTCARD)) // check if the element is within the valid card
		return OpStatus::OK;

	/****************************************
	 * do element specific tasks
	 ****************************************/
	switch (w_type)
	{
	case WE_PREV: // fall through
	case WE_REFRESH: // fall through
	case WE_GO:
	case WE_NOOP:
		{
			WML_ElementType e_type = (WML_ElementType) parent->Type();
			if (e_type == WE_ANCHOR
				|| e_type == WE_ONEVENT
				|| e_type == WE_DO)
			{
				tmp_task = context->GetTaskByElement(parent);
				if (context->SetTaskByElement(tmp_task, this) == OpStatus::ERR_NO_MEMORY)
					return OpStatus::ERR_NO_MEMORY;
			}
		}
		break;

	case WE_ONEVENT:
		{
			value = GetStringAttr(WA_TYPE, NS_IDX_WML);
			WML_EventType e_type = WML_Lex::GetEventType(value);

			if (e_type == WEVT_ONPICK && parent->IsMatchingType(HE_OPTION, NS_HTML))
			{
				tmp_task = context->GetTaskByElement(parent); // if exists, use the task of the OPTION element
				if (!tmp_task)
				{
					tmp_task = context->NewTask(this);
					if (context->SetTaskByElement(tmp_task, parent) == OpStatus::ERR_NO_MEMORY)
						return OpStatus::ERR_NO_MEMORY;
				}
			}
			else if ((e_type == WEVT_ONTIMER
				   || e_type == WEVT_ONENTERFORWARD
				   || e_type == WEVT_ONENTERBACKWARD)
				   &&
					  (parent->IsMatchingType(WE_CARD, NS_WML)
				   || parent->IsMatchingType(WE_TEMPLATE, NS_WML)
				   || parent->IsMatchingType(HE_BODY, NS_HTML)))
			{
				tmp_task = context->NewTask(this);

				if (context->SetTaskByElement(tmp_task, this) == OpStatus::ERR_NO_MEMORY)
					return OpStatus::ERR_NO_MEMORY;
				if (context->SetEventHandler(e_type, tmp_task) == OpStatus::ERR_NO_MEMORY)
					return OpStatus::ERR_NO_MEMORY;
			}
		}
		break;

	case WE_DO:
	case WE_ANCHOR:
		return context->SetTaskByElement(context->NewTask(this), this);

	case WE_CARD:
		if (GetBoolAttr(WA_NEWCONTEXT, NS_IDX_WML))
		{
			RETURN_IF_MEMORY_ERROR(context->SetNewContext());
		}
		// fall through

	case WE_TEMPLATE:

		value = GetAttrValue(WA_ONENTERFORWARD, NS_IDX_WML);
		if (value && context->SetEventHandler(WEVT_ONENTERFORWARD, context->NewTask(this)) == OpStatus::ERR_NO_MEMORY)
			return OpStatus::ERR_NO_MEMORY;

		value = GetAttrValue(WA_ONENTERBACKWARD, NS_IDX_WML);
		if (value && context->SetEventHandler(WEVT_ONENTERBACKWARD, context->NewTask(this)) == OpStatus::ERR_NO_MEMORY)
			return OpStatus::ERR_NO_MEMORY;

		value = GetAttrValue(WA_ONTIMER, NS_IDX_WML);
		if (value && context->SetEventHandler(WEVT_ONTIMER, context->NewTask(this)) == OpStatus::ERR_NO_MEMORY)
			return OpStatus::ERR_NO_MEMORY;

		break;

	case WE_ACCESS:
		{
			URL url = doc_man->GetCurrentDoc()->GetRefURL().url;

			value = GetStringAttr(WA_DOMAIN, NS_IDX_WML);
			if (value)
			{
				ServerName *host = (ServerName *) url.GetAttribute(URL::KServerName, (void*)NULL);
				ServerName *domain = g_url_api->GetServerName(value, TRUE);
				if(!host || !domain || host->GetCommonDomain(domain) != domain)
				{
					context->SetStatusOn(WS_NOACCESS);
					break;
				}
			}

			extra_value = GetStringAttr(WA_PATH, NS_IDX_WML);
			if (extra_value)
			{
#ifdef OOM_SAFE_API
				TRAPD(op_err, value = url.GetAttributeL(URL::KUniPath));
				if (OpStatus::IsError(op_err) || !value )
					value = UNI_L("");
#else
				if (! (value = url.GetAttribute(URL::KUniPath, TRUE).CStr()))
					value = UNI_L("");
#endif //OOM_SAFE_API
				unsigned int i = 0;
				unsigned int j = uni_strlen(extra_value);

				if (uni_strlen(value) >= j)
				{
					while (i < j && value[i] == extra_value[i])
						i++;

					if (i != j || (value[i] != '/' && (i == 0 || extra_value[i - 1] != '/')))
						context->SetStatusOn(WS_NOACCESS);
				}
				else
					context->SetStatusOn(WS_NOACCESS);
			}
		}
		break;

	case WE_TIMER:
		value = GetWmlName();
		extra_value = GetWmlValue();

		if (extra_value)
		{
#ifdef WML_SHORTEN_SHORT_DELAYS
			if( uni_atoi(extra_value) < 20 )
				extra_value = UNI_L("1");
			else
				break;
#endif // WML_SHORTEN_SHORT_DELAYS
			return context->SetTimer(value, extra_value);
		}
		break;

	default:
		break;
	}

	return OpStatus::OK;
}
#endif // _WML_SUPPORT_

LogicalDocument*
HTML_Element::GetLogicalDocument() const
{
	const HTML_Element *parent = this;
	while (parent && parent->Type() != HE_DOC_ROOT)
		parent = parent->Parent();

	if(!parent)
		return NULL;

	ComplexAttr *attr = (ComplexAttr*) parent->GetSpecialAttr(ATTR_LOGDOC,
		ITEM_TYPE_COMPLEX, (void*)NULL, SpecialNs::NS_LOGDOC);

	OP_ASSERT(!attr || attr->IsA(ComplexAttr::T_LOGDOC));

	return (LogicalDocument*)attr;
}

#ifdef _DEBUG

const uni_char *InsStr(HE_InsertType type)
{
	switch(type)
	{
	case HE_NOT_INSERTED: return UNI_L("HE_NOT_INSERTED");
	case HE_INSERTED_BY_DOM: return UNI_L("HE_INSERTED_BY_DOM");
	case HE_INSERTED_BY_PARSE_AHEAD: return UNI_L("HE_INSERTED_BY_PARSE_AHEAD");
	case HE_INSERTED_BY_LAYOUT: return UNI_L("HE_INSERTED_BY_LAYOUT");
	case HE_INSERTED_BY_CSS_IMPORT: return UNI_L("HE_INSERTED_BY_CSS_IMPORT");
#ifdef SVG_SUPPORT
	case HE_INSERTED_BY_SVG: return UNI_L("HE_INSERTED_BY_SVG");
#endif
	};
	return UNI_L("");
}

void HTML_Element::DumpDebugTree(int level)
{
#ifdef WIN32
	if (level == 0)
		OutputDebugString(UNI_L("\n---------------------\n"));
#endif // WIN32
	uni_char line[2000]; /* ARRAY OK 2009-05-07 stighal */

	if (level > 1000)
		level = 1000;
	for(int i = 0; i < level; i++)
		line[i] = ' ';

	const uni_char* tmptypename = HTM_Lex::GetTagString(Type());
	if (Type() == HE_TEXT)
		tmptypename = UNI_L("TEXT");
	else if (!tmptypename)
		tmptypename = UNI_L("UNKNOWN");

	if (Type() == HE_TEXT)
		uni_snprintf(&line[level], 2000 - level, UNI_L("%s %p: \"%s\"  %s\n"), tmptypename, this, TextContent(), InsStr(GetInserted()));
	else
		uni_snprintf(&line[level], 2000 - level, UNI_L("%s %p  %s\n"), tmptypename, this, InsStr(GetInserted()));

#ifdef WIN32
	OutputDebugString(line);
#endif // WIN32

	HTML_Element* tmp = FirstChild();
	while(tmp)
	{
		tmp->DumpDebugTree(level + 2);
		tmp = (HTML_Element*) tmp->Suc();
	}
}
#endif

#define DOM_LOWERCASE_NAME(name, name_length) \
			AutoTempBuffer lowercase_buf; \
			if (!case_sensitive && name_length) \
			{ \
				RETURN_IF_MEMORY_ERROR(lowercase_buf->Expand(name_length + 1)); \
				for (const uni_char *c = name; *c; c++) \
					lowercase_buf->Append(uni_tolower(*c)); \
				lowercase_buf->SetCachedLengthPolicy(TempBuffer::UNTRUSTED); \
				lowercase_buf->Append(static_cast<uni_char>(0)); \
				name = lowercase_buf->GetStorage(); \
			}

OP_STATUS HTML_Element::SetAttribute(const DocumentContext &context, Markup::AttrType attr, const uni_char *name, int ns_idx, const uni_char *value, unsigned value_length, ES_Thread *thread, BOOL case_sensitive, BOOL is_id, BOOL is_specified)
{
	/* Either ATTR_XML and a name, or not ATTR_XML and no name. */
	OP_ASSERT((attr == ATTR_XML) == (name != NULL));

	LogicalDocument *logdoc = context.logdoc;
	HLDocProfile *hld_profile = context.hld_profile;
	unsigned name_length = name ? uni_strlen(name) : 0;

	DOM_LOWERCASE_NAME(name, name_length);

	int index;
	if (ns_idx == NS_IDX_HTML)
		index = FindHtmlAttrIndex(attr, name);
	else
		index = FindAttrIndex(attr, name, ns_idx, case_sensitive, TRUE);
	if (index != -1)
	{
		/* We shouldn't find another attribute type unless attr == ATTR_XML. */
		OP_ASSERT(attr == ATTR_XML || attr == GetAttrItem(index));

		attr = GetAttrItem(index);
		ns_idx = GetAttrNs(index);
	}
	else
	{
		if (ns_idx == NS_IDX_ANY_NAMESPACE)
			ns_idx = NS_IDX_DEFAULT;

		if (attr == ATTR_XML)
			attr = htmLex->GetAttrType(name, name_length, g_ns_manager->GetNsTypeAt(ResolveNsIdx(ns_idx)), case_sensitive);
	}

	int resolved_ns_idx = ResolveNsIdx(ns_idx);
	BOOL is_same_value = index != -1 && IsSameAttrValue(index, name, value);

	OP_STATUS status = BeforeAttributeChange(context, thread, index, attr, resolved_ns_idx, is_same_value);

	if (OpStatus::IsMemoryError(status))
		return status;
	else if (OpStatus::IsError(status))
		// Script was not allowed to change the attribute.
		return OpStatus::OK;

	// If it's the same value then we don't need to set it at all, except if the
	// old value was owned by someone else (ITEM_TYPE_NUM and ITEM_TYPE_BOOL is
	// never owned by someone else).
	if (!is_same_value || GetItemType(index) != ITEM_TYPE_NUM && GetItemType(index) != ITEM_TYPE_BOOL && !GetItemFree(index))
	{
		HtmlAttrEntry entry;

		entry.attr = attr;
		entry.is_id = is_id;
		entry.is_specified = is_specified;
		entry.ns_idx = ns_idx;
		entry.value = value;
		entry.value_len = value_length;
		entry.name = name;
		entry.name_len = name_length;

		void *attr_value;
		ItemType attr_item_type;
		BOOL attr_need_free;
		BOOL attr_is_event;

		RETURN_IF_ERROR(ConstructAttrVal(hld_profile, &entry, FALSE, attr_value, attr_item_type, attr_need_free, attr_is_event));

		if (attr_item_type != ITEM_TYPE_UNDEFINED || index != -1)
		{
			if (index != -1 && GetAttrIsEvent(index) && logdoc)
				/* ConstructAttrVal will have called AddEventHandler, so if we're overwriting
				   an existing event handler attribute, we should remove one as well. */
				logdoc->RemoveEventHandler(GetEventType(attr, ns_idx));

			if (attr_item_type != ITEM_TYPE_UNDEFINED)
				if (index != -1)
					ReplaceAttrLocal(index, entry.attr, attr_item_type, attr_value, ns_idx, attr_need_free, FALSE, entry.is_id, entry.is_specified, attr_is_event);
				else
					index = SetAttr(entry.attr, attr_item_type, attr_value, attr_need_free, ns_idx, TRUE, entry.is_id, entry.is_specified, attr_is_event, -1, case_sensitive);
			else
				RemoveAttrAtIdx(index);

			if (hld_profile && GetNsType() == NS_HTML)
			{
				// FIXME: Move to HandleAttributeChange (unless it needs to run before ApplyPropertyChanges).
				HTML_ElementType type = Type();
				if (type == HE_OBJECT || type == HE_EMBED || type == HE_APPLET)
				{
					if (PrivateAttrs *pa = (PrivateAttrs*)GetSpecialAttr(ATTR_PRIVATE, ITEM_TYPE_PRIVATE_ATTRS, (void*)NULL, SpecialNs::NS_LOGDOC))
					{
						if (!entry.name && entry.attr)
						{
							entry.name = HTM_Lex::GetAttributeString(static_cast<Markup::AttrType>(entry.attr), NS_HTML);
							entry.name_len = uni_strlen(entry.name);
						}

						RETURN_IF_MEMORY_ERROR(pa->SetAttribute(hld_profile, Type(), entry));
					}
				}
			}

			// Only called for changed attributes
			status = HandleAttributeChange(context, thread, index, static_cast<Markup::AttrType>(entry.attr), ns_idx);
		}
	}

	// Must always be called if BeforeAttributeChange was called
	OP_STATUS status2 = AfterAttributeChange(context, thread, index, attr, ns_idx, is_same_value);
	return OpStatus::IsError(status) ? status : status2;
}

#ifdef DOCUMENT_EDIT_SUPPORT

OP_STATUS HTML_Element::SetStringHtmlAttribute(FramesDocument *frames_doc, ES_Thread *thread, Markup::AttrType attr, const uni_char *value)
{
	// This function uses NS_IDX_DEFAULT in a way that assumes this is an HTML element.
	OP_ASSERT(GetNsType() == NS_HTML);

	return SetAttribute(frames_doc, attr, NULL, NS_IDX_DEFAULT, value, uni_strlen(value), thread, FALSE, attr == ATTR_ID);
}

OP_STATUS HTML_Element::EnableContentEditable(FramesDocument* frames_doc)
{
	OP_ASSERT(IsContentEditable());

	// Avoid setting focus to the editable element
	g_input_manager->LockKeyboardInputContext(TRUE);

	OP_STATUS oom = frames_doc->SetEditable(FramesDocument::CONTENTEDITABLE);

	g_input_manager->LockKeyboardInputContext(FALSE);

	if (OpStatus::IsError(oom))
		return oom;

	if (frames_doc->GetDocumentEdit())
	{
		HTML_Element *body = frames_doc->GetDocumentEdit()->GetBody();
		if (body)
		{
			if (this->IsAncestorOf(body))
				frames_doc->GetDocumentEdit()->InitEditableRoot(body);
			else
				frames_doc->GetDocumentEdit()->InitEditableRoot(this);
		}
	}

	return OpStatus::OK;
}

void HTML_Element::DisableContentEditable(FramesDocument *frames_doc)
{
	if (frames_doc->GetDocumentEdit())
		frames_doc->GetDocumentEdit()->UninitEditableRoot(this);

	if (!frames_doc->GetDesignMode())
	{
		// If no element is editable anymore, we can disable editing in this document.
		BOOL has_editable_content = FALSE;
		HTML_Element* elm = frames_doc->GetDocRoot();
		while(elm)
		{
			if (elm->IsContentEditable())
			{
				has_editable_content = TRUE;
				break;
			}
			elm = elm->Next();
		}
		if (!has_editable_content)
			frames_doc->SetEditable(FramesDocument::CONTENTEDITABLE_OFF);
	}
}

#endif // DOCUMENT_EDIT_SUPPORT

static BOOL IsLengthAttribute(HTML_ElementType type, short attr)
{
	if (attr == ATTR_WIDTH)
		return (type == HE_HR || type == HE_OBJECT || type == HE_APPLET || type == HE_TABLE ||
				type == HE_COLGROUP || type == HE_COL || type == HE_TR || type == HE_TD || type == HE_TH || type == HE_IFRAME || type == HE_VIDEO);
	else if (attr == ATTR_HEIGHT)
		return type == HE_OBJECT || type == HE_APPLET || type == HE_TR || type == HE_TD || type == HE_TH || type == HE_IFRAME || type == HE_VIDEO;
	else if (attr == ATTR_HSPACE || attr == ATTR_VSPACE)
		return type == HE_OBJECT || type == HE_APPLET;
	else if (attr == ATTR_BORDER || attr == ATTR_CELLPADDING || attr == ATTR_CELLSPACING)
		return type == HE_TABLE;
	else if (attr == ATTR_CHAROFF)
		return (type == HE_COLGROUP || type == HE_COL || type == HE_THEAD || type == HE_TFOOT || type == HE_TBODY ||
				type == HE_TR || type == HE_TD || type == HE_TH);
	else
		return FALSE;
}

static BOOL IsUrlAttribute(HTML_ElementType type, short attr)
{
	if (attr == ATTR_HREF)
		return type == HE_A || type == HE_AREA || type == HE_BASE || type == HE_LINK;
	else if (attr == ATTR_SRC)
		return type == HE_INPUT || type == HE_IMG || type == HE_SCRIPT || type == HE_FRAME || type == HE_IFRAME || type == HE_AUDIO || type == HE_VIDEO || type == HE_SOURCE || type == HE_EMBED || type == Markup::HTE_TRACK;
	else if (attr == ATTR_DATA)
		return type == HE_OBJECT;
	else if (attr == ATTR_CITE)
		return type == HE_BLOCKQUOTE || type == HE_Q || type == HE_DEL || type == HE_INS;
	else if (attr == ATTR_LONGDESC)
		return type == HE_IMG || type == HE_FRAME || type == HE_IFRAME;
	else if (attr == ATTR_CODEBASE)
		return type == HE_OBJECT || type == HE_APPLET;
	else if (attr == ATTR_POSTER)
		return type == HE_VIDEO;
	else if (attr == ATTR_ITEMID)
		return TRUE;
	else if (attr == ATTR_MANIFEST)
		return type == HE_HTML;
	else if (attr == ATTR_ACTION)
		return type == HE_FORM;
	else
		return FALSE;
}

int HTML_Element::GetAttributeCount()
{
	if (!Markup::IsRealElement(Type()))
		return 0;
	return GetAttrCount();
}

OP_STATUS HTML_Element::GetAttributeName(FramesDocument *frames_doc, int index, TempBuffer *buffer, const uni_char *&name, int &ns_idx, BOOL *specified, BOOL *id)
{
	int attr_index = 0;

	while (index >= 0 && attr_index < GetAttrSize())
	{
		if (!GetAttrIsSpecial(attr_index++))
			--index;
	}

	--attr_index;

	OP_STATUS status = OpStatus::OK;

	if (index >= 0)
	{
		// There wasn't that many attributes
		name = NULL;
		ns_idx = NS_IDX_DEFAULT;
	}
	else
	{
		Markup::AttrType item = GetAttrItem(attr_index);

		if (item != ATTR_XML)
			name = HTM_Lex::GetAttributeString(item, g_ns_manager->GetNsTypeAt(GetResolvedAttrNs(attr_index)));
		else
			name = (const uni_char *) GetValueItem(attr_index);

		ns_idx = GetAttrNs(attr_index);

		if (specified)
			*specified = GetAttrIsSpecified(attr_index);
		if (id)
			*id = GetAttrIsId(attr_index);
	}

	return status;
}

const uni_char *HTML_Element::GetAttribute(FramesDocument *frames_doc, int attr, const uni_char *name, int ns_idx, BOOL case_sensitive, TempBuffer *buffer, BOOL resolve_urls, int at_known_index)
{
	int index = at_known_index;
	if (index == -1)
		if (ns_idx == NS_IDX_HTML)
			index = FindHtmlAttrIndex(attr, name);
		else
			index = FindAttrIndex(attr, name, ns_idx, case_sensitive, FALSE);

	const uni_char *value = NULL;

	if (index != -1)
	{
		if (attr == ATTR_XML)
			attr = GetAttrItem(index);

		ns_idx = GetResolvedAttrNs(index);

		BOOL is_html = g_ns_manager->GetNsTypeAt(ns_idx) == NS_HTML;

		if (attr != ATTR_XML && is_html)
		{
			const uni_char *value = GetAttrValue(attr, NS_IDX_HTML, Type(), FALSE, buffer, index);

			if (value)
			{
				if (resolve_urls)
				{
					LogicalDocument* logdoc = frames_doc ? frames_doc->GetLogicalDocument() : NULL;
					if (IsUrlAttribute(Type(), attr))
					{
						URL *href_url = GetUrlAttr(attr, NS_IDX_HTML, logdoc);
						return href_url ? href_url->GetAttribute(URL::KUniName_With_Fragment_Escaped).CStr() : NULL;
					}
					else if (attr == ATTR_BACKGROUND && Type() == HE_BODY)
					{
						// Stored as a StyleAttribute, so GetUrlAttr() will not work.
						URL bg_url = ResolveUrl(value, logdoc, ATTR_BACKGROUND);
						OpString bg_url_str;
						if (!bg_url.IsEmpty() &&
							OpStatus::IsSuccess(bg_url.GetAttribute(URL::KUniName_With_Fragment_Escaped, bg_url_str)))
						{
							buffer->Clear();
							OpStatus::Ignore(buffer->Append(bg_url_str));
							value = buffer->GetStorage();
						}
					}
				}

				return value;
			}
		}
		else
		{
			if (attr != ATTR_XML)
				attr = ATTR_ID; // arbitrary attribute that will cause no conversion in GetAttrValueValue.

			value = GetAttrValueValue(index, attr, HE_ANY, buffer);
		}
	}

	return value;
}

BOOL HTML_Element::HasAttribute(const uni_char* attr_name, int ns_idx)
{
	Markup::AttrType attr = Markup::HA_XML;
	NS_Type ns_type = ns_idx == NS_IDX_ANY_NAMESPACE ? NS_USER : g_ns_manager->GetNsTypeAt(ResolveNsIdx(ns_idx));

	if (ns_type != NS_USER && Type() != Markup::HTE_UNKNOWN)
		attr = HTM_Lex::GetAttrType(attr_name, ns_type, FALSE);

	return FindAttrIndex(attr, attr_name, ns_idx, FALSE) != ATTR_NOT_FOUND;
}

BOOL
HTML_Element::AreAttributesEqual(short attr, ItemType attr_item_type, void *left, void *right)
{
	BOOL is_same_value = FALSE;
	switch (attr_item_type)
	{
	case ITEM_TYPE_BOOL:
		is_same_value = !right == !left;
		break;

	case ITEM_TYPE_NUM:
		is_same_value = (long) right == (long) left;
		break;

	case ITEM_TYPE_NUM_AND_STRING:
		is_same_value = *static_cast<INTPTR*>(left) == *static_cast<INTPTR*>(right) &&
		                uni_str_eq(reinterpret_cast<uni_char*>(static_cast<char*>(left) + sizeof(INTPTR)), reinterpret_cast<uni_char*>(static_cast<char*>(right) + sizeof(INTPTR)));
		break;

	case ITEM_TYPE_STRING:
		if (attr == ATTR_XML)
		{
			const uni_char* new_name_value = static_cast<const uni_char *>(right);
			const uni_char* old_name_value = static_cast<const uni_char *>(left);
			int value_offset = uni_strlen(new_name_value) + 1;
			OP_ASSERT(uni_stricmp(new_name_value, old_name_value)==0); // Same attribute name
			is_same_value = uni_str_eq(new_name_value + value_offset, old_name_value + value_offset);
		}
		else
			is_same_value = uni_str_eq(static_cast<const uni_char *>(right), static_cast<const uni_char *>(left));
		break;

	case ITEM_TYPE_URL_AND_STRING:
		{
			UrlAndStringAttr *old_attr = static_cast<UrlAndStringAttr*>(left);
			UrlAndStringAttr *new_attr = static_cast<UrlAndStringAttr*>(right);
			if (old_attr->GetString() && new_attr->GetString())
				is_same_value = uni_str_eq(old_attr->GetString(), new_attr->GetString());
		}
		break;

	case ITEM_TYPE_COMPLEX:
		{
			ComplexAttr* left_complex_attr = static_cast<ComplexAttr*>(left);
			ComplexAttr* right_complex_attr = static_cast<ComplexAttr*>(right);

			if (!left_complex_attr || !right_complex_attr)
				is_same_value = left_complex_attr == right_complex_attr;
			else
				is_same_value = right_complex_attr->Equals(left_complex_attr);
		}
		break;
	}
	return is_same_value;
}

// Search for the parent of the option, first among the
// option parents and then optionally in another tree, that's
// supposed to be the previous parents of the option.
static HTML_Element* FindOptionParent(HTML_Element* option, HTML_Element* parent_tree = NULL)
{
	OP_ASSERT(option->IsMatchingType(HE_OPTION, NS_HTML) || option->IsMatchingType(HE_OPTGROUP, NS_HTML));
	HTML_Element* parent = option;
	for (;;)
	{
		parent = parent->Parent();
		if (!parent)
		{
			if (parent_tree)
			{
				parent = parent_tree;
				parent_tree = NULL;
			}
			else
			{
				break;
			}
		}
		BOOL is_option_parent =
			parent->IsMatchingType(HE_SELECT, NS_HTML) ||
			parent->IsMatchingType(HE_DATALIST, NS_HTML);
		if (is_option_parent)
		{
			break;
		}
	}
	return parent;
}

OP_STATUS HTML_Element::HandleDocumentTreeChange(const DocumentContext &context, HTML_Element *parent, HTML_Element *child, ES_Thread *thread, BOOL added)
{
	FramesDocument *frames_doc = context.frames_doc;
	LogicalDocument *logdoc = context.logdoc;
	HLDocProfile *hld_profile = context.hld_profile;
	DOM_Environment *environment = context.environment;

	BOOL chardata_changed = FALSE;
	OpVector<HTML_Element> changed_radio_buttons;

	HTML_Element *iter = child, *stop = (HTML_Element *) child->NextSibling();
	while (iter != stop)
	{
		BOOL skip_children = FALSE;

		if (iter->IsText() || iter->Type() == HE_COMMENT)
		{
			chardata_changed = TRUE;

#ifdef DISPLAY_WRITINGSYSTEM_HEURISTIC
			// analyze text to detect writing system
			if (added && hld_profile)
				hld_profile->AnalyzeText(iter);
#endif // DISPLAY_WRITINGSYSTEM_HEURISTIC
		}
		else if (iter->IsScriptElement())
		{
			skip_children = TRUE;
			if (added)
			{
				int handled = iter->GetSpecialNumAttr(ATTR_JS_SCRIPT_HANDLED, SpecialNs::NS_LOGDOC);

				/* Script already executed?. */
				BOOL handle_script_element = (handled & SCRIPT_HANDLED_EXECUTED) == 0;

				if (handle_script_element && environment)
				{
#ifdef USER_JAVASCRIPT
					if (environment->IsHandlingScriptElement(this))
						handle_script_element = FALSE;
					else
#endif // USER_JAVASCRIPT
					if (environment->SkipScriptElements())
						handle_script_element = FALSE;
				}

				if (handle_script_element)
					RETURN_IF_ERROR(iter->HandleScriptElement(hld_profile, thread, TRUE, FALSE));
			}
			else if (hld_profile)
			{
				// This will abort the script if it should be aborted, otherwise do nothing.
				// If it's an external script that hasn't loaded, we will have killed the
				// inline load so in that case it's essential that CancelInlineScript
				// does something or we will wait forever.
				RETURN_IF_ERROR(hld_profile->GetESLoadManager()->CancelInlineScript(iter));
			}
		}
		else if (iter->IsStyleElement())
		{
			if (hld_profile)
			{
				if (!added)
					iter->RemoveCSS(context);
				else
					RETURN_IF_ERROR(iter->LoadStyle(context, FALSE));
			}
		}
		else if (iter->GetNsType() == NS_HTML)
		{
			if (added)
			{
#ifdef DOCUMENT_EDIT_SUPPORT
				if (iter->IsContentEditable())
					RETURN_IF_ERROR(iter->EnableContentEditable(frames_doc));
				else
#endif // DOCUMENT_EDIT_SUPPORT
				{
					if (iter->IsFormElement())
					{
						FormValue* form_value = iter->GetFormValue();
						form_value->SetMarkedPseudoClasses(form_value->CalculateFormPseudoClasses(frames_doc, iter));
#if defined(WAND_SUPPORT) && defined(PREFILLED_FORM_WAND_SUPPORT)
						if (iter->IsMatchingType(HE_INPUT, NS_HTML))
						{
							if (g_wand_manager->HasMatch(frames_doc, iter))
							{
								if (thread)
									frames_doc->AddPendingWandElement(iter, thread->GetRunningRootThread());
								else
								{
									frames_doc->SetHasWandMatches(TRUE);
									g_wand_manager->InsertWandDataInDocument(frames_doc, iter, NO);
								}
							}

						}
#endif // defined(WAND_SUPPORT) && defined(PREFILLED_FORM_WAND_SUPPORT)
					}
				}
			}

#ifdef ACCESS_KEYS_SUPPORT
			if (hld_profile && iter->HasAttr(Markup::HA_ACCESSKEY))
			{
				const uni_char *key = iter->GetAccesskey();
				if (key)
					if (added)
						hld_profile->AddAccessKey(key, iter);
					else
						hld_profile->RemoveAccessKey(key, iter);
			}
#endif // ACCESS_KEYS_SUPPORT

			switch (iter->Type())
			{
			case HE_TITLE:
				RETURN_IF_ERROR(iter->HandleCharacterDataChange(context, thread, added));
				skip_children = TRUE;
				break;

			case HE_BODY:
				if (hld_profile && parent->IsMatchingType(HE_HTML, NS_HTML))
					if (added)
					{
						if (!hld_profile->GetBodyElm() || iter->Precedes(hld_profile->GetBodyElm()))
							hld_profile->SetBodyElm(iter);
					}
					else if (iter == hld_profile->GetBodyElm())
						hld_profile->ResetBodyElm();
				break;

			case HE_PROCINST:
			case HE_LINK:
				if (hld_profile && iter->IsLinkElement())
					if (added)
						RETURN_IF_ERROR(hld_profile->HandleLink(iter));
					else
					{
						RETURN_IF_ERROR(frames_doc->CleanInline(iter));
						hld_profile->RemoveLink(iter);
					}
				break;

			case HE_IFRAME:
				if (added)
				{
					RETURN_IF_MEMORY_ERROR(hld_profile->HandleNewIFrameElementInTree(iter, thread));
					skip_children = TRUE;
				}
				else
				{
					// Removed an iframe.
					OP_STATUS onloadstatus;
					if (thread)
						onloadstatus = frames_doc->ScheduleAsyncOnloadCheck(thread);
					else
						onloadstatus = FramesDocument::CheckOnLoad(frames_doc, NULL);
					if (OpStatus::IsError(onloadstatus))
						return OpStatus::ERR_NO_MEMORY;
				}

				break;

			case HE_OPTION:
			case HE_OPTGROUP:
				{
					HTML_Element* select = FindOptionParent(iter, parent);
					if (select && select->IsMatchingType(HE_SELECT, NS_HTML))
					{
						FormValueList* formvalue = FormValueList::GetAs(select->GetFormValue());
						formvalue->HandleOptionListChanged(frames_doc, select, iter, added);
					}
				}
				break;

			case HE_INPUT:
				if (added && // removals are handled earlier, in OutSafe
					iter->GetInputType() == INPUT_RADIO)
				{
					OpStatus::Ignore(changed_radio_buttons.Add(iter)); // We want to continue processing changes even if this fails
				}
				break;
			case HE_BASE:
				if (URL *url = iter->GetUrlAttr(ATTR_HREF, NS_IDX_HTML, logdoc))
					hld_profile->SetBaseURL(url, iter->GetStringAttr(ATTR_HREF, NS_IDX_HTML));
				break;

#if defined JS_PLUGIN_SUPPORT || defined MANUAL_PLUGIN_ACTIVATION
			case HE_OBJECT:
# ifdef JS_PLUGIN_SUPPORT
				if (DOM_Object *dom_obj = iter->GetESElement())
					if (ES_Object *plugin_obj = DOM_Utils::GetJSPluginObject(dom_obj))
					{
						EcmaScript_Object *hostobject = ES_Runtime::GetHostObject(plugin_obj);
						if (added)
							static_cast<JS_Plugin_HTMLObjectElement_Object *>(hostobject)->Inserted();
						else
							static_cast<JS_Plugin_HTMLObjectElement_Object *>(hostobject)->Removed();
					}
# endif // JS_PLUGIN_SUPPORT
# ifdef MANUAL_PLUGIN_ACTIVATION
			// Fall through.
			case HE_APPLET:
			case HE_EMBED:
				if (thread && ES_Runtime::GetIsExternal(thread->GetContext()))
					iter->SetPluginExternal(TRUE);
# endif // MANUAL_PLUGIN_ACTIVATION
				break;
#endif // JS_PLUGIN_SUPPORT || MANUAL_PLUGIN_ACTIVATION

			case HE_IMG:
				if (added)
				{
					// Looks like the page wants to load an image (and
					// since it might be hidden we do it here rather than in layout)
					URL image_url = iter->GetImageURL(FALSE, logdoc);
					if (!image_url.IsEmpty())
						frames_doc->LoadInline(&image_url, iter, IMAGE_INLINE);
				}
				break;
#ifdef MEDIA_HTML_SUPPORT
			case Markup::HTE_TRACK:
				// For <track> elements we only need to notify the
				// MediaElement if it is the root of the changed
				// subtree.
				if (iter != child)
					break;

				// Fall through.
			case HE_AUDIO:
			case HE_VIDEO:
			case HE_SOURCE:
			{
				HTML_Element* media_elm = iter;
				if (iter->Type() == Markup::HTE_SOURCE || iter->Type() == Markup::HTE_TRACK)
				{
					media_elm = iter->ParentActual();
					if (!media_elm)
					{
						OP_ASSERT(iter->Type() == Markup::HTE_SOURCE || iter->Type() == Markup::HTE_TRACK);
						media_elm = parent;
						if (!parent->IsIncludedActual())
							media_elm = media_elm->ParentActual();
						OP_ASSERT(media_elm);
					}
				}

				if (MediaElement* media = media_elm->GetMediaElement())
					RETURN_IF_ERROR(media->HandleElementChange(iter, added, thread));
				break;
			}
#endif // MEDIA_HTML_SUPPORT

			case HE_META:
				iter->CheckMetaElement(context, iter->ParentActual(), added);
				if (added && frames_doc->IsLoaded(TRUE))
					RETURN_IF_ERROR(frames_doc->CheckRefresh());
				break;

#ifdef JS_PLUGIN_SUPPORT
			case HE_PARAM:
				if (parent)
				{
					JS_Plugin_Object* jso = parent->GetJSPluginObject();
					if (jso)
					{
						const uni_char* name = iter->GetPARAM_Name();
						if (!name)
							break;
						const uni_char* value = NULL;
						if (added)
							value = iter->GetPARAM_Value();
						jso->ParamSet(name, value);
					}
				}
				break;
# endif // JS_PLUGIN_SUPPORT
			}
		}

#ifdef DNS_PREFETCHING
		if (added)
		{
			URL anchor_url = iter->GetAnchor_URL(frames_doc);
			if (!anchor_url.IsEmpty())
				logdoc->DNSPrefetch(anchor_url, DNS_PREFETCH_DYNAMIC);
		}
#endif // DNS_PREFETCHING

		if (skip_children)
			iter = (HTML_Element *) iter->NextSibling();
		else
			iter = (HTML_Element *) iter->Next();
	}

	RETURN_IF_ERROR(hld_profile->GetLayoutWorkplace()->HandleDocumentTreeChange(parent, child, added));

#ifdef SVG_SUPPORT
	g_svg_manager->OnSVGDocumentChanged(frames_doc, parent, child, added);
#endif // defined(SVG_SUPPORT)
	if (added && // removals are handled earlier, in OutSafe
		changed_radio_buttons.GetCount() > 0)
	{
		RETURN_IF_ERROR(FormManager::HandleChangedRadioGroups(frames_doc, changed_radio_buttons, added));
	}

	if (chardata_changed)
		return parent->HandleCharacterDataChange(context, thread, added, FALSE);
	else
		return OpStatus::OK;
}

OP_STATUS HTML_Element::HandleCharacterDataChange(const DocumentContext &context, ES_Thread *thread, BOOL added, BOOL update_pseudo_elm)
{
	FramesDocument *frames_doc = context.frames_doc;
	HLDocProfile *hld_profile = context.hld_profile;
#ifdef USER_JAVASCRIPT
	DOM_Environment *environment = context.environment;
#endif // USER_JAVASCRIPT

	HTML_Element *element = this;
#ifdef SVG_SUPPORT
	BOOL is_change_inside_svg = (element->GetNsType() == NS_SVG);
#endif // SVG_SUPPORT

	while (element)
	{
		if (element->IsStyleElement())
		{
			if (hld_profile)
				RETURN_IF_ERROR(LoadStyle(context, FALSE));
		}
		else if (element->IsScriptElement())
		{
			// When script text is added to an empty script that hasn't
			// previously been run and has no src/xlink:href attribute, that script
			// should be run.
			if (!added)
				return OpStatus::OK;

			if (URL* url = element->GetScriptURL(*hld_profile->GetURL(), hld_profile->GetLogicalDocument()))
				if (url->Type() != URL_NULL_TYPE)
					return OpStatus::OK;

			if (DataSrc* src_head = element->GetDataSrc())
				src_head->DeleteAll();

			int handled = element->GetSpecialNumAttr(ATTR_JS_SCRIPT_HANDLED, SpecialNs::NS_LOGDOC);

			/* Script already executed?. */
			BOOL already_executed = (handled & SCRIPT_HANDLED_EXECUTED) != 0;
			if (!already_executed)
			{
#ifdef USER_JAVASCRIPT
				if (environment && environment->IsHandlingScriptElement(element))
					return OpStatus::OK;
#endif // USER_JAVASCRIPT

				RETURN_IF_ERROR(element->HandleScriptElement(hld_profile, thread, TRUE, FALSE));
			}
		}
		else if (element->GetNsType() == NS_HTML)
		{
			switch (element->Type())
			{
			case HE_OPTION:
				{
					HTML_Element *select = FindOptionParent(element);
					if (select)
						if (SelectionObject *selection_object = (SelectionObject *) select->GetFormObject())
						{
							HTML_Element *iter = (HTML_Element *) element->Prev();
							int index = 0;

							while (iter != select)
							{
								if (iter->Type() == HE_OPTION)
									++index;
								iter = (HTML_Element *) iter->Prev();
							}

							BOOL selected = selection_object->IsSelected(index);
							BOOL disabled = selection_object->IsDisabled(index);

							if (index < selection_object->GetElementCount()) // The element hasn't been added to the select yet.
							{
								TempBuffer buffer;
								RETURN_IF_ERROR(buffer.Expand(element->GetTextContentLength() + 1));
								element->GetTextContent(buffer.GetStorage(), buffer.GetCapacity());
								RETURN_IF_ERROR(selection_object->ChangeElement(buffer.GetStorage(), selected, disabled, index));
								selection_object->ChangeSizeIfNeeded();
							}
						}
				}
				break;

			case HE_TITLE:
				if (Window *window = frames_doc->GetWindow())
					RETURN_IF_ERROR(window->UpdateTitle());
				break;

			case HE_TEXTAREA:
				FormValue *formvalue = element->GetFormValue();
				if (!formvalue->IsChangedFromOriginalByUser(element))
					RETURN_IF_ERROR(formvalue->ResetToDefault(element));
				break;
			}
		}
#ifdef SVG_SUPPORT
		else if (element->IsMatchingType(Markup::SVGE_TITLE, NS_SVG))
		{
			if (Window *window = frames_doc->GetWindow())
				RETURN_IF_ERROR(window->UpdateTitle());
			break;
		}
#endif // SVG_SUPPORT
		element = element->Parent();
	}

#ifdef SVG_SUPPORT
	if(is_change_inside_svg)
	{
		g_svg_manager->HandleCharacterDataChanged(frames_doc, this);
	}
#endif // SVG_SUPPORT

	if (update_pseudo_elm && GetUpdatePseudoElm() && hld_profile)
	{
		RETURN_IF_ERROR(hld_profile->GetLayoutWorkplace()->ApplyPropertyChanges(this, CSS_PSEUDO_CLASS_UNKNOWN, TRUE));
	}

	return OpStatus::OK;
}

OP_STATUS HTML_Element::BeforeAttributeChange(const DocumentContext &context, ES_Thread *thread, int attr_idx, short attr, int ns_idx, BOOL is_same_value)
{
	FramesDocument *frames_doc = context.frames_doc;
	LogicalDocument *logdoc = context.logdoc;
#if defined ACCESS_KEYS_SUPPORT || defined CSS_VIEWPORT_SUPPORT
	HLDocProfile *hld_profile = context.hld_profile;
#endif

	BOOL name_or_id_changed = FALSE;

	if (g_ns_manager->GetNsTypeAt(ResolveNsIdx(ns_idx)) == NS_HTML)
	{
		if (!is_same_value)
		{
			if ((attr == ATTR_NAME
				|| attr == ATTR_TYPE
				|| attr == ATTR_FORM)
				&& IsMatchingType(HE_INPUT, NS_HTML) && GetInputType() == INPUT_RADIO)
			{
				// Unregister from current radio group
				// Changing radio group
				if (logdoc)
					logdoc->GetRadioGroups().UnregisterFromRadioGroup(frames_doc, this);
			}
			else if (attr == ATTR_TYPE && IsMatchingType(HE_INPUT, NS_HTML) && GetInputType() == INPUT_FILE)
			{
				// Clear value when setting from file input to something else

				FormValue* old_form_value = GetFormValue();
				OP_ASSERT(old_form_value); // Every HE_INPUT must have a FormValue
				old_form_value->SetValueFromText(this, NULL);
			}
			else if (attr == ATTR_ID && IsMatchingType(HE_FORM, NS_HTML))
			{
				// Any radio buttons anywhere using ATTR_FORM to connect to this
				// form will/might become free radio buttons
				if (logdoc)
					logdoc->GetRadioGroups().UnregisterAllRadioButtonsConnectedByName(this);
				// This must be followed by a rebuilding of the radio groups
			}
#ifdef ACCESS_KEYS_SUPPORT
			else if (attr == ATTR_ACCESSKEY)
			{
				const uni_char *key = GetAccesskey();
				if (key && hld_profile)
					hld_profile->RemoveAccessKey(key, this);
			}
#endif // ACCESS_KEYS_SUPPORT
#ifdef CSS_VIEWPORT_SUPPORT
			else if (attr == ATTR_NAME && attr_idx != -1 && IsMatchingType(HE_META, NS_HTML))
			{
				if (GetItemType(attr_idx) == ITEM_TYPE_STRING)
				{
					const uni_char* old_name = static_cast<uni_char*>(GetValueItem(attr_idx));
					if (old_name && uni_stri_eq(old_name, "VIEWPORT"))
					{
						CSS_ViewportMeta* vp_meta = GetViewportMeta(context, FALSE);
						if (vp_meta)
						{
							if (hld_profile)
								hld_profile->GetCSSCollection()->RemoveCollectionElement(this);
							RemoveSpecialAttribute(ATTR_VIEWPORT_META, SpecialNs::NS_STYLE);
						}
					}
				}
			}
#endif // CSS_VIEWPORT_SUPPORT
			else if (attr == ATTR_HREF && IsMatchingType(HE_LINK, NS_HTML) && GetCSS())
			{
				// Need to remove the stylesheet from the CSSCollection before the url
				// attribute changes because the skip-optimization in CSSCollection
				// uses the url attribute to check the origin of the stylesheet being
				// removed.
				RemoveCSS(context);
			}
		}

		if (attr == ATTR_SRC)
		{
			if (IsMatchingType(HE_SCRIPT, NS_HTML))
			{
				BOOL in_document = FALSE;
				if (logdoc)
				{
					HTML_Element *root = logdoc->GetRoot();
					if (root && root->IsAncestorOf(this))
						in_document = TRUE;
				}

				if (in_document && !HasSpecialAttr(Markup::LOGA_ORIGINAL_SRC, SpecialNs::NS_LOGDOC))
				{
					URL *old_src_url = GetUrlAttr(Markup::HA_SRC, NS_IDX_HTML, logdoc);
					if (old_src_url)
					{
						URL *original_src_url = OP_NEW(URL, (*old_src_url));
						if (!original_src_url || SetSpecialAttr(Markup::LOGA_ORIGINAL_SRC, ITEM_TYPE_URL, original_src_url, TRUE, SpecialNs::NS_LOGDOC) == -1)
						{
							OP_DELETE(original_src_url);
							return OpStatus::ERR_NO_MEMORY;
						}
					}
				}
			}
#ifdef MEDIA_HTML_SUPPORT
			else if (IsMatchingType(Markup::HTE_TRACK, NS_HTML))
			{
				URL* track_url = GetUrlAttr(Markup::HA_SRC, NS_IDX_HTML, logdoc);
				if (track_url && frames_doc)
					frames_doc->StopLoadingInline(track_url, this, TRACK_INLINE);
			}
#endif // MEDIA_HTML_SUPPORT
			else
			{
				URL old_url = logdoc ? GetImageURL(TRUE, logdoc) : URL();

				if (frames_doc)
					frames_doc->StopLoadingInline(&old_url, this, IMAGE_INLINE, TRUE);
			}
		}
		else if (attr == ATTR_NAME && !is_same_value)
			name_or_id_changed = TRUE;

#ifdef JS_PLUGIN_SUPPORT
		if (Type() == HE_PARAM && attr == ATTR_NAME && Parent() && !is_same_value)
		{
			JS_Plugin_Object* jso = Parent()->GetJSPluginObject();
			if (jso)
			{
				const uni_char* name = GetPARAM_Name();
				if (name)
					jso->ParamSet(name, NULL);
			}
		}
#endif //JS_PLUGIN_SUPPORT
	}

	if (attr_idx != -1 && GetAttrIsId(attr_idx) && !is_same_value)
		name_or_id_changed = TRUE;

	if (name_or_id_changed && logdoc)
		return logdoc->RemoveNamedElement(this, FALSE);
	else
		return OpStatus::OK;
}

OP_STATUS HTML_Element::AfterAttributeChange(const DocumentContext &context, ES_Thread *thread, int attr_idx, short attr, int ns_idx, BOOL is_same_value)
{
	FramesDocument *frames_doc = context.frames_doc;
	LogicalDocument *logdoc = context.logdoc;
	DOM_Environment *environment = context.environment;

	NS_Type ns = g_ns_manager->GetNsTypeAt(ResolveNsIdx(ns_idx));

	HTML_Element *root = logdoc ? logdoc->GetRoot() : NULL;
	BOOL is_in_document = root && root->IsAncestorOf(this);

	if (frames_doc && logdoc)
	{
		// First a block of things that should be done regardless of whether
		// the change is in a document fragment or in the real document
		if (ns == NS_HTML && GetNsType() == NS_HTML)
		{
			HTML_ElementType type = Type();

			// These should always run, regardless of the document we're in or if we're a fragment.
			if (attr == ATTR_SRC)
			{
				if (type == HE_IMG || type == HE_INPUT && GetInputType() == INPUT_IMAGE)
				{
					HEListElm *helistelm = GetHEListElmForInline(IMAGE_INLINE);
					if (helistelm)
						helistelm->ResetEventSent();

					if (GetSpecialBoolAttr(ATTR_INLINE_ONLOAD_SENT, SpecialNs::NS_LOGDOC))
						SetSpecialBoolAttr(ATTR_INLINE_ONLOAD_SENT, FALSE, SpecialNs::NS_LOGDOC);

					URL url = GetImageURL(TRUE, logdoc);

					if (!url.IsEmpty() && frames_doc->GetLoadImages())
					{
						OP_LOAD_INLINE_STATUS load_status = frames_doc->LoadInline(&url, this, IMAGE_INLINE);

						helistelm = GetHEListElmForInline(IMAGE_INLINE);
						if (helistelm && !helistelm->GetLoadInlineElm())
						{
							// Remove dangling HEListElm since we did not load or started loading the image.
							OP_DELETE(helistelm);
							helistelm = NULL;
						}

						if (load_status == LoadInlineStatus::LOADING_REFUSED)
						{
							// Fake that we loaded it even though it was blocked (backwards compatible).
							load_status = frames_doc->HandleEvent(ONLOAD, NULL, this, SHIFTKEY_NONE);
						}
						else if (load_status != LoadInlineStatus::LOADING_STARTED)
						{
							// if LOADING_STARTED then an event will be sent through normal channels
							if (helistelm && !helistelm->GetEventSent())
								load_status = helistelm->SendImageFinishedLoadingEvent(frames_doc);
						}

						RETURN_IF_MEMORY_ERROR(load_status);
					}
					else if (helistelm)
					{
						// Was kept around to keep old_url available but if we're to replace it with
						// nothing we don't need or want it anymore
						OP_DELETE(helistelm);
					}
				} // end if HE_IMG/INPUT_IMAGE
			} // end if ATTR_SRC

#ifdef CANVAS_SUPPORT
			if (Type() == HE_CANVAS && (attr == ATTR_WIDTH || attr == ATTR_HEIGHT))
			{
				Canvas* canvas = (Canvas*)GetSpecialAttr(Markup::VEGAA_CANVAS, ITEM_TYPE_COMPLEX, NULL, SpecialNs::NS_OGP);
				if (canvas)
				{
					int w, h;
					if (attr == ATTR_WIDTH)
					{
						w = GetNumAttr(ATTR_WIDTH);
						if (w <= 0)
							w = 300;
						h = canvas->GetHeight(this);
					}
					else
					{
						w = canvas->GetWidth(this);
						h = GetNumAttr(ATTR_HEIGHT);
						if (h <= 0)
							h = 150;
					}

					RETURN_IF_ERROR(canvas->SetSize(w, h));
				}
			}
#endif // CANVAS_SUPPORT
#ifdef MEDIA_HTML_SUPPORT
			if (Type() == HE_AUDIO || Type() == HE_VIDEO)
			{
				if (MediaElement* media = GetMediaElement())
					RETURN_IF_ERROR(media->HandleAttributeChange(this, attr, thread));
			}
			else if (Type() == Markup::HTE_TRACK)
				if (TrackElement* track_element = GetTrackElement())
					RETURN_IF_ERROR(track_element->HandleAttributeChange(frames_doc, this, attr, thread));
#endif // MEDIA_HTML_SUPPORT

			if (IsFormElement())
			{
				FormValue* form_value = GetFormValue();
				form_value->SetMarkedPseudoClasses(form_value->CalculateFormPseudoClasses(frames_doc, this));
			}

		} // end if NS_HTML
	}

	BOOL name_or_id_changed = ns == NS_HTML && GetNsType() == NS_HTML && (attr == ATTR_ID || attr == ATTR_NAME);
	if (!is_same_value && (name_or_id_changed || attr_idx != -1 && GetAttrIsId(attr_idx)))
	{
		if (environment)
			environment->ElementCollectionStatusChanged(this, DOM_Environment::COLLECTION_NAME_OR_ID);

		if (is_in_document)
		{
			RETURN_IF_ERROR(logdoc->AddNamedElement(this, FALSE));

#ifdef XML_EVENTS_SUPPORT
			if (attr_idx != -1 && GetAttrIsId(attr_idx))
			{
				XML_Events_Registration *registration = frames_doc ? frames_doc->GetFirstXMLEventsRegistration() : NULL;
				while (registration)
				{
					RETURN_IF_ERROR(registration->HandleIdChanged(frames_doc, this));
					registration = static_cast<XML_Events_Registration *>(registration->Suc());
				}
			}
#endif // XML_EVENTS_SUPPORT
		}
	}

	if (!is_in_document)
		return OpStatus::OK;

	OP_ASSERT(frames_doc && logdoc); // Since we're in the document

	if (ns == NS_HTML && GetNsType() == NS_HTML)
	{
		// Whenever a script sets src on an iframe, it should be (re)loaded even
		// if it's the same URL.
		HTML_ElementType type = Type();
		if (frames_doc->IsCurrentDoc() && (attr == ATTR_SRC && (type == HE_FRAME || type == HE_IFRAME) ||
										   attr == ATTR_DATA && (type == HE_OBJECT)))
		{
			FramesDocElm *fde = FramesDocElm::GetFrmDocElmByHTML(this);

			if (!fde)
			{
				RETURN_IF_ERROR(frames_doc->Reflow(FALSE));
				fde = FramesDocElm::GetFrmDocElmByHTML(this);
			}

			if (fde)
			{
				DocumentManager *fde_doc_man = fde->GetDocManager();
				FramesDocument *fde_frames_doc = fde_doc_man->GetCurrentDoc();

				short src_attr = type == HE_OBJECT ? ATTR_DATA : ATTR_SRC;
				if (URL *url = GetUrlAttr(src_attr, NS_IDX_HTML, frames_doc->GetLogicalDocument()))
				{
					DocumentReferrer ref_url = frames_doc->GetDOMEnvironment()->GetCurrentScriptURL();
					if (ref_url.IsEmpty())
					{
						ES_Thread *interrupt_thread = thread;
						while (interrupt_thread)
							if (interrupt_thread->Type() == ES_THREAD_JAVASCRIPT_URL)
							{
								ref_url.url = static_cast<ES_JavascriptURLThread*>(interrupt_thread)->GetURL();
								break;
							}
							else
								interrupt_thread = interrupt_thread->GetInterruptedThread();
					}

					// Gecko and Webkit open about:blank on frame.src = ''.
					if (url->IsEmpty() && src_attr == ATTR_SRC && (type == HE_IFRAME || type == HE_FRAME))
						*url = g_url_api->GetURL("about:blank");

					if (fde_frames_doc)
					{
						DocumentManager::OpenURLOptions options;
						options.user_initiated = FALSE;
						options.entered_by_user = NotEnteredByUser;
						options.is_walking_in_history = FALSE;
						options.origin_thread = thread;
						options.from_html_attribute = TRUE;
						RETURN_IF_MEMORY_ERROR(fde_frames_doc->ESOpenURL(*url, DocumentReferrer(ref_url), TRUE, FALSE, FALSE, options));
					}
					else
						fde_doc_man->OpenURL(*url, ref_url, TRUE, FALSE, FALSE, FALSE, NotEnteredByUser, FALSE, FALSE, FALSE);

					if (thread)
						thread->Pause();
				}
			}
		} // end src/data on iframe/frame/object

		if (!is_same_value)
		{
			switch(attr)
			{
			case ATTR_FORM:
			// fallthrough
			case ATTR_NAME:
				if (Type() == HE_INPUT && GetInputType() == INPUT_RADIO)
				{
					// Changing radio group
					RETURN_IF_ERROR(logdoc->GetRadioGroups().RegisterRadio(FormManager::FindFormElm(frames_doc, this), this));
				}
				break;

			case ATTR_ID:
				if (Type() == HE_FORM)
				{
					// Radio buttons connected to this with a form attribute will
					// now be connected to nothing and somethings not connected
					// to anything will now be connected to this
					RETURN_IF_ERROR(logdoc->GetRadioGroups().RegisterNewRadioButtons(frames_doc, this));
				}
				break;

			case ATTR_TYPE:
				if (Type() == HE_INPUT && GetInputType() == INPUT_RADIO)
				{
					// It became a radio button so we have to register it
					RETURN_IF_ERROR(logdoc->GetRadioGroups().RegisterRadio(FormManager::FindFormElm(frames_doc, this), this));
				}
				break;

#ifdef ACCESS_KEYS_SUPPORT
			case ATTR_ACCESSKEY:
				{
					const uni_char* key = GetAccesskey();
					if (key)
						RETURN_IF_MEMORY_ERROR(logdoc->GetHLDocProfile()->AddAccessKey(key, this));
				}
				break;
#endif // ACCESS_KEYS_SUPPORT
			}
		} // end if (!is_same_value)
	} // end if (NS_HTML)

	return OpStatus::OK;
}

void HTML_Element::UpdateCollectionStatus(const DocumentContext &context, short attr, NS_Type ns, BOOL in_document)
{
	if (context.environment && ns == NS_HTML && GetNsType() == NS_HTML)
	{
		unsigned collections = 0;
		int type = Type();

		switch (attr)
		{
		case ATTR_CLASS:
			collections |= DOM_Environment::COLLECTION_CLASSNAME;
			break;

		case ATTR_TYPE:
			if (type == HE_INPUT)
				collections |= DOM_Environment::COLLECTION_FORM_ELEMENTS;
			break;

		case ATTR_HREF:
			if (in_document && (type == HE_A || type == HE_AREA))
				collections |= DOM_Environment::COLLECTION_LINKS;
			break;

		case ATTR_ITEMTYPE:
			if (in_document)
				collections |= DOM_Environment::COLLECTION_MICRODATA_TOPLEVEL_ITEMS;
			break;
		case ATTR_ITEMSCOPE:
		case ATTR_ITEMPROP:
			collections |= DOM_Environment::COLLECTION_MICRODATA_PROPERTIES;
			if (in_document)
				collections |= DOM_Environment::COLLECTION_MICRODATA_TOPLEVEL_ITEMS;
			break;
		case ATTR_ID:
		case ATTR_ITEMREF:
			collections |= DOM_Environment::COLLECTION_MICRODATA_PROPERTIES;
			break;

		case ATTR_REL:
			if (in_document && type == HE_LINK)
				collections |= DOM_Environment::COLLECTION_STYLESHEETS;
			break;

		case ATTR_FORM:
			if (IsFormElement())
				collections |= DOM_Environment::COLLECTION_FORM_ELEMENTS;
			break;

		case ATTR_FOR:
			if (type == HE_LABEL)
				collections |= DOM_Environment::COLLECTION_LABELS;
			break;

		case ATTR_DATA:
			if (in_document && type == HE_OBJECT)
				collections |= DOM_Environment::COLLECTION_APPLETS | DOM_Environment::COLLECTION_PLUGINS;
			break;
		}

		if (collections != 0)
			context.environment->ElementCollectionStatusChanged(this, collections);
	}
}

const uni_char *HTML_Element::GetAttrName(int attr_idx, Markup::AttrType attr, NS_Type ns)
{
	if (attr == ATTR_XML)
	{
		if (attr_idx >= 0)
			return (const uni_char*)GetValueItem(attr_idx);
		else
		{
			OP_ASSERT(!"No idea what the name of the attribute is");
			return NULL;
		}
	}
	else
	{
		return HTM_Lex::GetAttributeString(attr, ns);
	}
}

OP_STATUS HTML_Element::HandleAttributeChange(const DocumentContext &context, ES_Thread *thread, int attr_idx, Markup::AttrType attr, int ns_idx, BOOL was_removed, const uni_char* attr_name)
{
	FramesDocument *frames_doc = context.frames_doc;
	LogicalDocument *logdoc = context.logdoc;
	HLDocProfile *hld_profile = context.hld_profile;
	DOM_Environment *environment = context.environment;

	NS_Type ns = g_ns_manager->GetNsTypeAt(ResolveNsIdx(ns_idx));
	BOOL in_document = FALSE;

	if (logdoc)
	{
		HTML_Element *root = logdoc->GetRoot();
		if (root && root->IsAncestorOf(this))
			in_document = TRUE;
	}

	UpdateCollectionStatus(context, attr, ns, in_document);

	if (attr == ATTR_TYPE && ns == NS_HTML && IsMatchingType(HE_INPUT, NS_HTML))
		RETURN_IF_ERROR(HandleInputTypeChange(context));

	if (!frames_doc)
		return OpStatus::OK;

	// Clear the url cache if an attribute used through GetUrlAttr changes
	if (ns == NS_HTML && GetNsType() == NS_HTML)
	{
		HTML_ElementType type = Type();

#ifdef JS_PLUGIN_SUPPORT
		// Notify existing jsplugins of the change of attribute
		if (type == HE_OBJECT)
		{
			JS_Plugin_Object* jso = GetJSPluginObject();
			if (jso)
			{
				if (!was_removed || attr != ATTR_XML)
					attr_name = GetAttrName(attr_idx, attr, ns);
				const uni_char *attr_value = NULL;
				TempBuffer buffer;
				if (!was_removed)
						attr_value = GetAttribute(frames_doc, attr, attr_name, ns, TRUE, &buffer, FALSE, attr_idx);
				if (attr_name)
					jso->AttributeChanged(attr_name, attr_value);
			}
		}
#endif //JS_PLUGIN_SUPPORT

		// These should always run, regardless of the document we're in or if we're a fragment.
		switch (attr)
		{
			// Clear the url cache if the attribute change
		case ATTR_DATA:
		case ATTR_CODEBASE:
			ClearResolvedUrls();
			break;

		case ATTR_HREF:
			if (type == HE_BASE && in_document)
			{
				HLDocProfile *hld_profile = frames_doc->GetHLDocProfile();
				URL *url = GetA_URL(frames_doc->GetLogicalDocument());
				hld_profile->SetBaseURL(url);
				break;
			}
			break;

		case ATTR_TYPE:
#ifdef JS_PLUGIN_SUPPORT
			if (type == HE_OBJECT)
			{
				// If we insert a jsplugin using JavaScript, we need to
				// handle that just as if we had parsed it.

				// FIXME: No handling of overwriting an existing one yet.
				ES_Object *esobj = DOM_Utils::GetJSPluginObject(GetESElement());
				if (!esobj)
					RETURN_IF_MEMORY_ERROR(SetupJsPluginIfRequested(GetStringAttr(ATTR_TYPE), hld_profile));
			}
#endif
			break;

		case ATTR_VALUE:
			// Setting the value attribute at certain input types should affect the
			// value of the form element.
			if (type == HE_INPUT)
			{
				switch (GetInputType())
				{
				case INPUT_HIDDEN:
				case INPUT_CHECKBOX:
				case INPUT_RADIO:
				case INPUT_FILE:
					break;

				default:
					FormValue* form_value = GetFormValue();

					if (form_value->GetType() == FormValue::VALUE_NO_OWN_VALUE)
					{
						FormValueNoOwnValue* form_value_no_own_value = FormValueNoOwnValue::GetAs(form_value);
						form_value_no_own_value->SetValueAttributeFromText(this, GetValue());
					}
					else
						RETURN_IF_ERROR(form_value->SetValueFromText(this, GetValue()));
				}
			}
			break;

		case ATTR_MULTIPLE:
			if (type == Markup::HTE_INPUT && GetInputType() == INPUT_FILE)
				if (FormObject *form_object = GetFormObject())
					form_object->SetMultiple(GetBoolAttr(attr, ns_idx));
			break;

		case ATTR_SELECTED:
			if (type == Markup::HTE_OPTION && (was_removed || GetBoolAttr(attr, ns_idx)))
			{
				if (HTML_Element *select = FindOptionParent(this))
					if (select->IsMatchingType(HE_SELECT, NS_HTML))
					{
						FormValueList *form_value_list = FormValueList::GetAs(select->GetFormValue());
						if (!was_removed)
							RETURN_IF_ERROR(form_value_list->UpdateSelectedValue(select, this));
						else
							RETURN_IF_ERROR(form_value_list->SetInitialSelection(select, TRUE));
					}
			}
			break;

		case ATTR_CHECKED:
			if (type == HE_INPUT && (GetInputType() == INPUT_RADIO || GetInputType() == INPUT_CHECKBOX))
			{
				FormValue* form_value = GetFormValue();
				FormValueRadioCheck* bool_value = FormValueRadioCheck::GetAs(form_value);
				// Sometimes the checked attribute will modify the state of
				// the checkbox/radio button. Sometimes it isn't.
				if (bool_value->IsCheckedAttrChangingState(frames_doc, this))
				{
					SetBoolFormValue(frames_doc, GetChecked());
					logdoc->GetLayoutWorkplace()->ApplyPropertyChanges(this, CSS_PSEUDO_CLASS_GROUP_FORM, TRUE);
				}
			}
			break;
		}
	} // end if (NS_HTML)

	if (environment)
	{
		if (!was_removed || attr != ATTR_XML)
			attr_name = GetAttrName(attr_idx, attr, ns);

		if (attr_name)
			RETURN_IF_ERROR(environment->ElementAttributeChanged(this, attr_name, ns_idx));
	}

	if (!logdoc)
		return OpStatus::OK;

	if (!in_document)
		return OpStatus::OK;

#ifdef NS4P_COMPONENT_PLUGINS
	if (logdoc->GetLayoutWorkplace()->IsTraversing() || logdoc->GetLayoutWorkplace()->IsReflowing())
		return OpStatus::ERR_NOT_SUPPORTED;
#endif // NS4P_COMPONENT_PLUGINS

	logdoc->GetLayoutWorkplace()->ApplyPropertyChanges(this, 0, TRUE, attr, ns);

	if (ns == NS_HTML && GetNsType() == NS_HTML)
	{
		HTML_ElementType type = Type();

		switch (attr)
		{
		case ATTR_DISABLED:
			if (type == HE_SELECT || type == HE_INPUT
				|| type == HE_TEXTAREA || type == HE_OPTION
				|| type == HE_BUTTON
				|| type == HE_FIELDSET)
			{
				// Setting a field disabled might move the focus, and that must happen now,
				// not during a paint or some other vulnerable time.
				BOOL disabled = GetDisabled();
				FormObject* form_object = GetFormObject();
				if (form_object)
				{
					BOOL regain_focus = FALSE;
					if (!disabled)
						if (HTML_Document *html_document = frames_doc->GetHtmlDocument())
							regain_focus = html_document->GetFocusedElement() == this;

					form_object->SetEnabled(!disabled, regain_focus);
				}

				// If someone is disabling something during submit or
				// unload, then the page will probably not work correctly
				// if not reloaded.
				if (disabled && thread)
				{
					ES_ThreadInfo info = thread->GetOriginInfo();

					if (info.type == ES_THREAD_EVENT &&
						(info.data.event.type == ONSUBMIT ||
						 info.data.event.type == ONUNLOAD ||
						 info.data.event.type == ONMOUSEUP ||
						 info.data.event.type == ONMOUSEDOWN ||
						 info.data.event.type == ONCLICK))
					{
						// This has room for improvement. Since this is something
						// we would like to avoid, we could for instance limit it
						// to cases where the same thread is used for a submit.
						frames_doc->SetCompatibleHistoryNavigationNeeded();
					}
				}

				logdoc->GetLayoutWorkplace()->ApplyPropertyChanges(this, CSS_PSEUDO_CLASS_GROUP_FORM, TRUE);
			}
			break;

		case ATTR_READONLY:
			if (type == HE_INPUT || type == HE_TEXTAREA)
			{
				if (FormObject *form_object = GetFormObject())
					form_object->SetReadOnly(GetBoolAttr(attr, ns_idx));
				logdoc->GetLayoutWorkplace()->ApplyPropertyChanges(this, CSS_PSEUDO_CLASS_GROUP_FORM, TRUE);
			}
			break;

		case ATTR_MEDIA:
			if (type == HE_LINK || type == HE_STYLE)
			{
				if (CSS* stylesheet = GetCSS())
				{
					stylesheet->Removed(hld_profile->GetCSSCollection(), frames_doc);
					stylesheet->Added(hld_profile->GetCSSCollection(), frames_doc);
					stylesheet->MediaAttrChanged();
				}
			}
			break;

		case ATTR_HREF:
#ifdef DNS_PREFETCHING
			if (type == HE_A || type == HE_LINK || type == HE_AREA)
			{
				URL* anchor_url = GetUrlAttr(ATTR_HREF, NS_IDX_HTML, logdoc);
				if (anchor_url && !anchor_url->IsEmpty())
					logdoc->DNSPrefetch(*anchor_url, DNS_PREFETCH_DYNAMIC);
			}
			// Fall through
#endif // DNS_PREFETCHING
		case ATTR_REL:
			if (type == HE_LINK)
			{
				// Stylesheet changes (most likely). Trigger the update code by faking a removal and insertion into the tree.
				RETURN_IF_ERROR(HandleDocumentTreeChange(context, Parent(), this, thread, FALSE));
				RETURN_IF_ERROR(HandleDocumentTreeChange(context, Parent(), this, thread, TRUE));

				if (attr == ATTR_REL)
					g_input_manager->UpdateAllInputStates();
			}
			break;

		case ATTR_SRC:
			if (type == HE_SCRIPT)
			{
#ifdef USER_JAVASCRIPT
				if (environment && environment->IsHandlingScriptElement(this))
					break;
#endif // USER_JAVASCRIPT

				int handled = GetSpecialNumAttr(ATTR_JS_SCRIPT_HANDLED, SpecialNs::NS_LOGDOC);

				/* Script already executed. */
				if ((handled & SCRIPT_HANDLED_EXECUTED) == SCRIPT_HANDLED_EXECUTED)
					break;

				if (!was_removed)
				{
					BOOL handle_change = TRUE;
					URL *original_src_url = static_cast<URL*>(GetSpecialAttr(Markup::LOGA_ORIGINAL_SRC, ITEM_TYPE_URL, NULL, SpecialNs::NS_LOGDOC));
					if (original_src_url)
					{
						URL *current_src_url = GetUrlAttr(Markup::HA_SRC, NS_IDX_HTML, logdoc);
						handle_change = original_src_url == current_src_url;
					}

					if (handle_change)
					{
						BOOL parser_inserted = GetInserted() == HE_NOT_INSERTED;
						BOOL parser_blocking = parser_inserted && logdoc->GetParser();
						if (logdoc->IsXml())
							parser_blocking = FALSE;
						if (parser_blocking)
							RETURN_IF_MEMORY_ERROR(logdoc->GetParser()->AddBlockingScript(this));
						RETURN_IF_ERROR(HandleScriptElement(hld_profile, thread, TRUE, parser_inserted));
					}
				}
			}
#ifdef _PLUGIN_SUPPORT_
			// Fall through
		case ATTR_DATA:
			if (type == HE_OBJECT && attr == ATTR_DATA || type == HE_EMBED && attr == ATTR_SRC)
				logdoc->GetLayoutWorkplace()->ResetPlugin(this);
			else if (type == HE_IFRAME && attr == ATTR_SRC)
				logdoc->GetLayoutWorkplace()->HideIFrame(this);
#endif // _PLUGIN_SUPPORT_
			break;

		case ATTR_VALUE:
			if (type == HE_PARAM && Parent())
				Parent()->MarkExtraDirty(frames_doc);
			else if (type == HE_INPUT)
				switch (GetInputType())
				{
				case INPUT_BUTTON:
				case INPUT_HIDDEN:
				case INPUT_SUBMIT:
				case INPUT_RESET:
				case INPUT_IMAGE:
				case INPUT_CHECKBOX:
				case INPUT_RADIO:
					break;

				default:
					const uni_char *value = GetValue();
					RETURN_IF_ERROR(DOMSetFormValue(environment, value ? value : UNI_L("")));
				}
			break;

			// fall through
			// For every attribute change that may change a form elements validity
			// or other css3 pseudo class, we have to recalculate pseudo classes

		case ATTR_MIN:
		case ATTR_MAX:
		case ATTR_STEP:
			if (type == Markup::HTE_INPUT && GetInputType() == INPUT_RANGE)
			{
				if (FormObject* form_object = GetFormObject())
				{
					if (attr != ATTR_STEP)
					{
						double value = 0.0;
						if (!was_removed)
						{
							const uni_char *str = static_cast<const uni_char *>(GetAttr(attr, ITEM_TYPE_STRING, NULL, ns_idx));
							OP_ASSERT(str);
							value = uni_strtod(str, NULL);
						}
						else if (attr == ATTR_MAX)
							value = 100.0;

						if (attr == ATTR_MIN)
							form_object->SetMinMax(&value, NULL);
						else
							form_object->SetMinMax(NULL, &value);

					}
				}
				FormValue *form_value = GetFormValue();
				OP_ASSERT(form_value);

				OpString text_value;
				RETURN_IF_ERROR(form_value->GetValueAsText(this, text_value));
				RETURN_IF_ERROR(form_value->SetValueFromText(this, text_value));
			}
			// fall through
		case ATTR_MAXLENGTH:
		case ATTR_PATTERN:
		case ATTR_REQUIRED:
			if (IsFormElement())
				logdoc->GetLayoutWorkplace()->ApplyPropertyChanges(this, CSS_PSEUDO_CLASS_GROUP_FORM, TRUE);
			break;
#ifdef DOCUMENT_EDIT_SUPPORT
		case ATTR_CONTENTEDITABLE:
			if (IsContentEditable())
				RETURN_IF_ERROR(EnableContentEditable(frames_doc));
			else
				DisableContentEditable(frames_doc);
			break;
#endif // DOCUMENT_EDIT_SUPPORT
		}

		switch (type)
		{
		case HE_FRAME:
		case HE_IFRAME:
		case HE_OBJECT:
			{
				FramesDocElm *fde = FramesDocElm::GetFrmDocElmByHTML(this);
				if (fde)
				{
					if (type != HE_OBJECT && (attr == ATTR_MARGINHEIGHT || attr == ATTR_MARGINWIDTH))
					{
						fde->UpdateFrameMargins(this);

						DocumentManager *fde_doc_man = fde->GetDocManager();
						FramesDocument *fde_frames_doc = fde_doc_man->GetCurrentDoc();

						if (fde_frames_doc)
							if (LogicalDocument *fde_logdoc = fde_frames_doc->GetLogicalDocument())
								if (HTML_Element *fde_body = fde_logdoc->GetBodyElm())
									fde_body->MarkDirty(fde_frames_doc);
					}
					else if (attr == ATTR_NAME)
					{
						const uni_char *name = GetStringAttr(ATTR_NAME, NS_IDX_HTML);
						RETURN_IF_ERROR(fde->SetName(name));
					}
					else if (attr == ATTR_NORESIZE)
						fde->SetFrameNoresize(GetBoolAttr(ATTR_NORESIZE, NS_IDX_HTML));
					else if (attr == ATTR_SCROLLING)
					{
						fde->SetFrameScrolling(GetFrameScrolling());
						if (frames_doc->GetFramesPolicy() != FRAMES_POLICY_FRAME_STACKING)
						{
							fde->GetVisualDevice()->SetScrollType((VisualDevice::ScrollType)fde->GetFrameScrolling());
							fde->GetVisualDevice()->UpdateScrollbars();
						}
					}
				}
			}
			break;

		case HE_SELECT:
		case HE_TEXTAREA:
			if (type == HE_SELECT && (attr == ATTR_SIZE || attr == ATTR_MULTIPLE) ||
				type == HE_TEXTAREA && (attr == ATTR_COLS || attr == ATTR_ROWS))
			{
				if (layout_box)
					layout_box->MarkDisabledContent(frames_doc);
			}
			break;

		case HE_PARAM:
			if (attr == ATTR_NAME || attr == ATTR_VALUE)
			{
				if (Parent())
					Parent()->MarkExtraDirty(frames_doc);
			}
#ifdef JS_PLUGIN_SUPPORT
			if (Parent() && (attr == ATTR_VALUE || attr == ATTR_NAME))
			{
				JS_Plugin_Object* jso = Parent()->GetJSPluginObject();
				if (jso)
				{
					const uni_char* name = GetPARAM_Name();
					const uni_char* value = NULL;
					if (!was_removed)
						value = GetPARAM_Value();
					if (name)
						jso->ParamSet(name, value);
				}
			}
#endif //JS_PLUGIN_SUPPORT

			break;

#ifdef CSS_VIEWPORT_SUPPORT
		case HE_META:
			if (attr == ATTR_CONTENT || attr == ATTR_NAME)
			{
				// If this is a viewport meta (or just became one),
				// we need to parse the content attribute and store
				// the result in a CSS_ViewportMeta object.

				const uni_char* name = GetStringAttr(ATTR_NAME);

				if (name && uni_stri_eq(name, "VIEWPORT"))
				{
					const uni_char* content = GetStringAttr(ATTR_CONTENT);
					BOOL create = content && *content;
					CSS_ViewportMeta* viewport_meta = GetViewportMeta(context, create);
					if (viewport_meta)
					{
						OP_ASSERT(in_document && hld_profile);
						viewport_meta->ParseContent(content);
						CSSCollection* coll = hld_profile->GetCSSCollection();
						if (coll->IsInCollection(viewport_meta))
							viewport_meta->Added(coll, frames_doc);
						else
							coll->AddCollectionElement(viewport_meta);
					}
					else if (create)
						return OpStatus::ERR_NO_MEMORY;
				}
			}
			break;
#endif // CSS_VIEWPORT_SUPPORT
		}
	}
	else if (ns == NS_XML)
	{
#ifdef SVG_SUPPORT
		if (GetNsType() == NS_SVG)
			g_svg_manager->HandleSVGAttributeChange(frames_doc, this,
													attr, ns, was_removed);
#endif // SVG_SUPPORT

		// remove all cached URLs below this element
		if (attr == XMLA_BASE)
			ClearResolvedUrls();
	}
#ifdef SVG_SUPPORT
	else if ((ns == NS_SVG || ns == NS_XLINK) && GetNsType() == NS_SVG)
	{
		g_svg_manager->HandleSVGAttributeChange(frames_doc, this, attr, ns, was_removed);
	}
#endif // SVG_SUPPORT

	return OpStatus::OK;
}

OP_STATUS HTML_Element::HandleInputTypeChange(const DocumentContext &context)
{
	FramesDocument *frames_doc = context.frames_doc;
	LogicalDocument *logdoc = context.logdoc;
	HLDocProfile *hld_profile = context.hld_profile;

	FormValue* old_form_value = GetFormValue();
	OP_ASSERT(old_form_value); // Every HE_INPUT must have a FormValue

	BOOL updated_inplace = FALSE;

	/* If a transition between text-editable types, try
	   to perform the change in-place by reconfiguring the
	   underlying external representation. */
	if (old_form_value->GetType() == FormValue::VALUE_TEXT)
		if (FormObject *form_object = GetFormObject())
			updated_inplace = form_object->ChangeTypeInplace(this);

	BOOL refocus = FALSE;
	if (frames_doc)
		if (HTML_Document* html_doc = frames_doc->GetHtmlDocument())
			if (html_doc->GetFocusedElement() == this)
				refocus = TRUE;

	OP_STATUS status = OpStatus::OK;
	if (!updated_inplace)
	{
		/* Sacrifice old value and create new FormValue, copying
		   over the required state. */
		InputType new_input_type = GetInputType();

		FormValue* new_form_value;
		status = ConstructFormValue(hld_profile, new_form_value);
		if (OpStatus::IsSuccess(status))
		{
			// Must trigger an unexternalize so that the old value
			// is moved from the widget to the FormValue before the switch.
			// Otherwise the Unexternalize that will come later will have
			// a FormObject for the old type and a FormValue for the new type.
			if (layout_box)
				RemoveLayoutBox(frames_doc);

			new_form_value->PrepareToReplace(*old_form_value, this);
			if (new_input_type == INPUT_HIDDEN)
			{
				// Going from type=text to type=hidden should convert
				// the value into being the value attribute
				if (old_form_value->GetType() == FormValue::VALUE_TEXT)
				{
					uni_char* new_value = FormValueText::GetAs(old_form_value)->GetCopyOfInternalText();
					if (new_value)
					{
						int idx = SetAttr(ATTR_VALUE, ITEM_TYPE_STRING, new_value, TRUE);
						if (idx == -1) // OOM
							OP_DELETEA(new_value);
					}
				}
			}
			// This will delete the old formvalue
			SetSpecialAttr(ATTR_FORM_VALUE, ITEM_TYPE_COMPLEX, new_form_value, TRUE, SpecialNs::NS_LOGDOC);
			OP_ASSERT(GetFormValue() == new_form_value); // Since we're replacing it can't fail

			if (new_input_type == INPUT_FILE)
			{
				// *IMPORTANT* Don't preset a file value in any way!
				new_form_value->SetValueFromText(this, NULL);
				if (GetFormObject())
					RETURN_IF_ERROR(GetFormObject()->SetFormWidgetValue(UNI_L("")));
			}
		}
		else
		{
			// OOM, we can not change formvalue, let's change the type
			// back so that the formvalue and the type attribute are consistant
			const uni_char* old_input_type = old_form_value->GetFormValueTypeString(this);
			SetNumAttr(ATTR_TYPE, ATTR_GetKeyword(old_input_type, uni_strlen(old_input_type)));
		}
	}

	if (OpStatus::IsSuccess(status) && refocus)
	{
		frames_doc->GetHtmlDocument()->SetFocusedElement(this, FALSE);
		frames_doc->SetElementToRefocus(this);
	}
	if (logdoc)
		logdoc->GetLayoutWorkplace()->ApplyPropertyChanges(this, CSS_PSEUDO_CLASS_GROUP_FORM, TRUE);

	return status;
}

void HTML_Element::RemoveAttribute(short attr, int ns_idx /*=NS_IDX_HTML*/)
{
	int idx = FindAttrIndex(attr, NULL, ns_idx, FALSE);

	if (idx > -1)
	{
		RemoveAttrAtIdx(idx);
		OP_ASSERT(FindAttrIndex(attr, NULL, ns_idx, FALSE) == -1);
	}
}

void HTML_Element::RemoveSpecialAttribute(short attr, SpecialNs::Ns ns_idx)
{
	int index = FindSpecialAttrIndex(attr, ns_idx);
	if (index > -1)
	{
		RemoveAttrAtIdx(index);
		OP_ASSERT(FindSpecialAttrIndex(attr, static_cast<SpecialNs::Ns>(ns_idx)) == -1);
	}
}

void HTML_Element::RemoveAttrAtIdx(int idx)
{
	int to = GetAttrSize();
	if (0 <= idx && idx < to)
	{
		int last = to - 1;

		// find the last non-null attribute
		while (GetAttrItem(last) == ATTR_NULL)
			--last;

		if (idx != last)
		{
			ReplaceAttrLocal(idx, data.attrs[last].GetAttr(), data.attrs[last].GetType(), data.attrs[last].GetValue(), data.attrs[last].GetNsIdx(), data.attrs[last].NeedFree(), data.attrs[last].IsSpecial(), data.attrs[last].IsId(), data.attrs[last].IsSpecified());
			data.attrs[last].SetNeedFree(FALSE);
		}

		ReplaceAttrLocal(last, ATTR_NULL, ITEM_TYPE_BOOL, NULL, NS_IDX_DEFAULT, FALSE, TRUE);
	}
}

BOOL HTML_Element::IsNumericAttributeFloat(int attr)
{
	if (IsMatchingType(HE_METER, NS_HTML))
	{
		switch (attr)
		{
		case ATTR_MIN:
		case ATTR_MAX:
		case ATTR_LOW:
		case ATTR_HIGH:
		case ATTR_OPTIMUM:
		case ATTR_VALUE:
			return TRUE;
		};
	}
	else if (IsMatchingType(HE_PROGRESS, NS_HTML))
	{
		switch (attr)
		{
		case ATTR_MAX:
		case ATTR_VALUE:
			return TRUE;
		};
	}
	return FALSE;
}

int HTML_Element::DOMGetNamespaceIndex(DOM_Environment *environment, const uni_char *ns_uri, const uni_char *ns_prefix)
{
	int ns_uri_length = uni_strlen(ns_uri), ns_prefix_length = ns_prefix ? uni_strlen(ns_prefix) : 0;
	int ns_idx = g_ns_manager->GetNsIdx(ns_uri, ns_uri_length, ns_prefix, ns_prefix_length, FALSE, FALSE, TRUE);

	return ns_idx == NS_IDX_NOT_FOUND ? NS_IDX_ANY_NAMESPACE : ns_idx;
}

BOOL HTML_Element::DOMGetNamespaceData(DOM_Environment *environment, int ns_idx, const uni_char *&ns_uri, const uni_char *&ns_prefix)
{
	if (ns_idx != NS_IDX_DEFAULT)
		if (NS_Element *ns_element = g_ns_manager->GetElementAt(ns_idx))
		{
			ns_uri = ns_element->GetUri();
			ns_prefix = ns_element->GetPrefix();
			return TRUE;
		}

	return FALSE;
}

int HTML_Element::FindNamespaceIndex(const uni_char *ns_prefix)
{
	HTML_Element *element = this;

	while (element)
	{
		for (unsigned index = 0, count = element->GetAttrSize(); index < count; ++index)
			if (element->GetAttrNs(index) == NS_IDX_XMLNS)
			{
				const uni_char *prefix = (const uni_char *) element->GetValueItem(index);
				const uni_char *uri = prefix + uni_strlen(prefix) + 1;

				if (uni_str_eq(prefix, ns_prefix))
					return g_ns_manager->FindNsIdx(uri, uni_strlen(uri), prefix, uni_strlen(prefix));
			}

		element = element->ParentActual();
	}

	return NS_IDX_NOT_FOUND;
}

const uni_char *HTML_Element::DOMLookupNamespacePrefix(DOM_Environment *environment, const uni_char *ns_uri)
{
	HTML_Element *element = this;

	if (!ns_uri)
		ns_uri = UNI_L("");

	while (element)
	{
		// First check element's own namespace.
		const uni_char *this_elm_nsuri, *this_elm_prefix;
		if (DOMGetNamespaceData(environment, element->GetNsIdx(), this_elm_nsuri, this_elm_prefix))
			if (uni_str_eq(this_elm_nsuri, ns_uri))
				return this_elm_prefix;

		// Second, find xmlns attributes.
		for (unsigned index = 0, count = element->GetAttrSize(); index < count; ++index)
			if (element->GetAttrNs(index) == NS_IDX_XMLNS)
			{
				const uni_char *prefix = (const uni_char *) element->GetValueItem(index);
				const uni_char *uri = prefix + uni_strlen(prefix) + 1;

				if (uni_str_eq(uri, ns_uri))
				{
					const uni_char *ns_uri_tmp = DOMLookupNamespaceURI(environment, prefix);

					if (ns_uri_tmp && uni_str_eq(ns_uri, ns_uri_tmp))
						return prefix;
				}
			}

		element = element->ParentActual();
	}

	return NULL;
}

const uni_char *HTML_Element::DOMLookupNamespaceURI(DOM_Environment *environment, const uni_char *ns_prefix)
{
	HTML_Element *element = this;

	if (!ns_prefix)
		ns_prefix = UNI_L("");

	while (element)
	{
		if (element->GetNsIdx() > 0)
		{
			NS_Element *nselement = g_ns_manager->GetElementAt(element->GetNsIdx());
			if (uni_strcmp(nselement->GetPrefix(), ns_prefix) == 0)
				return nselement->GetUri();
		}

		for (unsigned index = 0, count = element->GetAttrSize(); index < count; ++index)
		{
			int attr_ns_idx = element->GetAttrNs(index);
			const uni_char *uri = static_cast<const uni_char*>(element->GetValueItem(index));
			if (attr_ns_idx == NS_IDX_XMLNS)
			{
				const uni_char *prefix;
				if (element->GetAttrItem(index) == Markup::HA_XML)
				{
					// HA_XML attributes are stored as name\0value
					prefix = uri;
					uri = prefix + uni_strlen(prefix) + 1;
				}
				else
					prefix = element->GetAttrNameString(index);

				if (uni_str_eq(prefix, ns_prefix))
					return *uri ? uri : NULL;
			}
			else if (attr_ns_idx == NS_IDX_XMLNS_DEF)
			{
				if (!*ns_prefix)
					return *uri ? uri : NULL;
			}
		}

		element = element->ParentActual();
	}

	return NULL;
}

const uni_char*
HTML_Element::DOMGetElementName(DOM_Environment *environment, TempBuffer *buffer, int &ns_idx, BOOL uppercase)
{
	ns_idx = GetNsIdx();
	// HTML elements are always in the XHTML namespace in HTML 5
	BOOL ascii_uppercase = uppercase && GetNsType() == NS_HTML;

	return GetTagName(ascii_uppercase, buffer);
}

BOOL HTML_Element::DOMHasAttribute(DOM_Environment *environment, int attr, const uni_char *name, int ns_idx, BOOL case_sensitive, int &found_at_index, BOOL specified)
{
	found_at_index = FindAttrIndex(attr, name, ns_idx, case_sensitive, TRUE);

	if (found_at_index != -1)
	{
		if (GetItemType(found_at_index) == ITEM_TYPE_BOOL)
		{
			BOOL is_html = g_ns_manager->GetNsTypeAt(GetResolvedAttrNs(found_at_index)) == NS_HTML;

			if (is_html && GetAttrItem(found_at_index) == ATTR_FRAMEBORDER)
				return TRUE;

			/* Note: the result here is (has-attribute-with-bool-type && has-non-false-value) */
			return GetValueItem(found_at_index) != NULL;
		}

		return !specified || GetAttrIsSpecified(found_at_index);
	}

	return FALSE;
}

BOOL HTML_Element::DOMHasAttribute(DOM_Environment *environment, const uni_char *name, int ns_idx, BOOL specified)
{
	int index;
	return DOMHasAttribute(environment, ATTR_XML, name, ns_idx, FALSE, index, specified);
}

int HTML_Element::DOMGetAttributeCount()
{
	return GetAttributeCount();
}

void HTML_Element::DOMGetAttributeName(DOM_Environment* environment, int index, TempBuffer *buffer, const uni_char *&name, int &ns_idx)
{
	OpStatus::Ignore(GetAttributeName(environment->GetFramesDocument(), index, buffer, name, ns_idx));
}

int HTML_Element::DOMGetAttributeNamespaceIndex(DOM_Environment *environment, const uni_char *name, int ns_idx)
{
	int index = FindAttrIndex(ATTR_XML, name, ns_idx, GetNsIdx() != NS_IDX_HTML, TRUE);

	if (index != -1)
		return GetAttrNs(index);
	else
		return NS_IDX_NOT_FOUND;
}

const uni_char* HTML_Element::DOMGetAttribute(DOM_Environment *environment, int attr, const uni_char *name, int ns_idx, BOOL case_sensitive, BOOL resolve_urls, int at_known_index)
{
	return GetAttribute(environment->GetFramesDocument(), attr, name, ns_idx, case_sensitive, environment->GetTempBuffer(), resolve_urls, at_known_index);
}

double HTML_Element::DOMGetNumericAttribute(DOM_Environment *environment, int attr, const uni_char *name, int ns_idx, BOOL &absent)
{
	int index = FindAttrIndex(attr, name, ns_idx, GetNsIdx() != NS_IDX_HTML, TRUE);
	if (index != -1)
	{
		attr = GetAttrItem(index);
		ns_idx = GetResolvedAttrNs(index);
	}

	HTML_ElementType type = Type();
	NS_Type elmns = GetNsType(), attrns = g_ns_manager->GetNsTypeAt(ResolveNsIdx(ns_idx));

	absent = index == -1;

	if (index != -1)
	{
		const uni_char *string_value = GetAttrValueValue(index, attr, HE_UNKNOWN);

		if (string_value)
		{
			uni_char* end_ptr;
			double value;
			value = uni_strtod(string_value, &end_ptr);

			/* For some attributes, a negative WIDTH/HEIGHT value means that it is a percentage
			   value. In that case we need the box rectangle, which we
			   will do later in this method. */

			if (elmns == NS_HTML &&
			    (Type() == HE_IMG
#ifdef CANVAS_SUPPORT
			            || Type() == HE_CANVAS
#endif // CANVAS_SUPPORT
			     ) &&
				 attrns == NS_HTML && (attr == ATTR_WIDTH || attr == ATTR_HEIGHT) &&
				 (value < 0 || end_ptr == string_value))
			{
				// Percentage or illegal value. Return the calculated result below.
			}
			else if (end_ptr == string_value && elmns == NS_HTML && attrns == NS_HTML && attr == ATTR_START && Type() == HE_OL)
			{
				// Ignore non-numeric attribute.
			}
			else
				return value;
		}
	}

	if (elmns == NS_HTML && attrns == NS_HTML)
	{
		absent = FALSE;

		if ((type == HE_IMG || type == HE_INPUT && GetInputType() == INPUT_IMAGE) && (attr == ATTR_WIDTH || attr == ATTR_HEIGHT))
		{
			/* We end up here when no WIDTH/HEIGHT attribute has been
			   specified for an IMG element, or when the WIDTH/HEIGHT
			   attribute has a percentage value. If no WIDTH/HEIGHT
			   attribute has been specified, it doesn't necessarily mean
			   that we can use the intrinsic dimensions of the image,
			   since the size of the IMG element may be affected by
			   CSS. So we need to get the box rectangle. */

			FramesDocument *frames_doc = environment->GetFramesDocument();
			LogicalDocument *logdoc = frames_doc ? frames_doc->GetLogicalDocument() : 0;

			if (logdoc)
			{
				if (logdoc->GetRoot()->IsAncestorOf(this))
				{
					RECT rect;
					BOOL got_rect = logdoc->GetBoxRect(this, CONTENT_BOX, rect);
					if (got_rect)
					{
						int value;
						if (attr == ATTR_WIDTH)
							value = rect.right - rect.left;
						else
							value = rect.bottom - rect.top;
						return static_cast<double>(value);
					}
				}

				/* If image has no ancestor (new Image()) or no layout
				   (display:none) we'll try to get intristic dimensions */
				unsigned w, h;
				if (DOMGetImageSize(environment, w, h))
					return static_cast<double>(attr == ATTR_WIDTH ? w : h);
			}

			/* If we failed to get the box rectangle, we assume that
			   the size of the box is 0x0. This is probably correct in
			   most cases, but logdoc->GetBoxRect() will return FALSE
			   in OOM cases as well... */

			absent = TRUE;
			return 0.0;
		}

		if (type == Markup::HTE_OL && attr == Markup::HA_START)
		{
			FramesDocument *frames_doc = environment->GetFramesDocument();
			LogicalDocument *logdoc = frames_doc ? frames_doc->GetLogicalDocument() : 0;

			if (logdoc && HasAttr(Markup::HA_REVERSED) && logdoc->GetRoot()->IsAncestorOf(this))
			{
				unsigned count;
				if (OpStatus::IsSuccess(logdoc->GetLayoutWorkplace()->CountOrderedListItems(this, count)))
					return count;
			}
			return 1.;
		}
		else if (type == Markup::HTE_COLGROUP && attr == Markup::HA_SPAN ||
				 type == Markup::HTE_PROGRESS && attr == Markup::HA_MAX ||
				 type == Markup::HTE_METER && attr == Markup::HA_MAX ||
				 (type == Markup::HTE_TD || type == Markup::HTE_TH) && (attr == Markup::HA_COLSPAN || attr == Markup::HA_ROWSPAN))
			return 1.;
#ifdef CANVAS_SUPPORT
		else if (type == HE_CANVAS && (attr == ATTR_WIDTH || attr == ATTR_HEIGHT))
		{
			Canvas* canvas = (Canvas*)GetSpecialAttr(Markup::VEGAA_CANVAS, ITEM_TYPE_COMPLEX, NULL, SpecialNs::NS_OGP);
			if (canvas)
				if (attr == ATTR_WIDTH)
					return (double) canvas->GetWidth(this);
				else
					return (double) canvas->GetHeight(this);
		}
#endif // CANVAS_SUPPORT
	}

	absent = TRUE;
	return 0.;
}

BOOL HTML_Element::DOMGetBooleanAttribute(DOM_Environment *environment, int attr, const uni_char *name, int ns_idx)
{
	int index = FindAttrIndex(attr, name, ns_idx, GetNsIdx() != NS_IDX_HTML, TRUE);
	if (index == -1 || GetItemType(index) == ITEM_TYPE_BOOL && !GetAttrItem(index))
		return FALSE;
	else
		return TRUE;
}

OP_STATUS HTML_Element::DOMSetAttribute(DOM_Environment *environment, Markup::AttrType attr, const uni_char *name, int ns_idx, const uni_char *value, unsigned value_length, BOOL case_sensitive)
{
	return SetAttribute(environment, attr, name, ns_idx, value, value_length, environment->GetCurrentScriptThread(), case_sensitive, FALSE, TRUE);
}

OP_STATUS HTML_Element::DOMSetNumericAttribute(DOM_Environment *environment, Markup::AttrType attr, const uni_char *name, int ns_idx, double value)
{
	LogicalDocument *logdoc = environment->GetFramesDocument() ? environment->GetFramesDocument()->GetLogicalDocument() : NULL;
	BOOL case_sensitive = !logdoc || logdoc->IsXml() || GetNsIdx() != NS_IDX_HTML;

	// Room for a 64 bit number including sign and terminator.
	uni_char value_string[DBL_MAX_10_EXP + 50]; // ARRAY OK 2010-10-20 emil
	if (IsNumericAttributeFloat(attr))
		RETURN_IF_ERROR(WebForms2Number::DoubleToString(value, value_string));
	else
		uni_ltoa(static_cast<int>(value), value_string, 10);
	return DOMSetAttribute(environment, attr, name, ns_idx, value_string, uni_strlen(value_string), case_sensitive);
}

OP_STATUS HTML_Element::DOMSetBooleanAttribute(DOM_Environment *environment, Markup::AttrType attr, const uni_char *name, int ns_idx, BOOL value)
{
	LogicalDocument *logdoc = environment->GetFramesDocument() ? environment->GetFramesDocument()->GetLogicalDocument() : NULL;
	BOOL case_sensitive = !logdoc || logdoc->IsXml() || GetNsIdx() != NS_IDX_HTML;

	int index = FindAttrIndex(attr, name, ns_idx, case_sensitive, FALSE);

	if (index != -1)
	{
		/* We shouldn't find another attribute type unless attr == ATTR_XML. */
		OP_ASSERT(attr == ATTR_XML || attr == GetAttrItem(index));

		attr = GetAttrItem(index);
		ns_idx = GetAttrNs(index);
	}
	else
	{
		if (ns_idx == NS_IDX_ANY_NAMESPACE)
			ns_idx = NS_IDX_DEFAULT;

		if (attr == ATTR_XML)
			attr = htmLex->GetAttrType(name, g_ns_manager->GetNsTypeAt(ResolveNsIdx(ns_idx)), case_sensitive);
	}

	if (index == -1 && value || index != -1 && (!value || !PTR_TO_BOOL(GetValueItem(index))))
	{
		DocumentContext context(environment);
		ES_Thread *thread = environment->GetCurrentScriptThread();

		OP_STATUS status = BeforeAttributeChange(context, thread, index, attr, ResolveNsIdx(ns_idx), FALSE);

		if (OpStatus::IsMemoryError(status))
			return status;
		else if (OpStatus::IsError(status))
			// Script was not allowed to change the attribute.
			return OpStatus::OK;

		if (value)
		{
#if 0
			/* This code is untested, and solves a problem that doesn't exist:
			   unknown attributes that are known to be booleans. */

			ItemType attr_item_type;
			void *attr_value;
			BOOL need_free;

			if (attr == ATTR_XML)
			{
				attr_item_type = ITEM_TYPE_STRING;
				attr_value = OP_NEWA(uni_char, uni_strlen(name) * 2 + 2);
				if (!attr_value)
					return OpStatus::ERR_NO_MEMORY;
				uni_strcpy((uni_char *) attr_value, name);
				uni_strcpy((uni_char *) attr_value + uni_strlen(name) + 1, name);
				need_free = TRUE;
			}
			else
			{
				attr_item_type = ITEM_TYPE_BOOL;
				attr_value = INT_TO_PTR(TRUE);
				need_free = FALSE;
			}

			index = SetAttr(attr, attr_item_type, attr_value, need_free, ns_idx);
#else // 0
			if (attr == ATTR_XML)
			{
				OP_ASSERT(FALSE);
				return OpStatus::OK;
			}

			index = SetBoolAttr(attr, TRUE, ns_idx);
#endif // 0
		}
		else
		{
			RemoveAttrAtIdx(index);
			index = -1;
		}

		status = HandleAttributeChange(context, thread, index, attr, ns_idx);
		OP_STATUS status2 = AfterAttributeChange(context, thread, index, attr, ns_idx, FALSE);
		return OpStatus::IsError(status) ? status : status2;
	}

	return OpStatus::OK;
}

OP_STATUS HTML_Element::DOMRemoveAttribute(DOM_Environment *environment, const uni_char *attr_name, int attr_ns_idx, BOOL case_sensitive)
{
	DocumentContext context(environment);

	int index = FindAttrIndex(ATTR_XML, attr_name, attr_ns_idx, case_sensitive, TRUE);

	if (index != -1)
	{
		Markup::AttrType attr = GetAttrItem(index);
		int ns_idx = GetAttrNs(index);
		int resolved_ns_idx = ResolveNsIdx(ns_idx);

		if (GetAttrIsEvent(index))
		{
			environment->RemoveEventHandler(GetEventType(attr, resolved_ns_idx));
			ClearEventHandler(environment, GetEventType(attr, resolved_ns_idx));
		}

		LogicalDocument *logdoc = context.logdoc;

		if (logdoc && logdoc->IsXml())
			if (XMLDocumentInformation *xmldocinfo = logdoc->GetXMLDocumentInfo())
				if (XMLDoctype *xmldoctype = xmldocinfo->GetDoctype())
					if (XMLDoctype::Element *elementdecl = xmldoctype->GetElement(GetTagName()))
					{
						TempBuffer buffer;

						const uni_char *prefix = g_ns_manager->GetPrefixAt(ns_idx >= 0 ? ns_idx : resolved_ns_idx);
						const uni_char *full_attr_name;

						if (prefix && *prefix)
						{
							RETURN_IF_ERROR(buffer.Append(prefix));
							RETURN_IF_ERROR(buffer.Append(':'));
							RETURN_IF_ERROR(buffer.Append(attr_name));

							full_attr_name = buffer.GetStorage();
						}
						else
							full_attr_name = attr_name;

						if (XMLDoctype::Attribute *attr = elementdecl->GetAttribute(full_attr_name, uni_strlen(full_attr_name)))
						{
							const uni_char *defaultvalue = attr->GetDefaultValue();
							if (defaultvalue)
								return SetAttribute(context, ATTR_XML, attr_name, ns_idx, defaultvalue, uni_strlen(defaultvalue), environment->GetCurrentScriptThread(), case_sensitive, attr->GetType() == XMLDoctype::Attribute::TYPE_Tokenized_ID, FALSE);
						}
					}

		ES_Thread *thread = environment->GetCurrentScriptThread();

		OP_STATUS status = BeforeAttributeChange(context, thread, index, attr, resolved_ns_idx, FALSE);

		if (OpStatus::IsMemoryError(status))
			return status;
		else if (OpStatus::IsError(status))
			// Script was not allowed to change the attribute.
			return OpStatus::OK;

		NS_Type ns = g_ns_manager->GetNsTypeAt(resolved_ns_idx);
		OpString removed_name;
		if (attr == ATTR_XML)
			status = removed_name.Set(GetAttrName(index, attr, ns));
		RemoveAttrAtIdx(index);
		index = -1;

		OP_STATUS status2 = HandleAttributeChange(context, thread, index, attr, ns_idx, TRUE, removed_name.CStr());
		status = OpStatus::IsError(status) ? status : status2;
		status2 = AfterAttributeChange(context, thread, index, attr, ns_idx, FALSE);
		return OpStatus::IsError(status) ? status : status2;
	}

	return OpStatus::OK;
}

void HTML_Element::SetIndeterminate(FramesDocument *frames_doc, BOOL indeterminate, BOOL apply_property_changes)
{
	// Update element attribute
	SetSpecialBoolAttr(FORMS_ATTR_INDETERMINATE, indeterminate, SpecialNs::NS_FORMS);

	// Update the form object
	if (FormObject* form_object = GetFormObject())
		form_object->SetIndeterminate(indeterminate);

	// Update the indeterminate pseudo class
	if (apply_property_changes && frames_doc)
		if (HTML_Element *root = frames_doc->GetDocRoot())
			if (root->IsAncestorOf(this))
				if (LogicalDocument* logdoc = frames_doc->GetLogicalDocument())
					logdoc->GetLayoutWorkplace()->ApplyPropertyChanges(this, CSS_PSEUDO_CLASS_GROUP_FORM, TRUE);
}

BOOL HTML_Element::GetIndeterminate()
{
	return GetSpecialBoolAttr(FORMS_ATTR_INDETERMINATE, SpecialNs::NS_FORMS);
}

BOOL HTML_Element::DOMGetImageSize(DOM_Environment *environment, unsigned& width, unsigned& height)
{
	OP_ASSERT(IsMatchingType(HE_IMG, NS_HTML));

	FramesDocument *frames_doc = environment->GetFramesDocument();
	if (frames_doc && !frames_doc->GetShowImages())
		return FALSE;

	HEListElm* hle = GetHEListElm(FALSE);
	if (hle)
	{
		Image image = hle->GetImage();
		if (!image.IsEmpty())
		{
			width = image.Width();
			height = image.Height();
			return TRUE;
		}
	}
	return FALSE;
}

void HTML_Element::DOMMoveToOtherDocument(DOM_Environment *environment, DOM_Environment *new_environment)
{
	if (ElementRef *iter = m_first_ref)
	{
		while (iter)
		{
			ElementRef *next_ref = iter->NextRef(); // get it now, because iter can be deleted by the call
			iter->OnInsert(environment->GetFramesDocument(), new_environment->GetFramesDocument());
			iter = next_ref;
		}
	}

	for (int index = 0, count = packed1.attr_size; index < count;)
		if (GetAttrItem(index) == ATTR_NULL)
			break;
		else
		{
			switch (GetItemType(index))
			{
#ifdef XML_EVENTS_SUPPORT
			case ITEM_TYPE_XML_EVENTS_REGISTRATION:
				if (XML_Events_Registration *registration = static_cast<XML_Events_Registration *>(GetValueItem(index)))
					registration->MoveToOtherDocument(environment->GetFramesDocument(), new_environment->GetFramesDocument());
				break;
#endif // XML_EVENTS_SUPPORT

			case ITEM_TYPE_COMPLEX:
				if (ComplexAttr *value = static_cast<ComplexAttr *>(GetValueItem(index)))
					if (!value->MoveToOtherDocument(environment->GetFramesDocument(), new_environment->GetFramesDocument()))
					{
						RemoveAttrAtIdx(index);
						continue;
					}
				break;
			}

			++index;
		}
}

OP_BOOLEAN
HTML_Element::DOMGetDataSrcContents(DOM_Environment *environment, TempBuffer *buffer)
{
	LogicalDocument *logdoc = environment->GetFramesDocument()->GetLogicalDocument();
	URL base_url = DeriveBaseUrl(logdoc);
	URL *url;
	if (IsLinkElement() || (url = GetScriptURL(base_url, logdoc)) && !url->IsEmpty())
	{
		DataSrc* src_head = GetDataSrc();

		for (DataSrcElm *src_elm = src_head->First(); src_elm; src_elm = src_elm->Suc())
			RETURN_IF_ERROR(buffer->Append(src_elm->GetSrc(), src_elm->GetSrcLen()));

		return OpBoolean::IS_TRUE;
	}
	return OpBoolean::IS_FALSE;
}

const uni_char* HTML_Element::DOMGetContentsString(DOM_Environment *environment, TempBuffer *buffer, BOOL just_childrens_text)
{
	if (Type() == HE_COMMENT || Type() == HE_PROCINST)
	{
		const uni_char *result = GetStringAttr(ATTR_CONTENT);
		return (result ? result : UNI_L(""));
	}
	else if (Type() == HE_TEXT)
	{
		const uni_char *result = Content();
		return (result ? result : UNI_L(""));
	}

	else if (!just_childrens_text && (IsScriptElement() || IsLinkElement()))
	{
		OP_BOOLEAN status = DOMGetDataSrcContents(environment, buffer);
		if (status != OpBoolean::IS_FALSE)
			return OpStatus::IsError(status) ? NULL : buffer->GetStorage();
		// Proceed if nothing found.
	}
	else if (IsMatchingType(HE_OPTION, NS_HTML))
	{
		if (OpStatus::IsError(GetOptionText(buffer)))
			return NULL;
	}

	HTML_Element *child = FirstChildActual();
	if (child && !child->SucActual())
		return !just_childrens_text || child->IsText() ?
				child->DOMGetContentsString(environment, buffer, just_childrens_text && Type() == HE_TEXTGROUP) :
				UNI_L("");
	else
	{
		for (; child; child = child->SucActual())
			if (!just_childrens_text || child->IsText())
				if (OpStatus::IsError(child->DOMGetContents(environment, buffer, just_childrens_text && Type() == HE_TEXTGROUP)))
					return NULL;
	}

	return (buffer->GetStorage() ? buffer->GetStorage() : UNI_L(""));
}


OP_STATUS HTML_Element::DOMGetContents(DOM_Environment *environment, TempBuffer *buffer, BOOL just_childrens_text)
{
	if (Type() == HE_COMMENT || Type() == HE_PROCINST)
		return buffer->Append(GetStringAttr(ATTR_CONTENT));
	else if (Type() == HE_TEXT)
		return buffer->Append(Content());
	else if (!just_childrens_text && (IsScriptElement() || IsLinkElement()))
	{
		OP_BOOLEAN status = DOMGetDataSrcContents(environment, buffer);
		if (status != OpBoolean::IS_FALSE)
			return OpStatus::IsError(status) ? status : OpStatus::OK;
		// Proceed if nothing found.
	}
	else if (IsMatchingType(HE_OPTION, NS_HTML))
		return GetOptionText(buffer);

	for (HTML_Element *child = FirstChildActual(); child; child = child->SucActual())
		if (!just_childrens_text || child->IsText())
			RETURN_IF_ERROR(child->DOMGetContents(environment, buffer, just_childrens_text && Type() == HE_TEXTGROUP));

	return OpStatus::OK;
}

OP_STATUS HTML_Element::DOMSetContents(DOM_Environment *environment, const uni_char *contents, HTML_Element::ValueModificationType modification_type /* = MODIFICATION_REPLACING_ALL */, unsigned offset /*= 0*/, unsigned length1 /*= 0*/, unsigned length2 /*= 0*/)
{
#ifndef HAS_NOTEXTSELECTION
	// If this modifies the nodes that define the selection, we'll need to update the selection.
	BOOL need_to_adjust_selection = FALSE;
	SelectionBoundaryPoint start, end;
	SelectionBoundaryPoint* points[2] = { &start, &end };
	// If we have deleted text before a selection marker in a node, we need to update that selection
	// This is similar to the code maintaining DOM_Range objects in DOM.
	if (FramesDocument* frames_doc = environment->GetFramesDocument())
	{
		if (HTML_Document* html_doc = frames_doc->GetHtmlDocument())
		{
			if (html_doc->GetSelection(start, end))
			{
				if (start.GetElement() && IsAncestorOf(start.GetElement()) ||
					end.GetElement() && IsAncestorOf(end.GetElement()))
				{
					// Adjust offsets to node offsets from local HE_TEXT element offsets in case we're in a HE_TEXTGROUP
					if (Type() == HE_TEXTGROUP)
					{
						need_to_adjust_selection = TRUE; // Since we will reallocate the text nodes.

						// HE_TEXTGROUP, need to make an HE_TEXTGROUP offset
						for (int i = 0; i < 2; i++)
						{
							SelectionBoundaryPoint* point = points[i];
							if (!point->GetElement() || !IsAncestorOf(point->GetElement()))
								continue;

							int old_offset = point->GetElementCharacterOffset();
							OP_ASSERT(point->GetElement() != this);
							HTML_Element* elm = point->GetElement()->Pred();
							while (elm)
							{
								int elm_len = elm->GetTextContentLength();
								old_offset += elm_len;
								offset += elm_len;
								elm = elm->Pred();
							}

							point->SetLogicalPosition(this, old_offset);
						}
					} // end handle HE_TEXTGROUP

					BOOL deleting;
					BOOL inserting;
					int delete_length;
					int insert_length;

					if (modification_type == MODIFICATION_DELETING
						|| modification_type == MODIFICATION_SPLITTING)
					{
						deleting = TRUE;
						inserting = FALSE;
						delete_length = length1;
						insert_length = 0;
					}
					else if (modification_type == MODIFICATION_INSERTING)
					{
						deleting = FALSE;
						inserting = TRUE;
						delete_length = 0;
						insert_length = length1;
					}
					else if (modification_type == MODIFICATION_REPLACING)
					{
						deleting = TRUE;
						inserting = TRUE;
						delete_length = length1;
						insert_length = length2;
					}
					else
					{
						OP_ASSERT(modification_type == MODIFICATION_REPLACING_ALL);
						deleting = TRUE;
						inserting = TRUE;
						delete_length = GetTextContentLength();
						insert_length = uni_strlen(contents);
						modification_type = MODIFICATION_REPLACING;
					}

					// Adjust offsets for the changed SelectionBoundaryPoint
					for (int i = 0; i < 2; i++)
					{
						SelectionBoundaryPoint* point = points[i];
						if (point->GetElement() != this)
							continue;
						unsigned point_offset = point->GetElementCharacterOffset();
						if (offset < point_offset)
						{
							need_to_adjust_selection = TRUE;
							if (deleting)
								if (offset + delete_length <= point_offset)
									point_offset -= delete_length;
								else
									point_offset = offset;

							if (inserting)
								point_offset += insert_length;

							point->SetLogicalPosition(this, point_offset);
						}
					} // end adjust offsets

				} // end if (is ancestor of selection point)
			} // getSelection
		} // HTML_Document
	} // FramesDocument
#endif // !HAS_NOTEXTSELECTION

	if (Type() == HE_COMMENT || Type() == HE_PROCINST)
	{
		uni_char *contents_copy = UniSetNewStr(contents);
		if (contents && !contents_copy)
			return OpStatus::ERR_NO_MEMORY;
		SetAttr(ATTR_CONTENT, ITEM_TYPE_STRING, contents_copy, TRUE, NS_IDX_DEFAULT);
		RETURN_IF_ERROR(environment->ElementCharacterDataChanged(this, modification_type, offset, length1, length2));
	}
	else if (IsText())
	{
		OP_ASSERT(!(Parent() && Parent()->Type() == HE_TEXTGROUP));
		// SetText invalidates the text box as well as setting the text
		RETURN_IF_ERROR(SetText(environment->GetFramesDocument(), contents, (contents ? uni_strlen(contents) : 0),
			modification_type, offset, length1, length2));

#ifndef HAS_NOTEXTSELECTION
		if (need_to_adjust_selection)
		{
			// Check both points in case we have created textgroups and have to move
			// them down to a suitable HE_TEXT child.
			for (int i = 0; i < 2; i++)
			{
				SelectionBoundaryPoint* point = points[i];
				if (point->GetElement() == this && Type() == HE_TEXTGROUP)
				{
					int child_offset = point->GetElementCharacterOffset();
					HTML_Element* child = FirstChild();
					while (child)
					{
						int local_length = child->GetTextContentLength();
						if (child_offset <= local_length)
						{
							point->SetLogicalPosition(child, child_offset);
							break;
						}
						else
							child_offset -= local_length;

						child = child->Suc();
					}

					OP_ASSERT(child || !"We decided for a position for the new selection point but suddenly that position doesn't exist");
				}
			}
		}
#endif // !HAS_NOTEXTSELECTION
	}
	else if (IsScriptElement() || IsLinkElement())
	{
		if (DataSrc* src_head = GetDataSrc())
		{
			URL src_origin = src_head->GetOrigin();
			src_head->DeleteAll();
			return src_head->AddSrc(contents, uni_strlen(contents), src_origin);
		}
	}

#ifndef HAS_NOTEXTSELECTION
	if (need_to_adjust_selection && environment->GetFramesDocument()->IsCurrentDoc())
	{
		environment->GetFramesDocument()->Reflow(FALSE); // SetSelection doesn't work on a dirty tree.
		environment->GetFramesDocument()->GetHtmlDocument()->SetSelection(&start, &end, TRUE);
	}
#endif // HAS_NOTEXTSELECTION

	return OpStatus::OK;
}

OP_STATUS HTML_Element::DOMSelectContents(DOM_Environment *environment)
{
	if (FramesDocument *frames_doc = environment->GetFramesDocument())
		if (frames_doc->IsCurrentDoc())
		{
			RETURN_IF_ERROR(frames_doc->Reflow(FALSE));

			GetFormValue()->SelectAllText(this);
		}

	return OpStatus::OK;
}

BOOL HTML_Element::DOMGetBoolFormValue(DOM_Environment *environment)
{
	// This method is only called for radio buttons and checkboxes
	FormValue* form_value = GetFormValue();
	FormValueRadioCheck* bool_value = FormValueRadioCheck::GetAs(form_value);
	return bool_value->IsChecked(this);
}

void HTML_Element::SetBoolFormValue(FramesDocument *frames_doc, BOOL value)
{
	FormValue* form_value = GetFormValue();
	FormValueRadioCheck* bool_value = FormValueRadioCheck::GetAs(form_value);
	bool_value->SetIsChecked(this, value, frames_doc, TRUE);
}

void HTML_Element::DOMSetBoolFormValue(DOM_Environment *environment, BOOL value)
{
	SetBoolFormValue(environment->GetFramesDocument(), value);
	if (environment->GetFramesDocument())
	{
		FormValue* form_value = GetFormValue();
		FormValueRadioCheck* bool_value = FormValueRadioCheck::GetAs(form_value);
		bool_value->SetWasChangedExplicitly(environment->GetFramesDocument(), this);
	}

	HandleFormValueChange(environment->GetFramesDocument(), environment->GetCurrentScriptThread());
}

OP_STATUS HTML_Element::DOMGetDefaultOutputValue(DOM_Environment *environment, TempBuffer *buffer)
{
	FormValueOutput* output_value = FormValueOutput::GetAs(GetFormValue());
	OpString tmp_string;
	OP_STATUS status = output_value->GetDefaultValueAsText(this, tmp_string);
	if (OpStatus::IsSuccess(status) && !tmp_string.IsEmpty())
	{
		status = buffer->Append(tmp_string.CStr());
	}
	RETURN_IF_MEMORY_ERROR(status);

	return OpStatus::OK;
}

OP_STATUS HTML_Element::DOMSetDefaultOutputValue(DOM_Environment *environment, const uni_char *value)
{
	FormValueOutput* output_value = FormValueOutput::GetAs(GetFormValue());
	RETURN_IF_MEMORY_ERROR(output_value->SetDefaultValueFromText(this, value));
	return OpStatus::OK;
}

OP_STATUS HTML_Element::DOMGetFormValue(DOM_Environment *environment, TempBuffer *buffer, BOOL with_crlf)
{
	OP_ASSERT(IsFormElement());

	// This is only called for form things
	FormValue* form_value = GetFormValue();
	OpString text_value;
	if (!with_crlf)
	{
		FormValueTextArea *text_area = FormValueTextArea::GetAs(form_value);
		RETURN_IF_ERROR(text_area->GetValueAsTextLF(this, text_value));
	}
	else
		RETURN_IF_ERROR(form_value->GetValueAsText(this, text_value));

	RETURN_IF_ERROR(buffer->Expand(text_value.Length() + 1));
	if (!text_value.CStr())
		*buffer->GetStorage() = '\0';
	else
	{
		buffer->Clear();
		buffer->Append(text_value.CStr());
	}

	if (GetInputType() == INPUT_FILE)
	{
		/* Strip double quotes and escape characters from the string; the script
		   expects a "plain" filename.	This method is not ideal, because it
		   looks at the string after the filename quoter has changed it -- user
		   intention may have been lost.  For example, if the user types in a
		   quoted filename then he may expect that to be passed to the script;
		   we strip the quoting here regardless.

		   This code handles only one file name. */

		OpString value_obj;
		RETURN_IF_MEMORY_ERROR(value_obj.Set(buffer->GetStorage()));
		uni_char *value = value_obj.CStr();
		UniParameterList file_name_list;
		RETURN_IF_MEMORY_ERROR(FormManager::ConfigureForFileSplit(file_name_list, value));
		*value = '\0';

		UniParameters* file_name_obj = file_name_list.First();
		if (file_name_obj)
		{
			const uni_char* file_name = file_name_obj->Name();
			if (file_name)
			{
				buffer->Clear();
				// We don't want scripts to have access to the path as that gives
				// the script information about the local computer's directory
				// structure. Strip everything before the last path seperator,
				// unless this is opera:config.
#ifdef OPERACONFIG_URL
				BOOL allow_path = FALSE;

				// Check the document URL
				FramesDocument* frames_doc = environment->GetFramesDocument();
				if (frames_doc)
				{
					URL url = frames_doc->GetOrigin()->security_context;
					allow_path = url.Type() == URL_OPERA && url.GetAttribute(URL::KName).CompareI("opera:config") == 0;
				}

				if (!allow_path)
#endif
				{
					const uni_char* last_sep = uni_strrchr(file_name, PATHSEPCHAR);
					if (last_sep)
						file_name = last_sep + 1;
					RETURN_IF_ERROR(buffer->Append(UNI_L("C:\\fakepath\\")));
				}
				RETURN_IF_ERROR(buffer->Append(file_name));
			}
		}
	}

	return OpStatus::OK;
}

OP_STATUS HTML_Element::DOMSetFormValue(DOM_Environment *environment, const uni_char *value)
{
	// No script is allowed to manipulate file upload fields
	OP_ASSERT(GetNsType() == NS_HTML);
	BOOL allow_script_to_change = Type() != HE_INPUT || GetInputType() != INPUT_FILE || !*value;
#ifdef OPERACONFIG_URL
	FramesDocument* frames_doc = environment->GetFramesDocument();
	if (frames_doc)
	{
		URL url = frames_doc->GetOrigin()->security_context;
		if (url.Type() == URL_OPERA && url.GetAttribute(URL::KName).CompareI("opera:config") == 0)
		{
			// opera:config is allowed to change anything
			allow_script_to_change = TRUE;
		}
	}
#endif // OPERACONFIG_URL
	if (!allow_script_to_change)
		return OpStatus::OK;

	OP_ASSERT(value);
	FormValue* form_value = GetFormValue();
	OP_ASSERT(form_value); // This function shouldn't be called unless it was a form thing.
	if (Type() == HE_OUTPUT)
	{
		FormValueOutput* form_value_output = FormValueOutput::GetAs(form_value);
		RETURN_IF_MEMORY_ERROR(form_value_output->SetOutputValueFromText(this, environment->GetFramesDocument(), value));
	}
	else
	{
		RETURN_IF_MEMORY_ERROR(form_value->SetValueFromText(this, value, TRUE));
	}

	HandleFormValueChange(environment->GetFramesDocument(), environment->GetCurrentScriptThread());

	return OpStatus::OK;
}

OP_STATUS HTML_Element::DOMStepUpDownFormValue(DOM_Environment *environment, int step_count)
{
	OP_ASSERT(IsMatchingType(HE_INPUT, NS_HTML));
	OP_STATUS status = GetFormValue()->StepUpDown(this, step_count);

	if (OpStatus::IsSuccess(status))
		HandleFormValueChange(environment->GetFramesDocument(), environment->GetCurrentScriptThread());

	return status;
}

OP_STATUS HTML_Element::DOMGetSelectedIndex(DOM_Environment *environment, int &index)
{
	FormValue* form_value = GetFormValue();
	OP_ASSERT(form_value);
	FormValueList* form_value_list = FormValueList::GetAs(form_value);
	unsigned int first_sel_index;
	OP_STATUS status = form_value_list->GetIndexOfFirstSelected(this, first_sel_index);
	if (OpStatus::IsSuccess(status))
	{
		index = static_cast<int>(first_sel_index);
		return OpStatus::OK;
	}

	if (OpStatus::IsMemoryError(status))
		return OpStatus::ERR_NO_MEMORY;

	OP_ASSERT(status == OpStatus::ERR); // There was nothing selected
	index = -1;

	return OpStatus::OK;
}

OP_STATUS HTML_Element::DOMSetSelectedIndex(DOM_Environment *environment, int index)
{
	FormValue* form_value = GetFormValue();
	FormValueList* list_value = FormValueList::GetAs(form_value);
	if (index < 0 || static_cast<unsigned int>(index) >= list_value->GetOptionCount(this))
	{
		RETURN_IF_MEMORY_ERROR(list_value->UnselectAll(this));
	}
	else
	{
		// Should only have one selected value after this
		RETURN_IF_MEMORY_ERROR(list_value->UnselectAll(this));
		RETURN_IF_MEMORY_ERROR(list_value->SelectValue(this, index, TRUE));
	}

	HandleFormValueChange(environment->GetFramesDocument(), environment->GetCurrentScriptThread());

	return OpStatus::OK;
}

OP_STATUS HTML_Element::DOMGetOptionSelected(DOM_Environment *environment, int index, BOOL &selected)
{
	FormValue* form_value = GetFormValue();
	OP_ASSERT(form_value);
	if (index >= 0)
	{
		FormValueList* form_value_list = FormValueList::GetAs(form_value);
		selected = form_value_list->IsSelected(this, index);
	}
	else
		selected = FALSE;

	return OpStatus::OK;
}

OP_STATUS HTML_Element::DOMSetOptionSelected(DOM_Environment *environment, int index, BOOL selected)
{
	FormValue* form_value = GetFormValue();
	OP_ASSERT(form_value);
	if (index >= 0)
	{
		FormValueList* form_value_list = FormValueList::GetAs(form_value);
		form_value_list->SelectValue(this, index, selected);
	}

	HandleFormValueChange(environment->GetFramesDocument(), environment->GetCurrentScriptThread());

	return OpStatus::OK;
}

void HTML_Element::DOMSelectUpdateLock(BOOL lock)
{
	FormValue* form_value = GetFormValue();
	OP_ASSERT(form_value);
	FormValueList* form_value_list = FormValueList::GetAs(form_value);
	form_value_list->LockUpdate(this, FALSE, lock);
}

// This is called when DOM (or something similarly non-user) has changed the value
void HTML_Element::HandleFormValueChange(FramesDocument *frames_doc, ES_Thread* thread)
{
	if (frames_doc)
		FormValueListener::HandleValueChanged(frames_doc, this, FALSE, FALSE, thread);
}

void HTML_Element::DOMGetSelection(DOM_Environment *environment, int &start, int &end, SELECTION_DIRECTION& direction)
{
	OP_ASSERT(IsFormElement());

	INT32 s, e;

	GetFormValue()->GetSelection(this, s, e, direction);

	start = s;
	end = e;
}

void HTML_Element::DOMSetSelection(DOM_Environment *environment, int start, int end, SELECTION_DIRECTION direction)
{
	OP_ASSERT(IsFormElement());

#ifndef RANGESELECT_FROM_EDGE
		if (direction == SELECTION_DIRECTION_NONE)
			direction = SELECTION_DIRECTION_FORWARD;
#endif // RANGESELECT_FROM_EDGE
	GetFormValue()->SetSelection(this, start, end, direction);
}

int HTML_Element::DOMGetCaretPosition(DOM_Environment *environment)
{
	OP_ASSERT(IsFormElement());
	return GetFormValue()->GetCaretOffset(this);
}

void HTML_Element::DOMSetCaretPosition(DOM_Environment *environment, int position)
{
	OP_ASSERT(IsFormElement());
	FormValue* form_value = GetFormValue();
	// When DOM sets caret position, we also want to remove the current selection
	form_value->SetSelection(this, position, position);
	form_value->SetCaretOffset(this, position);
}

const uni_char* HTML_Element::DOMGetPITarget(DOM_Environment *environment)
{
	return GetStringAttr(ATTR_TARGET, NS_IDX_DEFAULT);
}

OP_STATUS
HTML_Element::DOMGetInlineStyle(CSS_DOMStyleDeclaration *&styledeclaration, DOM_Environment *environment)
{
	styledeclaration = OP_NEW(CSS_DOMStyleDeclaration, (environment, this, NULL, CSS_DOMStyleDeclaration::NORMAL));
	return styledeclaration ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}

OP_STATUS
HTML_Element::DOMGetComputedStyle(CSS_DOMStyleDeclaration *&styledeclaration, DOM_Environment *environment, const uni_char *pseudo_class)
{
	styledeclaration = OP_NEW(CSS_DOMStyleDeclaration, (environment, this, NULL, CSS_DOMStyleDeclaration::COMPUTED, pseudo_class));
	return styledeclaration ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}

#ifdef CURRENT_STYLE_SUPPORT
OP_STATUS
HTML_Element::DOMGetCurrentStyle(CSS_DOMStyleDeclaration *&styledeclaration, DOM_Environment *environment)
{
	styledeclaration = OP_NEW(CSS_DOMStyleDeclaration, (environment, this, NULL, CSS_DOMStyleDeclaration::CURRENT, NULL));
	return styledeclaration ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}
#endif // CURRENT_STYLE_SUPPORT


#ifdef _PLUGIN_SUPPORT_
OpNS4Plugin* HTML_Element::DOMGetNS4Plugin()
{
	// FIXME: Inline
	return GetNS4Plugin();
}
#endif // _PLUGIN_SUPPORT_

OP_STATUS HTML_Element::DOMGetFrameProxyEnvironment(DOM_ProxyEnvironment*& frame_environment, FramesDocument*& frame_frames_doc, DOM_Environment* environment)
{
	frame_environment = NULL;
	frame_frames_doc = NULL;

	if (FramesDocElm *frames_doc_elm = FramesDocElm::GetFrmDocElmByHTML(this))
	{
		DocumentManager* docman = frames_doc_elm->GetDocManager();

		RETURN_IF_ERROR(docman->ConstructDOMProxyEnvironment());

		frame_environment = docman->GetDOMEnvironment();
		OP_ASSERT(frame_environment);

		frame_frames_doc = docman->GetCurrentDoc();
	}

	return OpStatus::OK;
}

BOOL HTML_Element::DOMElementLoaded(DOM_Environment *environment)
{
	InlineResourceType inline_type = INVALID_INLINE;

	if (IsMatchingType(HE_SCRIPT, NS_HTML) && GetStringAttr(ATTR_SRC))
		inline_type = SCRIPT_INLINE;
	else if (IsMatchingType(HE_IMG, NS_HTML))
		inline_type = IMAGE_INLINE;
	else if (IsMatchingType(HE_LINK, NS_HTML))
		if (FramesDocument *document = environment->GetFramesDocument())
			if (HLDocProfile *hld_profile = document->GetHLDocProfile())
				for (const LinkElement *link = hld_profile->GetLinkList(); link; link = link->Suc())
					if (link->IsElm(this))
					{
						inline_type = CSS_INLINE;
						break;
					}

	if (inline_type != INVALID_INLINE)
	{
		HEListElm *helistelm = GetHEListElmForInline(inline_type);
		return helistelm && helistelm->GetHandled();
	}

	return TRUE;
}

BOOL HTML_Element::DOMGetStylesheetDisabled(DOM_Environment *environment)
{
	FramesDocument *frames_doc = environment->GetFramesDocument();
	CSS *css = GetCSS();

	if (!frames_doc || !css)
	{
		BOOL disabled = GetSpecialBoolAttr(ATTR_STYLESHEET_DISABLED, SpecialNs::NS_LOGDOC);
		return disabled;
	}

	return !css->IsEnabled() || !css->CheckMedia(frames_doc, frames_doc->GetMediaType());
}

const uni_char* HTML_Element::DOMGetInputTypeString()
{
	OP_ASSERT(GetNsType() == NS_HTML);
	OP_ASSERT(Type() == HE_INPUT || Type() == HE_BUTTON);

	// Move all this code to an HTML_Element::DOMGetInputTypeString method?
	OP_ASSERT(INPUT_NOT_AN_INPUT_OBJECT == 0); // Since 0 is the default value of GetNumAttr.
	OP_ASSERT(INPUT_TEXT > 0);
	OP_ASSERT(INPUT_SUBMIT > 0);
	InputType type = (enum InputType)GetNumAttr(ATTR_TYPE);
	if (type == INPUT_NOT_AN_INPUT_OBJECT)
		type = Type() == HE_BUTTON ? INPUT_SUBMIT : INPUT_TEXT;

	// const uni_char *GetInputTypeString(InputType type); // FIXME: Fix exported prototype in logdoc
	return GetInputTypeString(type);
}

OP_STATUS HTML_Element::DOMSetStylesheetDisabled(DOM_Environment *environment, BOOL value)
{
	FramesDocument *frames_doc = environment->GetFramesDocument();
	if (!frames_doc)
		return OpStatus::OK;

	return SetStylesheetDisabled(frames_doc, value);
}

OP_STATUS HTML_Element::SetStylesheetDisabled(FramesDocument *frames_doc, BOOL value)
{
	HLDocProfile *hld_profile = frames_doc->GetHLDocProfile();
	if (!hld_profile)
		return OpStatus::OK;

	CSSCollection* coll = hld_profile->GetCSSCollection();

	CSS *css = GetCSS();
	while (css)
	{
		HTML_Element *css_he = css->GetHtmlElement();

		if (!IsAncestorOf(css_he))
			break;
		else if (!css->IsEnabled() == !value)
		{
			css->SetEnabled(!value);
			if (!value)
				css->Added(coll, frames_doc);
			else
				css->Removed(coll, frames_doc);
		}

		css = (CSS *) css->Pred();
	}

	SetSpecialBoolAttr(ATTR_STYLESHEET_DISABLED, value, SpecialNs::NS_LOGDOC);

	return OpStatus::OK;
}

OP_STATUS HTML_Element::DOMGetOffsetParent(DOM_Environment *environment, HTML_Element *&parent)
{
	parent = NULL;

	FramesDocument *frames_doc = environment->GetFramesDocument();
	if (!frames_doc || !frames_doc->IsCurrentDoc())
		return OpStatus::OK;

	LogicalDocument *logdoc = frames_doc->GetLogicalDocument();
	if (!logdoc)
		return OpStatus::OK;

#ifdef NS4P_COMPONENT_PLUGINS
	if (LayoutWorkplace* wp = logdoc->GetLayoutWorkplace())
		if (wp->IsTraversing() || wp->IsReflowing())
			return OpStatus::ERR_NOT_SUPPORTED;
#endif // NS4P_COMPONENT_PLUGINS

	HTML_Element* root = logdoc->GetRoot();
	if (!root || !root->IsAncestorOf(this))
		return OpStatus::OK;

	OP_STATUS status = OpStatus::OK;
	OpStatus::Ignore(status);

	if (OpStatus::IsSuccess(status = frames_doc->Reflow(FALSE)))
	{
		Head props_list;
		if (LayoutProperties::CreateCascade(this, props_list, logdoc->GetHLDocProfile()))
		{
			LayoutProperties *offset_cascade = ((LayoutProperties *)props_list.Last())->FindOffsetParent(logdoc->GetHLDocProfile());
			if (offset_cascade)
				parent = offset_cascade->html_element;
		}
		else
			status = OpStatus::ERR_NO_MEMORY;

		props_list.Clear();
	}

	return status;
}

static BoxRectType BoxRectTypeFromDOMPositionAndSizeType(HTML_Element::DOMPositionAndSizeType type)
{
	switch(type)
	{
	case HTML_Element::DOM_PS_OFFSET:
		return OFFSET_BOX;
	case HTML_Element::DOM_PS_CLIENT:
		return CLIENT_BOX;
	case HTML_Element::DOM_PS_CONTENT:
		return CONTENT_BOX;
	case HTML_Element::DOM_PS_BORDER:
		return BORDER_BOX;
	default:
		return SCROLL_BOX;
	}
}

static OpPoint BoxRectOriginFromDOMPositionAndSizeType(FramesDocument* doc, HTML_Element::DOMPositionAndSizeType type)
{
	if (type == HTML_Element::DOM_PS_BORDER || type == HTML_Element::DOM_PS_CONTENT)
		return doc->GetVisualViewport().TopLeft();
	else
		return OpPoint();
}

OP_STATUS HTML_Element::DOMGetPositionAndSize(DOM_Environment *environment, DOMPositionAndSizeType type, int &left, int &top, int &width, int &height)
{
	left = top = width = height = 0;

	FramesDocument *frames_doc = environment->GetFramesDocument();
	if (!frames_doc || !frames_doc->IsCurrentDoc())
		return OpStatus::OK;

	LogicalDocument *logdoc = frames_doc->GetLogicalDocument();
	if (!logdoc)
		return OpStatus::OK;

#ifdef NS4P_COMPONENT_PLUGINS
	if (LayoutWorkplace* wp = frames_doc->GetLayoutWorkplace())
		if (wp->IsTraversing() || wp->IsReflowing())
			return OpStatus::ERR_NOT_SUPPORTED;
#endif // NS4P_COMPONENT_PLUGINS

	if (frames_doc->IsFrameDoc())
	{
		if (IsMatchingType(HE_FRAME, NS_HTML))
		{
			// Everything asked seem to just return the view area
			// It's probably a too simple approximation but it
			// actually looked like that when I checked MSIE and Moz
			width = frames_doc->GetLayoutViewWidth();
			height = frames_doc->GetLayoutViewHeight();
		}
		return OpStatus::OK;
	}

	OP_STATUS status = OpStatus::OK;
	{
		OP_PROBE0(OP_PROBE_HTML_ELEMENT_DOMPOS_REFLOW);
		status = frames_doc->Reflow(FALSE);
	}

	if (OpStatus::IsSuccess(status) && layout_box)
	{
		BoxRectType brt = BoxRectTypeFromDOMPositionAndSizeType(type);
		OpPoint origin = BoxRectOriginFromDOMPositionAndSizeType(frames_doc, type);

		RECT rect;
		if (IsMatchingType(HE_TEXTAREA, NS_HTML) && brt == SCROLL_BOX && GetFormObject())
		{
			TextAreaObject* textarea_obj = static_cast<TextAreaObject*>(GetFormObject());
			textarea_obj->GetWidgetScrollPosition(left, top);
			textarea_obj->GetScrollableSize(width, height);
		}
		else
		if (logdoc->GetBoxRect(this, brt, rect))
		{
			left = rect.left - origin.x;
			top = rect.top - origin.y;
			width = rect.right - rect.left;
			height = rect.bottom - rect.top;
		}
	}

	return status;
}

OP_STATUS HTML_Element::DOMGetPositionAndSizeList(DOM_Environment *environment, DOMPositionAndSizeType type, OpVector<RECT> &rect_vector)
{
	FramesDocument *frames_doc = environment->GetFramesDocument();
	if (!frames_doc || !frames_doc->IsCurrentDoc())
		return OpStatus::OK;

	OP_ASSERT(rect_vector.GetCount() == 0);

	LogicalDocument *logdoc = frames_doc->GetLogicalDocument();
	if (!logdoc)
		return OpStatus::OK;

#ifdef NS4P_COMPONENT_PLUGINS
	if (LayoutWorkplace* wp = logdoc->GetLayoutWorkplace())
		if (wp->IsTraversing() || wp->IsReflowing())
			return OpStatus::ERR_NOT_SUPPORTED;
#endif // NS4P_COMPONENT_PLUGINS

	OP_STATUS status = OpStatus::OK;
	OpStatus::Ignore(status);

	if (OpStatus::IsSuccess(status = frames_doc->Reflow(FALSE)) && layout_box)
	{
		BoxRectType brt = BoxRectTypeFromDOMPositionAndSizeType(type);
		OpPoint origin = BoxRectOriginFromDOMPositionAndSizeType(frames_doc, type);

		RectList rect_list;
		if (!layout_box->GetRectList(frames_doc, brt, rect_list))
		{
			// Might be OOM, but may also be just unsupported case
			// in GetBoxRect. Assuming unsupported for now to avoid
			// killing scripts by mistake, so let's just drop down
			// and return an empty list.

			// status = OpStatus::ERR_NO_MEMORY;
		}
		else
			for (RectListItem *iter = rect_list.First(); iter != NULL && OpStatus::IsSuccess(status); iter = iter->Suc())
			{
				RECT *new_rect = OP_NEW(RECT, (iter->rect));
				if (!new_rect || OpStatus::IsMemoryError(rect_vector.Add(new_rect)))
					status = OpStatus::ERR_NO_MEMORY;
				else
				{
					new_rect->left -= origin.x;
					new_rect->right -= origin.x;
					new_rect->top -= origin.y;
					new_rect->bottom -= origin.y;
				}
			}

		rect_list.Clear();
	}

	return status;
}

OP_STATUS HTML_Element::DOMGetXYPosition(DOM_Environment *environment, int &x, int &y)
{
	/* Image.x and Image.y the way firefox does it (which is old and compatible with NS4),
	 * there isn't really any spec/documentation on it, see CORE-20517.
	 *
	 * It mostly behaves like offsetTop/Left with the following exceptions:
	 * - when the offsetParent is a Table or TableCell it behaves as if the offsetParent was the body
	 * - when the offsetParent is a fixed positioned element, x/y is the distance to the offsetParent,
	 *   instead of the body.
	 */
	int width, height; // not really used, needed for the PositionAndSize calls
	HTML_Element *offset_parent;

	RETURN_IF_ERROR(DOMGetPositionAndSize(environment, HTML_Element::DOM_PS_OFFSET, x, y, width, height));
	RETURN_IF_ERROR(DOMGetOffsetParent(environment, offset_parent));

	if (offset_parent)
	{
		if (offset_parent->Type() == HE_TABLE ||
			offset_parent->Type() == HE_TD ||
			offset_parent->Type() == HE_TH)
		{
			int tmp_x, tmp_y;
			HTML_Element *element = offset_parent;

			while(element)
			{
				GET_FAILED_IF_ERROR(element->DOMGetPositionAndSize(
						environment, HTML_Element::DOM_PS_OFFSET, tmp_x, tmp_y, width, height));
				x += tmp_x;
				y += tmp_y;
				GET_FAILED_IF_ERROR(element->DOMGetOffsetParent(environment, element));
			}

			if (FramesDocument *frames_doc = environment->GetFramesDocument())
				offset_parent = frames_doc->GetHLDocProfile()->GetBodyElm();
			else
				offset_parent = NULL;
		}
		else if (offset_parent->GetLayoutBox() && offset_parent->GetLayoutBox()->IsFixedPositionedBox())
		{
			RECT this_rect, parent_rect;

			if (GetLayoutBox()->GetRect(environment->GetFramesDocument(), BOUNDING_BOX, this_rect) &&
				offset_parent->GetLayoutBox()->GetRect(environment->GetFramesDocument(), BOUNDING_BOX, parent_rect))
			{
				x = this_rect.left - parent_rect.left;
				y = this_rect.top - parent_rect.top;
			}
		}
	}

	return OpStatus::OK;
}

OP_STATUS HTML_Element::DOMGetClientRects(DOM_Environment *environment, RECT *bounding_rect, OpVector<RECT> *rect_vector, HTML_Element *end_elm, int text_start_offset/*=0*/, int text_end_offset/*=-1*/)
{
	FramesDocument *frames_doc = environment->GetFramesDocument();
	if (!frames_doc || !frames_doc->IsCurrentDoc())
		return OpStatus::OK;

	LogicalDocument *logdoc = frames_doc->GetLogicalDocument();
	if (!logdoc)
		return OpStatus::OK;

	ClientRectObject rect_traversal(frames_doc, bounding_rect, rect_vector, this, end_elm, text_start_offset, text_end_offset);

	return rect_traversal.Traverse(logdoc->GetRoot());
}

OP_STATUS HTML_Element::DOMSetPositionAndSize(DOM_Environment *environment, DOMPositionAndSizeType type, int *left, int *top, int *width, int *height)
{
	FramesDocument *frames_doc = environment->GetFramesDocument();
	if (!frames_doc || !frames_doc->IsCurrentDoc())
		return OpStatus::OK;

#ifdef NS4P_COMPONENT_PLUGINS
	if (LayoutWorkplace* wp = frames_doc->GetLayoutWorkplace())
		if (wp->IsTraversing() || wp->IsReflowing())
			return OpStatus::ERR_NOT_SUPPORTED;
#endif // NS4P_COMPONENT_PLUGINS

	RETURN_IF_ERROR(frames_doc->Reflow(FALSE, TRUE));

	if (layout_box && type == DOM_PS_SCROLL)
	{
		if (frames_doc->GetHLDocProfile() && frames_doc->GetHLDocProfile()->IsInStrictMode()
			? IsMatchingType(HE_HTML, NS_HTML) : IsMatchingType(HE_BODY, NS_HTML))
		{
			if (frames_doc->GetLogicalDocument()->GetRoot()->IsAncestorOf(this))
				frames_doc->DOMSetScrollOffset(left, top);
		}
		else if (IsMatchingType(HE_TEXTAREA, NS_HTML) && GetFormObject())
		{
			TextAreaObject* textarea_obj = static_cast<TextAreaObject*>(GetFormObject());
			int current_left, current_top;
			textarea_obj->GetWidgetScrollPosition(current_left, current_top);
			textarea_obj->SetWidgetScrollPosition(left ? *left : current_left, top ? *top : current_top);
		}
		else
			layout_box->SetScrollOffset(left, top);
	}

	return OpStatus::OK;
}

OP_STATUS HTML_Element::DOMScrollIntoView(DOM_Environment *environment, BOOL align_to_top)
{
	FramesDocument *frames_doc = environment->GetFramesDocument();
	if (!frames_doc || !frames_doc->IsCurrentDoc())
		return OpStatus::OK;

	HTML_Document *html_doc = frames_doc->GetHtmlDocument();
	if (!html_doc)
		return OpStatus::OK;

	LogicalDocument* logdoc = frames_doc->GetLogicalDocument();
	if (!logdoc)
		return OpStatus::OK;

#ifdef NS4P_COMPONENT_PLUGINS
	if (LayoutWorkplace* wp = logdoc->GetLayoutWorkplace())
		if (wp->IsTraversing() || wp->IsReflowing())
			return OpStatus::ERR_NOT_SUPPORTED;
#endif // NS4P_COMPONENT_PLUGINS

	RETURN_IF_ERROR(frames_doc->Reflow(FALSE));

	html_doc->ScrollToElement(this, align_to_top ? SCROLL_ALIGN_TOP : SCROLL_ALIGN_BOTTOM, FALSE, VIEWPORT_CHANGE_REASON_SCRIPT_SCROLL);

	return OpStatus::OK;
}

OP_STATUS HTML_Element::DOMGetDistanceToRelative(DOM_Environment *environment, int &left, int &top, BOOL &relative_found)
{
	left = 0;
	top = 0;
	relative_found = FALSE;

	FramesDocument *frames_doc = environment->GetFramesDocument();
	if (!frames_doc || !frames_doc->IsCurrentDoc())
		return OpStatus::OK;

	LogicalDocument *logdoc = frames_doc->GetLogicalDocument();
	if (!logdoc)
		return OpStatus::OK;

#ifdef NS4P_COMPONENT_PLUGINS
	if (LayoutWorkplace* wp = logdoc->GetLayoutWorkplace())
		if (wp->IsTraversing() || wp->IsReflowing())
			return OpStatus::ERR_NOT_SUPPORTED;
#endif // NS4P_COMPONENT_PLUGINS

	OP_STATUS status = OpStatus::OK;
	OpStatus::Ignore(status);

	if (OpStatus::IsSuccess(status = frames_doc->Reflow(FALSE)) && layout_box &&
		(!layout_box->IsPositionedBox() || layout_box->IsAbsolutePositionedBox()))
	{
		HTML_Element *parent = ParentActual();

		while (parent)
		{
			Box *parent_box = parent->layout_box;

			if (!parent_box || parent_box->IsPositionedBox() && !parent_box->IsAbsolutePositionedBox())
			{
				relative_found = TRUE;
				break;
			}

			parent = parent->ParentActual();
		}

		if (!parent)
			parent = logdoc->GetBodyElm();

		if (parent && parent->layout_box)
		{
			RECT parent_rect, this_rect;

			if (logdoc->GetBoxRect(parent, BOUNDING_BOX, parent_rect) && logdoc->GetBoxRect(this, BOUNDING_BOX, this_rect))
			{
				left = this_rect.left - parent_rect.left;
				top = this_rect.top - parent_rect.top;
			}
		}
	}

	return status;
}

OP_STATUS HTML_Element::DOMGetPageInfo(DOM_Environment *environment, unsigned int& current_page, unsigned int& page_count)
{
	current_page = 0;
	page_count = 1;

	FramesDocument *frames_doc = environment->GetFramesDocument();
	if (!frames_doc || !frames_doc->IsCurrentDoc())
		return OpStatus::OK;

	RETURN_IF_ERROR(frames_doc->Reflow(FALSE, TRUE));

	if (layout_box)
		layout_box->GetPageInfo(current_page, page_count);

	return OpStatus::OK;
}

OP_STATUS HTML_Element::DOMSetCurrentPage(DOM_Environment *environment, unsigned int current_page)
{
	FramesDocument *frames_doc = environment->GetFramesDocument();
	if (!frames_doc || !frames_doc->IsCurrentDoc())
		return OpStatus::OK;

	RETURN_IF_ERROR(frames_doc->Reflow(FALSE, TRUE));

	if (layout_box)
		layout_box->SetCurrentPageNumber(current_page);

	return OpStatus::OK;
}

OP_STATUS HTML_Element::DOMInsertChild(DOM_Environment *environment, HTML_Element *child, HTML_Element *reference)
{
	for (HTML_Element *iter = child, *stop = child->NextSiblingActual(); iter != stop; iter = iter->NextActual())
		iter->SetInserted(HE_INSERTED_BY_DOM);

	if (reference)
		return child->PrecedeSafe(environment, reference);
	else
		return child->UnderSafe(environment, this);
}

OP_STATUS HTML_Element::DOMRemoveFromParent(DOM_Environment *environment)
{
	DocumentContext context(environment);

	LogicalDocument *logdoc = context.logdoc;

	BOOL in_document = FALSE;
	if (logdoc && logdoc->GetRoot())
		in_document = logdoc->GetRoot()->IsAncestorOf(this);

	Remove(context);

	if (in_document)
	{
		if (logdoc->GetDocRoot() == this)
		{
			logdoc->SetDocRoot(NULL);
			environment->NewRootElement(NULL);
		}

#ifdef DOCUMENT_EDIT_SUPPORT
		FramesDocument *frames_doc = context.frames_doc;
		if (frames_doc->GetDocumentEdit())
		{
			if (IsContentEditable())
				DisableContentEditable(frames_doc);
			if (frames_doc->GetDocumentEdit())
				frames_doc->GetDocumentEdit()->OnElementRemoved(this);
		}
#endif // DOCUMENT_EDIT_SUPPORT
	}

	return OpStatus::OK;
}

OP_STATUS HTML_Element::DOMRemoveAllChildren(DOM_Environment *environment)
{
	DocumentContext context(environment);

	FramesDocument *frames_doc = context.frames_doc;
	LogicalDocument *logdoc = context.logdoc;

	BOOL in_document = FALSE;
	if (logdoc && logdoc->GetRoot())
		in_document = logdoc->GetRoot()->IsAncestorOf(this);

	BOOL removed_something = FALSE;
	BOOL need_extra_dirty = FALSE;

	// Disconnect all real children
	while (HTML_Element *child = FirstChildActual())
	{
		removed_something = TRUE;
		child->Remove(context);

		if (child->Clean(context))
			child->Free(context);
	}

	// Remove all children that are invisible to script (the rest was removed above)
	while (HTML_Element *child = FirstChild())
	{
		removed_something = TRUE;
		child->Remove(context);

		need_extra_dirty = TRUE;

		if (child->Clean(context))
			child->Free(context);
	}

	if (in_document && removed_something)
	{
#ifdef DOCUMENT_EDIT_SUPPORT
		if (frames_doc->GetDocumentEdit())
			frames_doc->GetDocumentEdit()->OnElementRemoved(this);
#endif

		if (need_extra_dirty)
			MarkExtraDirty(frames_doc);
	}

	return OpStatus::OK;
}

OP_STATUS HTML_Element::DOMSetInnerHTML(DOM_Environment *environment, const uni_char *html, int html_len, HTML_Element *actual_parent_element, BOOL use_xml_parser)
{
	FramesDocument *frames_doc = environment->GetFramesDocument();
	if (!frames_doc || !frames_doc->GetHLDocProfile()) // Can happen if the element came from an old document that has been replaced/freed
		return OpStatus::ERR;

#ifdef MANUAL_PLUGIN_ACTIVATION
	ES_Thread *thread = environment->GetCurrentScriptThread();
	frames_doc->GetHLDocProfile()->ESSetScriptExternal(thread && ES_Runtime::GetIsExternal(thread->GetContext()));
#endif // MANUAL_PLUGIN_ACTIVATION

	OP_STATUS ret_stat = SetInnerHTML(frames_doc, html, html_len, actual_parent_element, use_xml_parser);

#ifdef MANUAL_PLUGIN_ACTIVATION
	frames_doc->GetHLDocProfile()->ESSetScriptExternal(FALSE);
#endif // MANUAL_PLUGIN_ACTIVATION

	return ret_stat;
}

OP_STATUS HTML_Element::SetInnerHTML(FramesDocument* frames_doc, const uni_char* html, int html_len/*=-1*/, HTML_Element *actual_parent_element/*=NULL*/, BOOL use_xml_parser/*=FALSE*/)
{
	OP_PROBE4(OP_PROBE_HTML_ELEMENT_SETINNERHTML);
	if (!frames_doc)
		return OpStatus::ERR;

	if (!frames_doc->IsContentChangeAllowed())
		return OpStatus::OK;

	HLDocProfile *hld_profile = frames_doc->GetHLDocProfile();
	LogicalDocument *logdoc = frames_doc->GetLogicalDocument();
	if (!logdoc || !hld_profile)
		return OpStatus::ERR;

	BOOL in_document = FALSE;
	if (HTML_Element *root = logdoc->GetRoot())
		if (root->IsAncestorOf(this))
			in_document = TRUE;

	hld_profile->SetHandleScript(FALSE);
	hld_profile->SetIsParsingInnerHTML(!in_document);

	OP_STATUS status = logdoc->ParseFragment(this, actual_parent_element, html, html_len, use_xml_parser);

	hld_profile->SetIsParsingInnerHTML(FALSE);
#ifdef DOCUMENT_EDIT_SUPPORT
	hld_profile->SetHandleScript(!frames_doc->GetDesignMode());
#else
	hld_profile->SetHandleScript(TRUE);
#endif // DOCUMENT_EDIT_SUPPORT

	if (in_document)
	{
		MarkDirty(frames_doc);
	}

	return status;
}

void HTML_Element::DOMMarqueeStartStop(BOOL stop)
{
	SetSpecialBoolAttr(ATTR_MARQUEE_STOPPED, stop, SpecialNs::NS_LOGDOC);
}

OP_STATUS HTML_Element::DOMCreateNullElement(HTML_Element *&element, DOM_Environment *environment)
{
	element = NEW_HTML_Element();
	if (!element)
		return OpStatus::ERR_NO_MEMORY;

	element->SetEndTagFound();
	g_ns_manager->GetElementAt(0)->IncRefCount();
	return OpStatus::OK;
}

OP_STATUS HTML_Element::DOMCreateElement(HTML_Element *&element, DOM_Environment *environment, const uni_char *name, int ns_idx/*=NS_IDX_DEFAULT*/, BOOL case_sensitive/*=FALSE*/)
{
	FramesDocument *frames_doc = environment->GetFramesDocument();
	if (!frames_doc)
		return OpStatus::ERR;
	LogicalDocument *logdoc = frames_doc->GetLogicalDocument();
	if (!logdoc)
		return OpStatus::ERR;
	HLDocProfile *hld_profile = logdoc->GetHLDocProfile();

	HtmlAttrEntry html_attrs[2];
	LogdocXmlName elm_name;

	unsigned name_length = name ? uni_strlen(name) : 0;

	DOM_LOWERCASE_NAME(name, name_length);

	int elm_type = HTM_Lex::GetElementType(name, g_ns_manager->GetNsTypeAt(ns_idx), case_sensitive);

	if (elm_type == HE_UNKNOWN)
	{
		html_attrs[0].ns_idx = SpecialNs::NS_LOGDOC;
		html_attrs[0].attr = ATTR_XML_NAME;
		html_attrs[0].is_id = FALSE;
		html_attrs[0].is_special = TRUE;
		html_attrs[0].is_specified = FALSE;
		RETURN_IF_MEMORY_ERROR(elm_name.SetName(name, name_length, FALSE));

		html_attrs[0].value = reinterpret_cast<uni_char*>(&elm_name); // urgh
		html_attrs[0].value_len = 0;
		html_attrs[1].attr = ATTR_NULL;
	}
	else
		html_attrs[0].attr = ATTR_NULL;

	element = NEW_HTML_Element();
	if (!element || element->Construct(hld_profile, ns_idx, (HTML_ElementType) elm_type, html_attrs, HE_INSERTED_BY_DOM))
	{
		DELETE_HTML_Element(element);
		return OpStatus::ERR_NO_MEMORY;
	}

	element->SetEndTagFound();

	// Add default attributes found in the doctype/dtd
	if (logdoc->IsXml())
	{
		if (XMLDocumentInformation *xmldocinfo = logdoc->GetXMLDocumentInfo())
			if (XMLDoctype *xmldoctype = xmldocinfo->GetDoctype())
				if (XMLDoctype::Element *elementdecl = xmldoctype->GetElement(element->GetTagName()))
				{
					XMLDoctype::Attribute **attrs = elementdecl->GetAttributes(), **attrs_end = attrs + elementdecl->GetAttributesCount();

					while (attrs != attrs_end)
					{
						XMLDoctype::Attribute *attr = *attrs++;
						const uni_char *defaultvalue = attr->GetDefaultValue();
						if (defaultvalue)
						{
							const uni_char *qname = attr->GetAttributeName();

							XMLCompleteNameN completenameN(qname, uni_strlen(qname));
							XMLCompleteName completename;

							if (OpStatus::IsMemoryError(completename.Set(completenameN)))
							{
								DELETE_HTML_Element(element);
								return OpStatus::ERR_NO_MEMORY;
							}

							int attr_ns_idx = NS_IDX_DEFAULT;

							if (completename.GetPrefix())
								if (HTML_Element *root = logdoc->GetDocRoot())
								{
									attr_ns_idx = root->FindNamespaceIndex(completename.GetPrefix());

									if (attr_ns_idx == NS_IDX_NOT_FOUND)
										continue;
								}
								else
									continue;
							else
								attr_ns_idx = NS_IDX_DEFAULT;

							if (OpStatus::IsMemoryError(element->SetAttribute(environment, ATTR_XML, completename.GetLocalPart(), attr_ns_idx, defaultvalue, uni_strlen(defaultvalue), environment->GetCurrentScriptThread(), case_sensitive, attr->GetType() == XMLDoctype::Attribute::TYPE_Tokenized_ID, FALSE)))
							{
								DELETE_HTML_Element(element);
								return OpStatus::ERR_NO_MEMORY;
							}
						}
					}
				}
	}

	return OpStatus::OK;
}

OP_STATUS HTML_Element::DOMCloneElement(HTML_Element *&element, DOM_Environment *environment, HTML_Element *prototype, BOOL deep)
{
	FramesDocument *frames_doc = environment->GetFramesDocument();
	if (!frames_doc)
		return OpStatus::ERR;

	HLDocProfile *hld_profile = frames_doc->GetHLDocProfile();
	if (!hld_profile)
		return OpStatus::ERR;

	element = NEW_HTML_Element();
	if (!element)
		return OpStatus::ERR_NO_MEMORY;

	if (OpStatus::IsMemoryError(element->Construct(hld_profile, prototype)) ||
		deep && OpStatus::IsMemoryError(element->DeepClone(hld_profile, prototype)))
	{
		if (element->Clean(frames_doc))
			element->Free(frames_doc);

		return OpStatus::ERR_NO_MEMORY;
	}

	element->SetInserted(HE_INSERTED_BY_DOM);
	element->SetEndTagFound();
	return OpStatus::OK;
}

#define uni_str_eq_safe(a, b) ((a) == (b) || ((a) && (b) && uni_str_eq(a,b)))

/* static */
OP_BOOLEAN HTML_Element::DOMAreEqualNodes(const HTML_Element *left, HLDocProfile *left_hldoc,
		const HTML_Element *right, HLDocProfile *right_hldoc, BOOL case_sensitive)
{
	OP_ASSERT(left && right);
	OP_ASSERT(left != right); // Don't compare the same nodes, it's a waste of time.

	// success means that the code traversed the tree to the end without finding differences.
	BOOL oom = FALSE, success = FALSE;
	TempBuffer left_buffer, right_buffer;
	const HTML_Element *left_stop = left->NextSiblingActual(), *right_stop = right->NextSiblingActual();

	OP_NEW_DBG("HTML_Element::DOMAreEqualNodes","logdoc");

	while (!success)
	{
		if (left == NULL || right == NULL)
			break;
#ifdef _DEBUG
		OP_DBG(("Comparing %d/%S vs. %d/%S\n", left->Type(), left->GetTagName(),
				right->Type(), right->GetTagName()));
# define DOMAreEqualNodes_trace(expr) { OP_DBG(("Failed at line %d\n", __LINE__)); expr; }
#else
# define DOMAreEqualNodes_trace(expr) expr
#endif

		// Deal with TEXTGROUP here because of Type() perhaps not matching.
		if (left->IsText() && right->IsText())
		{
			// nodeName == #text
			// localName == always null
			// namespaceURI == always null
			// prefix == always null
			// nodeValue is the text checked below

			const uni_char *left_text, *right_text;
			unsigned left_length, right_length;
			if (OpStatus::IsMemoryError(DOMAreEqualNodes_GetText(left, left_buffer, left_text, left_length)) ||
				OpStatus::IsMemoryError(DOMAreEqualNodes_GetText(right, right_buffer, right_text, right_length)))
			{
				oom = TRUE;
				break;
			}
			if (left_length != right_length || !uni_str_eq_safe(left_text, right_text))
				DOMAreEqualNodes_trace(break);
		}
		else if (left->Type() != right->Type())
			DOMAreEqualNodes_trace(break);

		switch (left->Type())
		{
		case HE_TEXTGROUP:
		case HE_TEXT:
			// These were handled above.
			break;

		case HE_PROCINST:
		{
			// nodeName == proc inst target, checked below
			// localName == always null
			// namespaceURI == always null
			// prefix == always null
			// nodeValue checked below after fall through
			const uni_char *left_target = left->GetStringAttr(ATTR_TARGET, NS_IDX_DEFAULT);
			const uni_char *right_target = right->GetStringAttr(ATTR_TARGET, NS_IDX_DEFAULT);
			if (!uni_str_eq_safe(left_target, right_target))
				DOMAreEqualNodes_trace(goto DOMAreEqualNodes_end);
			// allow fall through
		}
		case HE_COMMENT:
		{
			// nodeName == #comment or the proc inst target, checked above
			// localName == always null
			// namespaceURI == always null
			// prefix == always null
			// nodeValue is the comment text or proc inst data, checked below
			const uni_char *left_content = left->GetStringAttr(ATTR_CONTENT);
			const uni_char *right_content = right->GetStringAttr(ATTR_CONTENT);
			if (!uni_str_eq_safe(left_content, right_content))
				DOMAreEqualNodes_trace(goto DOMAreEqualNodes_end);
			break;
		}
		case HE_ENTITY:
		case HE_ENTITYREF:
			// These are never kept in the tree after parsing, nor is the DOM code enabled.
			OP_ASSERT(!"Not supported ATM");
			DOMAreEqualNodes_trace(goto DOMAreEqualNodes_end);

		case HE_DOCTYPE:
		{
			// nodeName == the dtd name
			// localName == always null
			// namespaceURI == always null
			// prefix == always null
			// nodeValue always null
			// publicId, systemId and internalSubset need to be checked too because it's a doctype
			// and need to check too the entities map and notations map
			const uni_char *left_pubid, *left_sysid, *left_name, *left_intsubset;
			const uni_char *right_pubid, *right_sysid, *right_name, *right_intsubset;
			DOMAreEqualNodes_GetDocTypeHEInfo(left, left_hldoc, left_pubid, left_sysid, left_name, left_intsubset);
			DOMAreEqualNodes_GetDocTypeHEInfo(right, right_hldoc, right_pubid, right_sysid, right_name, right_intsubset);

			if (!uni_str_eq_safe(left_pubid, right_pubid) ||
				!uni_str_eq_safe(left_sysid, right_sysid) ||
				!uni_str_eq_safe(left_name, right_name) ||
				!uni_str_eq_safe(left_intsubset, right_intsubset))
				DOMAreEqualNodes_trace(goto DOMAreEqualNodes_end);
			break;
		}
		case HE_DOC_ROOT:
		case HE_UNKNOWN:
		default: // includes all HE types after HE_UNKNOWN
			if (left->Type() != HE_DOC_ROOT)
			{
				OP_ASSERT(Markup::IsRealElement(left->Type()));
				// nodeName == elements name or tagName. Special case for unknown types, all others are already checked by Type()
				// localName == the tagName (again)
				// namespaceURI == compare namespace index
				// prefix == compare namespace index
				// nodeValue == null
				if (left->Type() == HE_UNKNOWN)
				{
					const uni_char *left_tag, *right_tag;
					left_tag = left->GetTagName();
					right_tag = right->GetTagName();
					OP_ASSERT(left_tag && right_tag);
					if (case_sensitive ? uni_strcmp(left_tag, right_tag) != 0 : uni_stricmp(left_tag, right_tag) != 0)
						DOMAreEqualNodes_trace(goto DOMAreEqualNodes_end);
				}
				if (left->GetNsIdx() != right->GetNsIdx())
					// Compares namespace and prefix.
					DOMAreEqualNodes_trace(goto DOMAreEqualNodes_end);

				// Compare attributes -> O(N^2) is pain
				unsigned left_attr_count = 0;
				int left_attr_index = 0, attr_size = left->GetAttrSize();
				for (; left_attr_index < attr_size; left_attr_index++)
				{
					if (left->GetAttrIsSpecial(left_attr_index))
						continue;
					left_attr_count++;

					// nodeName == the attr name
					// localName == the attr name
					// namespaceURI == compare namespace index
					// prefix == compare namespace index
					// nodeValue == the attr value

					//int attr, const uni_char *name, int ns_idx, BOOL case_sensitive, BOOL strict_ns
					int right_attr_index = right->FindAttrIndex(
							left->GetAttrItem(left_attr_index),
							left->GetAttrNameString(left_attr_index),
							left->GetAttrNs(left_attr_index),
							case_sensitive,
							FALSE);
					// Attr not found -> this validates nodeName, localName, namespaceURI and prefix (hopefully).
					if (right_attr_index < 0)
						DOMAreEqualNodes_trace(break);

					ItemType attr_type = left->GetItemType(left_attr_index);
					if (attr_type == right->GetItemType(right_attr_index))
					{
						void *left_attr_value = left->GetValueItem(left_attr_index);
						if (!AreAttributesEqual(left->GetAttrItem(left_attr_index), attr_type, left_attr_value, right->GetValueItem(right_attr_index)))
						{
							if (attr_type == ITEM_TYPE_COMPLEX && !static_cast<ComplexAttr*>(left_attr_value)->Equals(static_cast<ComplexAttr*>(left_attr_value)))
							{
								// Unfortunate hack. Most derived classes from
								// ComplexAttr do not implement Equals so we need
								// to fallback to string serialization and comparison,
								// if that works too.
								goto DOMAreEqualNodes_compareAttrsString;
							}
							DOMAreEqualNodes_trace(break);
						}
					}
					else
					{
DOMAreEqualNodes_compareAttrsString:
						// Different types... just serialize to string. This section will rarely run anyway,
						// because the attributes are the same, so they will most likely be of the same type.
						left_buffer.Clear();
						right_buffer.Clear();

						const uni_char *left_value = left->GetAttrValueValue(left_attr_index, left->GetAttrItem(left_attr_index), HE_UNKNOWN, &left_buffer);
						// Need to copy the value to the TempBuffer because of use of global shared buffers by logdoc.
						if (left_value && left_value != left_buffer.GetStorage())
						{
							if (OpStatus::IsMemoryError(left_buffer.Append(left_value)))
							{
								oom = TRUE;
								DOMAreEqualNodes_trace(break);
							}
							else
								left_value = left_buffer.GetStorage();
						}

						const uni_char *right_value = right->GetAttrValueValue(right_attr_index, right->GetAttrItem(right_attr_index), HE_UNKNOWN, &right_buffer);

						if (!uni_str_eq_safe(left_value, right_value))
							DOMAreEqualNodes_trace(break);
					}
				}
				// Different number of attributes or loop aborted before
				// traversing all attributes.
				if (left_attr_index < attr_size || left_attr_count != (unsigned)right->GetAttrCount())
					DOMAreEqualNodes_trace(goto DOMAreEqualNodes_end);
			}
		}

		if (left->IsText())
		{
			// Special case so this loop won't go through
			// the children of HE_TEXTGROUP.
			left = left->NextSiblingActual();
			right = right->NextSiblingActual();
		}
		else
		{
			left = left->NextActual();
			right = right->NextActual();
		}

		if (left == left_stop && right == right_stop)
		{
			// Both trees are the same size, and if we
			// got here then they are also equal.
			success = TRUE;
			break;
		}
		else if (left == left_stop || right == right_stop)
			// One of the trees is bigger than the other.
			break;
	}
DOMAreEqualNodes_end:
	if (oom)
		return OpStatus::ERR_NO_MEMORY;
	else if (success)
		return OpBoolean::IS_TRUE;
	else
		return OpBoolean::IS_FALSE;
}

#undef uni_str_eq_safe

/* static */
void HTML_Element::DOMAreEqualNodes_GetDocTypeHEInfo(const HTML_Element *element, HLDocProfile *hldoc,
		const uni_char *&pubid, const uni_char *&sysid, const uni_char *&name, const uni_char *&intsubset)
{
	const XMLDocumentInformation *docinfo = element->GetXMLDocumentInfo();
	if (docinfo)
	{
		pubid = docinfo->GetPublicId();
		sysid = docinfo->GetSystemId();
		name = docinfo->GetDoctypeName();
		intsubset = docinfo->GetInternalSubset();
	}
	else if (hldoc)
	{
		pubid = hldoc->GetDoctypePubId();
		sysid = hldoc->GetDoctypeSysId();
		name = hldoc->GetDoctypeName();
		intsubset = NULL;
	}
	else
		pubid = sysid = name = intsubset = NULL;
}

/* static */
OP_STATUS HTML_Element::DOMAreEqualNodes_GetText(const HTML_Element *elm, TempBuffer &buffer, const uni_char *&result, unsigned &result_length)
{
	if (elm->Type() == HE_TEXTGROUP)
	{
		buffer.Clear();
		for (HTML_Element *child = elm->FirstChild(); child; child = child->Suc())
		{
			OP_ASSERT(child->Type() == HE_TEXT);
			RETURN_IF_ERROR(buffer.Append(child->Content(), child->ContentLength()));
		}
		result = buffer.GetStorage();
		result_length = buffer.Length();
	}
	else
	{
		result = elm->Content();
		result_length = elm->ContentLength();
	}
	return OpStatus::OK;
}

OP_STATUS HTML_Element::DOMCreateTextNode(HTML_Element *&element, DOM_Environment *environment, const uni_char *text, BOOL comment, BOOL cdata_section)
{
	FramesDocument *frames_doc = environment->GetFramesDocument();
	if (!frames_doc)
		return OpStatus::ERR;

	HLDocProfile *hld_profile = frames_doc->GetHLDocProfile();
	if (!hld_profile)
		return OpStatus::ERR;

	if (comment)
	{
		element = NEW_HTML_Element();
		if (!element)
			return OpStatus::ERR_NO_MEMORY;

		HtmlAttrEntry ha_list[2];

		ha_list[0].attr = ATTR_CONTENT;
		ha_list[0].ns_idx = NS_IDX_DEFAULT;
		ha_list[0].value = text;
		ha_list[0].value_len = uni_strlen(text);
		ha_list[1].attr = ATTR_NULL;

		if (element->Construct(hld_profile, NS_IDX_HTML, HE_COMMENT, ha_list, HE_INSERTED_BY_DOM) == OpStatus::ERR_NO_MEMORY)
		{
			DELETE_HTML_Element(element);
			return OpStatus::ERR_NO_MEMORY;
		}
	}
	else
	{
		BOOL expand_wml_vars = FALSE; // Or? It used to be this way, but that doesn't mean it's right
		element = HTML_Element::CreateText(text, uni_strlen(text), FALSE, cdata_section, expand_wml_vars);
		if (!element)
		{
			return OpStatus::ERR_NO_MEMORY;
		}
		element->SetInserted(HE_INSERTED_BY_DOM);
	}

	return OpStatus::OK;
}

OP_STATUS HTML_Element::DOMCreateProcessingInstruction(HTML_Element *&element, DOM_Environment *environment, const uni_char *target, const uni_char *data)
{
	OP_ASSERT(target);
	OP_ASSERT(data);

	FramesDocument *frames_doc = environment->GetFramesDocument();
	if (!frames_doc)
		return OpStatus::ERR;

	HLDocProfile *hld_profile = frames_doc->GetHLDocProfile();
	if (!hld_profile)
		return OpStatus::ERR;

	element = NEW_HTML_Element();
	if (!element)
		return OpStatus::ERR_NO_MEMORY;

	HtmlAttrEntry ha_list[9];

	ha_list[0].attr = ATTR_TARGET;
	ha_list[0].ns_idx = NS_IDX_DEFAULT;
	ha_list[0].value = target;
	ha_list[0].value_len = uni_strlen(target);
	ha_list[1].attr = ATTR_CONTENT;
	ha_list[1].ns_idx = NS_IDX_DEFAULT;
	ha_list[1].value = data;
	ha_list[1].value_len = uni_strlen(data);
	ha_list[2].attr = ATTR_NULL;

	if (uni_str_eq(target, "xml-stylesheet"))
	{
		int lexer_status;
		int ha_index = 2;
		int attributes[] = { ATTR_HREF, ATTR_TYPE, ATTR_TITLE, ATTR_MEDIA, ATTR_CHARSET, ATTR_ALTERNATE };
		int attribute_found[] = { FALSE,  FALSE,  FALSE,  FALSE,  FALSE,  FALSE };
		OP_ASSERT(sizeof(attributes) == sizeof(attribute_found));
		uni_char* attr_p = const_cast<uni_char*>(data); // HTM_Lex doesn't want a const pointer
		uni_char* end = const_cast<uni_char*>(data + ha_list[1].value_len);
		do
		{
			lexer_status = htmLex->GetAttrValue(&attr_p, end, ha_list+ha_index,
				TRUE, hld_profile, FALSE, NS_HTML, 1, FALSE);
			if (lexer_status == HTM_Lex::ATTR_RESULT_FOUND)
			{
				for (size_t i = 0; i < sizeof(attributes)/sizeof(*attributes); i++)
				{
					if (ha_list[ha_index].attr == attributes[i] && !attribute_found[i])
					{
						ha_index++;
						attribute_found[i] = TRUE;
						break;
					}
				}
			}
			ha_list[ha_index].attr = ATTR_NULL;
		}
		while (lexer_status == HTM_Lex::ATTR_RESULT_FOUND);
	}

	if (element->Construct(hld_profile, NS_IDX_HTML, HE_PROCINST, ha_list, HE_INSERTED_BY_DOM) == OpStatus::ERR_NO_MEMORY)
	{
		DELETE_HTML_Element(element);
		return OpStatus::ERR_NO_MEMORY;
	}

	return OpStatus::OK;
}

OP_STATUS HTML_Element::DOMCreateDoctype(HTML_Element *&element, DOM_Environment *environment, XMLDocumentInformation *docinfo)
{
	FramesDocument *frames_doc = environment->GetFramesDocument();
	if (!frames_doc)
		return OpStatus::ERR;

	HLDocProfile *hld_profile = frames_doc->GetHLDocProfile();
	if (!hld_profile)
		return OpStatus::ERR;

	element = NEW_HTML_Element();
	if (!element)
		return OpStatus::ERR_NO_MEMORY;

	HtmlAttrEntry ha_list[1];
	ha_list[0].attr = ATTR_NULL;

	if (element->Construct(hld_profile, NS_IDX_HTML, HE_DOCTYPE, ha_list, HE_INSERTED_BY_DOM) == OpStatus::ERR_NO_MEMORY)
	{
		DELETE_HTML_Element(element);
		return OpStatus::ERR_NO_MEMORY;
	}

	if (docinfo && docinfo->GetDoctypeDeclarationPresent())
	{
		XMLDocumentInfoAttr *attr;

		if (OpStatus::IsMemoryError(XMLDocumentInfoAttr::Make(attr, docinfo)))
		{
			DELETE_HTML_Element(element);
			return OpStatus::ERR_NO_MEMORY;
		}

		if (element->SetSpecialAttr(ATTR_XMLDOCUMENTINFO, ITEM_TYPE_COMPLEX, static_cast<ComplexAttr *>(attr), TRUE, SpecialNs::NS_LOGDOC) == -1)
		{
			OP_DELETE(attr);
			DELETE_HTML_Element(element);
			return OpStatus::ERR_NO_MEMORY;
		}
	}

	return OpStatus::OK;
}

void HTML_Element::DOMFreeElement(HTML_Element *element, DOM_Environment *environment)
{
	if (element && element->Clean(environment))
		element->Free(environment);
}

BOOL HTML_Element::HasEventHandlerAttribute(FramesDocument *doc, DOM_EventType type)
{
	for (unsigned index = 0, size = GetAttrSize(); index < size; ++index)
		if (GetAttrIsEvent(index) && GetEventType(GetAttrItem(index), GetResolvedAttrNs(index)) == type)
			return TRUE;
	return FALSE;
}

BOOL HTML_Element::HasEventHandlerAttributes(FramesDocument *doc)
{
	for (unsigned index = 0, size = GetAttrSize(); index < size; ++index)
		if (GetAttrIsEvent(index))
			return TRUE;
	return FALSE;
}

BOOL HTML_Element::DOMHasEventHandlerAttribute(DOM_Environment *environment, DOM_EventType type)
{
	return HasEventHandlerAttribute(environment->GetFramesDocument(), type);
}

BOOL HTML_Element::DOMHasEventHandlerAttributes(DOM_Environment *environment)
{
	return HasEventHandlerAttributes(environment->GetFramesDocument());
}

OP_STATUS HTML_Element::DOMSetEventHandlers(DOM_Environment *environment)
{
	FramesDocument *doc = environment->GetFramesDocument();
	if (doc
#ifdef DOCUMENT_EDIT_SUPPORT
		&& !doc->GetDesignMode()
#endif
		)
		for (unsigned index = 0, size = GetAttrSize(); index < size; ++index)
			if (GetAttrIsEvent(index))
			{
				short attr_item = GetAttrItem(index);
				int ns_idx = GetResolvedAttrNs(index);
				NS_Type ns_type = g_ns_manager->GetNsTypeAt(ns_idx);

				BOOL already_set = FALSE;
				for (unsigned index2 = index; index2 != 0 && !already_set; --index2)
					if (GetAttrItem(index2 - 1) == attr_item && g_ns_manager->GetNsTypeAt(GetResolvedAttrNs(index2 - 1)) == ns_type)
						already_set = TRUE;

				if (!already_set)
				{
					DOM_EventType type = GetEventType(GetAttrItem(index), ns_idx);
					OP_ASSERT(type != DOM_EVENT_NONE);

					const uni_char *value = (const uni_char *) GetValueItem(index);
					RETURN_IF_ERROR(SetEventHandler(doc, type, value, uni_strlen(value)));
				}
			}

	return OpStatus::OK;
}

OP_STATUS HTML_Element::SendEvent(DOM_EventType event, FramesDocument *doc)
{
	if (event == ONLOAD || event == ONERROR)
		if (SetSpecialBoolAttr(ATTR_INLINE_ONLOAD_SENT, TRUE, SpecialNs::NS_LOGDOC) == -1)
			return OpStatus::ERR_NO_MEMORY;

#ifdef DELAYED_SCRIPT_EXECUTION
	if (GetInserted() == HE_INSERTED_BY_PARSE_AHEAD && (event == ONLOAD || event == ONERROR))
	{
		SetSpecialBoolAttr(event == ONLOAD ? ATTR_JS_DELAYED_ONLOAD : ATTR_JS_DELAYED_ONERROR, TRUE, SpecialNs::NS_LOGDOC);
		return OpStatus::OK;
	}
#endif // DELAYED_SCRIPT_EXECUTION

	return doc->HandleEvent(event, NULL, this, SHIFTKEY_NONE);
}

BOOL HTML_Element::HasEventHandler(FramesDocument *doc, DOM_EventType type, BOOL local_only)
{
	DOM_Environment *environment = doc->GetDOMEnvironment();

	if (environment)
		if (local_only)
			return environment->HasLocalEventHandler(this, type);
		else
			return environment->HasEventHandler(this, type);
	else
	{
		HTML_Element *element = this;

		unsigned count;
		if (doc->GetLogicalDocument()->GetEventHandlerCount(type, count) && count == 0)
			return FALSE;

		while (element)
		{
			if (DOM_Utils::GetEventTargetElement(element)->HasEventHandlerAttribute(doc, type))
				return TRUE;
			else if (local_only)
				break;
			else
				element = DOM_Utils::GetEventPathParent(element, this);
		}

		return FALSE;
	}
}

#ifdef XSLT_SUPPORT

void HTML_Element::XSLTStripWhitespace(LogicalDocument *logdoc, XSLT_Stylesheet *stylesheet)
{
	LogdocXSLTHandler::StripWhitespace(logdoc, this, stylesheet);
}

#endif // XSLT_SUPPORT

const uni_char*	HTML_Element::GetXmlName() const
{
	LogdocXmlName *name = static_cast<LogdocXmlName*>(GetSpecialAttr(ATTR_XML_NAME, ITEM_TYPE_COMPLEX, (void*)NULL, SpecialNs::NS_LOGDOC));
	return name ? name->GetName().GetLocalPart() : NULL;
}

const uni_char*	HTML_Element::GetTagName(BOOL as_uppercase, TempBuffer *buffer) const
{
	Markup::Type type = Type();
	if (Markup::HasNameEntry(type))
		return HTM_Lex::GetElementString(type, GetNsIdx(), as_uppercase);
	else if (type == Markup::HTE_UNKNOWN)
	{
		const uni_char *name = GetXmlName();

		if (!as_uppercase || !buffer)
			return name;

		if (OpStatus::IsError(buffer->Append(name)))
			return NULL;

		uni_char *tmp_name = buffer->GetStorage();

		if (tmp_name)
		{
			while (*tmp_name)
			{
				if (*tmp_name >= 'a' && *tmp_name <= 'z')
					*tmp_name = *tmp_name - 'a' + 'A';
				tmp_name++;
			}
		}
		return buffer->GetStorage();
	}
	else
		return NULL;
}

const uni_char* HTML_Element::GetAttrNameString(int i) const
{
	if (GetAttrIsSpecial(i))
		return NULL;

	Markup::AttrType attr_code = GetAttrItem(i);

	if (attr_code == ATTR_XML)
		return (uni_char*)GetValueItem(i);

	return HTM_Lex::GetAttributeString(attr_code, g_ns_manager->GetNsTypeAt(GetResolvedAttrNs(i)));
}

const uni_char* HTML_Element::GetAttrValueString(int i) const
{
	return GetAttrValueValue(i, GetAttrItem(i));
}

void HTML_Element::SetFormObjectBackup(FormObject* fobj)
{
	SetSpecialAttr(ATTR_FORMOBJECT_BACKUP, ITEM_TYPE_NUM, (void*) fobj, TRUE, SpecialNs::NS_LOGDOC);
}

FormObject* HTML_Element::GetFormObjectBackup()
{
	return (FormObject*) GetSpecialAttr(ATTR_FORMOBJECT_BACKUP, ITEM_TYPE_NUM, NULL, SpecialNs::NS_LOGDOC);
}

void HTML_Element::DestroyFormObjectBackup()
{
	FormObject* form_object = GetFormObjectBackup();
	if (form_object)
	{
		OP_DELETE(form_object);
		SetFormObjectBackup(NULL);
	}
}

#ifdef DOCUMENT_EDIT_SUPPORT

BOOL HTML_Element::IsContentEditable(BOOL inherit)
{
	HTML_Element* helm = this;
	while(helm)
	{
		/* MS spec: "Though the TABLE, COL, COLGROUP, TBODY, TD, TFOOT, TH, THEAD, and TR elements cannot
		be set as content editable directly, a content editable SPAN, or DIV element can be placed
		inside the individual table cells (TD and TH elements). See the example below." */
		BOOL allowed = TRUE;
		NS_Type ns = helm->GetNsType();
		if(ns == NS_HTML)
		{
			switch (helm->Type())
			{
			case HE_TABLE:
			case HE_COL:
			case HE_COLGROUP:
			case HE_TBODY:
			case HE_TD:
			case HE_TFOOT:
			case HE_TH:
			case HE_THEAD:
			case HE_TR:
				allowed = FALSE;
			}
		}

		if (allowed)
		{
			BOOL3 is_content_editable = helm->GetContentEditableValue();
			if (is_content_editable == YES)
				return TRUE;
			else if (is_content_editable == NO)
				return FALSE;
		}
		if (!inherit)
			return FALSE;
		helm = helm->ParentActual();
	}
	return FALSE;
}

BOOL3
HTML_Element::GetContentEditableValue()
{
	const uni_char* val = GetStringAttr(ATTR_CONTENTEDITABLE);
	if (val)
	{
		if (uni_stricmp(val, UNI_L("TRUE")) == 0 || !*val) // "true" or the empty string turns on documentedit
			return YES;
		else if (uni_stricmp(val, UNI_L("FALSE")) == 0)
			return NO;
	}

	return MAYBE;
}

#endif // DOCUMENT_EDIT_SUPPORT

#if defined(DOCUMENT_EDIT_SUPPORT) && defined(WIDGETS_IME_SUPPORT)
void HTML_Element::SetIMEStyling(int value)
{
	SetSpecialNumAttr(ATTR_IME_STYLING, value, SpecialNs::NS_LOGDOC);
}

int HTML_Element::GetIMEStyling()
{
	return (int) GetSpecialNumAttr(ATTR_IME_STYLING, SpecialNs::NS_LOGDOC);
}
#endif

#ifdef SVG_SUPPORT
const ClassAttribute* HTML_Element::GetSvgClassAttribute() const
{
	return g_svg_manager->GetClassAttribute(const_cast<HTML_Element*>(this), FALSE);
}

void HTML_Element::SetSVGContext(SVGContext* ctx)
{
	OP_ASSERT(IsText() || GetNsType() == NS_SVG); // Dont' call this for random elements. Only SVG elements can store an SVGContext
	OP_ASSERT(ctx || GetSVGContext());
	OP_ASSERT(!ctx || !GetSVGContext());
	svg_context = ctx;
}

void HTML_Element::DestroySVGContext()
{
	if (IsText() || GetNsType() == NS_SVG)
	{
		OP_DELETE(svg_context);
		svg_context = NULL;
	}
}
#endif // SVG_SUPPORT

HEListElm* HTML_Element::BgImageIterator::Next()
{
	if (!m_current)
		m_current = m_elm->GetFirstReference();
	else
		m_current = m_current->NextRef();

	while (m_current)
	{
		if (m_current->IsA(ElementRef::HELISTELM))
		{
			HEListElm *ref = static_cast<HEListElm*>(m_current);
			if (ref->GetInlineType() == BGIMAGE_INLINE || ref->GetInlineType() == EXTRA_BGIMAGE_INLINE)
				return ref;
		}

		m_current = m_current->NextRef();
	}

	return NULL;
}

HEListElm* HTML_Element::GetHEListElmForInline(InlineResourceType inline_type)
{
	ElementRef *iter = m_first_ref;
	while (iter)
	{
		if (iter->IsA(ElementRef::HELISTELM))
		{
			HEListElm *ref = static_cast<HEListElm*>(iter);
			if (ref->GetInlineType() == inline_type)
				return ref;
		}

		iter = iter->NextRef();
	}

	return NULL;
}

HEListElm* HTML_Element::GetHEListElm(BOOL background)
{
	return GetHEListElmForInline(background ? BGIMAGE_INLINE : IMAGE_INLINE);
}

void HTML_Element::UndisplayImage(FramesDocument* doc, BOOL background)
{
	if (background)
	{
		HTML_Element::BgImageIterator iter(this);
		HEListElm *iter_elm = iter.Next();
		while (iter_elm)
		{
			iter_elm->Undisplay();
			iter_elm = iter.Next();
		}
	}
	else
	{
		HEListElm* helm = GetHEListElmForInline(IMAGE_INLINE);
		if (helm)
			helm->Undisplay();
	}
}

#ifdef _PRINT_SUPPORT_

HTML_Element* HTML_Element::CopyAll(HLDocProfile* hld_profile)
{
	HTML_Element* copy_root = NEW_HTML_Element();

	if (!copy_root || copy_root->Construct(hld_profile, this, TRUE) == OpStatus::ERR_NO_MEMORY)
	{
		DELETE_HTML_Element(copy_root);
		hld_profile->SetIsOutOfMemory(TRUE);
		return NULL;
	}


	HTML_Element* this_iter = this;
	HTML_Element* copy_iter = copy_root;

	HTML_Element* next;

	BOOL sidestep = FALSE;

	while (this_iter)
	{
		HTML_Element* first_child_actual = this_iter->FirstChildActual();
		HTML_Element* suc_actual = NULL;
		if (!first_child_actual || sidestep)
			suc_actual = this_iter->SucActual();
		if (first_child_actual || suc_actual)
		{
			if (first_child_actual && !sidestep)
				next = first_child_actual;
			else
				next = suc_actual;

			HTML_Element* new_elm = NEW_HTML_Element();

			if (!new_elm || new_elm->Construct(hld_profile, next, TRUE) == OpStatus::ERR_NO_MEMORY)
			{
				DELETE_HTML_Element(new_elm);
				hld_profile->SetIsOutOfMemory(TRUE);
				copy_root->Free(hld_profile->GetFramesDocument());
				return NULL;
			}

			if (new_elm->GetNs() == Markup::HTML
				&& (new_elm->Type() == HE_IFRAME || new_elm->Type() == HE_FRAME || new_elm->Type() == HE_FRAMESET || new_elm->Type() == HE_OBJECT))
			{
				FramesDocElm *fde = FramesDocElm::GetFrmDocElmByHTML(next);
				if (fde)
					fde->SetPrintTwinElm(new_elm);
			}

			if (this_iter->FirstChildActual() && !sidestep)
				new_elm->Under(copy_iter);
			else
				new_elm->Follow(copy_iter);

			sidestep = FALSE;

			this_iter = next;
			copy_iter = new_elm;
		}
		else
		{
			while (this_iter && !this_iter->SucActual())
			{
				this_iter = this_iter->ParentActual();
				copy_iter = copy_iter->ParentActual();
			}

			sidestep = TRUE;
		}
	}
	return copy_root;
}
#endif // _PRINT_SUPPORT_

URL*
HTML_Element::GetUrlAttr(short attr, int ns_idx/*=NS_IDX_HTML*/, LogicalDocument *logdoc/*=NULL*/)
{
	OP_PROBE4(OP_PROBE_LOGDOC_GETURLATTR);

	int index = FindAttrIndex(attr, NULL, ns_idx, GetNsIdx() != NS_IDX_HTML, ns_idx != NS_IDX_HTML);
	if (index == -1)
		return NULL;

	ns_idx = GetAttrNs(index);

	UrlAndStringAttr *url_attr;
	if (GetItemType(index) == ITEM_TYPE_STRING)
	{
		uni_char *string_val = static_cast<uni_char*>(GetValueItem(index));
		OP_STATUS oom = UrlAndStringAttr::Create(string_val, uni_strlen(string_val), url_attr);
		if (OpStatus::IsMemoryError(oom))
			return NULL;

		ReplaceAttrLocal(index, attr, ITEM_TYPE_URL_AND_STRING, (void*)url_attr, ns_idx, TRUE, FALSE, FALSE, GetAttrIsSpecified(index), FALSE);
	}
	else
		url_attr = static_cast<UrlAndStringAttr*>(GetValueItem(index));

	if (URL* cached_url = url_attr->GetResolvedUrl())
		return cached_url;

	const uni_char *url_str = url_attr->GetString();
	if (!url_str)
		return NULL;

	URL ret_url;
#ifdef _PLUGIN_SUPPORT_
	// URLs with only whitespace should be treated as the empty string (DSK-154635)
	if (attr == ATTR_DATA && WhiteSpaceOnly(url_str))
		ret_url = ResolveUrl(UNI_L(""), logdoc, attr);
	else
#endif // _PLUGIN_SUPPORT_
		ret_url = ResolveUrl(url_str, logdoc, attr);

	OP_ASSERT(uni_strchr(url_str, ':') || logdoc); // Or we will put a relative unresolved url in the cache and all future accesses of it will be wrong
#ifdef WEB_TURBO_MODE
	// Plugin URLs should not use Turbo
	if ((packed2.object_is_embed || Type() == HE_EMBED) && ret_url.GetAttribute(URL::KUsesTurbo))
	{
		const OpStringC8 url_str = ret_url.GetAttribute(URL::KName_With_Fragment_Username_Password_NOT_FOR_UI, URL::KNoRedirect);
		ret_url = g_url_api->GetURL(url_str);
	}
#endif // WEB_TURBO_MODE

#if defined LOGDOC_LOAD_IMAGES_FROM_PARSER && defined _WML_SUPPORT_
	/* In WML images may have attribute set to some variable. Moreover variable value may be set later than during parsing - e.g. by the timer - so
	 * in such a case, if URL can not be resolved, do not cache it.
	 */
	if (logdoc && Type() == HE_IMG && ret_url.IsEmpty() && logdoc->GetHLDocProfile()->IsWml() && logdoc->GetHLDocProfile()->HasWmlContent() &&
		logdoc->GetHLDocProfile()->WMLGetContext()->NeedSubstitution(url_str, uni_strlen(url_str)))
		return NULL;
#endif // LOGDOC_LOAD_IMAGES_FROM_PARSER && _WML_SUPPORT_

	if (OpStatus::IsMemoryError(url_attr->SetResolvedUrl(ret_url)))
		return NULL;

	return url_attr->GetResolvedUrl();
}

URL
HTML_Element::DeriveBaseUrl(LogicalDocument *&logdoc) const
{
	OP_PROBE4(OP_PROBE_LOGDOC_DERIVEBASEURL);

	URL parent_base;

	if (Parent())
	{
		parent_base = Parent()->DeriveBaseUrl(logdoc);
	}
	else
	{
		if (!logdoc)
		{
			if (Type() == HE_DOC_ROOT)
			{
				logdoc = static_cast<LogicalDocument *>(static_cast<ComplexAttr *>(GetSpecialAttr(ATTR_LOGDOC,
					ITEM_TYPE_COMPLEX, NULL, SpecialNs::NS_LOGDOC)));
			}

			if (!logdoc)
				return URL();
		}

		if (URL *base_url = logdoc->GetBaseURL())
		{
			// See 2.5 URLs and 2.5.1 Terminology in HTML5 on base url and url resolving.
			if (IsAboutBlankURL(*base_url))
			{
				FramesDocument *frames_doc = logdoc->GetDocument();
				FramesDocument *candidate = NULL;
				URL *candidate_base_url = NULL;
				while (frames_doc)
				{
					LogicalDocument *candidate_logdoc = frames_doc->GetLogicalDocument();
					candidate_base_url = candidate_logdoc ? candidate_logdoc->GetBaseURL() : NULL;
					if (!IsAboutBlankURL(frames_doc->GetURL()) && candidate_base_url)
					{
						if (!IsAboutBlankURL(*candidate_base_url))
						{
							candidate = frames_doc;
							break;
						}
					}
					frames_doc = frames_doc->GetParentDoc();
				}
				if (candidate)
					parent_base = *candidate_base_url;
				else
					parent_base = *base_url;
			}
			else
				parent_base = *base_url;
		}
	}

	const uni_char *xml_base = GetStringAttr(XMLA_BASE, NS_IDX_XML);
	if (xml_base)
	{
		return g_url_api->GetURL(parent_base, xml_base);
	}

	return parent_base;
}

URL
#ifdef WEB_TURBO_MODE
HTML_Element::ResolveUrl(const uni_char *url_str, LogicalDocument *logdoc/*=NULL*/, short attr/*=ATTR_NULL*/, BOOL set_context_id/*=FALSE*/) const
#else // WEB_TURBO_MODE
HTML_Element::ResolveUrl(const uni_char *url_str, LogicalDocument *logdoc/*=NULL*/, short attr/*=ATTR_NULL*/) const
#endif // WEB_TURBO_MODE
{
	OP_PROBE4(OP_PROBE_LOGDOC_RESOLVEURL);

	if (!url_str)
		return URL();

	URL base_url = DeriveBaseUrl(logdoc);

	if (!logdoc)
	{
		// OP_ASSERT(!"The url will be resolved as an absolute url since we couldn't find a base url.");
		// Whatever you wanted to do, see if you can do it some other way that
		// doesn't trigger an url resolve.
		return g_url_api->GetURL(url_str);
	}

#ifdef _WML_SUPPORT_
	if (logdoc->IsWml())
	{
		WML_Context *wc = logdoc->GetHLDocProfile()->WMLGetContext();
		uni_char *sub_buf = (uni_char*)g_memory_manager->GetTempBuf();
		wc->SubstituteVars(url_str, uni_strlen(url_str), sub_buf,
			UNICODE_DOWNSIZE(g_memory_manager->GetTempBufLen()), FALSE);
		url_str = sub_buf;
	}
#endif // _WML_SUPPORT_

	URL doc_url;
	if (logdoc->GetFramesDocument())
		doc_url = logdoc->GetFramesDocument()->GetURL();
	else
		doc_url = base_url;

	if (attr == ATTR_DATA && Type() == HE_OBJECT)
	{
		const uni_char *codebase_str = GetStringAttr(ATTR_CODEBASE);
		if (codebase_str)
		{
			OpString tmp_str;
			if (tmp_str.Set(codebase_str) == OpStatus::ERR_NO_MEMORY)
				return URL();

			if (tmp_str.Find("adobe.com/pub/shockwave/cabs") == KNotFound &&
				tmp_str.Find("macromedia.com/pub/shockwave/cabs") == KNotFound &&
				tmp_str.Find("macromedia.com/flash2/cabs") == KNotFound &&
				tmp_str.Find("activex.microsoft.com/activex/controls/mplayer/") == KNotFound)
			{
				base_url = g_url_api->GetURL(base_url, codebase_str);
			}
		}
	}
	else if (attr == ATTR_USEMAP)
		base_url = doc_url;

	if (!*url_str && logdoc->GetFramesDocument()
		&& logdoc->GetFramesDocument()->IsGeneratedDocument())
	{
#ifdef WEB_TURBO_MODE
		if (set_context_id)
		{
			const OpStringC doc_url_str
				= doc_url.GetAttribute(URL::KUniName_With_Fragment_Username_Password_NOT_FOR_UI,
					URL::KNoRedirect);
			URL new_url = g_url_api->GetURL(doc_url_str, logdoc->GetCurrentContextId());
			return new_url;
		}
		else
#endif // WEB_TURBO_MODE
			return doc_url;
	}
	else
	{
		URL *urlp = CreateUrlFromString(url_str, uni_strlen(url_str),
			&base_url, logdoc->GetHLDocProfile(),
			Type() == HE_IMG && attr == ATTR_SRC, // check_for_internal_img
			attr == ATTR_HREF && Type() != Markup::HTE_LINK // accept_empty -> if true, empty str resolves to base url.
#ifdef WEB_TURBO_MODE
			, set_context_id
#endif // WEB_TURBO_MODE
			);
		if (!urlp)
			return URL();

		URL resolved_url = *urlp;
		OP_DELETE(urlp);
		return resolved_url;
	}
}

void HTML_Element::ClearResolvedUrls()
{
	if (Type() == HE_TEXT)
		return;

	int stop_at = GetAttrSize();
	for (int i = 0; i < stop_at; i++)
	{
		if (data.attrs[i].GetType() == ITEM_TYPE_URL_AND_STRING)
		{
			UrlAndStringAttr *url_attr = static_cast<UrlAndStringAttr*>(data.attrs[i].GetValue());
			OpStatus::Ignore(url_attr->SetResolvedUrl(static_cast<URL*>(NULL)));
		}
	}

	HTML_Element *child = FirstChild();
	while (child)
	{
		child->ClearResolvedUrls();
		child = child->Suc();
	}
}

BOOL HTML_Element::HasParentTable() const
{
	HTML_Element* parent = ParentActualStyle();

	while (parent && parent->Type() != Markup::HTE_TABLE)
		parent = parent->ParentActualStyle();

	return parent != NULL;
}

BOOL HTML_Element::HasParentElement(HTML_ElementType type, int ns_idx /* = NS_IDX_HTML*/, BOOL stop_at_special /* = TRUE*/) const
{
	// The list of elements below must be kept somewhat in sync with the code to find a matching element
	// to end in HTML_Element::Load or we will get hangs like the one in bug 339368.
	// Exclude the root element from this (see method documentation).
	for (const HTML_Element* parent = this; parent->Parent(); parent = parent->Parent())
	{
		HTML_ElementType parent_type = parent->Type();
		if (parent_type == type && g_ns_manager->CompareNsType(parent->GetNsIdx(), ns_idx))
			return TRUE;
		else if (stop_at_special &&
				(parent_type == HE_TABLE || parent_type == HE_BUTTON || parent_type == HE_SELECT || parent_type == HE_DATALIST || parent_type == HE_APPLET || parent_type == HE_OBJECT || parent_type == HE_MARQUEE) &&
				parent->GetNsType() == NS_HTML)
			return FALSE;
	}

	return FALSE;
}

void HTML_Element::MarkContainersDirty(FramesDocument* doc)
{
	if (layout_box)
		layout_box->MarkContainersDirty(doc);
}

void HTML_Element::MarkDirty(FramesDocument* doc, BOOL delete_minmax_widths, BOOL needs_update)
{
	if (!doc)
		return;

	LayoutWorkplace* wp = doc->GetLogicalDocument() ? doc->GetLogicalDocument()->GetLayoutWorkplace() : NULL;

#ifdef _DEBUG
		if (wp)
			// marking things dirty during traverse is forbidden. Marking thigs dirty during reflow is only allowed under certain circumstances.
			OP_ASSERT(!wp->IsTraversing() && !wp->IsReflowing());

		/* we can not mark dirty on a -o-content-size iframe document while the parent doc is traversing. */
		if (doc->IsInlineFrame() && doc->GetDocManager()->GetFrame() && doc->GetDocManager()->GetFrame()->GetNotifyParentOnContentChange())
		{
			FramesDocument* parent_doc = doc->GetDocManager() ? doc->GetDocManager()->GetParentDoc() : NULL;
			LayoutWorkplace* parent_wp = parent_doc && parent_doc->GetLogicalDocument() ? parent_doc->GetLogicalDocument()->GetLayoutWorkplace() : NULL;
			if (parent_wp)
				OP_ASSERT(!parent_wp->IsTraversing());
		}
#endif

#ifdef LAYOUT_YIELD_REFLOW

	/* The tree changed, we need to mark the yield cascade dirty and discard it. */

	if (wp)
		wp->MarkYieldCascade(this);

#endif // LAYOUT_YIELD_REFLOW

#ifdef SVG_SUPPORT
	if (GetNsType() == NS_SVG && Type() != Markup::SVGE_SVG)
	{
		HTML_Element* parent = Parent();
		while  (parent)
		{
			if (parent->IsMatchingType(Markup::SVGE_SVG, NS_SVG))
			{
				// SVG element inside a SVG block, so it won't affect the HTML layout engine
				return;
			}
			parent = parent->Parent();
		}
	}
#endif // SVG_SUPPORT

	if (needs_update)
		packed2.needs_update = TRUE;

	HTML_Element *elm = this;
	do
	{
#ifdef SVG_SUPPORT
		if (elm->IsMatchingType(Markup::SVGE_FOREIGNOBJECT, NS_SVG) && elm->HasAttr(Markup::XLINKA_HREF, NS_IDX_XLINK))
		{
			g_svg_manager->HandleInlineChanged(doc, elm);
			return;
		}
#endif // SVG_SUPPORT

		if (!(elm->packed2.dirty & ELM_DIRTY) ||
			(delete_minmax_widths && !(elm->packed2.dirty & ELM_MINMAX_DELETED)))
		{
			elm->packed2.dirty |= ELM_DIRTY;

			if (delete_minmax_widths)
			{
				if (elm->layout_box)
					elm->layout_box->ClearMinMaxWidth();

				elm->packed2.dirty |= ELM_MINMAX_DELETED;
			}

			HTML_Element* parent = elm->Parent();
			if (parent)
			{
				elm = parent;
				continue;
			}

			HTML_Element* doc_root = doc->GetDocRoot();

			if (elm->Type() == HE_DOC_ROOT && doc_root == elm)
			{
				if (delete_minmax_widths)
				{
					wp->HandleContentSizedIFrame(TRUE);
				}

#ifdef _PRINT_SUPPORT_
				if (doc->IsCurrentDoc() || doc->IsPrintDocument())
#else // _PRINT_SUPPORT_
				if (doc->IsCurrentDoc())
#endif // _PRINT_SUPPORT_
					doc->PostReflowMsg();
			}
		}
		break;
	} while (1);
}

#ifdef SVG_SUPPORT
/** A SVG fragment root is defined to be a <svg> element in the svg
	namespace with a parent outside the svg namespace.  The fragment
	must be placed in a document root or an element in another
	namespace. */
static inline BOOL IsSVGFragmentRoot(HTML_Element *elm)
{
	if (!elm->IsMatchingType(Markup::SVGE_SVG, NS_SVG))
		return FALSE;

	if (!elm->Parent() || elm->Parent()->GetNsType() == NS_SVG)
		return FALSE;

	return TRUE;
}
#endif

void HTML_Element::MarkExtraDirty(FramesDocument* doc, int successors)
{
	if (doc)
	{
#ifdef _DEBUG
		if (doc->GetLogicalDocument())
		{
			LayoutWorkplace* wp = doc->GetLogicalDocument()->GetLayoutWorkplace();
			// marking things dirty during traverse is forbidden. Marking thigs dirty during reflow is only allowed under certain circumstances.
			OP_ASSERT(!wp->IsTraversing() && !wp->IsReflowing());
		}

		/* we can not mark dirty on a -o-content-size iframe document while the parent doc is traversing. */
		if (doc->IsInlineFrame() && doc->GetDocManager()->GetFrame() && doc->GetDocManager()->GetFrame()->GetNotifyParentOnContentChange())
		{
			FramesDocument* parent_doc = doc->GetDocManager() ? doc->GetDocManager()->GetParentDoc() : NULL;
			LayoutWorkplace* parent_wp = parent_doc && parent_doc->GetLogicalDocument() ? parent_doc->GetLogicalDocument()->GetLayoutWorkplace() : NULL;
			if (parent_wp)
				OP_ASSERT(!parent_wp->IsTraversing());
		}
#endif

#ifdef LAYOUT_YIELD_REFLOW
		LayoutWorkplace* wp = doc->GetLogicalDocument() ? doc->GetLogicalDocument()->GetLayoutWorkplace() : NULL;
		if (wp)
		{
			if (wp->GetYieldForceLayoutChanged() == LayoutWorkplace::FORCE_FROM_ELEMENT)
				/* If we do something drastic to the document, we need to force all elements */
				wp->SetYieldForceLayoutChanged(LayoutWorkplace::FORCE_ALL);

			/* The tree changed, we need to mark the yield cascade dirty and discard it */
			wp->MarkYieldCascade(this);
		}
#endif // LAYOUT_YIELD_REFLOW

		int type = Type();
		int ns_type = GetNsType();
		if (((type == HE_FRAMESET || type == HE_FRAME) && ns_type == NS_HTML) && doc->IsFrameDoc())
		{
			doc->PostReflowFramesetMsg();
			return;
		}

		HTML_Element* const parent = Parent();

#ifdef SVG_SUPPORT
		if (GetNsType() == NS_SVG)
		{
			BOOL is_svg_fragment_root = IsSVGFragmentRoot(this);
			if (!is_svg_fragment_root && type != Markup::SVGE_FOREIGNOBJECT)
			{
				HTML_Element* ancestor = Parent();
				while (ancestor)
				{
					if (IsSVGFragmentRoot(ancestor))
					{
						// SVG element inside a SVG block, so it won't affect the HTML
						// layout engine. If we mark something dirty and the HTML
						// layout engine sees it, it will do an Invalidate on the
						// whole Line with SVG which will force a full repaint,
						// which will make it impossible to have DOM animations in SVG.
						return;
					}
					ancestor = ancestor->Parent();
				}
			}
		}
#endif // SVG_SUPPORT

		packed2.dirty |= ELM_BOTH_DIRTY;

		if (parent)
		{
			if (GetInserted() == HE_INSERTED_BY_LAYOUT || parent->GetInserted() == HE_INSERTED_BY_LAYOUT)
				parent->MarkExtraDirty(doc);
			else
				parent->MarkDirty(doc);
		}
		else if (type == HE_DOC_ROOT)
		{
			BOOL post_message;

#ifdef _PRINT_SUPPORT_
			if (doc->IsPrintDocument())
				post_message = doc->GetLogicalDocument()->GetPrintRoot() == this;
			else
#endif // _PRINT_SUPPORT_
				post_message = doc->GetLogicalDocument()->GetRoot() == this;

			if (post_message)
			{
#ifdef _PRINT_SUPPORT_
				if (doc->IsCurrentDoc() || doc->IsPrintDocument())
#else // _PRINT_SUPPORT_
				if (doc->IsCurrentDoc())
#endif // _PRINT_SUPPORT_
					doc->PostReflowMsg();
			}
		}

		HTML_Element* element = this;
		while (successors)
		{
			element = element->SucActual();

			if (element)
				switch (element->Type())
				{
				case HE_TEXT:
				case HE_TEXTGROUP:
				case HE_COMMENT:
				case HE_PROCINST:
				case HE_DOCTYPE:
				case HE_ENTITY:
				case HE_ENTITYREF:
					break;
				default:
					element->MarkExtraDirty(doc);
					successors--;
					break;
				}
			else
				break;
		}
	}
}

void HTML_Element::MarkPropsDirty(FramesDocument* doc, int successor_subtrees, BOOL mark_this_subtree)
{
	if (!doc)
		return;

#ifdef _DEBUG
	LayoutWorkplace* wp = doc->GetLogicalDocument() ? doc->GetLogicalDocument()->GetLayoutWorkplace() : NULL;

	if (wp)
		OP_ASSERT(!wp->IsTraversing() && !wp->IsReflowing());
#endif // _DEBUG

	packed2.props_clean = 0;

	// Mark child_props_dirty on ancestors that don't already have it set.

	HTML_Element* ancestor;

	for (ancestor = Parent(); ancestor && !ancestor->packed2.child_props_dirty; ancestor = ancestor->Parent())
		ancestor->packed2.child_props_dirty = 1;

	if (!ancestor && doc->IsCurrentDoc())
		// Root element didn't have child_props_dirty - until now.

		doc->PostReflowMsg();

	int subtrees = successor_subtrees;

	if (subtrees < INT_MAX && mark_this_subtree)
		subtrees ++;

	if (subtrees)
	{
		/* Possible performance improvement to consider here: Add another bit of information to
		   HTML_Element which expresses "reload props on entire subtree", instead of going through
		   the entire subtree now. */

		HTML_Element* elm = mark_this_subtree ? this : SucActualStyle();

		while (subtrees > 0 && elm)
		{
			HTML_Element* next_elm = elm->SucActualStyle();

			if (Markup::IsRealElement(elm->Type()))
			{
				HTML_Element* stop = next_elm;
				if (!stop)
					stop = elm->NextSiblingActualStyle();

				for (HTML_Element* child = elm->FirstChildActualStyle(); child && child != stop; child = child->NextActualStyle())
					if (Markup::IsRealElement(child->Type()))
					{
						child->packed2.props_clean = 0;
						child->packed2.child_props_dirty = 1;

						// The *Actual*() methods may skip children. Be sure to mark them as well.

						for (HTML_Element* p = child->Parent(); p && !p->packed2.child_props_dirty; p = p->Parent())
							p->packed2.child_props_dirty = 1;
					}

				elm->packed2.props_clean = 0;
				elm->packed2.child_props_dirty = 1;

				// The *Actual*() methods may skip children. Be sure to mark them as well.

				for (HTML_Element* p = elm->Parent(); p && !p->packed2.child_props_dirty; p = p->Parent())
					p->packed2.child_props_dirty = 1;

				-- subtrees;
			}

			elm = next_elm;
		}
	}
}

int HTML_Element::CountParams() const
{
	if (Type() == HE_PARAM)
		return 1;
	else
	{
		int count = 0;

		for (HTML_Element* he = FirstChildActual(); he; he = he->SucActual())
		{
			if (he->Type() != HE_APPLET && he->Type() != HE_OBJECT)
				count += he->CountParams();
		}

		return count;
	}
}

const uni_char* HTML_Element::GetParamURL() const
{
	if (Type() == HE_PARAM)
	{
		const uni_char* param_name = GetPARAM_Name();
		if (param_name)
		{
			if (uni_stri_eq(param_name, "FILENAME"))
				return GetPARAM_Value();
			else if (uni_stri_eq(param_name, "MOVIE"))
				return GetPARAM_Value();
			else if (uni_stri_eq(param_name, "SRC"))
				return GetPARAM_Value();
			else if (uni_stri_eq(param_name, "URL"))
				return GetPARAM_Value();
		}

		return NULL;
	}
	else
	{
		const uni_char* url = NULL;
		HTML_Element* he = FirstChildActual();

		while (he && !url)
		{
			if (he->Type() != HE_OBJECT && he->Type() != HE_APPLET)
				url = he->GetParamURL();
			he = he->SucActual();
		}

		return url;
	}
}

const uni_char* HTML_Element::GetParamType(const uni_char* &codetype) const
{
	if (Type() == HE_PARAM)
	{
		const uni_char* param_name = GetPARAM_Name();
		if (param_name)
		{
			if (uni_stri_eq(param_name, "TYPE"))
				return GetPARAM_Value();
			else if (!codetype && uni_stri_eq(param_name, "CODETYPE"))
				codetype = GetPARAM_Value();
		}

		return NULL;
	}
	else
	{
		const uni_char* type = NULL;
		HTML_Element* he = FirstChildActual();

		while (he && !type)
		{
			if (he->Type() != HE_OBJECT && he->Type() != HE_APPLET)
				type = he->GetParamType(codetype);
			he = he->SucActual();
		}

		return type;
	}
}

#ifdef _PLUGIN_SUPPORT_

OpNS4Plugin* HTML_Element::GetNS4Plugin()
{
	if (layout_box && layout_box->IsContentEmbed())
		return static_cast<EmbedContent*>(static_cast<Content_Box*>(layout_box)->GetContent())->GetOpNS4Plugin();
	else
		return NULL;
}

OP_STATUS HTML_Element::GetEmbedAttrs(int& argc, const uni_char** &argn, const uni_char** &argv) const
{
	OP_ASSERT(Type() == HE_EMBED || Type() == HE_APPLET || Type() == HE_OBJECT);

	argn = NULL;
	argv = NULL;

	int attr_count = 0;
	int param_count = 0;

	PrivateAttrs* pa = (PrivateAttrs*)GetSpecialAttr(ATTR_PRIVATE, ITEM_TYPE_PRIVATE_ATTRS, (void*)0, SpecialNs::NS_LOGDOC);
	if (pa)
		attr_count = pa->GetLength();

	if (Type() == HE_APPLET || Type() == HE_OBJECT)
		param_count = CountParams() + 1; // Params + "PARAM"

	argc = attr_count + param_count;

	if (argc)
	{
		argn = OP_NEWA(const uni_char*, argc); // FIXME:REPORT_MEMMAN-KILSMO
		argv = OP_NEWA(const uni_char*, argc); // FIXME:REPORT_MEMMAN-KILSMO
		if (!argn || !argv)
		{
			OP_DELETEA(argn);
			OP_DELETEA(argv);
			argn = NULL;
			argv = NULL;
			return OpStatus::ERR_NO_MEMORY;
		}

		if (pa)
		{
			uni_char** pa_argn = pa->GetNames();
			uni_char** pa_argv = pa->GetValues();

			for (int i = 0; i < attr_count; i++)
			{
				argn[i] = pa_argn[i];
				argv[i] = pa_argv[i];
				if (!argv[i])
					argv[i] = UNI_L("");
			}
		}

		if (param_count)
		{
			int next_index = attr_count;

			// Add a placeholder that shows the plugin where attributes end and params start
			argn[next_index] = UNI_L("PARAM");
			argv[next_index++] = NULL;

			GetParams(argn, argv, next_index);

			OP_ASSERT(next_index <= argc);

			if (next_index < argc)
				argc = next_index;
		}
	}

	return OpStatus::OK;
}
#endif // _PLUGIN_SUPPORT_

void HTML_Element::GetParams(const uni_char** names, const uni_char** values, int& next_index) const
{
	HTML_Element* stop = NextSiblingActual();
	const HTML_Element* it = NextActual();
	while (it != stop)
	{
		if (it->IsMatchingType(HE_PARAM, NS_HTML))
		{
			names[next_index] = it->GetPARAM_Name();

			if (names[next_index])
			{
				values[next_index] = it->GetPARAM_Value();
				if (!values[next_index])
					values[next_index] = UNI_L("");

				next_index++;
			}
		}

		if (it->GetNsType() == NS_HTML && (it->Type() == HE_APPLET || it->Type() == HE_OBJECT || it->Type() == HE_PARAM))
			// Skip this subtree
			it = it->NextSiblingActual();
		else
			it = it->NextActual();
	}
}

/**
 * Tries to determine if the parser is currently working
 * inside the element (or has just inserted it). Will error on
 * the side of TRUE for backwards compatibility.
 */
static BOOL IsParsingInsideElement(HTML_Element* elm, LogicalDocument* logdoc)
{
	OP_ASSERT(elm && logdoc);

	BOOL parsing_inside_element = logdoc->IsParsingUnderElm(elm);

	return parsing_inside_element;
}

OP_BOOLEAN HTML_Element::GetResolvedEmbedType(URL* inline_url, HTML_ElementType &resolved_type, LogicalDocument* logdoc)
{
	OP_ASSERT(Type() == HE_EMBED);

	if (!inline_url || inline_url->IsEmpty())
	{
		resolved_type = HE_EMBED;
		return OpBoolean::IS_TRUE;
	}

	const uni_char* type_str = GetStringAttr(ATTR_TYPE);

#ifndef _APPLET_2_EMBED_
	if (type_str &&
		(uni_strnicmp(type_str, UNI_L("application/x-java-applet"), 25) == 0 ||
		 uni_strnicmp(type_str, UNI_L("application/java"), 16) == 0))
	{
		resolved_type = HE_APPLET;
		return OpBoolean::IS_TRUE;
	}
#endif // !_APPLET_2_EMBED_

	OpString8 resource_type;
	RETURN_IF_ERROR(resource_type.SetUTF8FromUTF16(type_str));

	URLStatus url_stat = inline_url->Status(TRUE);

	if (url_stat == URL_LOADING_FAILURE)
	{
		resolved_type = HE_EMBED;
		return OpBoolean::IS_TRUE;
	}
	else if (url_stat == URL_UNLOADED ||
		(url_stat == URL_LOADING && inline_url->ContentType() == URL_UNDETERMINED_CONTENT))
	{
		return OpBoolean::IS_FALSE;
	}
	else if (inline_url->Type() == URL_HTTP || inline_url->Type() == URL_HTTPS)
	{
		uint32 http_response = inline_url->GetAttribute(URL::KHTTP_Response_Code, TRUE);

		if (url_stat == URL_LOADING && http_response == HTTP_NO_RESPONSE)
			return OpBoolean::IS_FALSE;
		else if (http_response >= 400)
		{
			resolved_type = HE_EMBED;
			return OpBoolean::IS_TRUE;
		}

		if (g_pcnet->GetIntegerPref(PrefsCollectionNetwork::TrustServerTypes))
			RETURN_IF_ERROR(inline_url->GetAttribute(URL::KMIME_Type, resource_type, TRUE));
	}

	return GetResolvedHtmlElementType(inline_url, logdoc, resource_type.CStr(),  HE_EMBED, resolved_type);
}

OP_BOOLEAN HTML_Element::GetResolvedHtmlElementType(URL* inline_url, LogicalDocument* logdoc, const char* resource_type,  HTML_ElementType default_type, HTML_ElementType& resolved_type, BOOL is_currently_parsing_this /* = FALSE */)
{
	OP_ASSERT(inline_url);

	ViewActionReply reply;
	OpString mime_type;
	RETURN_IF_ERROR(mime_type.Set(resource_type));
	RETURN_IF_MEMORY_ERROR(Viewers::GetViewAction(*inline_url, mime_type, reply, TRUE));

	resolved_type = default_type;

	if (reply.action == VIEWER_PLUGIN)
	{
		if (is_currently_parsing_this)
			return OpBoolean::IS_FALSE;

		if (default_type == HE_OBJECT)
		{
#ifdef SVG_SUPPORT
			// Prevents execution of applets and plugins for foreignObjects in svg in <img> elements, DSK-240651.
			if (logdoc && !g_svg_manager->IsEcmaScriptEnabled(logdoc->GetFramesDocument()))
			{
				resolved_type = HE_OBJECT; // return HE_OBJECT because that will give a nodisplay box.
			}
			else
#endif // SVG_SUPPORT
# ifdef USE_FLASH_PREF
			if (!g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::FlashEnabled)
				&& reply.mime_type.Length() && HTML_Element::IsFlashType(reply.mime_type.CStr()))
			{
				resolved_type = HE_OBJECT; // return HE_OBJECT because that will give a nodisplay box.
			}
			else
# endif // USE_FLASH_PREF
				resolved_type = CheckLocalCodebase(logdoc, HE_EMBED, reply.mime_type.CStr());
		}

		return OpBoolean::IS_TRUE;
	}
	else
	{
		URLContentType inline_url_cnt_type = inline_url->ContentType();

		if (inline_url_cnt_type == URL_UNDETERMINED_CONTENT)
			if (Viewer* v = g_viewers->FindViewerByMimeType(resource_type))
				inline_url_cnt_type = v->GetContentType();

		switch (inline_url_cnt_type)
		{
# ifdef SVG_SUPPORT
			case URL_SVG_CONTENT:
				resolved_type = (!logdoc || logdoc->GetFramesDocument()->GetShowImages()) ? HE_IFRAME : default_type;
				return OpBoolean::IS_TRUE;
# endif
			case URL_HTML_CONTENT:
			case URL_TEXT_CONTENT:
			case URL_CSS_CONTENT:
			case URL_X_JAVASCRIPT:
			case URL_XML_CONTENT:
# ifdef _WML_SUPPORT_
			case URL_WML_CONTENT:
# endif
				resolved_type = HE_IFRAME;
				return OpBoolean::IS_TRUE;

			/*
			case URL_AVI_CONTENT:
			case URL_MPG_CONTENT:
				break;
			*/

			case URL_PNG_CONTENT:
			case URL_GIF_CONTENT:
			case URL_JPG_CONTENT:
			case URL_XBM_CONTENT:
			case URL_BMP_CONTENT:
# ifdef _WML_SUPPORT_
			case URL_WBMP_CONTENT:
# endif
			case URL_WEBP_CONTENT:
				resolved_type = HE_IMG;
				return OpBoolean::IS_TRUE;

			case URL_MIDI_CONTENT:
			case URL_WAV_CONTENT:
				resolved_type = HE_EMBED;
				return OpBoolean::IS_TRUE;

# ifdef HAS_ATVEF_SUPPORT // TV: works with object
			case URL_TV_CONTENT:
			{
				if (default_type == HE_OBJECT)
				{
					resolved_type = HE_IMG;
					return OpBoolean::IS_TRUE;
				}
			}
# endif // HAS_ATVEF_SUPPORT
		}
	}

	return OpBoolean::IS_TRUE;
}

#if defined(_APPLET_2_EMBED_) && defined(_PLUGIN_SUPPORT_)
static BOOL IsJavaEnabled()
{
	Viewer* viewer = g_viewers->FindViewerByMimeType(UNI_L("application/x-java-applet"));
	return viewer && (viewer->GetAction() == VIEWER_PLUGIN);
}
#endif // defined(_APPLET_2_EMBED_) && defined(_PLUGIN_SUPPORT_)

OP_BOOLEAN HTML_Element::GetResolvedObjectType(URL* inline_url, HTML_ElementType &resolved_type, LogicalDocument* logdoc)
{
#if !defined _PLUGIN_SUPPORT_ && defined SVG_SUPPORT
	OP_ASSERT(Type() == HE_OBJECT || Type() == HE_EMBED || Type() == HE_IMG); // This is not right, but it saves us some asserts.
#else
	OP_ASSERT(Type() == HE_OBJECT);
#endif

	URLType url_type = URL_UNKNOWN;
#ifndef ONLY_USE_TYPE_FOR_OBJECTS
	if (inline_url)
		url_type = inline_url->Type();
#endif // ONLY_USE_TYPE_FOR_OBJECTS

#ifdef DELAYED_SCRIPT_EXECUTION
	HTML_Element *child = FirstChild();
	HTML_Element *stop = NextSibling();
	while (child && child != stop)
	{
		if (child->Type() == HE_PARAM && child->GetInserted() == HE_INSERTED_BY_PARSE_AHEAD || child->Type() == HE_SCRIPT && logdoc->GetHLDocProfile()->ESIsFirstDelayedScript(child))
		{
			resolved_type = HE_OBJECT;
			return OpBoolean::IS_FALSE;
		}
		child = child->Next();
	}
#endif // DELAYED_SCRIPT_EXECUTION

#ifdef _PLUGIN_SUPPORT_
#ifdef SVG_SUPPORT
	// Prevents execution of applets and plugins for foreignObjects in svg in <img> elements, DSK-240651.
	BOOL suppress_plugins = logdoc && !g_svg_manager->IsEcmaScriptEnabled(logdoc->GetFramesDocument());
#else
	BOOL suppress_plugins = FALSE;
#endif // SVG_SUPPORT
#endif // _PLUGIN_SUPPORT_

	BOOL is_currently_parsing_this = logdoc && !GetEndTagFound() && !logdoc->IsParsed() && IsParsingInsideElement(this, logdoc);
	const uni_char* class_id = GetStringAttr(ATTR_CLASSID);

	if (class_id && *class_id)
	{
#if defined(_APPLET_2_EMBED_) && defined(_PLUGIN_SUPPORT_)
		if (!suppress_plugins && uni_strni_eq(class_id, "JAVA:", 5) && IsJavaEnabled())
		{
			resolved_type = CheckLocalCodebase(logdoc, HE_APPLET);
			return OpBoolean::IS_TRUE;
		}
		else
#endif // defined(_APPLET_2_EMBED_) && defined(_PLUGIN_SUPPORT_)
		if (uni_stri_eq(class_id, "HTTP://WWW.OPERA.COM/ERA"))
		{
			if (is_currently_parsing_this)
			{
				// wait for object element to complete
				return OpBoolean::IS_FALSE;
			}
			else
			{
				resolved_type = HE_IFRAME;
				return OpBoolean::IS_TRUE;
			}
		}
		// Unknown CLASSID -> Should use the fallback according to the
		// HTML5 plugin loading algorithm, see 4.8.6 The object element
		resolved_type = HE_OBJECT;
		return OpBoolean::IS_TRUE;
	}

#ifdef HAS_ATVEF_SUPPORT
	if (url_type == URL_TV)
	{
#ifdef JS_PLUGIN_ATVEF_VISUAL
		// If this is an OBJECT pointing to a visual jsplugin, we need to
		// register a listener. inline_url will point to an internal "tv:"
		// URL made up specially for this object, which we need to ask
		// it to listen to.
		JS_Plugin_Object *obj_p = NULL;
		OP_BOOLEAN is_jsplugin = IsJSPluginObject(logdoc->GetHLDocProfile(), &obj_p);
		if (is_jsplugin == OpBoolean::IS_TRUE && obj_p->IsAtvefVisualPlugin() &&
			!GetSpecialBoolAttr(Markup::JSPA_TV_LISTENER_REGISTERED,
			             SpecialNs::NS_JSPLUGINS))
		{
			// Get the JS_Plugin_HTMLObjectElement_Object that correspond to
			// the OBJECT tag. This is not the same as the obj_p we just
			// retrieved...
			ES_Object *esobj =
				DOM_Utils::GetJSPluginObject(GetESElement());
			EcmaScript_Object *ecmascriptobject = NULL;
			JS_Plugin_Object *jsobject = NULL;
			if (esobj && (ecmascriptobject = ES_Runtime::GetHostObject(esobj)) != NULL)
			{
				OP_ASSERT(ecmascriptobject->IsA(ES_HOSTOBJECT_JSPLUGIN));
				jsobject =
					static_cast<JS_Plugin_Object *>(ecmascriptobject);
			}

			// Check that we actually did request visualization when
			// instantiating this object.
			if (jsobject && jsobject->IsAtvefVisualPlugin())
			{
				// Listen to the tv: URL we have set up.
				if (OpStatus::IsSuccess(jsobject->RegisterAsTvListener(inline_url->GetAttribute(URL::KUniName, FALSE))))
				{
					SetSpecialBoolAttr(Markup::JSPA_TV_LISTENER_REGISTERED,
									   TRUE, SpecialNs::NS_JSPLUGINS);
				}
			}
		}
		else if (OpStatus::IsMemoryError(is_jsplugin))
		{
			return is_jsplugin;
		}
#endif

		resolved_type = HE_IMG;
		return OpBoolean::IS_TRUE;
	}
#endif

	const uni_char* type_str = GetStringAttr(ATTR_TYPE);

	if (!type_str && !(type_str = GetStringAttr(ATTR_CODETYPE)))
	{
		// check if there is a <PARAM> with name == type or codetype
		const uni_char* codetype_str = NULL;

		type_str = GetParamType(codetype_str);

		if (!type_str)
			type_str = codetype_str;

		if (!type_str && is_currently_parsing_this)
		{
			// wait for object element to complete
			resolved_type = HE_OBJECT;
			return OpBoolean::IS_FALSE;
		}
	}

	if (is_currently_parsing_this)
	{
		// wait for object element to complete
		resolved_type = HE_OBJECT;
		return OpBoolean::IS_FALSE;
	}

#if defined(_APPLET_2_EMBED_) && defined(_PLUGIN_SUPPORT_)
	if (!suppress_plugins &&
		type_str &&
			(uni_strni_eq(type_str, "APPLICATION/X-JAVA-APPLET", 25) ||
			 uni_strni_eq(type_str, "APPLICATION/JAVA", 16)) &&
		IsJavaEnabled())
	{
		resolved_type = CheckLocalCodebase(logdoc, HE_APPLET);
		return OpBoolean::IS_TRUE;
	}
#endif // defined(_APPLET_2_EMBED_) && defined(_PLUGIN_SUPPORT_)

#ifdef ONLY_USE_TYPE_FOR_OBJECTS
	// Despite not loading the URL we'll still use any cached data in it to make
	// an educated guess about what resource we'll get if we do load it. This
	// is really wrong, but since ONLY_USE_TYPE_FOR_OBJECTS is an optional feature
	// they know that already.
	if (inline_url && !inline_url->IsEmpty())
	{
		URLContentType inline_url_cnt_type = inline_url->ContentType();

		if (inline_url_cnt_type == URL_UNDETERMINED_CONTENT)
		{
			Viewer* v;
			OpString resource_type;
			const char* url_mimetype = inline_url->GetAttribute(URL::KMIME_Type, TRUE).CStr();

			if (url_mimetype && g_pcnet->GetIntegerPref(PrefsCollectionNetwork::TrustServerTypes))
				RETURN_IF_ERROR(resource_type.Set(url_mimetype));
			else
				RETURN_IF_ERROR(resource_type.Set(type_str));

			if (g_viewers->FindViewerByMimeType(resource_type, v) == OpStatus::ERR_NO_MEMORY)
				return OpStatus::ERR_NO_MEMORY;

			if (v)
				inline_url_cnt_type = v->GetContentType();
		}
		switch (inline_url_cnt_type)
		{
		case URL_HTML_CONTENT:
		case URL_TEXT_CONTENT:
		case URL_XML_CONTENT:
# ifdef _WML_SUPPORT_
		case URL_WML_CONTENT:
# endif
			resolved_type = HE_IFRAME;
			return OpBoolean::IS_TRUE;
		}
	}

	URL sniffer_url; // Empty URL so that the viewer system can focus on the mime type.
	BOOL check_with_viewers = TRUE;

#else // ONLY_USE_TYPE_FOR_OBJECTS
	URL sniffer_url = inline_url ? *inline_url : URL();
	BOOL check_with_viewers = FALSE;
	if (sniffer_url.IsEmpty())
		check_with_viewers = TRUE;
#endif // ONLY_USE_TYPE_FOR_OBJECTS

	if (check_with_viewers)
	{
#ifdef _PLUGIN_SUPPORT_
		HTML_Element* embed_present = GetFirstElm(HE_EMBED);
		if (!embed_present && type_str)
		{
			ViewActionReply reply;
			RETURN_IF_MEMORY_ERROR(Viewers::GetViewAction(sniffer_url, type_str, reply, TRUE));

			resolved_type = reply.action != VIEWER_PLUGIN || suppress_plugins ? HE_OBJECT : CheckLocalCodebase(logdoc, HE_EMBED, reply.mime_type.CStr());
		}
		else
#endif // _PLUGIN_SUPPORT_
			resolved_type = HE_OBJECT;

		return OpBoolean::IS_TRUE;
	}

	OP_ASSERT(!sniffer_url.IsEmpty());
	URLStatus url_stat = sniffer_url.Status(TRUE);

	OpString8 resource_type;
	RETURN_IF_ERROR(resource_type.SetUTF8FromUTF16(type_str));

	if (url_stat == URL_LOADING_FAILURE)
	{
		resolved_type = HE_OBJECT;
		return OpBoolean::IS_TRUE;
	}
	else if (url_stat == URL_UNLOADED ||
	         (url_stat == URL_LOADING && inline_url->ContentType() == URL_UNDETERMINED_CONTENT))
	{
		return OpBoolean::IS_FALSE;
	}
	else if (url_type == URL_HTTP || url_type == URL_HTTPS)
	{
		uint32 http_response = inline_url->GetAttribute(URL::KHTTP_Response_Code, TRUE);

		if (http_response == HTTP_NO_RESPONSE && url_stat == URL_LOADING)
			return OpBoolean::IS_FALSE;
		else if (http_response >= 400)
		{
			resolved_type = HE_OBJECT;
			return OpBoolean::IS_TRUE;
		}

		if (g_pcnet->GetIntegerPref(PrefsCollectionNetwork::TrustServerTypes))
			RETURN_IF_ERROR(sniffer_url.GetAttribute(URL::KMIME_Type, resource_type, TRUE));
	}

	return GetResolvedHtmlElementType(&sniffer_url, logdoc, resource_type.CStr(),  HE_OBJECT, resolved_type, is_currently_parsing_this);
}

OP_BOOLEAN HTML_Element::GetResolvedImageType(URL* inline_url, HTML_ElementType &resolved_type, BOOL doc_loading, FramesDocument* doc)
{
	OP_ASSERT(doc);
	OP_ASSERT(Type() == HE_IMG || Type() == HE_INPUT);

	resolved_type = Type();

#ifdef SVG_SUPPORT
	if (!inline_url)
	{
		return OpBoolean::IS_FALSE;
	}
	else if (inline_url->Status(TRUE) == URL_LOADING_FAILURE)
	{
		return OpBoolean::IS_TRUE;
	}
	else if (inline_url->ContentType() == URL_SVG_CONTENT)
	{
		resolved_type = HE_IFRAME;
	}
#endif // SVG_SUPPORT

	return OpBoolean::IS_TRUE;
}

// The following methods handles traversing of the three skipping the elements inserted by the LayoutEngine

HTML_Element* HTML_Element::ParentActual() const
{
	const HTML_Element *candidate = Parent();
	while (candidate && !candidate->IsIncludedActual())
		candidate = candidate->Parent();

	return const_cast<HTML_Element*>(candidate); // casting to avoid constness trouble
}

HTML_Element* HTML_Element::SucActual() const
{
	OP_PROBE3(OP_PROBE_HTML_ELEMENT_SUCACTUAL);

	HTML_Element *stop = ParentActual() ? (HTML_Element *) ParentActual()->NextSibling() : NULL;

	for (HTML_Element *e = (HTML_Element *) NextSibling(); e && e != stop; e = (HTML_Element *) e->Next())
		if (e->IsIncludedActual())
		{
			OP_ASSERT(e->ParentActual() == ParentActual());
			return e;
		}

	return NULL;
}

HTML_Element* HTML_Element::PredActual() const
{
	HTML_Element *stop = ParentActual() ? (HTML_Element *) ParentActual()->PrevSibling() : NULL;

	for (HTML_Element *e = (HTML_Element *) PrevSibling(); e && e != stop; e = (HTML_Element *) e->PrevSibling())
		if (e->IsIncludedActual())
		{
			OP_ASSERT(e->ParentActual() == ParentActual());
			OP_ASSERT(e->SucActual() == (IsIncludedActual() ? this : SucActual()));
			return e;
		}
		else if (HTML_Element *lc = e->LastChildActual())
		{
			OP_ASSERT(lc->ParentActual() == ParentActual());
			OP_ASSERT(lc->SucActual() == (IsIncludedActual() ? this : SucActual()));
			return lc;
		}

	return NULL;
}

HTML_Element* HTML_Element::FirstChildActual() const
{
	for (HTML_Element *e = FirstChild(); e; e = e->FirstChild())
		if (e->IsIncludedActual())
		{
			OP_ASSERT(e->ParentActual() == (IsIncludedActual() ? this : ParentActual()));
			return e;
		}
		else if (!e->FirstChild())
		{
			HTML_Element *stop = (HTML_Element *) NextSibling();
			for (e = (HTML_Element *) e->Next(); e && e != stop; e = (HTML_Element *) e->Next())
				if (e->IsIncludedActual())
				{
					OP_ASSERT(e->ParentActual() == (IsIncludedActual() ? this : ParentActual()));
					return e;
				}
			break;
		}

	return NULL;
}

HTML_Element* HTML_Element::LastChildActual() const
{
	for (HTML_Element *e = LastChild(); e; e = e->LastChild())
		if (e->IsIncludedActual())
		{
			OP_ASSERT(e->ParentActual() == (IsIncludedActual() ? this : ParentActual()));
			return e;
		}
		else if (!e->LastChild())
		{
			for (e = (HTML_Element *) e->Prev(); e && e != this; e = (HTML_Element *) e->Prev())
				if (e->IsIncludedActual())
				{
					const HTML_Element *parent = IsIncludedActual() ? this : ParentActual();

					while (e->ParentActual() != parent)
					{
						OP_ASSERT(IsAncestorOf(e));
						e = e->ParentActual();
					}

					return e;
				}
			break;
		}

	return NULL;
}

HTML_Element *HTML_Element::PrevSiblingActual() const
{
	for (const HTML_Element *leaf = this; leaf; leaf = leaf->ParentActual())
		if (leaf->PredActual())
			return leaf->PredActual();

	return NULL;
}

HTML_Element *HTML_Element::FirstLeafActual() const
{
	HTML_Element *leaf = FirstChildActual();
	if (!leaf)
		return NULL;

	while (HTML_Element *next_child = leaf->FirstChildActual())
		leaf = next_child;
	return leaf;
}

HTML_Element *HTML_Element::LastLeafActual() const
{
	HTML_Element *leaf = LastChildActual();
	if (!leaf)
		return NULL;

	while (leaf->LastChildActual())
		leaf = leaf->LastChildActual();

	return leaf;
}

HTML_Element* HTML_Element::NextActual() const
{
	for (HTML_Element *e = (HTML_Element *) Next(); e; e = (HTML_Element *) e->Next())
		if (e->IsIncludedActual())
			return e;

	return NULL;
}

HTML_Element* HTML_Element::NextSiblingActual() const
{
	for( const HTML_Element *leaf = this; leaf; leaf = leaf->ParentActual() )
	{
		HTML_Element *candidate = leaf->SucActual();
		if( candidate )
			return candidate;
	}

	return NULL;
}

HTML_Element* HTML_Element::PrevActual() const
{
	for (HTML_Element *e = (HTML_Element *) Prev(); e; e = (HTML_Element *) e->Prev())
		if (e->IsIncludedActual())
			return e;

	return NULL;
}

#ifdef DELAYED_SCRIPT_EXECUTION

HTML_Element* HTML_Element::SucActualStyle() const
{
	HTML_Element *stop = ParentActualStyle() ? (HTML_Element *) ParentActualStyle()->NextSibling() : NULL;

	for (HTML_Element *e = (HTML_Element *) NextSibling(); e && e != stop; e = (HTML_Element *) e->Next())
		if (e->IsIncludedActualStyle())
		{
			OP_ASSERT(e->ParentActualStyle() == ParentActualStyle());
			return e;
		}

	return NULL;
}

HTML_Element* HTML_Element::PredActualStyle() const
{
	HTML_Element *stop = ParentActualStyle() ? (HTML_Element *) ParentActualStyle()->PrevSibling() : NULL;

	for (HTML_Element *e = (HTML_Element *) PrevSibling(); e && e != stop; e = (HTML_Element *) e->PrevSibling())
		if (e->IsIncludedActualStyle())
		{
			OP_ASSERT(e->ParentActualStyle() == ParentActualStyle());
			OP_ASSERT(e->SucActualStyle() == (IsIncludedActualStyle() ? this : SucActualStyle()));
			return e;
		}
		else if (HTML_Element *lc = e->LastChildActualStyle())
		{
			OP_ASSERT(lc->ParentActualStyle() == ParentActualStyle());
			OP_ASSERT(lc->SucActualStyle() == (IsIncludedActualStyle() ? this : SucActualStyle()));
			return lc;
		}

	return NULL;
}

HTML_Element* HTML_Element::FirstChildActualStyle() const
{
	for (HTML_Element *e = FirstChild(); e; e = e->FirstChild())
		if (e->IsIncludedActualStyle())
		{
			OP_ASSERT(e->ParentActualStyle() == (IsIncludedActualStyle() ? this : ParentActualStyle()));
			return e;
		}
		else if (!e->FirstChild())
		{
			HTML_Element *stop = (HTML_Element *) NextSibling();
			for (e = (HTML_Element *) e->Next(); e && e != stop; e = (HTML_Element *) e->Next())
				if (e->IsIncludedActualStyle())
				{
					OP_ASSERT(e->ParentActualStyle() == (IsIncludedActualStyle() ? this : ParentActualStyle()));
					return e;
				}
			break;
		}

	return NULL;
}

HTML_Element* HTML_Element::LastChildActualStyle() const
{
	for (HTML_Element *e = LastChild(); e; e = e->LastChild())
		if (e->IsIncludedActualStyle())
		{
			OP_ASSERT(e->ParentActualStyle() == (IsIncludedActualStyle() ? this : ParentActualStyle()));
			return e;
		}
		else if (!e->LastChild())
		{
			for (e = (HTML_Element *) e->Prev(); e && e != this; e = (HTML_Element *) e->Prev())
				if (e->IsIncludedActualStyle())
				{
					const HTML_Element *parent = IsIncludedActualStyle() ? this : ParentActualStyle();

					while (e->ParentActualStyle() != parent)
					{
						OP_ASSERT(IsAncestorOf(e));
						e = e->ParentActualStyle();
					}

					return e;
				}
			break;
		}

	return NULL;
}

HTML_Element* HTML_Element::LastLeafActualStyle() const
{
	HTML_Element *leaf = LastChildActualStyle();
	if (!leaf)
		return NULL;

	while( leaf->LastChildActualStyle() )
		leaf = leaf->LastChildActualStyle();

	return leaf;
}

HTML_Element* HTML_Element::FirstLeafActualStyle() const
{
	HTML_Element *leaf = FirstChildActualStyle();
	if (!leaf)
		return NULL;

	while (leaf->FirstChildActualStyle())
		leaf = leaf->FirstChildActualStyle();

	return leaf;
}

HTML_Element* HTML_Element::NextActualStyle() const
{
	for (HTML_Element* e = (HTML_Element*) Next(); e; e = (HTML_Element*) e->Next())
		if (e->IsIncludedActualStyle())
			return e;

	return NULL;
}

HTML_Element* HTML_Element::PrevActualStyle() const
{
	for (HTML_Element* e = (HTML_Element*) Prev(); e; e = (HTML_Element*) e->Prev())
		if (e->IsIncludedActualStyle())
			return e;

	return NULL;
}

HTML_Element* HTML_Element::NextSiblingActualStyle() const
{
	for (const HTML_Element* leaf = this; leaf; leaf = leaf->ParentActualStyle())
	{
		HTML_Element* candidate = leaf->SucActualStyle();
		if (candidate)
			return candidate;
	}

	return NULL;
}

#endif // DELAYED_SCRIPT_EXECUTION

BOOL HTML_Element::OutSafe(const DocumentContext &context, BOOL going_to_delete/*=TRUE*/)
{
	if (FramesDocument *doc = context.frames_doc)
		if (LogicalDocument* logdoc = doc->GetLogicalDocument())
			logdoc->GetLayoutWorkplace()->SignalHtmlElementRemoved(this);

#if defined SEARCH_MATCHES_ALL && !defined HAS_NO_SEARCHTEXT
	CleanSearchHit(context.frames_doc);
#endif // SEARCH_MATCHES_ALL && !HAS_NO_SEARCHTEXT

	BOOL had_body = FALSE;
	HTML_Element *parent = Parent();

	if (parent)
	{
		if (FramesDocument *doc = context.frames_doc)
		{
			if (!doc->IsBeingDeleted())
			{
				doc->RemoveFromSelection(this);
				had_body = context.logdoc && context.logdoc->GetBodyElm() != NULL;
			}

			if (context.logdoc)
			{
				// FIXME: OOM handling here (though OOM is unlikely to occur and if it does leaves everything in an acceptable state).
				if (!doc->IsBeingDeleted())
					OpStatus::Ignore(context.logdoc->RemoveNamedElement(this, TRUE));

				// Must be done before the tree has changed so
				// that we can figure out which forms each radio button belonged to
				context.logdoc->GetRadioGroups().UnregisterAllRadioButtonsInTree(doc, this);
			}

#ifdef DOCUMENT_EDIT_SUPPORT
			if (doc->GetDocumentEdit())
				doc->GetDocumentEdit()->OnBeforeElementOut(this);
#endif // DOCUMENT_EDIT_SUPPORT

			// Let the hover bubble up to something that is not being removed
			doc->BubbleHoverToParent(this);

#if defined SEARCH_MATCHES_ALL && !defined HAS_NO_SEARCHTEXT
			if (IsReferenced() && doc->GetHtmlDocument())
				// Needs unchanged tree to find all elements in the selection
				doc->GetHtmlDocument()->RemoveElementFromSearchHit(this);
#endif // SEARCH_MATCHES_ALL && !HAS_NO_SEARCHTEXT
		}

		if (DOM_Environment *environment = context.environment)
		{
			if (Parent())
				OpStatus::Ignore(environment->ElementRemoved(this));
		}

		Out();

		if (!context.frames_doc || !context.frames_doc->IsBeingDeleted())
			ClearResolvedUrls();
	}

	// This will clean the subtree recursively
	BOOL return_value = Clean(context, going_to_delete);

	if (had_body && !context.logdoc->GetBodyElm() && context.logdoc->GetDocRoot() && context.logdoc->GetDocRoot()->IsMatchingType(HE_HTML, NS_HTML))
	{
		// Lost the document body element. Find a replacement.
		for (HTML_Element *child = context.logdoc->GetDocRoot()->FirstChildActualStyle(); child; child = child->SucActualStyle())
			if (child->IsMatchingType(HE_BODY, NS_HTML) || child->IsMatchingType(HE_FRAMESET, NS_HTML))
			{
				context.logdoc->SetBodyElm(child);
				break;
			}
	}

	return return_value;
}

void HTML_Element::OutSafe(int *ptr)
{
	OP_ASSERT(ptr == NULL);
	OutSafe(static_cast<FramesDocument *>(NULL), TRUE);
}

static void SendElementRefOnInserted(HTML_Element* elm, FramesDocument* doc)
{
	for (ElementRef* ref = elm->GetFirstReference(); ref;)
	{
		ElementRef* next_ref = ref->NextRef();
		ref->OnInsert(NULL, doc);
		ref = next_ref;
	}
}

static OP_STATUS ElementSignalInserted(const HTML_Element::DocumentContext &context, HTML_Element* element, BOOL mark_dirty)
{
	FramesDocument *doc = context.frames_doc;
	LogicalDocument *logdoc = context.logdoc;
	HTML_Element *parent = element->Parent(), *root = logdoc ? logdoc->GetRoot() : NULL;
	OP_STATUS status = OpStatus::OK; OpStatus::Ignore(status);

	if (root && root->IsAncestorOf(element))
	{
		if (!logdoc->GetDocRoot() && parent == root)
		{
			HTML_Element* doc_root_candidate = element;
			do
			{
				if (Markup::IsRealElement(doc_root_candidate->Type()))
				{
					if (doc_root_candidate->IsIncludedActual())
						break;

#ifdef DELAYED_SCRIPT_EXECUTION
					// The doc_root can also be speculatively inserted in which case we should also pick it up but
					// it's important to not by accident pick up the wrapper elements we use to style XML documents,
					// thus the !logdoc->IsXml() check.
					if (!logdoc->IsXml() && doc_root_candidate->IsIncludedActualStyle())
						break;
#endif //  DELAYED_SCRIPT_EXECUTION
				}

				doc_root_candidate = doc_root_candidate->NextActualStyle();
			}
			while (doc_root_candidate);

			if (doc_root_candidate)
				logdoc->SetDocRoot(doc_root_candidate);
		}

		if (context.logdoc->GetDocRoot() && context.logdoc->GetDocRoot()->IsMatchingType(HE_HTML, NS_HTML) &&
		    (parent->Type() == HE_DOC_ROOT || element->ParentActualStyle() == context.logdoc->GetDocRoot()))
		{
			// Someone might just have inserted a better document body.
			for (HTML_Element *child = context.logdoc->GetDocRoot()->FirstChildActualStyle(); child; child = child->SucActualStyle())
				if (child->IsMatchingType(HE_BODY, NS_HTML) || child->IsMatchingType(HE_FRAMESET, NS_HTML))
				{
					context.logdoc->SetBodyElm(child);
					break;
				}
		}

		if (mark_dirty)
		{
			int sibling_subtrees = logdoc->GetHLDocProfile()->GetCSSCollection()->GetSuccessiveAdjacent();

			if (sibling_subtrees < INT_MAX && context.hld_profile->GetCSSCollection()->MatchFirstChild() && element->IsFirstChild())
				/* If the element inserted is first-child, its sibling (if any) is it no longer.
				   Must recalculate its properties, and all siblings of it that may be affected by
				   this change. Note that we have to add one for :first-child even if we have
				   succ_adj > 0 because for :first-child + elm, we would have to load properties
				   for the old first-child in addition to the old first-child's sibling. */

				++ sibling_subtrees;

			element->MarkPropsDirty(doc, sibling_subtrees, TRUE);

#ifdef SVG_SUPPORT
			BOOL is_insertion_to_svg = parent->IsMatchingType(Markup::SVGE_SVG, NS_SVG); // This is the only kind that isn't covered by the svg code in Mark[Extra]Dirty.
			if (is_insertion_to_svg)
			{
				// Nothing to do. We only need to avoid the other code blocks with
				// MarkDirty so that we don't trigger a full repaint of the SVG which
				// would kill the performance.
			}
			else
#endif // SVG_SUPPORT
				if (parent->HasBeforeOrAfter())
					parent->MarkExtraDirty(doc);
				else
					element->MarkExtraDirty(doc);
		}

		if (OpStatus::IsMemoryError(logdoc->AddNamedElement(element, TRUE)))
			status = OpStatus::ERR_NO_MEMORY;

#ifndef HAS_NOTEXTSELECTION
		if (TextSelection* text_selection = context.frames_doc->GetTextSelection())
			text_selection->InsertElement(element);
#endif // !HAS_NOTEXTSELECTION

#ifdef DOCUMENT_EDIT_SUPPORT
		if (context.frames_doc->GetDocumentEdit())
			context.frames_doc->GetDocumentEdit()->OnElementInserted(element);
#endif // DOCUMENT_EDIT_SUPPORT

#ifdef XML_EVENTS_SUPPORT
		if (context.frames_doc->HasXMLEvents())
		{
			HTML_Element* next_sibling = element->NextSibling();
			HTML_Element* element_it = element;
			while (element_it != next_sibling)
			{
				if (element_it->HasXMLEventAttribute())
					element_it->HandleInsertedElementWithXMLEvent(context.frames_doc);

				element_it = element_it->Next();
			}
		}

		for (XML_Events_Registration *registration = context.frames_doc->GetFirstXMLEventsRegistration();
		     registration;
		     registration = (XML_Events_Registration *) registration->Suc())
			if (OpStatus::IsMemoryError(registration->HandleElementInsertedIntoDocument(context.frames_doc, element)))
				status = OpStatus::ERR_NO_MEMORY;
#endif // XML_EVENTS_SUPPORT

		ES_Thread *current_thread = NULL;

		if (context.environment)
		{
			if (element == logdoc->GetDocRoot())
				if (OpStatus::IsMemoryError(context.environment->NewRootElement(element)))
					status = OpStatus::ERR_NO_MEMORY;

			if (OpStatus::IsMemoryError(context.environment->ElementInserted(element)))
				status = OpStatus::ERR_NO_MEMORY;

			current_thread = context.environment->GetCurrentScriptThread();
		}

		if (OpStatus::IsMemoryError(parent->HandleDocumentTreeChange(context, parent, element, current_thread, TRUE)))
			status = OpStatus::ERR_NO_MEMORY;

		SendElementRefOnInserted(element, doc);
	}
	else if (context.environment)
	{
		if (OpStatus::IsMemoryError(context.environment->ElementInserted(element)))
			status = OpStatus::ERR_NO_MEMORY;

		/* Perform actions that really must happen on element trees being
		   created via innerHTML, but on elements that may not be inserted
		   later into the document.

		   That is, the alternatives of doing this in either HandleDocumentTreeChange()
		   (on addition to the tree) or HLDocProfile::InsertElementInternal()
		   (on document construction) aren't available as options. Options
		   that are preferable if you really don't have schedule any actions this
		   eagerly (== before document tree insertion.) */

		BOOL skip_script_elms = context.environment->SkipScriptElements();
		BOOL load_images = doc && doc->IsCurrentDoc();

		HTML_Element *stop = element->NextSibling();
		for (HTML_Element *iter = element; iter != stop; iter = iter->Next())
		{
			/* If we would have skipped any inserted script elements now if they
			   had been inserted into the document here, we need to make sure
			   they're skipped when they are inserted into as well (if they ever
			   are.) */
			if (skip_script_elms && iter->IsMatchingType(HE_SCRIPT, NS_HTML))
			{
				long handled = iter->GetSpecialNumAttr(ATTR_JS_SCRIPT_HANDLED, SpecialNs::NS_LOGDOC);
				iter->SetSpecialNumAttr(ATTR_JS_SCRIPT_HANDLED, handled | SCRIPT_HANDLED_EXECUTED, SpecialNs::NS_LOGDOC);
			}
			/* Eagerly load images on innerHTML insertion: CORE-19120 + CORE-46429. */
			else if (load_images && OpStatus::IsSuccess(status) && iter->IsMatchingType(HE_IMG, NS_HTML) && !iter->GetHEListElmForInline(IMAGE_INLINE))
			{
				if (URL *src = iter->GetUrlAttr(ATTR_SRC, NS_IDX_HTML, doc->GetLogicalDocument()))
					if (!src->IsEmpty() && doc->GetLoadImages())
						if (OpStatus::IsMemoryError(doc->LoadInline(src, iter, IMAGE_INLINE)))
							status = OpStatus::ERR_NO_MEMORY;
			}
		}

#ifdef MEDIA_HTML_SUPPORT
		// The HTML5 media resource selection algorithm waits for
		// source elements to be inserted (even when the media element
		// is not in a document). Similarly tracks can be added by
		// inserting track elements as children of the media element.
		if (OpStatus::IsSuccess(status))
		{
			if ((element->Type() == Markup::HTE_SOURCE ||
				 element->Type() == Markup::HTE_TRACK) &&
				element->GetNsType() == NS_HTML &&
				element->ParentActual())
			{
				MediaElement* media = element->ParentActual()->GetMediaElement();
				if (media)
				{
					if (OpStatus::IsMemoryError(media->HandleElementChange(element, TRUE, context.environment->GetCurrentScriptThread())))
						status = OpStatus::ERR_NO_MEMORY;
				}
			}
		}
#endif // MEDIA_HTML_SUPPORT

		SendElementRefOnInserted(element, doc);
	}

	return status;
}

OP_STATUS HTML_Element::UnderSafe(const DocumentContext &context, HTML_Element* parent, BOOL mark_dirty)
{
	Under(parent);
	return ElementSignalInserted(context, this, mark_dirty);
}

OP_STATUS HTML_Element::PrecedeSafe(const DocumentContext &context, HTML_Element* sibling, BOOL mark_dirty)
{
	Precede(sibling);
	return ElementSignalInserted(context, this, mark_dirty);
}

OP_STATUS HTML_Element::FollowSafe(const DocumentContext &context, HTML_Element* sibling, BOOL mark_dirty)
{
	Follow(sibling);
	return ElementSignalInserted(context, this, mark_dirty);
}

DOM_EventType HTML_Element::GetEventType(int attr, int attr_ns_idx)
{
	NS_Type attr_ns_type = g_ns_manager->GetNsTypeAt(ResolveNsIdx(attr_ns_idx));

	if (attr_ns_type == NS_HTML)
	{
		switch (attr)
		{
		case ATTR_ONLOAD:
			return ONLOAD;
		case ATTR_ONUNLOAD:
			return ONUNLOAD;
		case ATTR_ONBLUR:
			return ONBLUR;
		case ATTR_ONFOCUS:
			return ONFOCUS;
		case ATTR_ONFOCUSIN:
			return ONFOCUSIN;
		case ATTR_ONFOCUSOUT:
			return ONFOCUSOUT;
		case ATTR_ONERROR:
			return ONERROR;
		case ATTR_ONSUBMIT:
			return ONSUBMIT;
		case ATTR_ONCLICK:
			return ONCLICK;
		case ATTR_ONDBLCLICK:
			return ONDBLCLICK;
		case ATTR_ONCHANGE:
			return ONCHANGE;
		case ATTR_ONKEYDOWN:
			return ONKEYDOWN;
		case ATTR_ONKEYPRESS:
			return ONKEYPRESS;
		case ATTR_ONKEYUP:
			return ONKEYUP;
		case ATTR_ONMOUSEOVER:
			return ONMOUSEOVER;
		case ATTR_ONMOUSEENTER:
			return ONMOUSEENTER;
		case ATTR_ONMOUSEOUT:
			return ONMOUSEOUT;
		case ATTR_ONMOUSELEAVE:
			return ONMOUSELEAVE;
		case ATTR_ONMOUSEMOVE:
			return ONMOUSEMOVE;
		case ATTR_ONMOUSEUP:
			return ONMOUSEUP;
		case ATTR_ONMOUSEDOWN:
			return ONMOUSEDOWN;
		case ATTR_ONMOUSEWHEEL:
			return ONMOUSEWHEEL;
		case ATTR_ONRESET:
			return ONRESET;
		case ATTR_ONSELECT:
			return ONSELECT;
		case ATTR_ONRESIZE:
			return ONRESIZE;
		case ATTR_ONSCROLL:
			return ONSCROLL;
		case ATTR_ONHASHCHANGE:
			return ONHASHCHANGE;
		case ATTR_ONINPUT:
			return ONINPUT;
		case ATTR_ONFORMINPUT:
			return ONFORMINPUT;
		case ATTR_ONINVALID:
			return ONINVALID;
		case ATTR_ONFORMCHANGE:
			return ONFORMCHANGE;
		case ATTR_ONCONTEXTMENU:
			return ONCONTEXTMENU;
#ifdef MEDIA_HTML_SUPPORT
		case ATTR_ONLOADSTART:
			return ONLOADSTART;
		case ATTR_ONPROGRESS:
			return ONPROGRESS;
		case ATTR_ONSUSPEND:
			return ONSUSPEND;
		case ATTR_ONSTALLED:
			return ONSTALLED;
		case ATTR_ONLOADEND:
			return ONLOADEND;
		case ATTR_ONEMPTIED:
			return MEDIAEMPTIED;
		case ATTR_ONPLAY:
			return MEDIAPLAY;
		case ATTR_ONPAUSE:
			return MEDIAPAUSE;
		case ATTR_ONLOADEDMETADATA:
			return MEDIALOADEDMETADATA;
		case ATTR_ONLOADEDDATA:
			return MEDIALOADEDDATA;
		case ATTR_ONWAITING:
			return MEDIAWAITING;
		case ATTR_ONPLAYING:
			return MEDIAPLAYING;
		case ATTR_ONSEEKING:
			return MEDIASEEKING;
		case ATTR_ONSEEKED:
			return MEDIASEEKED;
		case ATTR_ONTIMEUPDATE:
			return MEDIATIMEUPDATE;
		case ATTR_ONENDED:
			return MEDIAENDED;
		case ATTR_ONCANPLAY:
			return MEDIACANPLAY;
		case ATTR_ONCANPLAYTHROUGH:
			return MEDIACANPLAYTHROUGH;
		case ATTR_ONRATECHANGE:
			return MEDIARATECHANGE;
		case ATTR_ONDURATIONCHANGE:
			return MEDIADURATIONCHANGE;
		case ATTR_ONVOLUMECHANGE:
			return MEDIAVOLUMECHANGE;
		case Markup::HA_ONCUECHANGE:
			return MEDIACUECHANGE;
#endif // MEDIA_HTML_SUPPORT
#ifdef CLIENTSIDE_STORAGE_SUPPORT
		case ATTR_ONSTORAGE:
			return STORAGE;
#endif // CLIENTSIDE_STORAGE_SUPPORT
#ifdef TOUCH_EVENTS_SUPPORT
		case ATTR_ONTOUCHSTART:
# ifdef PI_UIINFO_TOUCH_EVENTS
			if(g_op_ui_info->IsTouchEventSupportWanted())
# endif // PI_UIINFO_TOUCH_EVENTS
				return TOUCHSTART;
			break;
		case ATTR_ONTOUCHMOVE:
# ifdef PI_UIINFO_TOUCH_EVENTS
			if(g_op_ui_info->IsTouchEventSupportWanted())
# endif // PI_UIINFO_TOUCH_EVENTS
				return TOUCHMOVE;
			break;
		case ATTR_ONTOUCHEND:
# ifdef PI_UIINFO_TOUCH_EVENTS
			if(g_op_ui_info->IsTouchEventSupportWanted())
# endif // PI_UIINFO_TOUCH_EVENTS
				return TOUCHEND;
			break;
		case ATTR_ONTOUCHCANCEL:
# ifdef PI_UIINFO_TOUCH_EVENTS
			if(g_op_ui_info->IsTouchEventSupportWanted())
# endif // PI_UIINFO_TOUCH_EVENTS
				return TOUCHCANCEL;
			break;
#endif // TOUCH_EVENTS_SUPPORT
		case ATTR_ONPOPSTATE:
			return ONPOPSTATE;
#ifdef PAGED_MEDIA_SUPPORT
		case Markup::HA_ONPAGECHANGE:
			return ONPAGECHANGE;
#endif // PAGED_MEDIA_SUPPORT
#ifdef DRAG_SUPPORT
		case Markup::HA_ONDRAG:
			return ONDRAG;
		case Markup::HA_ONDRAGOVER:
			return ONDRAGOVER;
		case Markup::HA_ONDRAGENTER:
			return ONDRAGENTER;
		case Markup::HA_ONDRAGLEAVE:
			return ONDRAGLEAVE;
		case Markup::HA_ONDRAGSTART:
			return ONDRAGSTART;
		case Markup::HA_ONDRAGEND:
			return ONDRAGEND;
		case Markup::HA_ONDROP:
			return ONDROP;
#endif // DRAG_SUPPORT
#ifdef USE_OP_CLIPBOARD
		case Markup::HA_ONCOPY:
			return ONCOPY;
		case Markup::HA_ONCUT:
			return ONCUT;
		case Markup::HA_ONPASTE:
			return ONPASTE;
#endif // USE_OP_CLIPBOARD
		}
	}
#ifdef SVG_DOM
	else if (attr_ns_type == NS_SVG)
	{
		switch (attr)
		{
			case Markup::SVGA_ONFOCUSIN:
				return ONFOCUSIN;
			case Markup::SVGA_ONFOCUSOUT:
				return ONFOCUSOUT;
			case Markup::SVGA_ONACTIVATE:
				return DOMACTIVATE;
			case Markup::SVGA_ONCLICK:
				return ONCLICK;
			case Markup::SVGA_ONMOUSEDOWN:
				return ONMOUSEDOWN;
			case Markup::SVGA_ONMOUSEUP:
				return ONMOUSEUP;
			case Markup::SVGA_ONMOUSEOVER:
				return ONMOUSEOVER;
			case Markup::SVGA_ONMOUSEENTER:
				return ONMOUSEENTER;
			case Markup::SVGA_ONMOUSEMOVE:
				return ONMOUSEMOVE;
			case Markup::SVGA_ONMOUSEOUT:
				return ONMOUSEOUT;
			case Markup::SVGA_ONMOUSELEAVE:
				return ONMOUSELEAVE;
			case Markup::SVGA_ONUNLOAD:
				return SVGUNLOAD;
			case Markup::SVGA_ONLOAD:
				return SVGLOAD;
			case Markup::SVGA_ONABORT:
				return SVGABORT;
			case Markup::SVGA_ONERROR:
				return SVGERROR;
			case Markup::SVGA_ONRESIZE:
				return SVGRESIZE;
			case Markup::SVGA_ONSCROLL:
				return SVGSCROLL;
			case Markup::SVGA_ONZOOM:
				return SVGZOOM;
			case Markup::SVGA_ONBEGIN:
				return BEGINEVENT;
			case Markup::SVGA_ONEND:
				return ENDEVENT;
			case Markup::SVGA_ONREPEAT:
				return REPEATEVENT;
#ifdef PROGRESS_EVENTS_SUPPORT
			case Markup::SVGA_ONLOADSTART:
				return ONLOADSTART;
			case Markup::SVGA_ONPROGRESS:
				return ONPROGRESS;
			case Markup::SVGA_ONSUSPEND:
				return ONSUSPEND;
			case Markup::SVGA_ONSTALLED:
				return ONSTALLED;
			case Markup::SVGA_ONLOADEND:
				return ONLOADEND;
#endif // PROGRESS_EVENTS_SUPPORT
#ifdef TOUCH_EVENTS_SUPPORT
			case Markup::SVGA_ONTOUCHSTART:
# ifdef PI_UIINFO_TOUCH_EVENTS
				if(g_op_ui_info->IsTouchEventSupportWanted())
# endif // PI_UIINFO_TOUCH_EVENTS
					return TOUCHSTART;
				break;
			case Markup::SVGA_ONTOUCHMOVE:
# ifdef PI_UIINFO_TOUCH_EVENTS
				if(g_op_ui_info->IsTouchEventSupportWanted())
# endif // PI_UIINFO_TOUCH_EVENTS
					return TOUCHMOVE;
				break;
			case Markup::SVGA_ONTOUCHEND:
# ifdef PI_UIINFO_TOUCH_EVENTS
				if(g_op_ui_info->IsTouchEventSupportWanted())
# endif // PI_UIINFO_TOUCH_EVENTS
					return TOUCHEND;
				break;
			case Markup::SVGA_ONTOUCHCANCEL:
# ifdef PI_UIINFO_TOUCH_EVENTS
				if(g_op_ui_info->IsTouchEventSupportWanted())
# endif // PI_UIINFO_TOUCH_EVENTS
					return TOUCHCANCEL;
				break;
#endif // TOUCH_EVENTS_SUPPORT
#ifdef DRAG_SUPPORT
		case Markup::SVGA_ONDRAG:
			return ONDRAG;
		case Markup::SVGA_ONDRAGOVER:
			return ONDRAGOVER;
		case Markup::SVGA_ONDRAGENTER:
			return ONDRAGENTER;
		case Markup::SVGA_ONDRAGLEAVE:
			return ONDRAGLEAVE;
		case Markup::SVGA_ONDRAGSTART:
			return ONDRAGSTART;
		case Markup::SVGA_ONDRAGEND:
			return ONDRAGEND;
		case Markup::SVGA_ONDROP:
			return ONDROP;
#endif // DRAG_SUPPORT
#ifdef USE_OP_CLIPBOARD
		case Markup::SVGA_ONCOPY:
			return ONCOPY;
		case Markup::SVGA_ONCUT:
			return ONCUT;
		case Markup::SVGA_ONPASTE:
			return ONPASTE;
#endif // USE_OP_CLIPBOARD
		}
	}
#endif // SVG_DOM

	return DOM_EVENT_NONE;
}

#ifdef XML_EVENTS_SUPPORT
OP_STATUS HTML_Element::HandleInsertedElementWithXMLEvent(FramesDocument* frames_doc)
{
	OP_ASSERT(HasXMLEventAttribute());

	RETURN_IF_MEMORY_ERROR(frames_doc->ConstructDOMEnvironment());

	if (!frames_doc->GetDOMEnvironment())
		return OpStatus::OK; // No scripts to worry about

	XML_Events_Registration *registration = GetXMLEventsRegistration();
	if (!registration)
	{
		registration = OP_NEW(XML_Events_Registration, (this));
		if (!registration)
			return OpStatus::ERR_NO_MEMORY;

		int attr_index = SetSpecialAttr(ATTR_XML_EVENTS_REGISTRATION, ITEM_TYPE_XML_EVENTS_REGISTRATION, registration, TRUE, SpecialNs::NS_LOGDOC);
		if (attr_index == -1)
		{
			OP_DELETE(registration);
			return OpStatus::ERR_NO_MEMORY;
		}

		frames_doc->AddXMLEventsRegistration(registration);
	}

	int xml_ev_attrs[] = {
		XML_EV_EVENT, XML_EV_PHASE, XML_EV_TARGET, XML_EV_HANDLER,
		XML_EV_OBSERVER,XML_EV_PROPAGATE,XML_EV_DEFAULTACTION };
	for (unsigned i = 0; i < sizeof(xml_ev_attrs)/sizeof(*xml_ev_attrs); i++)
	{
		const uni_char* value;
		int attr = xml_ev_attrs[i];
		if ((value = GetStringAttr(attr, NS_IDX_EVENT)) != NULL)
			HandleXMLEventAttribute(frames_doc, registration, attr, value, uni_strlen(value));
	}
	return OpStatus::OK;
}

OP_STATUS HTML_Element::HandleXMLEventAttribute(FramesDocument* frames_doc, XML_Events_Registration *registration, int ns_event_attr, const uni_char* value, int value_len)
{
	OP_ASSERT(frames_doc->GetDOMEnvironment());
	OP_ASSERT(GetXMLEventsRegistration() == registration);

	OP_STATUS ret_stat = OpStatus::OK;

	switch (ns_event_attr)
	{
	case XML_EV_EVENT:
		ret_stat = registration->SetEventType(frames_doc, value, value_len);
		break;

	case XML_EV_TARGET:
		ret_stat = registration->SetTargetId(frames_doc, value, value_len);
		break;

	case XML_EV_OBSERVER:
		ret_stat = registration->SetObserverId(frames_doc, value, value_len);
		break;

	case XML_EV_HANDLER:
		ret_stat = registration->SetHandlerURI(frames_doc, value, value_len);
		break;

	case XML_EV_PHASE:
		if (value_len == 7 && uni_strncmp(UNI_L("capture"), value, value_len) == 0)
			registration->SetCapture(TRUE);
		break;

	case XML_EV_PROPAGATE:
		if (value_len == 4 && uni_strncmp(UNI_L("stop"), value, value_len) == 0)
			registration->SetStopPropagation(TRUE);
		break;

	case XML_EV_DEFAULTACTION:
		if (value_len == 6 && uni_strncmp(UNI_L("cancel"), value, value_len) == 0)
			registration->SetPreventDefault(TRUE);
		break;
	}

	return ret_stat;
}

BOOL HTML_Element::HasXMLEventAttribute()
{
	if (!Markup::IsRealElement(Type()))
		return FALSE;
	for (int i = 0 ; i < GetAttrCount(); i++)
	{
		if (!GetAttrIsSpecial(i))
		{
			int ns_idx = GetAttrNs(i);

			NS_Type ns = g_ns_manager->GetNsTypeAt(ResolveNsIdx(ns_idx));
			if (ns == NS_EVENT)
				return TRUE;
		}
	}
	return FALSE;
}


#endif // XML_EVENTS_SUPPORT


void HTML_Element::UpdateLinkVisited(FramesDocument* doc)
{
	// Use ApplyPropertyChanges instead and let the layout engine determine what has to be done more exactly?
	if (GetLayoutBox())
	{
		URL href_url = GetAnchor_URL(doc);
		if (!href_url.IsEmpty())
		{
			if (href_url.GetAttribute(URL::KIsFollowed, URL::KFollowRedirect))
				MarkPropsDirty(doc);
		}

		for (HTML_Element* child = FirstChildActual(); child; child = child->SucActual())
			child->UpdateLinkVisited(doc);
	}
}

/** Deprecated! Please use Box::GetRect() */

BOOL HTML_Element::GetBoxRect(FramesDocument* doc, BoxRectType type, RECT& rect)
{
	return GetLayoutBox() && GetLayoutBox()->GetRect(doc, type, rect);
}

const XMLDocumentInformation *HTML_Element::GetXMLDocumentInfo() const
{
	if (Type() == HE_DOCTYPE)
	{
		XMLDocumentInfoAttr *attr = static_cast<XMLDocumentInfoAttr *>(static_cast<ComplexAttr *>(GetSpecialAttr(ATTR_XMLDOCUMENTINFO, ITEM_TYPE_COMPLEX, NULL, SpecialNs::NS_LOGDOC)));
		if (attr)
			return attr->GetDocumentInfo();
	}
	return NULL;
}

#ifdef _WML_SUPPORT_
const uni_char*
HTML_Element::GetHtmlOrWmlStringAttr(short html_attr, short wml_attr) const
{
	if (GetNsType() == NS_WML)
	{
		const uni_char *candidate = GetStringAttr(wml_attr, NS_IDX_WML);
		if (candidate)
			return candidate;
	}

	return GetStringAttr(html_attr);
}
#endif // _WML_SUPPORT_

HTTP_Method HTML_Element::GetMethod() const
{
#ifdef _WML_SUPPORT_
	if (GetNsType() == NS_WML && HasAttr(WA_METHOD, NS_IDX_WML))
		return (HTTP_Method)GetNumAttr(WA_METHOD, NS_IDX_WML, HTTP_METHOD_GET);
#endif // _WML_SUPPORT_

	return (HTTP_Method)GetNumAttr(ATTR_METHOD, NS_IDX_HTML, HTTP_METHOD_GET);
}

InputType
HTML_Element::GetInputType() const
{
#ifdef _WML_SUPPORT_
	if (GetNsType() == NS_WML && HasAttr(WA_TYPE, NS_IDX_WML))
	{
		return (InputType)GetNumAttr(WA_TYPE, NS_IDX_WML, INPUT_TEXT);
	}
#endif // _WML_SUPPORT_

	InputType def = ((HTML_ElementType) packed1.type == HE_BUTTON) ? INPUT_SUBMIT : INPUT_TEXT;
	InputType type = (InputType)GetNumAttr(ATTR_TYPE, NS_IDX_HTML, def);

	return type;
}


const uni_char* HTML_Element::GetElementTitle() const
{
#if defined SVG_SUPPORT || defined _WML_SUPPORT_
	NS_Type ns_type = GetNsType();
#endif // SVG_SUPPORT || _WML_SUPPORT_

#ifdef SVG_SUPPORT
	if (ns_type == NS_SVG)
		return (const uni_char*)GetAttr(Markup::XLINKA_TITLE, ITEM_TYPE_STRING, NULL, NS_IDX_XLINK);
#endif // SVG_SUPPORT

#ifdef _WML_SUPPORT_
	if (ns_type == NS_WML)
	{
		// we don't want the card title popping up when hovering <do>/<go>-elements
		if ((WML_ElementType)Type() == WE_CARD)
			return NULL;
		else
		{
			const uni_char *elm_title = GetStringAttr(WA_TITLE, NS_IDX_WML);
			if (elm_title)
				return elm_title;
		}
	}
#endif //_WML_SUPPORT

	// We let the html:title attribute work wherever it is
	return GetStringAttr(ATTR_TITLE);
}

BOOL HTML_Element::GetAutocompleteOff()
{
	if (Type() == HE_INPUT || Type() == HE_FORM)
	{
		const uni_char* on_status = GetAttrValue(UNI_L("AUTOCOMPLETE"));
		if (on_status && uni_stricmp(on_status, UNI_L("OFF")) == 0)
			return TRUE;
	}
	return FALSE;
}

BOOL HTML_Element::GetUnselectable()
{
	const uni_char *unselectable = GetStringAttr(ATTR_UNSELECTABLE);
	if (unselectable && uni_stri_eq(unselectable, UNI_L("on")))
		return TRUE;
	return FALSE;
}

int HTML_Element::GetRowsOrCols(BOOL get_row) const
{
	if (GetNsType() == NS_HTML && (Type() == HE_TEXTAREA || Type() == HE_INPUT))
	{
		short attr = (get_row) ? ATTR_ROWS : ATTR_COLS;
		int val = (int)GetNumAttr(attr);
		if (!val)
		{
			const uni_char* size = GetStringAttr(ATTR_SIZE);
			if (size && !HasAttr(attr))
			{
				const uni_char *tmp = size;
				while (*tmp && *tmp++ != ',') {}
				if (*tmp)
				{
					if (get_row)
						tmp = size;
					val = uni_atoi(tmp);
				}
			}
			if (!val && Type() == HE_TEXTAREA)
				val = get_row ? 2 : 20;
		}

		return val;
	}

	return 0;
}

int	HTML_Element::GetRows() const
{
	return GetRowsOrCols(TRUE);
}

int	HTML_Element::GetCols() const
{
	return GetRowsOrCols(FALSE);
}

BOOL HTML_Element::GetMultiple() const
{
#ifdef _WML_SUPPORT_
	if (GetNsType() == NS_WML)
	{
		BOOL multiple = GetBoolAttr(WA_MULTIPLE, NS_IDX_WML);
		if (multiple)
			return multiple;
	}
#endif // _WML_SUPPORT_

	if (Type() == HE_SELECT)
		return GetBoolAttr(ATTR_MULTIPLE);
	else
		return FALSE;
}

int HTML_Element::GetTabIndex(FramesDocument* doc /*= NULL */)
{
#ifdef _WML_SUPPORT_
	if (GetNsType() == NS_WML)
	{
		int index = (int)GetNumAttr(WA_TABINDEX, NS_IDX_WML, -1);
		if (index != -1)
			return index;
	}
#endif // _WML_SUPPORT_

	int default_value = -1; // Default value for things that are normally not focusable

	if (IsFormElement() ||
		(IsMatchingType(HE_A, NS_HTML) ||
		IsMatchingType(HE_AREA, NS_HTML)) && HasAttr(ATTR_HREF)
#ifdef DOCUMENT_EDIT_SUPPORT
		|| IsContentEditable()
#endif // DOCUMENT_EDIT_SUPPORT
		)
	{
		default_value = 0; // Default value for things that are normally focusable.
	}

#ifdef MEDIA_HTML_SUPPORT
	if (IsMatchingType(HE_VIDEO, NS_HTML) || IsMatchingType(HE_AUDIO, NS_HTML)
#ifdef DOM_JIL_API_SUPPORT
		|| IsMatchingType(HE_OBJECT, NS_HTML)
#endif //DOM_JIL_API_SUPPORT
		)
	{
		Media* media_elm = GetMedia();
		if (media_elm && media_elm->IsFocusable())
		{
			default_value = 0;
		}
	}
#endif // MEDIA_HTML_SUPPORT

#ifdef DOCUMENT_EDIT_SUPPORT
	// tabIndex of editable iframes is 0
	if (IsMatchingType(HE_IFRAME, NS_HTML))
	{
		if (doc)
		{
			FramesDocElm *fde = FramesDocElm::GetFrmDocElmByHTML(this);
			// No reason to tab to an empty or readonly iframe
			if (fde && fde->GetCurrentDoc() && fde->GetCurrentDoc()->GetDocumentEdit())
			{
				default_value = 0;
			}
		}
	}
#endif // DOCUMENT_EDIT_SUPPORT
	return (int)GetNumAttr(ATTR_TABINDEX, NS_IDX_HTML, default_value);
}

#if defined SAVE_DOCUMENT_AS_TEXT_SUPPORT
// line_length == 0 means start of new line, line_length < 0 means (-line_length) empty lines.

/**
 * Wrapper method to encapsulate the LEAVE functionality and convert it to OP_STATUS.
 */
static OP_STATUS WriteToStream(UnicodeOutputStream* out, const uni_char* str, int len)
{
	TRAPD(status, out->WriteStringL(str, len));
	return status;
}

/**
 * Wrapper method to encapsulate the LEAVE functionality and convert it to OP_STATUS.
 */
static OP_STATUS WriteToStream(UnicodeOutputStream* out, uni_char c)
{
	TRAPD(status, out->WriteStringL(&c, 1));
	return status;
}

OP_STATUS HTML_Element::WriteAsText(UnicodeOutputStream* out, HLDocProfile* hld_profile, LayoutProperties* cascade, int max_line_length, int& line_length, BOOL& trailing_ws, BOOL& prevent_newline, BOOL& pending_newline)
{
	OpString new_line_string;
	RETURN_IF_ERROR(new_line_string.Set(NEWLINE));

	if (Type() == HE_TEXT)
	{
		const uni_char* text_content = LocalContent();
		if (text_content)
		{
			WordInfo word_info;
			FontSupportInfo font_info(0);
			font_info.current_font = 0; // we are not interested in fonts ...

			const uni_char* tmp = text_content;
			short white_space = cascade->GetProps()->white_space;

			int len = GetTextContentLength();
			for (;;)
			{
				const uni_char* word = tmp;
				word_info.Reset();
				word_info.SetLength(0);

				if (!GetNextTextFragment(tmp, len, word_info, CSSValue(white_space),
						white_space == CSS_VALUE_nowrap, TRUE, font_info,
						hld_profile ? hld_profile->GetFramesDocument() : NULL,
#ifdef FONTSWITCHING
										 hld_profile ? hld_profile->GetPreferredScript() :
#endif // FONTSWITCHING
										 WritingSystem::Unknown))
					break;

				len -= (tmp-word);

				if (word_info.GetLength() && *word != 173) // Soft hyphen
				{
					if (pending_newline)
					{
						prevent_newline = FALSE;
						pending_newline = FALSE;
						trailing_ws = FALSE;
						WriteToStream(out, new_line_string.CStr(), new_line_string.Length());//FIXME:OOM
						line_length = 0;
					}

					if (line_length > 0 && trailing_ws && max_line_length >= 0 && line_length + word_info.GetLength() > (unsigned int) max_line_length && (white_space == CSS_VALUE_normal || white_space == CSS_VALUE_pre_wrap))
					{
						WriteToStream(out, new_line_string.CStr(), new_line_string.Length());//FIXME:OOM
						line_length = 0;
					}

					if (line_length < 0)
						line_length = 0;

					WriteToStream(out, word, word_info.GetLength());//FIXME:
					trailing_ws = FALSE;
					line_length += word_info.GetLength();
				}

				if (word_info.IsTabCharacter())
				{
					if (line_length < 0)
						line_length = 0;

					WriteToStream(out, '\t');//FIXME:OOM
					trailing_ws = TRUE;
					line_length++;
				}
				else
				{
					// Trailing ws is only interesting if not in pre/pre-wrap since in pre-wrap and
					// pre it will come as a seperate word anyway.
					BOOL word_has_trailing_ws = word_info.HasTrailingWhitespace() && white_space != CSS_VALUE_pre && white_space != CSS_VALUE_pre_wrap;
					if (line_length > 0 && word_has_trailing_ws)
					{
						if (!trailing_ws)
						{
							WriteToStream(out, ' ');//FIXME:OOM
							trailing_ws = TRUE;
							line_length++;
						}
					}
				}

				if (word_info.HasEndingNewline())
				{
					WriteToStream(out, new_line_string.CStr(), new_line_string.Length());//FIXME:OOM
					line_length = 0;
				}
			}
		}
	}
	else if (Type() == HE_BR)
	{
		prevent_newline = FALSE;
		pending_newline = FALSE;
		trailing_ws = FALSE;
		WriteToStream(out, new_line_string.CStr(), new_line_string.Length());//FIXME:OOM
		if (line_length > 0)
			line_length = 0;
		else
			line_length--;
	}
	else
	{
		BOOL is_block = FALSE;
		LayoutProperties* child_cascade;

		switch (cascade->GetProps()->display_type)
		{
		case CSS_VALUE_none:
			return OpStatus::OK;

		case CSS_VALUE_table_cell:
			prevent_newline = TRUE;
			break;

		case CSS_VALUE_block:
			if (cascade->GetProps()->float_type != CSS_VALUE_none)
				break; // treat floats as inline

		case CSS_VALUE_list_item:
			if (pending_newline)
			{
				pending_newline = FALSE;
				trailing_ws = FALSE;
				WriteToStream(out, new_line_string.CStr(), new_line_string.Length());//FIXME:OOM
				line_length = 0;
			}

			if (!prevent_newline && line_length >= 0)
			{
				trailing_ws = FALSE;
				WriteToStream(out, new_line_string.CStr(), new_line_string.Length());//FIXME:OOM
				if (line_length > 0)
					WriteToStream(out, new_line_string.CStr(), new_line_string.Length());//FIXME:OOM
				line_length = -1;
			}

			is_block = TRUE;
			break;
		}

		for (HTML_Element* child = FirstChild(); child; child = child->Suc())
		{
			child_cascade = cascade->GetChildProperties(hld_profile, child);
			if (child_cascade)
			{
				OP_STATUS status = child->WriteAsText(out, hld_profile, child_cascade, max_line_length, line_length, trailing_ws, prevent_newline, pending_newline);
				child_cascade->Clean();
				RETURN_IF_ERROR(status);
			}
			else
				return OpStatus::ERR_NO_MEMORY;
		}

		if (is_block)
		{
			prevent_newline = FALSE;

			if (line_length > 0)
				pending_newline = TRUE;
		}
		else if (cascade->GetProps()->display_type == CSS_VALUE_table_row)
		{
			prevent_newline = FALSE;
			pending_newline = FALSE;
			trailing_ws = FALSE;
			if (line_length > 0)
			{
				WriteToStream(out, new_line_string.CStr(), new_line_string.Length());//FIXME:OOM
				line_length = 0;
			}
		}
		else if (line_length > 0 && cascade->GetProps()->display_type == CSS_VALUE_table_cell)
		{
			trailing_ws = TRUE;
			WriteToStream(out, ' ');//FIXME:OOM
			line_length++;
		}
	}

	return OpStatus::OK;
}
#endif // SAVE_DOCUMENT_AS_TEXT_SUPPORT

int
HTML_AttrIterator::Count()
{
	int i, count;
	for (i = 0, count = 0; i < element->GetAttrSize(); ++i)
	{
		short attr = element->GetAttrItem(i);
		if (attr != ATTR_NULL && !element->GetAttrIsSpecial(i))
			++count;
	}
	return count;
}

BOOL
HTML_AttrIterator::GetNext(const uni_char *&name, const uni_char *&value, int* ns_idx /* = NULL */)
{
	int ns_idx_tmp;
	BOOL specified, id;

	if (GetNext(name, value, ns_idx_tmp, specified, id))
	{
		if (ns_idx)
			*ns_idx = ns_idx_tmp;

		return TRUE;
	}
	else
		return FALSE;
}

const uni_char*
HTML_ImmutableAttrIterator::GetNextId()
{
	int attr_size = element->GetAttrSize();
	while (idx < attr_size)
	{
		if (element->GetAttrIsId(idx) &&
			element->GetItemType(idx) == ITEM_TYPE_STRING)
		{
			return element->GetAttrValueString(idx++);
		}

		idx++;
	}
	return NULL;
}

BOOL
HTML_AttrIterator::GetNext(const uni_char *&name, const uni_char *&value, int &ns_idx, BOOL &specified, BOOL &id)
{
	int i, j;
	for (i = 0, j = idx; i < element->GetAttrSize(); ++i)
	{
		short attr = element->GetAttrItem(i);
		if (attr != ATTR_NULL && !element->GetAttrIsSpecial(i))
			if (j == 0)
			{
				buffer.Clear();

				name = element->GetAttrNameString(i); OP_ASSERT(name);
				value = element->GetAttrValueValue(i, attr, HE_ANY, &buffer);
				ns_idx = element->GetAttrNs(i);
				specified = element->data.attrs[i].IsSpecified();
				id = element->GetAttrIsId(i);

				if (!value)
					/* Not just to be safe: if an attribute that is generated into the buffer is
					   empty and the buffer has no storage allocated already, GetAttrValueValue
					   returns NULL when it actually means "empty string".

					   Also just to be safe.  :-) */
					value = UNI_L("");

				++idx;
				return TRUE;
			}
			else
				--j;
	}

	return FALSE;
}

#ifdef WEB_TURBO_MODE

BOOL
HTML_AttrIterator::GetNext(int& attr, int& ns_idx, BOOL& is_special, void*& obj, ItemType& item_type)
{
	int i, j;
	for (i = 0, j = idx; i < element->GetAttrSize(); ++i)
	{
		short a = element->GetAttrItem(i);
		if (a != ATTR_NULL)
			if (j == 0)
			{
				attr = a;
				idx++;
				ns_idx = element->GetAttrNs(i);
				is_special = element->GetAttrIsSpecial(i);
				obj = element->GetValueItem(i);
				item_type = element->GetItemType(i);
				return TRUE;
			}
			else
				--j;
	}
	return FALSE;
}

#endif // WEB_TURBO_MODE

#ifdef SVG_SUPPORT

BOOL
HTML_ImmutableAttrIterator::GetNext(int& attr, int& ns_idx, BOOL& is_special, void*& obj, ItemType& item_type)
{
	int attr_size = element->GetAttrSize();
	while (idx < attr_size)
	{
		short a = element->GetAttrItem(idx);
		if (a != ATTR_NULL)
		{
			attr = a;
			ns_idx = element->GetAttrNs(idx);
			is_special = element->GetAttrIsSpecial(idx);
			obj = element->GetValueItem(idx);
			item_type = element->GetItemType(idx);

			idx++;
			return TRUE;
		}

		idx++;
	}
	return FALSE;
}

#endif // SVG_SUPPORT

void
HTML_AttrIterator::Reset(HTML_Element *new_element)
{
	element = new_element;
	idx = 0;
}

COLORREF HTML_Element::GetCssBackgroundColorFromStyleAttr(FramesDocument* doc /* = NULL */)
{
	/* This is wrong! Avoid using this function. */

	if (IsPropsDirty())
	{
		if (doc)
			doc->GetLogicalDocument()->GetLayoutWorkplace()->UnsafeLoadProperties(this);
		else
			OP_ASSERT(!"Will return the wrong value since we had no Document pointer.");
	}

	return CssPropertyItem::GetCssBackgroundColor(this);
}

COLORREF HTML_Element::GetCssColorFromStyleAttr(FramesDocument* doc /* = NULL */)
{
	/* This is wrong! Avoid using this function.
	   The color CSS property is inherited, so the cascade is needed to give correct results. */

	if (IsPropsDirty())
	{
		if (doc)
			doc->GetLogicalDocument()->GetLayoutWorkplace()->UnsafeLoadProperties(this);
		else
			OP_ASSERT(!"Will return the wrong value since we had no Document pointer.");
	}

	return CssPropertyItem::GetCssColor(this);
}

CursorType HTML_Element::GetCursorType()
{
	/* This is wrong! Avoid using this function.
	   The cursor CSS property is inherited, so the cascade is needed to give correct results. */

	return CssPropertyItem::GetCursorType(this);
}

void HTML_Element::DeleteCssProperties()
{
	if (css_properties)
	{
		if (packed2.shared_css)
			g_sharedCssManager->DeleteSharedCssProperties(css_properties, GetCssPropLen() * sizeof(CssPropertyItem));
		else
			OP_DELETEA(css_properties);

		css_properties = 0;
		packed2.shared_css = 0;
		SetCssPropLen(0);
	}
}

void HTML_Element::UnshareCssProperties()
{
	if (packed2.shared_css || !css_properties)
	{
		DeleteCssProperties();
		packed2.shared_css = 0;
	}
}

#ifdef MANUAL_PLUGIN_ACTIVATION
BOOL
HTML_Element::GetPluginActivated()
{
	return GetSpecialBoolAttr(ATTR_PLUGIN_ACTIVE, SpecialNs::NS_LOGDOC);
}

void
HTML_Element::SetPluginActivated(BOOL activate)
{
	SetSpecialBoolAttr(ATTR_PLUGIN_ACTIVE, activate, SpecialNs::NS_LOGDOC);
}

BOOL
HTML_Element::GetPluginExternal()
{
	return GetSpecialBoolAttr(ATTR_PLUGIN_EXTERNAL, SpecialNs::NS_LOGDOC);
}

void
HTML_Element::SetPluginExternal(BOOL external)
{
	SetSpecialBoolAttr(ATTR_PLUGIN_EXTERNAL, external, SpecialNs::NS_LOGDOC);
}
#endif // MANUAL_PLUGIN_ACTIVATION

BOOL HTML_Element::IsFirstChild()
{
	if (Parent())
	{
		HTML_Element* child = ParentActual()->FirstChildActual();

		while (child && !Markup::IsRealElement(child->Type()))
			child = child->SucActual();

		return (child == this);
	}
	else
		return FALSE;
}

BOOL
HTML_Element::IsPreFocused() const
{
	if (IsMatchingType(HE_INPUT, NS_HTML))
	{
		InputType inp_type = GetInputType();
		switch (inp_type)
		{
		case INPUT_CHECKBOX:
		case INPUT_RADIO:
		case INPUT_SUBMIT:
		case INPUT_RESET:
		case INPUT_BUTTON:
			return FALSE;
		}
	}
	else if (!IsMatchingType(HE_TEXTAREA, NS_HTML) && !IsMatchingType(HE_SELECT, NS_HTML))
		return FALSE;

	return GetSpecialBoolAttr(ATTR_PREFOCUSED, SpecialNs::NS_LOGDOC);
}

BOOL
HTML_Element::IsDisplayingReplacedContent()
{
	return GetLayoutBox() && GetLayoutBox()->IsContentReplaced();
}

#if defined(JS_PLUGIN_SUPPORT)
JS_Plugin_Object* HTML_Element::GetJSPluginObject()
{
	JS_Plugin_Object* jso = NULL;
	ES_Object* esobj = DOM_Utils::GetJSPluginObject(GetESElement());
	if (esobj)
	{
		EcmaScript_Object* eso = ES_Runtime::GetHostObject(esobj);
		if (eso)
		{
			OP_ASSERT(eso->IsA(ES_HOSTOBJECT_JSPLUGIN));
			jso = static_cast<JS_Plugin_Object*>(eso);
		}
	}
	return jso;
}

OP_BOOLEAN HTML_Element::IsJSPluginObject(HLDocProfile* hld_profile, JS_Plugin_Object **obj_pp) const
{
	if (Type() == HE_OBJECT)
	{
		const uni_char* type = GetStringAttr(ATTR_TYPE);
		if (type)
		{
			FramesDocument *frames_doc = hld_profile->GetFramesDocument();

			OP_STATUS status = frames_doc->ConstructDOMEnvironment();
			RETURN_IF_MEMORY_ERROR(status);
			if (OpStatus::IsError(status))
			{
				// No scripting allowed, this is not a jsplugin object
				return OpBoolean::IS_FALSE;
			}

			DOM_Environment *environment = frames_doc->GetDOMEnvironment();
			if (environment && environment->IsEnabled())
				if (JS_Plugin_Context *ctx = environment->GetJSPluginContext())
					if (ctx->HasObjectHandler(type, obj_pp))
						return OpBoolean::IS_TRUE;
		}
	}
	return OpBoolean::IS_FALSE;
}

OP_STATUS HTML_Element::PassParamsForJSPlugin(LogicalDocument* logdoc)
{
	OP_STATUS res = OpStatus::OK;

	if (Type() == HE_OBJECT)
	{
		int param_count = CountParams();

		if (param_count)
		{
			JS_Plugin_Object *handler = NULL;
			JS_Plugin_Object *js_obj = NULL;

			if (IsJSPluginObject(logdoc->GetHLDocProfile(), &handler) == OpBoolean::IS_TRUE && (js_obj = GetJSPluginObject()) != NULL)
			{
				int next_index = 0;
				const uni_char** names = OP_NEWA(const uni_char*, param_count);
				const uni_char** values = OP_NEWA(const uni_char*, param_count);

				if (!names || !values)
				{
					OP_DELETEA(names);
					OP_DELETEA(values);
					return OpStatus::ERR_NO_MEMORY;
				}

				GetParams(names, values, next_index);
				if (param_count == next_index)
				{
					for (int i = 0; i < param_count; i++)
					{
						if (names[i])
							js_obj->ParamSet(names[i], values[i]);
					}
				}
				else
					res = OpStatus::ERR_PARSING_FAILED;

				OP_DELETEA(names);
				OP_DELETEA(values);
			}
		}
    }

    return res;
}
#endif // JS_PLUGIN_SUPPORT

#ifdef INTERNAL_SPELLCHECK_SUPPORT
HTML_Element::SPC_ATTR_STATE HTML_Element::SpellcheckEnabledByAttr()
{
	HTML_Element* he = this;
	SPC_ATTR_STATE spellcheck = SPC_ENABLE_DEFAULT;

	while (he)
	{
		spellcheck = static_cast<SPC_ATTR_STATE>(he->GetNumAttr(ATTR_SPELLCHECK));
		if (spellcheck == SPC_ENABLE || spellcheck == SPC_DISABLE)
			return spellcheck;

		he = he->Parent();
	}

	return IsMatchingType(HE_INPUT, NS_HTML) ? SPC_DISABLE_DEFAULT : SPC_ENABLE_DEFAULT;
}
#endif // INTERNAL_SPELLCHECK_SUPPORT

#ifdef WEB_TURBO_MODE
OP_STATUS HTML_Element::UpdateTurboMode(LogicalDocument *logdoc, URL_CONTEXT_ID context_id)
{
	HTML_AttrIterator iter(this);

	int attr_code = 0;
	int attr_ns_idx = 0;
	BOOL attr_is_special = FALSE;
	void *attr_value = NULL;
	ItemType attr_type;

	// Check if this is a link.
	if (IsMatchingType(HE_A, NS_HTML))
	{
		while (iter.GetNext(attr_code, attr_ns_idx, attr_is_special, attr_value, attr_type))
		{
			if (attr_code == ATTR_HREF
				&& attr_ns_idx == NS_IDX_HTML
				&& attr_is_special == FALSE
				&& attr_type == ITEM_TYPE_URL_AND_STRING)
			{
				UrlAndStringAttr *url_attr = static_cast<UrlAndStringAttr*>(attr_value);
				URL new_url = ResolveUrl(url_attr->GetString(), logdoc, attr_code, TRUE);
				OpStatus::Ignore(url_attr->SetResolvedUrl(new_url));
			}
		}

		return OpStatus::OK;
	}

#ifdef _WML_SUPPORT_
	// WML Links are handled in WML_Context::GetWmlUrl()
#endif //_WML_SUPPORT_

#ifdef SVG_SUPPORT
# if 0
	if (IsMatchingType(Markup::SVGE_A, NS_SVG))
	{
		// TODO: fix this for SVG links
	}
# endif // 0
#endif // SVG_SUPPORT

	int idx = FindSpecialAttrIndex(ATTR_CSS_LINK, SpecialNs::NS_LOGDOC);
	if (idx != -1)
	{
		URL_CONTEXT_ID current_context_id = logdoc->GetCurrentContextId();
		URL *css_url = static_cast<URL*>(GetValueItem(idx));
		if (css_url && css_url->GetContextId() != current_context_id)
		{
			const OpStringC8 css_str
				= css_url->GetAttribute(URL::KName_With_Fragment_Username_Password_NOT_FOR_UI, URL::KNoRedirect);
			URL new_url = g_url_api->GetURL(css_str.CStr(), current_context_id);
			URL *allocated_url = new URL(new_url);
			ReplaceAttrLocal(idx, ATTR_CSS_LINK, ITEM_TYPE_URL, allocated_url, SpecialNs::NS_LOGDOC, TRUE, TRUE, FALSE, FALSE, FALSE);
		}
	}

	return OpStatus::OK;
}

OP_STATUS HTML_Element::DisableTurboForImage()
{
	short attr = ATTR_NULL;
	if (GetNsType() != NS_HTML)
		return OpStatus::ERR_NOT_SUPPORTED;

	HTML_ElementType type = Type();
	switch (type)
	{
	case HE_IMG:
#ifdef PICTOGRAM_SUPPORT
		if (GetSpecialAttr(ATTR_LOCALSRC_URL, ITEM_TYPE_URL, NULL, SpecialNs::NS_LOGDOC) != NULL)
			return OpStatus::OK;
#endif // PICTOGRAM_SUPPORT
		// fall through
	case HE_INPUT:
		attr = ATTR_SRC;
		break;
	case HE_OBJECT:
#ifdef PICTOGRAM_SUPPORT
		if (GetSpecialAttr(ATTR_LOCALSRC_URL, ITEM_TYPE_URL, NULL, SpecialNs::NS_LOGDOC) != NULL)
			return OpStatus::OK;
#endif // PICTOGRAM_SUPPORT
		attr = ATTR_DATA;
		break;
	case HE_TABLE:
	case HE_TD:
	case HE_TH:
	case HE_BODY:
		attr = ATTR_BACKGROUND;
		break;
	default:
		return OpStatus::ERR_NOT_SUPPORTED;
	}

	UrlAndStringAttr *url_attr = static_cast<UrlAndStringAttr*>(GetAttr(attr, ITEM_TYPE_URL_AND_STRING, (void*)NULL));
	if (url_attr)
	{
		URL *old_url = url_attr->GetResolvedUrl();
		if (!old_url || old_url->IsEmpty() || !old_url->GetAttribute(URL::KUsesTurbo))
			return	OpStatus::OK; // Either no URL to convert or URL is not using Turbo

		const OpStringC img_url_str = old_url->GetAttribute(URL::KUniName_With_Fragment_Username_Password_NOT_FOR_UI, URL::KNoRedirect);
		URL new_url = g_url_api->GetURL(img_url_str);
		if (new_url.IsEmpty())
			return OpStatus::ERR;

		// There is currently only support for one src/data/background attribute
		return url_attr->SetResolvedUrl(new_url);
	}

	return OpStatus::OK;
}
#endif // WEB_TURBO_MODE

void HTML_Element::ResetExtraBackgroundList()
{
	HTML_Element::BgImageIterator iter(this);
	HEListElm *iter_elm = iter.Next();
	while (iter_elm)
	{
		if (iter_elm->GetInlineType() == EXTRA_BGIMAGE_INLINE)
			iter_elm->SetActive(FALSE);
		iter_elm = iter.Next();
	}
}

void HTML_Element::CommitExtraBackgroundList()
{
	HTML_Element::BgImageIterator iter(this);
	HEListElm *iter_elm = iter.Next();
	while (iter_elm)
	{
		HEListElm *next_iter_elm = iter.Next();
		if (!iter_elm->IsActive())
			OP_DELETE(iter_elm);
		iter_elm = next_iter_elm;
	}
}

#ifdef ON_DEMAND_PLUGIN
void HTML_Element::SetIsPluginPlaceholder()
{
	SetSpecialBoolAttr(ATTR_PLUGIN_PLACEHOLDER, TRUE, SpecialNs::NS_LOGDOC);
}

void HTML_Element::SetPluginDemanded()
{
	SetSpecialBoolAttr(ATTR_PLUGIN_DEMAND, TRUE, SpecialNs::NS_LOGDOC);
	RemoveSpecialAttribute(ATTR_PLUGIN_PLACEHOLDER, SpecialNs::NS_LOGDOC);
# ifdef MANUAL_PLUGIN_ACTIVATION
	SetPluginActivated(TRUE);
# endif // MANUAL_PLUGIN_ACTIVATION
}
#endif // ON_DEMAND_PLUGIN

#ifdef MEDIA_HTML_SUPPORT
OP_STATUS HTML_Element::ConstructMediaElement(int attr_index, MediaElement **element)
{
	OP_ASSERT(!GetSpecialAttr(Markup::MEDA_COMPLEX_MEDIA_ELEMENT, ITEM_TYPE_COMPLEX, NULL, SpecialNs::NS_MEDIA));

	MediaElement* media;
	media = OP_NEW(MediaElement, (this));
	if (!media)
		return OpStatus::ERR_NO_MEMORY;

	if (attr_index >= 0)
		SetAttrLocal(attr_index, Markup::MEDA_COMPLEX_MEDIA_ELEMENT, ITEM_TYPE_COMPLEX, media, SpecialNs::NS_MEDIA, TRUE, TRUE);
	else
		if (SetSpecialAttr(Markup::MEDA_COMPLEX_MEDIA_ELEMENT, ITEM_TYPE_COMPLEX, media, TRUE, SpecialNs::NS_MEDIA) < 0)
			return OpStatus::ERR_NO_MEMORY;

	if (element)
		*element = media;

	return OpStatus::OK;
}

MediaElement* HTML_Element::GetMediaElement()
{
	MediaElement *media = static_cast<MediaElement *>(GetSpecialAttr(Markup::MEDA_COMPLEX_MEDIA_ELEMENT, ITEM_TYPE_COMPLEX, NULL, SpecialNs::NS_MEDIA));

	if (!media &&
		(IsMatchingType(HE_AUDIO, NS_HTML) || IsMatchingType(HE_VIDEO, NS_HTML)) &&
		OpStatus::IsError(ConstructMediaElement(-1, &media)))
		media = NULL;

	return media;
}
#endif // MEDIA_HTML_SUPPORT

#ifdef MEDIA_SUPPORT
Media* HTML_Element::GetMedia()
{
	Markup::AttrType attr_id = static_cast<Markup::AttrType>(GetSpecialNumAttr(Markup::MEDA_MEDIA_ATTR_IDX, SpecialNs::NS_MEDIA, Markup::HA_NULL));
	if (attr_id != Markup::HA_NULL)
		return static_cast<Media *>(GetSpecialAttr(attr_id, ITEM_TYPE_COMPLEX, NULL, SpecialNs::NS_MEDIA));
	return NULL;
}

BOOL HTML_Element::SetMedia(Markup::AttrType media_attr, Media *media, BOOL need_free)
{
	if(SetSpecialNumAttr(Markup::MEDA_MEDIA_ATTR_IDX, media_attr, SpecialNs::NS_MEDIA) != -1)
		return SetSpecialAttr(media_attr, ITEM_TYPE_COMPLEX, reinterpret_cast<void*>(media), need_free, SpecialNs::NS_MEDIA) != -1;

	return FALSE;
}
#endif // MEDIA_SUPPORT

void HTML_Element::ConstructL(LogicalDocument *logdoc, Markup::Type type, Markup::Ns ns, HTML5TokenWrapper *token)
{
	OP_PROBE8_L(OP_PROBE_HTML_ELEMENT_CONSTRUCTL);

	NS_Type ns_type = NS_HTML;
	int ns_idx = NS_IDX_HTML;
	if (ns == Markup::SVG)
	{
		ns_type = NS_SVG;
		ns_idx = NS_IDX_SVG;
	}
	else if (ns == Markup::MATH)
	{
		ns_type = NS_MATHML;
		ns_idx = NS_IDX_MATHML;
	}

	g_ns_manager->GetElementAt(GetNsIdx())->DecRefCount();
	SetNsIdx(ns_idx);
	g_ns_manager->GetElementAt(ns_idx)->IncRefCount();

	SetType(type);
	SetInserted(HE_NOT_INSERTED);

	HLDocProfile *hld_profile = logdoc->GetHLDocProfile();
	HtmlAttrEntry *ha_list = NULL;
	unsigned token_attr_count = 0;
	unsigned attr_count_extra = 0;

	if (token)
	{
		ha_list = token->GetOrCreateAttrEntries(token_attr_count, attr_count_extra, logdoc);
		if (!ha_list)
			LEAVE_IF_ERROR(OpStatus::ERR_NO_MEMORY);
	}
	int attr_count = attr_count_extra;
	attr_count += GetNumberOfExtraAttributesForType(type, ns_type, hld_profile, ha_list);

	if (attr_count)
	{
		LEAVE_IF_NULL(data.attrs = NEW_HTML_Attributes((attr_count)));

		SetAttrSize(attr_count);
		REPORT_MEMMAN_INC(attr_count * sizeof(AttrItem));
	}
	else
		data.attrs = NULL;

	unsigned attr_i = 0, set_attr_i = 0;
	if (attr_count_extra > 0)
	{
		for (; attr_i < attr_count_extra; attr_i++)
		{
			HtmlAttrEntry *hae = &ha_list[attr_i];

			const uni_char* orig_hae_value = hae->value;
			UINT orig_hae_value_len = hae->value_len;
			BOOL may_steal = hae->delete_after_use;

			void *value;
			ItemType item_type;
			BOOL need_free, is_event;
			OP_STATUS status = ConstructAttrVal(hld_profile,
				hae,
				may_steal,
				value,
				item_type,
				need_free,
				is_event,
				ha_list,
				&set_attr_i);
			if (OpStatus::IsError(status))
			{
				if (token)
					token->DeleteAllocatedAttrEntries();
				LEAVE(status);
			}

			if (may_steal && !hae->value) // value has been stolen
			{
				hae->delete_after_use = FALSE;
				hae->value = orig_hae_value;
				hae->value_len = orig_hae_value_len;
			}

			if (item_type != ITEM_TYPE_UNDEFINED)
				SetAttrLocal(set_attr_i++, hae->attr, item_type, value, hae->ns_idx, need_free, hae->is_special, hae->is_id, hae->is_specified, is_event);
		}
	}

	OP_STATUS oom_stat = OpStatus::OK;
	if (attr_count - attr_count_extra > 0)
		oom_stat = SetExtraAttributesForType(set_attr_i, ns_type, ha_list, token_attr_count, hld_profile);

	if (attr_count_extra > 0 && token)
		token->DeleteAllocatedAttrEntries();

#if defined(JS_PLUGIN_SUPPORT)
	if (type == HE_OBJECT)
		LEAVE_IF_FATAL(SetupJsPluginIfRequested(GetStringAttr(ATTR_TYPE), hld_profile));
#endif // JS_PLUGIN_SUPPORT

#if defined(SVG_SUPPORT)
	if (ns_type == NS_SVG && type == Markup::SVGE_SVG)
	{
		SVGContext* ctx = g_svg_manager->CreateDocumentContext(this, hld_profile->GetLogicalDocument());
		if (!ctx)
			oom_stat = OpStatus::ERR_NO_MEMORY;
	}
#endif // SVG_SUPPORT

	LEAVE_IF_ERROR(oom_stat);
}

Markup::Ns HTML_Element::GetNs() const
{
	NS_Type ns_type = GetNsType();
	if (ns_type == NS_SVG)
		return Markup::SVG;
	else if (ns_type == NS_MATHML)
		return Markup::MATH;
	else
		return Markup::HTML;
}

void HTML_Element::AddAttributeL(LogicalDocument *logdoc, const HTML5TokenBuffer *name, const uni_char *value, unsigned value_len)
{
	Markup::AttrType attr = g_html5_name_mapper->GetAttrTypeFromTokenBuffer(name);
	LEAVE_IF_ERROR(SetAttribute(logdoc, attr, attr == Markup::HA_XML ? name->GetBuffer() : NULL, NS_IDX_DEFAULT, value, value_len, NULL, FALSE, attr == Markup::HA_ID, TRUE));
}

/**
 * Checks if the codebase attribute is a local file, which might be a security issue
 */
HTML_ElementType HTML_Element::CheckLocalCodebase(LogicalDocument* logdoc, HTML_ElementType type, const uni_char* mime_type)
{
	OP_ASSERT(type == HE_APPLET || type == HE_EMBED);

	if (type == HE_APPLET || (type == HE_EMBED && mime_type && uni_strni_eq(mime_type, "APPLICATION/X-JAVA", 18)))
	{
		URL* url = GetUrlAttr(ATTR_CODEBASE, NS_IDX_HTML, logdoc);
		if (url && url->Type() == URL_FILE && logdoc && logdoc->GetFramesDocument())
		{
			URLType doc_url_type = logdoc->GetFramesDocument()->GetURL().Type();
			if (doc_url_type != URL_FILE && doc_url_type != URL_EMAIL)
				return HE_OBJECT;
		}
	}
	return type;
}

#ifdef CSS_VIEWPORT_SUPPORT
CSS_ViewportMeta* HTML_Element::GetViewportMeta(const DocumentContext &context, BOOL create)
{
	// Sanity check.
	OP_ASSERT(Type() == HE_META);

	CSS_ViewportMeta* viewport_meta = static_cast<CSS_ViewportMeta*>(GetSpecialAttr(ATTR_VIEWPORT_META, ITEM_TYPE_COMPLEX, NULL, SpecialNs::NS_STYLE));
	if (!viewport_meta && create)
	{
		viewport_meta = OP_NEW(CSS_ViewportMeta, (this));
		if (viewport_meta)
		{
			if (SetSpecialAttr(ATTR_VIEWPORT_META, ITEM_TYPE_COMPLEX, viewport_meta, TRUE, SpecialNs::NS_STYLE) == -1)
			{
				OP_DELETE(viewport_meta);
				viewport_meta = NULL;
			}
		}

		if (!viewport_meta)
			context.hld_profile->SetIsOutOfMemory(TRUE);
	}

	return viewport_meta;
}
#endif // CSS_VIEWPORT_SUPPORT

/* static */
HTML_Element *HTML_Element::GetCommonAncestorActual(HTML_Element *left, HTML_Element *right)
{
	HTML_Element *tmp;

	if (left == right || !left || !right)
		return left;

	//1. First get tree height from each node
	unsigned path_height_left = 0, path_height_right = 0;
	tmp = left;
	while (tmp)
	{
		path_height_left++;
		tmp = tmp->ParentActual();
	}
	tmp = right;
	while (tmp)
	{
		path_height_right++;
		tmp = tmp->ParentActual();
	}

	//2. Climb up the difference in height from each
	//   two nodes so they get to the same level
	while (path_height_left > path_height_right)
	{
		path_height_left--;
		left = left->ParentActual();
	}
	while (path_height_right > path_height_left)
	{
		path_height_right--;
		right = right->ParentActual();
	}

	//3. Continue traversing upwards until they are the same, or NULL
	while (left != right)
	{
		OP_ASSERT(left && right); // The trees are the same height by now, this must uphold.
		left = left->ParentActual();
		right = right->ParentActual();
	}

	return left;
}

OP_STATUS
HE_AncestorToDescendantIterator::Init(HTML_Element *ancestor, HTML_Element *descendant, BOOL skip_ancestor)
{
	OP_ASSERT(descendant);
	OP_ASSERT(!ancestor || (ancestor && ancestor->IsAncestorOf(descendant)));

	m_path.Clear();
	for (; descendant; descendant = descendant->ParentActual())
	{
		if (skip_ancestor && ancestor == descendant)
			break;
		RETURN_IF_ERROR(m_path.Add(descendant));
		if (ancestor == descendant)
			break;
	};
	return OpStatus::OK;
}

HTML_Element *
HE_AncestorToDescendantIterator::GetNextActual()
{
	HTML_Element *current = NULL;
	if (m_path.GetCount() > 0)
	{
		current = m_path.Get(m_path.GetCount()-1);
		m_path.Remove(m_path.GetCount()-1);
	}
	return current;
}
#ifdef DRAG_SUPPORT
static BOOL IsInteractiveHtmlElement(HTML_Element* elem, FramesDocument* doc)
{
	return elem->IsMatchingType(Markup::HTE_INPUT, NS_HTML) || elem->IsMatchingType(Markup::HTE_SELECT, NS_HTML) ||
	       elem->IsMatchingType(Markup::HTE_TEXTAREA, NS_HTML) || elem->IsMatchingType(Markup::HTE_OPTION, NS_HTML) ||
	       elem->IsMatchingType(Markup::HTE_ISINDEX, NS_HTML) || elem->IsMatchingType(Markup::HTE_BUTTON, NS_HTML)
#ifdef DOCUMENT_EDIT_SUPPORT
	       || (doc && elem->IsContentEditable(TRUE) && (!doc->GetDocumentEdit()->m_layout_modifier.IsActive()
	       || doc->GetDocumentEdit()->m_layout_modifier.m_helm != elem))
#endif // DOCUMENT_EDIT_SUPPORT
	       ;
}

BOOL HTML_Element::IsDraggable(FramesDocument* doc)
{
	return GetDraggable(doc) != NULL;
}

HTML_Element* HTML_Element::GetDraggable(FramesDocument* doc)
{
	if (IsInteractiveHtmlElement(this, doc))
		return NULL;

	HTML_Element* elm = this;
	do
	{
		if (elm->GetNsType() == NS_HTML)
		{
			const uni_char* drag_attr = elm->GetStringAttr(Markup::HA_DRAGGABLE);
			if (drag_attr)
			{
				if (uni_stri_eq(drag_attr, "true"))
					return elm;
				else if (uni_stri_eq(drag_attr, "false"))
					return NULL;
			}

			if (elm->Type() == HE_IMG)
				return elm;
		}
#ifdef SVG_SUPPORT
		else if (elm->GetNsType() == NS_SVG)
		{
			if (doc && g_svg_manager->IsEditableElement(doc, elm))
				return NULL;

			const uni_char* drag_attr = elm->GetStringAttr(Markup::SVGA_DRAGGABLE, NS_IDX_SVG);
			if (drag_attr)
			{
				if (uni_stri_eq(drag_attr, "true"))
					return elm;
				else if (uni_stri_eq(drag_attr, "false"))
					return NULL;
			}
		}
#endif // SVG_SUPPORT

		// We need FramesDocument to be sure the element is draggable by default but if we don't have one
		// return TRUE for HTML <a href="url"> at least.
		if ((doc && elm->IsAnchorElement(doc)) ||
		    (elm->IsMatchingType(Markup::HTE_A, NS_HTML) && elm->HasAttr(Markup::HA_HREF)))
			return elm;

		elm = elm->ParentActual();
	}
	while (elm);

	return NULL;
}

const uni_char* HTML_Element::GetDropzone()
{
	DropzoneAttribute* dropzone_attr = NULL;
	if (GetNsType() == NS_HTML)
		dropzone_attr = static_cast<DropzoneAttribute*>(GetAttr(Markup::HA_DROPZONE, ITEM_TYPE_COMPLEX, NULL));
#ifdef SVG_SUPPORT
	else if (GetNsType() == NS_SVG)
		dropzone_attr = static_cast<DropzoneAttribute*>(GetAttr(Markup::SVGA_DROPZONE, ITEM_TYPE_COMPLEX, NULL, NS_IDX_SVG));
#endif // SVG_SUPPORT

	if (!dropzone_attr)
		return NULL;
	return dropzone_attr->GetAttributeString();
}

#endif // DRAG_SUPPORT

