/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * spoon / Arjan van Leeuwen (arjanl)
 */

#ifndef ST_MENU_CONTEXT_H
#define ST_MENU_CONTEXT_H

#ifdef SELFTEST

#include "modules/windowcommander/OpWindowCommander.h"
#ifdef INTERNAL_SPELLCHECK_SUPPORT
#include "modules/spellchecker/opspellcheckerapi.h"
#endif // INTERNAL_SPELLCHECK_SUPPORT

/**
 * A dummy implementation of OpDocumentContext to prefill the class with arbitrary values.
 * The real implementation is DocumentInteractionContext (in doc)
 */
class ST_OpDocumentContext : public OpDocumentContext
{
public:
	ST_OpDocumentContext()
		: m_packed_init(0)
#ifdef INTERNAL_SPELLCHECK_SUPPORT 
		, m_spell_session_id(0)
#endif // INTERNAL_SPELLCHECK_SUPPORT 
	{}
protected:
	ST_OpDocumentContext(unsigned int packed_init
#ifdef INTERNAL_SPELLCHECK_SUPPORT 
		, int spell_session_id
#endif // INTERNAL_SPELLCHECK_SUPPORT 
		)
		: m_packed_init(packed_init)
#ifdef INTERNAL_SPELLCHECK_SUPPORT 
		, m_spell_session_id(spell_session_id)
#endif // INTERNAL_SPELLCHECK_SUPPORT 
	{}
public:
	virtual ~ST_OpDocumentContext() {}

	virtual OpDocumentContext* CreateCopy()
	{
#ifdef INTERNAL_SPELLCHECK_SUPPORT 
		return OP_NEW(ST_OpDocumentContext, (m_packed_init, m_spell_session_id));
#else
		return OP_NEW(ST_OpDocumentContext, (m_packed_init));
#endif // INTERNAL_SPELLCHECK_SUPPORT
	}

	virtual OpPoint GetScreenPosition() { return OpPoint(0, 0); }
	virtual BOOL HasLink() { return m_packed.has_link; }
	virtual BOOL HasImage() { return m_packed.has_image; }
	virtual BOOL HasBGImage() { return FALSE; }
	virtual BOOL HasTextSelection() { return m_packed.has_text_selection; }
	virtual BOOL HasMailtoLink() { return m_packed.has_mailto_link; }
	virtual BOOL HasEditableText() { return m_packed.has_editable_text; }
	virtual BOOL HasKeyboardOrigin() { return FALSE; }
	virtual BOOL IsFormElement() { return FALSE; }
	virtual BOOL IsReadonly() { return FALSE; }

#ifdef SVG_SUPPORT
	virtual BOOL HasSVG() { return m_packed.has_SVG; }

# ifdef DOC_CAP_SVG_CONTEXTMENU_ADDITIONS
	virtual BOOL HasSVGAnimation() { return FALSE; }
	virtual BOOL IsSVGAnimationRunning() { return FALSE; }
	virtual BOOL IsSVGZoomAndPanAllowed() { return FALSE; }
# endif // DOC_CAP_SVG_CONTEXTMENU_ADDITIONS
#endif // SVG_SUPPORT

#ifdef DOC_CAP_MEDIA_CONTEXT_MENU
	virtual BOOL IsVideo() { return m_packed.has_video; }
	virtual BOOL HasMedia() { return m_packed.has_audio; }
	virtual BOOL IsMediaPlaying() { return FALSE; }
	virtual BOOL IsMediaMuted() { return FALSE; }
	virtual BOOL HasMediaControls() { return FALSE; }
	virtual BOOL IsMediaAvailable() { return FALSE; }
	virtual OP_STATUS GetMediaAddress(OpString* media_address) { return OpStatus::ERR_NOT_SUPPORTED; }
	virtual URL_CONTEXT_ID GetMediaAddressContext() { return 0; }
#endif // DOC_CAP_MEDIA_CONTEXT_MENU

	virtual BOOL HasPlugin() { return FALSE; }

#ifdef WIC_TEXTFORM_CLIPBOARD_CONTEXT
	virtual BOOL IsTextFormElementInputTypePassword() { return FALSE; }
	virtual BOOL IsTextFormElementEmpty() { return FALSE; }
#endif // WIC_TEXTFORM_CLIPBOARD_CONTEXT

	virtual OP_STATUS GetSelectedText(OpString* selected_text) { return OpStatus::ERR_NOT_SUPPORTED; }
	virtual OP_STATUS GetLinkAddress(OpString* link_address) { return OpStatus::ERR_NOT_SUPPORTED; }

#ifdef WIC_DOCCTX_LINK_DATA
	virtual OP_STATUS GetLinkTitle(OpString* link_title) { return OpStatus::ERR_NOT_SUPPORTED; }
	virtual OP_STATUS GetLinkText(OpString* link_text) { return OpStatus::ERR_NOT_SUPPORTED; }
#endif // WIC_DOCCTX_LINK_DATA

#ifdef DOC_CAP_HAS_HANDLEMIDDLECLICK
	virtual OP_STATUS GetLinkData(LinkDataType data_type, OpString* link_data) { return OpStatus::ERR_NOT_SUPPORTED; }
	virtual OP_STATUS GetLinkData(LinkDataType data_type, OpString8* link_data) { return OpStatus::ERR_NOT_SUPPORTED; }
#endif // DOC_CAP_HAS_HANDLEMIDDLECLICK

	virtual OP_STATUS GetLinkTitle(OpString* link_title) { return OpStatus::ERR_NOT_SUPPORTED; }
	virtual OP_STATUS GetImageAddress(OpString* image_address) { return OpStatus::ERR_NOT_SUPPORTED; }
	virtual OP_STATUS GetBGImageAddress(OpString* image_address) { return OpStatus::ERR_NOT_SUPPORTED; }
	virtual OP_STATUS GetSuggestedImageFilename(OpString* filename) { return OpStatus::ERR_NOT_SUPPORTED; }
	virtual URL_CONTEXT_ID GetImageAddressContext() { return 0; }
	virtual URL_CONTEXT_ID GetBGImageAddressContext() { return 0; }
	virtual OP_STATUS GetImageAltText(OpString* image_alt_text) { return OpStatus::ERR_NOT_SUPPORTED; }
	virtual OP_STATUS GetImageLongDesc(OpString* image_longdesc) { return OpStatus::ERR_NOT_SUPPORTED; }
	virtual BOOL HasCachedImageData() { return FALSE; }
	virtual BOOL HasCachedBGImageData() { return FALSE; }

	virtual BOOL HasFullQualityImage() { return m_packed.has_full_quality_image; }

	virtual OpView* GetView() { return NULL; }

#ifdef INTERNAL_SPELLCHECK_SUPPORT
	virtual int GetSpellSessionId() { return m_spell_session_id; }
#endif // INTERNAL_SPELLCHECK_SUPPORT

#ifdef WIC_USE_DOWNLOAD_CALLBACK
	virtual OP_STATUS CreateImageURLInformation(URLInformation** url_info) { return OpStatus::ERR_NOT_SUPPORTED; }
	virtual OP_STATUS CreateBGImageURLInformation(URLInformation** url_info) { return OpStatus::ERR_NOT_SUPPORTED; }
	virtual OP_STATUS CreateDocumentURLInformation(URLInformation** url_info) { return OpStatus::ERR_NOT_SUPPORTED; }
	virtual OP_STATUS CreateLinkURLInformation(URLInformation** url_info) { return OpStatus::ERR_NOT_SUPPORTED; }
#endif // WIC_USE_DOWNLOAD_CALLBACK

	void SetHasTextSelection(BOOL has_text_selection) { m_packed.has_text_selection = has_text_selection; }
	void SetHasLink(BOOL has_link) { m_packed.has_link = has_link; }
	void SetIsInDocument(BOOL is_in_document) { m_packed.is_in_document = is_in_document; }
	void SetHasMailtoLink(BOOL has_mailto_link) { m_packed.has_mailto_link = has_mailto_link; }
	void SetHasImage(BOOL has_image) { m_packed.has_image = has_image; }
#ifdef DOC_CAP_MEDIA_CONTEXT_MENU
	void SetHasVideo(BOOL has_video) { m_packed.has_video = has_video; }
	void SetHasAudio(BOOL has_audio) { m_packed.has_audio = has_audio; }
#endif // DOC_CAP_MEDIA_CONTEXT_MENU
#ifdef SVG_SUPPORT
	void SetHasSVG(BOOL has_SVG) { m_packed.has_SVG = has_SVG; }
#endif // SVG_SUPPORT
	void SetHasEditableText(BOOL has_editable_text) { m_packed.has_editable_text = has_editable_text; }
#ifdef INTERNAL_SPELLCHECK_SUPPORT
	void SetSpellSessionID(int spell_session_id) { m_spell_session_id = spell_session_id; }
#endif // INTERNAL_SPELLCHECK_SUPPORT
	void SetHasFullQualityImage(BOOL has_full_quality_image) { m_packed.has_full_quality_image = has_full_quality_image; }

private:
	union { 
		struct	{
			unsigned int has_text_selection:1; 
			unsigned int has_link:1; 
			unsigned int is_in_document:1; 
			unsigned int has_mailto_link:1; 
			unsigned int has_image:1;
#ifdef DOC_CAP_MEDIA_CONTEXT_MENU
			unsigned int has_video:1; 
			unsigned int has_audio:1; 
#endif // DOC_CAP_MEDIA_CONTEXT_MENU
#ifdef SVG_SUPPORT
			unsigned int has_SVG:1; 
#endif // SVG_SUPPORT
			unsigned int has_editable_text:1; 
			unsigned int has_full_quality_image:1;
		}				m_packed; 
		unsigned int	m_packed_init; 
	}; 

#ifdef INTERNAL_SPELLCHECK_SUPPORT 
	int		m_spell_session_id;
#endif // INTERNAL_SPELLCHECK_SUPPORT 
};

class ST_ContextMenuResolver : public ContextMenuResolver
{
#ifdef INTERNAL_SPELLCHECK_SUPPORT
public:
	ST_ContextMenuResolver()
		: m_has_spellcheck_word(FALSE)
		, m_can_disable_spellcheck(FALSE)
	{}

	ST_ContextMenuResolver(BOOL has_spellcheck_word, BOOL can_disable_spellcheck)
		: m_has_spellcheck_word(has_spellcheck_word)
		, m_can_disable_spellcheck(can_disable_spellcheck)
	{}

protected:
	virtual BOOL HasSpellCheckWord(int spell_session_id) { return m_has_spellcheck_word; }
	virtual BOOL CanDisableSpellcheck(int spell_session_id) { return m_can_disable_spellcheck; }

private:
	BOOL m_has_spellcheck_word;
	BOOL m_can_disable_spellcheck;
#endif // INTERNAL_SPELLCHECK_SUPPORT
};


class ST_StubURLInformation : public URLInformation
{
public:

	virtual DoneListener *						SetDoneListener(DoneListener * listener) { return NULL; }
	virtual const uni_char*						URLName() { return NULL; }
	virtual const uni_char*						SuggestedFilename() { return NULL; }
	virtual const char *						MIMEType() { return NULL; }
	virtual OP_STATUS							SetMIMETypeOverride(const char *mime_type) { return OpStatus::ERR_NOT_SUPPORTED; }
	virtual long								Size() { return 0; }
	virtual const uni_char *					SuggestedSavePath() { return NULL; }
	virtual const uni_char *					HostName() { return NULL; }
	virtual OP_STATUS							DownloadDefault(DownloadContext * context, const uni_char * downloadpath = NULL) { return OpStatus::ERR_NOT_SUPPORTED; }
	virtual OP_STATUS							DownloadTo(DownloadContext * context, const uni_char * downloadpath = NULL) { return OpStatus::ERR_NOT_SUPPORTED; }
	virtual URL_ID								GetURL_Id() { return 0; }
	virtual void								ReleaseDocument() {}
	virtual void								URL_Information_Done() {}
    virtual URLInformation::GadgetContentType   GetGadgetContentType() { return URLInformation::URLINFO_GADGET_INSTALL_CONTENT; }
    virtual const uni_char*                     URLUniFullName() { return NULL; }
#ifdef SECURITY_INFORMATION_PARSER
	virtual OP_STATUS							CreateSecurityInformationParser(OpSecurityInformationParser** parser) { return OpStatus::ERR_NOT_SUPPORTED; }
	virtual int									GetSecurityMode() { return 0; }
	virtual int									GetTrustMode() { return 0; }
	virtual UINT32								SecurityLowStatusReason() { return 0; }
	virtual const uni_char*						ServerUniName() const { return NULL; }
#endif // SECURITY_INFORMATION_PARSER
#ifdef TRUST_INFO_RESOLVER
	virtual OP_STATUS							CreateTrustInformationResolver(OpTrustInformationResolver** resolver, OpTrustInfoResolverListener * listener) { return OpStatus::ERR_NOT_SUPPORTED; }
#endif // TRUST_INFO_RESOLVER
};

#endif // SELFTEST

#endif // ST_MENU_CONTEXT_H
