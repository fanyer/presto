/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef CSS_FONTFACE_RULE_H
#define CSS_FONTFACE_RULE_H

#include "modules/style/src/css_ruleset.h"
#include "modules/style/css_property_list.h"
#include "modules/style/css_webfont.h"
#include "modules/doc/externalinlinelistener.h"
#include "modules/svg/svg_workplace.h"
#include "modules/url/url_loading.h"

class CSS_FontfaceRule : public CSS_DeclarationBlockRule
	, public CSS_WebFont
	, public ExternalInlineListener
#ifdef SVG_SUPPORT
	, public SVGWorkplace::LoadListener
#endif // SVG_SUPPORT
{
public:
	CSS_FontfaceRule(CSS_property_list* props) : CSS_DeclarationBlockRule(props),
												 m_used_element(NULL), m_packed_init(0), m_src_idx(-1), m_font_ref(0)
	{}

	virtual Type GetType() { return FONT_FACE; }

	virtual OP_STATUS GetCssText(CSS* stylesheet, TempBuffer* buf, unsigned int indent_level = 0);
	virtual CSS_PARSE_STATUS SetCssText(HLDocProfile* hld_prof, CSS* stylesheet, const uni_char* text, int len);

	OP_STATUS CreateCSSFontDescriptor(OpFontInfo** out_fontinfo);
	virtual void RemovedWebFont(FramesDocument* doc);
	virtual const uni_char* GetFamilyName();
	virtual short GetStyle();
	virtual short GetWeight();
	virtual URL GetSrcURL() { return m_used_url; }
	virtual CSS_WebFont::Format GetFormat() { return static_cast<CSS_WebFont::Format>(m_packed.used_format); }
	virtual LoadStatus GetLoadStatus();
	virtual OP_STATUS SetLoadStatus(FramesDocument* doc, LoadStatus status);
	virtual OP_STATUS Load(FramesDocument* doc);
	virtual BOOL IsLocalFont() { return m_packed.is_local; }

	/** ExternalInlineListener API */
	virtual void LoadingProgress(const URL& url) {}
	virtual void LoadingStopped(FramesDocument* doc, const URL& url);
	virtual void LoadingRedirected(const URL& from, const URL& to);

	/** SVGWorkplace::LoadListener API */
	virtual void OnLoad(FramesDocument* doc, URL url, HTML_Element* resolved_element);
	virtual void OnLoadingFailed(FramesDocument* doc, URL url);

private:

	/** Called from Load to start loading first src url, or from LoadingStopped
		if the previous url could not be loaded. */
	OP_STATUS LoadNextInSrc(FramesDocument* doc);

	/** Get next url to load in the src descriptor. */
	OP_STATUS GetNextInSrc(FramesDocument* doc);

	/** The url from the src descriptor which is being used. */
	URL m_used_url;

	/** Guard-object used to prevent concurrent reloads from wiping
		out the font-file. Should be set across the loading and use of
		a resource. */
	IAmLoadingThisURL m_loading_url;

	HTML_Element* m_used_element;

	union
	{
		struct
		{
			unsigned int used_format:7; /** The format (CSS_WebFont::Format) of the font from the src descriptor which is being used. */
			unsigned int is_local:1;
			/** The load status of the current font (CSS_WebFont::LoadStatus).
				Initially WEBFONT_NOTLOADED.
				WEBFONT_LOADING if we're in the process of loading a src url. m_src_num will indicate which we're loading.
				WEBFONT_LOADED if we've successfully loaded a src url. m_src_num will say which.
				WEBFONT_NOTFOUND if we could not load any of the resources in the src descriptor. */
			unsigned int load_status:2;
			/**
			   set to TRUE when in format-mismatch mode - if loading
			   of the font failed with the specified format, the other
			   (sfnt vs svg) codepath is tried. this is used to avoid
			   infinite bouncing between the two codepaths.
			 */
			unsigned int format_mismatch:1;
		} m_packed; // 11 bits
		unsigned int m_packed_init;
	};

	/** The index into the generic value array that points to the element before the next resource to use. */
	short m_src_idx;

	OpFontManager::OpWebFontRef m_font_ref;
};

#endif // CSS_FONTFACE_RULE_H
