/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2005-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef _SVG_MANAGER_
#define _SVG_MANAGER_

#ifdef SVG_SUPPORT

#include "modules/svg/SVGManager.h"
#include "modules/svg/src/svgpch.h"
#include "modules/svg/src/SVGVector.h"
#include "modules/svg/src/SVGGlyphCache.h"
#include "modules/svg/src/SVGAverager.h"
#include "modules/svg/src/SVGErrorReport.h"
#include "modules/svg/src/SVGCache.h"
#include "modules/svg/src/SVGMatrix.h"
#include "modules/svg/src/SVGInternalEnumTables.h"
#include "modules/svg/src/svgmediamanager.h"

#include "modules/util/adt/opvector.h"
#include "modules/util/tempbuf.h"

#include "modules/svg/src/animation/svganimationworkplace.h"

class VisualDevice;
class SVGDocumentContext;
class SVGRenderer;
class HTML_Element;
class URL;
class SVGNumberPair;
class SVGSystemFontManager;
class SVGEmbeddedFontData;
class SVGFontInfo;
class HTMLayoutProperties;
class DOM_Environment;

class TreeIntersection;

class SVGManagerImpl : public SVGManager
{
private:
	SVGCache					m_cache;
	OpAutoVector<OpFont>        m_svg_opfonts;	// Systemfont fallbacks are here
	OpVector<OpFont>			m_current_svgfonts; ///< Registered SVG fonts are here
	SVGGlyphCache				m_glyph_cache;		///< Caches glyph outlines

	TempBuffer                  m_temp_buffer;

	SVGAverager                 m_time_averager;

#ifdef SVG_SUPPORT_EMBEDDED_FONTS
	SVGSystemFontManager*       m_system_font_manager;
#endif // SVG_SUPPORT_EMBEDDED_FONTS

#ifdef SVG_SUPPORT_MEDIA
	SVGMediaManager				m_media_manager;
#endif // SVG_SUPPORT_MEDIA

#ifdef SVG_SUPPORT_TEXTSELECTION
	BOOL					m_is_in_textselection_mode;
#endif // SVG_SUPPORT_TEXTSELECTION

	BOOL					m_get_presentation_values;

	int						m_imageref_id_counter;

#ifdef SVG_SUPPORT_ANIMATION_LISTENER
	struct AnimationListenerItem : public Link
	{
		BOOL has_ownership;
		SVGAnimationWorkplace::AnimationListener *listener;
	};
	Head animation_listeners;
#endif // SVG_SUPPORT_ANIMATION_LISTENER

  	OP_STATUS				Create();

	friend class SvgModule;

  	SVGManagerImpl();
  	~SVGManagerImpl();

	/**
	 * Handle one SVGLoad event sent to the root element of a svg
	 * fragment. The event is then sent to all children in pre-order,
	 * the children receive the event before its parent.
	 */
	OP_STATUS				HandleSVGLoad(SVGDocumentContext* doc_ctx);
	OP_STATUS				HandleSVGEvent(DOM_EventType event_type, SVGDocumentContext* doc_ctx, HTML_Element* target);

	/**
	 * Handle events sent to the document rather than to a
	 * element.
	 */
	OP_BOOLEAN				HandleDocumentEvent(const SVGManager::EventData& data);

	/**
	 * Send a SVG*-event to a specified element. This function
	 * switches between two methods of sending the event. If scripting
	 * is enabled, the DOM environment is non-NULL and handles the
	 * event. Otherwise the SVG module handles the event
	 * internally. The animation workplace then handles the event.
	 */
	OP_STATUS				SendSVGEvent(SVGDocumentContext* doc_ctx,
										 DOM_Environment* dom_environment,
										 DOM_EventType eventtype,
										 HTML_Element* element);

	/**
	 * Find a element in a svg using a intersection point
	 *
	 * @param tree_isect Intersection object
	 * @param intersection_point The intersection point in user coordinates
	 */
	HTML_Element*           FindTopMostElement(TreeIntersection& tree_isect, const SVGNumberPair& intersection_point);

  public:

	OP_BOOLEAN				FindTextPosition(SVGDocumentContext* doc_ctx, const SVGNumberPair& intersection_point, SelectionBoundaryPoint& out_pos);

	virtual BOOL			IsInTextSelectionMode() const {
#ifdef SVG_SUPPORT_TEXTSELECTION
								return m_is_in_textselection_mode;
#else
								return FALSE;
#endif // SVG_SUPPORT_TEXTSELECTION
	}

	virtual OP_BOOLEAN		ParseAttribute(HTML_Element* felement, SVG_DOCUMENT_CLASS* doc, int attr_name,
										   int ns_idx, const uni_char* str, unsigned strlen, void** outValue, ItemType &outValueType);
	OP_STATUS				ParseAttribute(HTML_Element* element, SVG_DOCUMENT_CLASS* doc, SVGObjectType object_type, int attr_name,
										   int ns_idx, const uni_char* str, unsigned strlen, SVGObject** result);
	OP_STATUS				ParseSVGAttribute(HTML_Element* element, SVG_DOCUMENT_CLASS* doc, SVGObjectType object_type,
											  Markup::AttrType attr_type, const uni_char* str, unsigned strlen, SVGObject **result);
	OP_STATUS				ParseXLinkAttribute(HTML_Element* element, SVG_DOCUMENT_CLASS* doc, SVGObjectType object_type,
												Markup::AttrType attr_type, const uni_char* str, unsigned strlen, SVGObject **result);

	virtual OP_BOOLEAN		FindSVGElement(HTML_Element* element, SVG_DOCUMENT_CLASS *doc, int x, int y, HTML_Element** target, int& offset_x, int& offset_y);
	virtual OP_BOOLEAN      NavigateToElement(HTML_Element *elm, SVG_DOCUMENT_CLASS *doc, URL** outurl, DOM_EventType event = ONCLICK, const uni_char** window_target = NULL, ShiftKeyState modifiers = SHIFTKEY_NONE);
	virtual void			InvalidateCaches(HTML_Element* root, SVG_DOCUMENT_CLASS *doc = NULL);
	virtual OP_STATUS		StartAnimation(HTML_Element* root, SVG_DOCUMENT_CLASS* doc);
	virtual OP_STATUS		StopAnimation(HTML_Element* root,  SVG_DOCUMENT_CLASS* doc);
	virtual OP_STATUS		PauseAnimation(HTML_Element* root,  SVG_DOCUMENT_CLASS* doc);
	virtual OP_STATUS		UnpauseAnimation(HTML_Element* root,  SVG_DOCUMENT_CLASS* doc);
#ifdef ACCESS_KEYS_SUPPORT
	virtual OP_STATUS       HandleAccessKey(SVG_DOCUMENT_CLASS* doc, HTML_Element* element, uni_char access_key);
#endif // ACCESS_KEYS_SUPPORT
	virtual SVGContext*		CreateDocumentContext(HTML_Element* root, LogicalDocument* logdoc);

	virtual OP_STATUS       CreatePath(SVGPath** path);
	virtual BOOL			OnInputAction(OpInputAction* action, HTML_Element* clicked_element, SVG_DOCUMENT_CLASS *doc);
	virtual OP_STATUS		OnSVGDocumentChanged(SVG_DOCUMENT_CLASS *doc, HTML_Element *parent, HTML_Element *child, BOOL is_addition);
	virtual OP_STATUS		HandleStyleChange(SVG_DOCUMENT_CLASS *doc, HTML_Element* element, unsigned int changes);
	virtual OP_STATUS		HandleSVGAttributeChange(SVG_DOCUMENT_CLASS *doc, HTML_Element*, Markup::AttrType attr, NS_Type ns, BOOL was_removed);
	virtual OP_STATUS		HandleCharacterDataChanged(SVG_DOCUMENT_CLASS *doc, HTML_Element* element);
	virtual OP_STATUS		HandleInlineIgnored(SVG_DOCUMENT_CLASS *doc, HTML_Element* element);
	virtual OP_STATUS		HandleInlineChanged(SVG_DOCUMENT_CLASS *doc, HTML_Element* element, BOOL is_content_changed);
	virtual OP_STATUS		RepaintElement(SVG_DOCUMENT_CLASS* doc, HTML_Element* element);

	virtual HTML_Element*	GetEventTarget(HTML_Element* svg_elm);
	virtual	CursorType		GetCursorForElement(HTML_Element* elm);
	virtual URL*			GetXLinkURL(HTML_Element* elm, URL *root_url = NULL);

	virtual BOOL			IsVisible(HTML_Element* element);

	virtual BOOL			IsTextContentElement(HTML_Element* element);

	virtual SVGImage*		GetSVGImage(LogicalDocument* log_doc,
										HTML_Element* svg_elm);

	virtual OP_STATUS		GetToolTip(HTML_Element* elm, OpString& result);

	virtual void			ClearTextSelection(HTML_Element* svg_elm);
	virtual BOOL			GetTextSelectionLength(HTML_Element* elm, unsigned& length);
	virtual BOOL			GetTextSelection(HTML_Element* elm, TempBuffer& buffer);
	virtual void			GetSelectionRects(SVG_DOCUMENT_CLASS* doc, HTML_Element* svg_elm, OpVector<OpRect>& list);
	virtual void			RemoveSelectedContent(HTML_Element* svg_elm);
	virtual void			ShowCaret(SVG_DOCUMENT_CLASS* doc, HTML_Element* svg_elm);
	virtual void			HideCaret(SVG_DOCUMENT_CLASS* doc, HTML_Element* svg_elm);
	virtual void			InsertTextAtCaret(HTML_Element* svg_elm, const uni_char* text);
	virtual BOOL			IsWithinSelection(SVG_DOCUMENT_CLASS* doc, HTML_Element* elm, const OpPoint& doc_point);

	virtual OP_BOOLEAN		TransformToElementCoordinates(HTML_Element* element, SVG_DOCUMENT_CLASS* doc, int& x, int& y);

	virtual BOOL			IsEditableElement(SVG_DOCUMENT_CLASS* doc, HTML_Element* elm);
	virtual void			BeginEditing(SVG_DOCUMENT_CLASS* doc, HTML_Element* elm, FOCUS_REASON reason);

	virtual BOOL			IsFocusableElement(SVG_DOCUMENT_CLASS *doc, HTML_Element* elm);
	virtual OP_STATUS		GetNavigationData(HTML_Element* elm, OpRect& elm_box);
	virtual OP_STATUS		GetElementRect(HTML_Element* elm, OpRect& elm_box);

	virtual BOOL			HasNavTargetInDirection(HTML_Element* current_elm, int direction, int nway, HTML_Element*& preferred_next_elm);

	virtual OP_STATUS		GetNavigationIterator(HTML_Element* elm, const OpRect* search_area, LayoutProperties* layout_props, SVGTreeIterator** nav_iterator);
#ifdef SEARCH_MATCHES_ALL
	virtual OP_STATUS		GetHighlightUpdateIterator(HTML_Element* elm, LayoutProperties* layout_props, SelectionElm* first_hit, SVGTreeIterator** nav_iterator);
#endif // SEARCH_MATCHES_ALL
#ifdef RESERVED_REGIONS
	virtual OP_STATUS		GetReservedRegionIterator(HTML_Element* elm, const OpRect* search_area, SVGTreeIterator** region_iterator);
#endif // RESERVED_REGIONS

	virtual void			ReleaseIterator(SVGTreeIterator* iter);

	virtual void			ScrollToRect(OpRect rect, SCROLL_ALIGN align, HTML_Element *scroll_to);

	virtual void			UpdateHighlight(VisualDevice* vd, HTML_Element* elm, BOOL in_paint);

	virtual void			HandleEventFinished(DOM_EventType event, HTML_Element *target);

	virtual BOOL			AllowFrameForElement(HTML_Element* elm);
	virtual BOOL			AllowInteractionWithElement(HTML_Element* elm);
	virtual BOOL			IsEcmaScriptEnabled(SVG_DOCUMENT_CLASS* doc);

	virtual const ClassAttribute* GetClassAttribute(HTML_Element* elm, BOOL baseVal);

	virtual OP_STATUS		InsertElement(HTML_Element* elm);
	virtual OP_STATUS		EndElement(HTML_Element* elm);

	virtual OP_STATUS		ApplyFilter(SVGURLReference ref, SVG_DOCUMENT_CLASS* doc,
										SVGFilterInputImageGenerator* generator,
										const OpRect& screen_targetobjectbbox,
										OpPainter* painter, OpRect& affected_area);

	virtual OP_STATUS		GetOpFont(HTML_Element* fontelm, UINT32 size, OpFont** out_opfont);
	virtual OP_STATUS		GetOpFontInfo(HTML_Element* font_element, OpFontInfo** out_fontinfo);
	virtual OP_STATUS		DrawString(VisualDevice* vis_dev, OpFont* font, int x, int y, const uni_char* txt, int len, int char_spacing_extra = 0);

#ifdef SVG_GET_PRESENTATIONAL_ATTRIBUTES
	virtual OP_STATUS GetPresentationalAttributes(HTML_Element* elm, HLDocProfile* hld_profile, CSS_property_list* list);
#endif // SVG_GET_PRESENTATIONAL_ATTRIBUTES

	virtual void SwitchDocument(LogicalDocument* logdoc, HTML_Element* root);

	void					ForceRedraw(SVGDocumentContext* doc_ctx);

	void					SetPresentationValues(BOOL val) { m_get_presentation_values = val; }
	BOOL					GetPresentationValues() const { return m_get_presentation_values; }

#ifdef SVG_SUPPORT_EMBEDDED_FONTS
	SVGEmbeddedFontData*	GetMatchingSystemFont(FontAtt& fontatt);
#endif // SVG_SUPPORT_EMBEDDED_FONTS

	OP_STATUS				RegisterSVGFont(OpFont* font) { return m_current_svgfonts.Add(font); }
	void					UnregisterSVGFont(OpFont* font) { m_current_svgfonts.RemoveByItem(font); }
	BOOL					IsSVGFont(OpFont* font) const
	{
		return m_current_svgfonts.Find(font) != -1
#ifdef SVG_SUPPORT_EMBEDDED_FONTS
			|| m_svg_opfonts.Find(font) != -1
#endif // SVG_SUPPORT_EMBEDDED_FONTS
			;
	}

	/**
     * [Internal API] Get a font. Don't forget to release it when done.
     */
	OpFont* GetFont(FontAtt &fontatt, UINT32 scale, SVGDocumentContext* doc);

	/**
     * [Internal API] Release a font.
     */
	void ReleaseFont(OpFont* font);

	/**
	 * [Internal API]
	 * Select element according to a certain criteria (intersect / enclose)
	 *
	 * @param root The root of the svg
	 * @param selection_rect The rectangle used as basis for the selection
	 * @param need_enclose Whether the selected elements should be enclosed by the
	 * selection_rect or not
	 * @param selected List of the elements that matched the criteria
	 */
	OP_STATUS SelectElements(SVGDocumentContext* doc_ctx,
							 const SVGRect& selection_rect, BOOL need_enclose,
							 OpVector<HTML_Element>& selected);
	
	/**
	 * Get a pointer to the tempbuffer
	 */
	TempBuffer *			GetTempBuf();

	/**
	 * Get a pointer to the empty temp buffer
	 */
	TempBuffer *            GetEmptyTempBuf();

	/**
	* Report an error to the console log.
	*/
	void 					ReportError(HTML_Element* anyelm, const uni_char* message, SVG_DOCUMENT_CLASS* doc_fallback = NULL, BOOL report_href = FALSE);

	OP_BOOLEAN              HandleEvent(const SVGManager::EventData& data);
	int						GetImageRefId() { return m_imageref_id_counter++; }

	/**
	 * Will not return NULL.
	 */
	SVGCache*               GetCache() { return &m_cache; }

	unsigned int			GetAverageFrameTime() { return m_time_averager.GetAverage(); }
	void					AddFrameTimeSample(unsigned int ft) { m_time_averager.AddSample(ft); }

#ifdef SVG_SUPPORT_MEDIA
	SVGMediaManager*		GetMediaManager() { return &m_media_manager; }
#endif // SVG_SUPPORT_MEDIA

	SVGGlyphCache&			GetGlyphCache() { return m_glyph_cache; }

	virtual void			HandleFontDeletion(OpFont* font_about_to_be_deleted);

	SVGPaint			default_stroke_prop;
	SVGPaint			default_fill_prop;
	SVGURLReference		default_url_reference_prop;

#ifdef SVG_SUPPORT_ANIMATION_LISTENER
	OP_STATUS				RegisterAnimationListener(SVGAnimationWorkplace::AnimationListener *listener);
	void					UnregisterAnimationListener(SVGAnimationWorkplace::AnimationListener *listener);
	void					UnregisterAllAnimationListeners();
	void					ConnectAnimationListeners(SVGAnimationWorkplace *animation_workplace);
#endif // SVG_SUPPORT_ANIMATION_LISTENER

	SVGDashArray		dasharray_isnone_prop;
	const SVGMatrix		identity_matrix;

#ifdef _DEBUG
	int					seq_num;
	int					svg_object_refcount;
#endif // _DEBUG
#ifdef USE_OP_CLIPBOARD
	virtual void Paste(HTML_Element* elm, OpClipboard* clipboard);
	virtual void Cut(HTML_Element* elm, OpClipboard* clipboard);
	virtual void Copy(HTML_Element* elm, OpClipboard* clipboard);
	virtual void ClipboardOperationEnd(HTML_Element* elm);
#endif // USE_OP_CLIPBOARD
};

/**
 * We can allow ourself to access the full SVGManagerImpl interface
 */
#define g_svg_manager_impl ((SVGManagerImpl*)g_svg_manager)
#define g_svg_identity_matrix g_svg_manager_impl->identity_matrix

/**
* Temporarily set the presentation values flag. The object will restore
* the original value when destroyed.
*/
class TempPresentationValues
{
public:
	TempPresentationValues(BOOL temp_value)
		: original_value(g_svg_manager_impl->GetPresentationValues())
	{
		g_svg_manager_impl->SetPresentationValues(temp_value);
	}

	~TempPresentationValues()
	{
		g_svg_manager_impl->SetPresentationValues(original_value);
	}
private:
	BOOL original_value;
};

#ifdef _DEBUG
#define REPORT_NOTICE(elm,msg) do { g_svg_manager_impl->ReportError(elm, msg); } while(0)
#else
#define REPORT_NOTICE(elm,msg) ((void)0)
#endif // _DEBUG

#ifdef SVG_LOG_ERRORS
#define SVG_NEW_ERROR(elm) SVGErrorReport svg_error_object(elm)
#define SVG_NEW_ERROR2(doc, elm) SVGErrorReport svg_error_object(elm, doc)
#define SVG_REPORT_ERROR(args)	svg_error_object.Report args
#define SVG_REPORT_ERROR_HREF(args)	svg_error_object.ReportWithHref args
#else // !SVG_LOG_ERRORS
#define SVG_NEW_ERROR(elm) ((void)0)
#define SVG_NEW_ERROR2(doc, elm) ((void)0)
#define SVG_REPORT_ERROR(args) ((void)0)
#define SVG_REPORT_ERROR_HREF(args) ((void)0)
#endif // !SVG_LOG_ERRORS

#endif // SVG_SUPPORT
#endif // _SVG_MANAGER_
