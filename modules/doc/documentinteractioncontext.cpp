/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2002-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/doc/documentinteractioncontext.h"

#include "modules/logdoc/htm_elm.h"

#include "modules/doc/frm_doc.h"

#ifdef WIC_TEXTFORM_CLIPBOARD_CONTEXT
#include "modules/forms/formvalue.h"
#endif

#ifdef SVG_SUPPORT
#include "modules/svg/svg_image.h"
#include "modules/svg/SVGManager.h"
#endif // SVG_SUPPORT

#ifdef MEDIA_HTML_SUPPORT
#include "modules/media/mediaelement.h"
#endif // MEDIA_HTML_SUPPORT

#ifdef WIC_USE_DOWNLOAD_CALLBACK
#include "modules/windowcommander/src/TransferManagerDownload.h" // internal file in windowcommander but we have to use it since desktop is casting our objects to it. If we create URLInformation objects of some other taste, the cast will just cause crashes. This is one symptom of how incredibly broken our external download API:s are but we have had no time to create working API:s.
#endif // WIC_USE_DOWNLOAD_CALLBACK

#include "modules/viewers/viewers.h" // To be able to create the TransferManagerDownload object since it uses viewer constants in its constructor

DocumentInteractionContext::DocumentInteractionContext(FramesDocument* doc, const OpPoint& screen_pos, OpView* view, OpWindow* window, HTML_Element* html_elm)
	: m_doc(doc),
	  m_screen_pos(screen_pos),
	  // m_packed inited in the body of the constructor
	  m_html_element(html_elm),
	  m_view(view),
	  m_window(window)
#ifdef INTERNAL_SPELLCHECK_SUPPORT
	  , m_spell_session_id(0)
#endif // INTERNAL_SPELLCHECK_SUPPORT
{
	OP_STATIC_ASSERT(sizeof(m_packed) <= sizeof(m_packed_init));

	m_packed_init = 0;

	OP_ASSERT(m_packed.reserved == 0); // To make sure the whole struct was zeroed
}

DocumentInteractionContext::~DocumentInteractionContext()
{
	if (m_doc)
	{
		m_doc->UnregisterDocumentInteractionCtx(this);
	}
}

/* virtual */
OpDocumentContext* DocumentInteractionContext::CreateCopy()
{
	// Reference count to avoid this?
	DocumentInteractionContext* copy = OP_NEW(DocumentInteractionContext, (m_doc, m_screen_pos, m_view, m_window, m_html_element.GetElm()));
	if (!copy)
	{
		return NULL;
	}

	copy->m_image_element.SetElm(m_image_element.GetElm());

	// Copy all flags
	copy->m_packed_init = m_packed_init;
	copy->m_link_address = m_link_address;
	copy->m_image_address = m_image_address;
#ifdef INTERNAL_SPELLCHECK_SUPPORT
	copy->m_spell_session_id = m_spell_session_id;
#endif // INTERNAL_SPELLCHECK_SUPPORT

	if (OpStatus::IsError(copy->m_link_title.Set(m_link_title)))
	{
		OP_DELETE(copy);
		return NULL;
	}

	if (m_doc)
	{
		if (OpStatus::IsError(m_doc->RegisterDocumentInteractionCtx(copy)))
		{
			// Clear all references to pointers. Not very usable but maybe a little.
			copy->HandleFramesDocumentDeleted();
		}
	}

	return copy;
}

#ifdef WIC_TEXTFORM_CLIPBOARD_CONTEXT
/* virtual */
BOOL DocumentInteractionContext::IsTextFormElementInputTypePassword()
{
	if (!IsFormElement() || !HasEditableText())
		return FALSE;

	return m_html_element->GetInputType() == INPUT_PASSWORD;
}

/* virtual */
BOOL DocumentInteractionContext::IsTextFormElementInputTypeFile()
{
	return m_html_element.GetElm() &&
		m_html_element->IsMatchingType(HE_INPUT, NS_HTML) &&
		m_html_element->GetInputType() == INPUT_FILE;
}

/* virtual */
BOOL DocumentInteractionContext::IsTextFormMultiline()
{
	return m_html_element.GetElm() && m_html_element->IsMatchingType(HE_TEXTAREA, NS_HTML);
}

/* virtual */
BOOL DocumentInteractionContext::IsTextFormElementEmpty()
{
	if (!IsFormElement() || !HasEditableText())
		return TRUE;

	FormValue* form_value = m_html_element->GetFormValue();

	OpString text;

	if (!OpStatus::IsSuccess(form_value->GetValueAsText(m_html_element.GetElm(), text)))
		return TRUE;

	return !text.HasContent();
}
#endif

/* virtual */
OP_STATUS DocumentInteractionContext::GetLinkAddress(OpString* link_address)
{
    // Maybe it should be FALSE/NoRedirect?
    return m_link_address.GetAttribute(URL::KUniName_With_Fragment, *link_address, TRUE);
}

/* virtual */
OP_STATUS DocumentInteractionContext::GetLinkData(LinkDataType data_type, OpString* link_data)
{
	if (data_type == AddressForUI)
		return m_link_address.GetAttribute(URL::KUniName_With_Fragment, *link_data, FALSE);
	else if (data_type == AddressNotForUI)
		return m_link_address.GetAttribute(URL::KUniName_With_Fragment_Username_Password_NOT_FOR_UI, *link_data, FALSE);
	else if (data_type == AddressNotForUIEscaped)
		return m_link_address.GetAttribute(URL::KUniName_With_Fragment_Username_Password_Escaped_NOT_FOR_UI, *link_data, FALSE);
	else
		return OpStatus::ERR_NOT_SUPPORTED;
}

/* virtual */
OP_STATUS DocumentInteractionContext::GetLinkData(LinkDataType data_type, OpString8* link_data)
{
	if (data_type == AddressForUIEscaped)
		return m_link_address.GetAttribute(URL::KName_With_Fragment_Escaped, *link_data, FALSE);
	else
		return OpStatus::ERR_NOT_SUPPORTED;
}

/* virtual */
BOOL DocumentInteractionContext::HasBGImage()
{
	if (m_doc && !m_doc->GetBGImageURL().IsEmpty())
		return TRUE;
	else
		return FALSE;
}

/* virtual */
OP_STATUS DocumentInteractionContext::GetImageAddress(OpString* image_address)
{
	// kFollowRedirect makes sense here since it should be the url of the image
	// that is currently displayed.
	return m_image_address.GetAttribute(URL::KUniName_With_Fragment, *image_address, TRUE);
}

/* virtual */
OP_STATUS DocumentInteractionContext::GetBGImageAddress(OpString* image_address)
{
	if (!m_doc)
	{
		return OpStatus::ERR;
	}
	// kFollowRedirect makes sense here since it should be the url of the background image
	// that is currently displayed.
	return m_doc->GetBGImageURL().GetAttribute(URL::KUniName_With_Fragment, *image_address, TRUE);
}

/* virtual */
OP_STATUS DocumentInteractionContext::GetSuggestedImageFilename(OpString* filename)
{
	// kFollowRedirect makes sense here since it should be the url of the image
	// that is currently displayed.
	return m_image_address.GetAttribute(URL::KSuggestedFileName_L, *filename, TRUE);
}

/* virtual */
URL_CONTEXT_ID DocumentInteractionContext::GetImageAddressContext()
{
	return m_image_address.GetContextId();
}

/* virtual */
URL_CONTEXT_ID DocumentInteractionContext::GetBGImageAddressContext()
{
	if (!m_doc)
		return 0;
	return m_doc->GetBGImageURL().GetContextId();
}

/* virtual */
OP_STATUS DocumentInteractionContext::GetImageAltText(OpString* image_alt_text)
{
	if (m_image_element.GetElm())
	{
		const uni_char* alt_attribute = m_image_element->GetStringAttr(ATTR_ALT);
		return image_alt_text->Set(alt_attribute);
	}
	return OpStatus::OK;
}

/* virtual */
OP_STATUS DocumentInteractionContext::GetImageLongDesc(OpString* image_long_desc)
{
	if (m_image_element.GetElm())
	{
		const uni_char* longdesc_attribute = m_image_element->GetStringAttr(ATTR_LONGDESC);
		return image_long_desc->Set(longdesc_attribute);
	}
	return OpStatus::OK;
}

/* virtual */
BOOL DocumentInteractionContext::HasCachedImageData()
{
	if (!m_image_address.IsEmpty())
	{
		return m_image_address.GetAttribute(URL::KIsImage, TRUE) && m_image_address.GetAttribute(URL::KDataPresent, TRUE);
	}
	return FALSE;
}

#ifdef WEB_TURBO_MODE
BOOL DocumentInteractionContext::HasFullQualityImage()
{
	return (m_packed.has_image && (!m_image_address.GetAttribute(URL::KUsesTurbo) || !m_image_address.GetAttribute(URL::KTurboCompressed)));
}
#endif // WEB_TURBO_MODE

/* virtual */
BOOL DocumentInteractionContext::HasCachedBGImageData()
{
	if (m_doc)
	{
		URL url = m_doc->GetBGImageURL();
		return !url.IsEmpty() && url.GetAttribute(URL::KIsImage, TRUE);
	}
	return FALSE;
}

#ifdef SVG_SUPPORT
/* virtual */
BOOL DocumentInteractionContext::HasSVGAnimation()
{
	if (HasSVG())
		if (SVGImage* img = g_svg_manager->GetSVGImage(m_doc->GetLogicalDocument(), m_html_element.GetElm()))
			return img->HasAnimation();

	return FALSE;
}

/* virtual */
BOOL DocumentInteractionContext::IsSVGAnimationRunning()
{
	if (HasSVG())
		if (SVGImage* img = g_svg_manager->GetSVGImage(m_doc->GetLogicalDocument(), m_html_element.GetElm()))
			return img->IsAnimationRunning();

	return FALSE;
}

/* virtual */
BOOL DocumentInteractionContext::IsSVGZoomAndPanAllowed()
{
	if (HasSVG())
		if (SVGImage* img = g_svg_manager->GetSVGImage(m_doc->GetLogicalDocument(), m_html_element.GetElm()))
			return img->IsZoomAndPanAllowed();

	return FALSE;
}

#endif // SVG_SUPPORT

#ifdef MEDIA_HTML_SUPPORT

/* virtual */
BOOL DocumentInteractionContext::IsMediaPlaying()
{
	if (m_html_element.GetElm())
	{
		MediaElement* media_elm = m_html_element->GetMediaElement();
		if (media_elm)
			return !media_elm->GetPaused() && !media_elm->GetPlaybackEnded();
	}
	return FALSE;
}

/* virtual */
BOOL DocumentInteractionContext::IsMediaMuted()
{
	if (m_html_element.GetElm())
	{
		MediaElement* media_elm = m_html_element->GetMediaElement();
		if (media_elm)
			return media_elm->GetMuted();
	}
	return FALSE;
}

/* virtual */
BOOL DocumentInteractionContext::HasMediaControls()
{
	return m_html_element.GetElm() && m_html_element->HasAttr(Markup::HA_CONTROLS);
}

/* virtual */
BOOL DocumentInteractionContext::IsMediaAvailable()
{
	if (m_html_element.GetElm())
	{
		MediaElement* media_elm = m_html_element->GetMediaElement();
		if (media_elm)
			return media_elm->GetReadyState() != MediaReady::NOTHING;
	}
	return FALSE;
}

/* virtual */
OP_STATUS DocumentInteractionContext::GetMediaAddress(OpString* media_address)
{
	if (m_html_element.GetElm())
	{
		MediaElement* media_elm = m_html_element->GetMediaElement();
		if (media_elm)
			return media_elm->GetCurrentSrc()->GetAttribute(URL::KUniName_With_Fragment, *media_address, TRUE);
	}
	return OpStatus::ERR;
}

/* virtual */
URL_CONTEXT_ID DocumentInteractionContext::GetMediaAddressContext()
{
	if (m_html_element.GetElm())
	{
		MediaElement* media_elm = m_html_element->GetMediaElement();
		if (media_elm)
			return media_elm->GetCurrentSrc()->GetContextId();
	}
	return 0;
}
#endif // MEDIA_HTML_SUPPORT

#ifdef WIC_USE_DOWNLOAD_CALLBACK

/* virtual */
OP_STATUS DocumentInteractionContext::CreateImageURLInformation(URLInformation** url_info)
{
	OP_ASSERT(url_info);

	if (m_image_address.IsEmpty() || !m_doc)
	{
		return OpStatus::ERR;
	}

#ifdef WIC_USE_DOWNLOAD_CALLBACK
	ViewActionFlag dummy_flag;
	URLInformation* cui = OP_NEW(TransferManagerDownloadCallback, (m_doc->GetDocManager(), m_image_address, NULL, dummy_flag));
	if (!cui)
	{
		return OpStatus::ERR_NO_MEMORY;
	}

	*url_info = cui;
	return OpStatus::OK;
#else
	return OpStatus::ERR;
#endif // WIC_USE_DOWNLOAD_CALLBACK
}

/* virtual */
OP_STATUS DocumentInteractionContext::CreateBGImageURLInformation(URLInformation** url_info)
{
	OP_ASSERT(url_info);

	if (!m_doc || m_doc->GetBGImageURL().IsEmpty())
	{
		return OpStatus::ERR;
	}

#ifdef WIC_USE_DOWNLOAD_CALLBACK
	ViewActionFlag dummy_flag;
	URLInformation* cui = OP_NEW(TransferManagerDownloadCallback, (m_doc->GetDocManager(), m_doc->GetBGImageURL(), NULL, dummy_flag));
	if (!cui)
	{
		return OpStatus::ERR_NO_MEMORY;
	}

	*url_info = cui;
	return OpStatus::OK;
#else
	return OpStatus::ERR;
#endif // WIC_USE_DOWNLOAD_CALLBACK
}

/* virtual */
OP_STATUS DocumentInteractionContext::CreateDocumentURLInformation(URLInformation** url_info)
{
	OP_ASSERT(url_info);

	if (!m_doc)
	{
		return OpStatus::ERR;
	}

#ifdef WIC_USE_DOWNLOAD_CALLBACK
	ViewActionFlag dummy_flag;
	URLInformation* cui = OP_NEW(TransferManagerDownloadCallback, (m_doc->GetDocManager(), m_doc->GetURL(), NULL, dummy_flag));
	if (!cui)
	{
		return OpStatus::ERR_NO_MEMORY;
	}

	*url_info = cui;
	return OpStatus::OK;
#else
	return OpStatus::ERR;
#endif // WIC_USE_DOWNLOAD_CALLBACK
}

/* virtual */
OP_STATUS DocumentInteractionContext::CreateLinkURLInformation(URLInformation** url_info)
{
	OP_ASSERT(url_info);

	if (m_link_address.IsEmpty() || !m_doc)
	{
		return OpStatus::ERR;
	}

#ifdef WIC_USE_DOWNLOAD_CALLBACK
	ViewActionFlag dummy_flag;
	URLInformation* cui = OP_NEW(TransferManagerDownloadCallback, (m_doc->GetDocManager(), m_link_address, NULL, dummy_flag));
	if (!cui)
	{
		return OpStatus::ERR_NO_MEMORY;
	}

	*url_info = cui;
	return OpStatus::OK;
#else
	return OpStatus::ERR;
#endif // WIC_USE_DOWNLOAD_CALLBACK
}
#endif // WIC_USE_DOWNLOAD_CALLBACK

void DocumentInteractionContext::SetHasImage(URL& image_address, HTML_Element* image_element)
{
	m_image_address = image_address;
	if (image_element)
	{
		m_image_element.SetElm(image_element);
	}
	m_packed.has_image = TRUE;
}

