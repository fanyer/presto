/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2002-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef DOCUMENTINTERACTIONCONTEXT_H
#define DOCUMENTINTERACTIONCONTEXT_H

#include "modules/windowcommander/OpWindowCommander.h" // OpDocumentContext
#include "modules/util/opstring.h" // OpString
#include "modules/logdoc/elementref.h"

class FramesDocument;
class OpView;
class OpWindow;

class DocumentInteractionContext : public OpDocumentContext
{
	// Might be NULL in case the document has gone away
	FramesDocument* m_doc;

	OpPoint m_screen_pos;
	union
	{
		struct
		{
			unsigned has_link:1;
			unsigned has_image:1;
			unsigned has_text_selection:1;
			unsigned has_mailto_link:1;
			unsigned has_svg:1;
			unsigned has_editable_text:1;
			unsigned has_media:1;
			unsigned has_plugin:1;
			unsigned has_keyboard_origin:1;
			unsigned is_form_element:1;
			unsigned is_readonly:1;
			unsigned is_video:1;

			unsigned reserved:20; // Documenting how many free bits we have
		} m_packed;

		int m_packed_init;
	};

	URL m_link_address;
	OpString m_link_title;
	AutoNullElementRef m_image_element;
	AutoNullElementRef m_html_element;
	URL m_image_address; // If we have m_image_element we don't really need to keep the URL too. FIX?

	OpView* m_view;
	OpWindow* m_window;

#ifdef INTERNAL_SPELLCHECK_SUPPORT
	int m_spell_session_id;
#endif // INTERNAL_SPELLCHECK_SUPPORT


public:
	/**
	 * Creates a pretty much empty DocumentInteractionContext. It is not registered with the document. That has to be done by the caller.
	 *
	 * @param[in] doc The document.
	 *
	 * @param[in] screen_pos The position in screen coordinates for where this
	 * context was initiated. In the case of a mouse intiated context menu that
	 * is the position of the mouse click.
	 *
	 * @param[in] html_elm The HTML_Element that is the current focus of the OpDocumentContext. Typically
	 * the clicked element or the element with the caret or the focused element
	 * depending on why this object is created. May be NULL if it's not possible to connect this object
	 * to an HTML_Element.
	 */
	DocumentInteractionContext(FramesDocument* doc, const OpPoint& screen_pos, OpView* view, OpWindow* window, HTML_Element* html_elm);

	~DocumentInteractionContext();

	void HandleFramesDocumentDeleted()
	{
		m_doc = NULL;
		m_window = NULL;
		m_view = NULL;

		m_image_element.Reset();
		m_html_element.Reset();
	}

	// START OpDocumentContext interface

	virtual OpDocumentContext* CreateCopy();

	virtual OpPoint GetScreenPosition() { return m_screen_pos; }
	virtual BOOL HasLink() { return m_packed.has_link; }
	virtual BOOL HasImage() { return m_packed.has_image; }
	virtual BOOL HasBGImage();
	virtual BOOL HasTextSelection() { return m_packed.has_text_selection; }
	virtual BOOL HasMailtoLink() { return m_packed.has_mailto_link; }

	virtual BOOL HasSVG() { return m_packed.has_svg; }
#ifdef SVG_SUPPORT
	virtual BOOL HasSVGAnimation();
	virtual BOOL IsSVGAnimationRunning();
	virtual BOOL IsSVGZoomAndPanAllowed();
#endif // SVG_SUPPORT

	virtual BOOL HasPlugin() { return m_packed.has_plugin; }

#ifdef MEDIA_HTML_SUPPORT
	virtual BOOL HasMedia() { return m_packed.has_media; }
	virtual BOOL IsVideo() { return m_packed.is_video; }
	virtual BOOL IsMediaPlaying();
	virtual BOOL IsMediaMuted();
	virtual BOOL HasMediaControls();
	virtual BOOL IsMediaAvailable();

	virtual OP_STATUS GetMediaAddress(OpString* media_address);
	virtual URL_CONTEXT_ID GetMediaAddressContext();
#endif // MEDIA_HTML_SUPPORT

	virtual BOOL HasEditableText() { return m_packed.has_editable_text; }

	virtual BOOL HasKeyboardOrigin() { return m_packed.has_keyboard_origin; }

	virtual BOOL IsFormElement() { return m_packed.is_form_element; }
#ifdef WIC_TEXTFORM_CLIPBOARD_CONTEXT
	virtual BOOL IsTextFormElementInputTypePassword();
	virtual BOOL IsTextFormElementInputTypeFile();
	virtual BOOL IsTextFormElementEmpty();
	virtual BOOL IsTextFormMultiline();
#endif

	virtual BOOL IsReadonly() { return m_packed.is_readonly; }

	virtual OP_STATUS GetLinkAddress(OpString* link_address);
	virtual OP_STATUS GetLinkData(LinkDataType data_type, OpString* link_data);
	virtual OP_STATUS GetLinkData(LinkDataType data_type, OpString8* link_data);
	virtual OP_STATUS GetLinkTitle(OpString* link_title) { return link_title->Set(m_link_title); }
	virtual OP_STATUS GetImageAddress(OpString* image_address);
	virtual OP_STATUS GetBGImageAddress(OpString* image_address);
	virtual OP_STATUS GetSuggestedImageFilename(OpString* filename);
	virtual URL_CONTEXT_ID GetImageAddressContext();
	virtual URL_CONTEXT_ID GetBGImageAddressContext();
	virtual OP_STATUS GetImageAltText(OpString* image_alt_text);
	virtual OP_STATUS GetImageLongDesc(OpString* image_long_desc);
	virtual BOOL HasCachedImageData();
#ifdef WEB_TURBO_MODE
	virtual BOOL HasFullQualityImage();
#endif // WEB_TURBO_MODE
	virtual BOOL HasCachedBGImageData();
	virtual OpView* GetView() { return m_view; }
	virtual OpWindow* GetWindow() { return m_window; }

#ifdef INTERNAL_SPELLCHECK_SUPPORT
	/**
	 * Returns the spell session id related to this document position.
	 * Can be used by the platform to create an OpSpellUiSession object
	 * with OpSpellUiSession::Create.
	 */
	virtual int GetSpellSessionId() { return m_spell_session_id; }
#endif // INTERNAL_SPELLCHECK_SUPPORT

#ifdef WIC_USE_DOWNLOAD_CALLBACK
	virtual OP_STATUS CreateImageURLInformation(URLInformation** url_info);

	virtual OP_STATUS CreateBGImageURLInformation(URLInformation** url_info);

	virtual OP_STATUS CreateDocumentURLInformation(URLInformation** url_info);

	virtual OP_STATUS CreateLinkURLInformation(URLInformation** url_info);
#endif // WIC_USE_DOWNLOAD_CALLBACK

	// END OpDocumentContext interface

	OP_STATUS SetHasLink(URL& link_address, const uni_char* link_title)
	{
		m_link_address = link_address;
		RETURN_IF_ERROR(m_link_title.Set(link_title));
		m_packed.has_link = TRUE;
		return OpStatus::OK;
	}

	/**
	 * @param[in] image_element Can be NULL but in that case
	 * some methods related to images won't work.
	 */
	void SetHasImage(URL& image_address, HTML_Element* image_element);

	/** Doc internal method  */
	void SetHasTextSelection()
	{
		m_packed.has_text_selection = TRUE;
	}

	/** Doc internal method  */
	// FIXME: This should be obvious in the SetHasLink function.
	void SetHasMailtoLink() { m_packed.has_mailto_link = TRUE; }

	/** Doc internal method  */
	void SetHasSVG() { m_packed.has_svg = TRUE; }

	/** Doc internal method  */
	void SetHasEditableText() { m_packed.has_editable_text = TRUE; }

	/** Doc internal method  */
	void SetHasKeyboardOrigin() { m_packed.has_keyboard_origin = TRUE; }

	/** Doc internal method  */
	void SetIsFormElement() { m_packed.is_form_element = TRUE; }

	/** Doc internal method  */
	void SetIsReadonly() { m_packed.is_readonly = TRUE; }

#ifdef INTERNAL_SPELLCHECK_SUPPORT
	/** Doc internal method  */
	void SetSpellSessionId(int spell_session_id) { m_spell_session_id = spell_session_id; }
#endif // INTERNAL_SPELLCHECK_SUPPORT

#ifdef MEDIA_HTML_SUPPORT
	void SetHasMedia() { m_packed.has_media = TRUE; }
	void SetIsVideo() { m_packed.is_video = TRUE; }
#endif // MEDIA_HTML_SUPPORT

#ifdef _PLUGIN_SUPPORT_
	void SetHasPlugin() { m_packed.has_plugin = TRUE; }
#endif
	/** Doc internal method  */
	URL GetImageURL() { OP_ASSERT(HasImage()); return m_image_address; }

	/** Doc internal method  */
	HTML_Element* GetImageElement() { OP_ASSERT(HasImage()); return m_image_element.GetElm(); }

	/**
	 * Doc internal method
	 *.Get the HTML_Element that was connected to this object. Typically
	 * the clicked element or the element with the caret or the focused element depending on
	 * why this was created. Might be NULL.
	 */
	HTML_Element* GetHTMLElement() { return m_html_element.GetElm(); }
	/**
	 * Doc internal method
	 * Get the FramesDocument that was connected to this object. Might be NULL.
	 */
	FramesDocument* GetDocument() { return m_doc; }
};

#endif // !DOCUMENTINTERACTIONCONTEXT_H
