/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef SVG_WORKPLACE_IMPL_H
#define SVG_WORKPLACE_IMPL_H

#ifdef SVG_SUPPORT

class SVGImage;
class SVGImageRef;
class SVGImageImpl;
class SVGDependencyGraph;

#include "modules/svg/svg_workplace.h"

#include "modules/hardcore/timer/optimer.h"
#include "modules/hardcore/mh/messobj.h"

#include "modules/util/OpHashTable.h"
#include "modules/url/url2.h"

/**
 * This class is the part of LogicalDocument that takes care of
 * everything SVG related.
 */
class SVGWorkplaceImpl :
	public SVGWorkplace,
	public OpTimerListener,
	private MessageObject
{
public:
	SVGWorkplaceImpl(SVG_DOCUMENT_CLASS* doc) :
		m_doc(doc),
		m_listening(FALSE),
		m_delayed_actions_pending(FALSE),
		m_forced_reflow(FALSE),
		m_lag(0),
		m_svg_load_counter(1),
		m_begin_time(-1),
		m_depgraph(NULL)
#ifdef SVG_THROTTLING
		, m_last_allowed_invalidation_timestamp(op_nan(NULL))
		, m_next_allowed_invalidation_timestamp(op_nan(NULL))
		, m_last_load_check_timestamp(op_nan(NULL))
		, m_throttling_needed(FALSE)
		, m_current_throttling_fps(SVG_THROTTLING_FPS_HIGH)
		, m_last_adapt_step(0)
#endif // SVG_THROTTLING
	{
		OP_ASSERT(doc);
	}

	virtual ~SVGWorkplaceImpl();

	/** Clean up all document-related references held by this workplace. */
	virtual void Clean();

	/**
	 * Sets the visible flag for SVG:s dependent on the scroll position.
	 *
	 * @param visible_area The document area visible on
	 * screen. Anything outside doesn't need to bother with painting
	 * related calculations.
	 */
	virtual void RegisterVisibleArea(const OpRect& visible_area);

	/**
	 * Set as current document.
	 *
	 * Call when the document containing this workplace has been
	 * enabled/disabled.
	 *
	 * @param state TRUE if it is the current document, FALSE otherwise
	 */
	virtual OP_STATUS SetAsCurrentDoc(BOOL state);

	/**
	 * Determine if any of the SVGImage's in this workplace has
	 * selected text.
	 */
	virtual BOOL HasSelectedText();

	/** Gets the selected range. */
	BOOL GetSelection(SelectionBoundaryPoint &anchor, SelectionBoundaryPoint &focus);

	/**
	 * Initiates loading of an SVG from a URL.
	 *
	 * Used for 'document related' loads, such as for background-image.
	 */
	virtual OP_STATUS GetSVGImageRef(URL url, SVGImageRef** out_ref);

	/**
	 * This is a list of all images (including inactive images, that only are parts of another svg image).
	 *
	 * Only to be used internally in the svg module.
	 *
	 * @param image The SVG image to insert
	 */
	void InsertIntoSVGList(SVGImage* image);

	/**
	 * This is a list of all images (including inactive images, that only are parts of another svg image).
	 *
	 * Only to be used internally in the svg module.
	 *
	 * @param image The SVG image to remove
	 */
	void RemoveFromSVGList(SVGImage* image);

	/**
	 * Returns the first SVG on this page. "First" doesn't
	 * necessarily mean that it is earliest in the document,
	 * but can be in any order. Only real SVG:s are returned,
	 * i.e. no SVG:s that are part of another SVG is included.
	 *
	 * @return NULL if there are no SVG:s.
	 */
	virtual SVGImage* GetFirstSVG();

	/**
	 * Returns the next SVG on this page. Only real SVG:s are returned,
	 * i.e. no SVG:s that are part of another SVG is included.
	 *
	 * @param prev_svg An SVG returned from an earlier call to GetFirstSVG or GetNextSVG.
	 *
	 * @return NULL if there are no more SVG:s.
	 */
	virtual SVGImage* GetNextSVG(SVGImage* prev_svg);

	virtual OP_STATUS LoadCSSResource(URL url, SVGWorkplace::LoadListener* listener);

	virtual void InvalidateFonts();

	virtual void StopLoadingCSSResource(URL url, SVGWorkplace::LoadListener* listener);

	virtual OP_STATUS GetDefaultStyleProperties(HTML_Element* element, CSS_Properties* css_properties);

	/**
	 * Hook to call when a subtree has been removed. This will assure
	 * that things that pending actions don't trample over previously
	 * destroyed elements.
	 */
	void HandleRemovedSubtree(HTML_Element* subtree);

	/**
	 * Add an element that should be discarded (removed from the tree)
	 * as soon as possible.
	 */
	void AddPendingDiscard(HTML_Element* element);

	/**
	 * Add an element that needs to be marked extra dirty.
	 */
	OP_STATUS AddPendingMarkExtraDirty(HTML_Element* elm);

	/**
	 * Queue a (forced) reflow for the document this workplace corresponds to.
	 */
	OP_STATUS QueueReflow();

	/**
	 * OpTimerListener interface.
	 */
	virtual void	OnTimeOut(OpTimer* timer);
	BOOL 			IsListening() { return m_listening; }
	void			SetListening(BOOL new_state) { m_listening = new_state; }

	void			ScheduleLayoutPass(unsigned int max_delay);
	void			ScheduleInvalidation(SVGImageImpl *img);

	/**
	 * Stop timers that shouldn't run when in the page history.
	 */
	void			StopAllTimers();

	SVGDependencyGraph *GetDependencyGraph() { return m_depgraph; }
	OP_STATUS			CreateDependencyGraph();

	void			SignalInlineIgnored(URL fullurl);
	void			SignalInlineChanged(URL fullurl);
	HTML_Element*	GetProxyElement(URL fullurl);
	OP_STATUS		CreateProxyElement(URL fullurl, HTML_Element** proxy_element = NULL, SVGWorkplace::LoadListener* listener = NULL);

	BOOL			IsProxyElement(HTML_Element* elm);

	SVG_DOCUMENT_CLASS* GetDocument() { return m_doc; }

	double			GetLastLag() { return m_lag; }

	/**
	 * Before the load event has arrived, this document begin is set
	 * to a negative value (-1).
	 *
	 * Between the load event has arrived and we start animations,
	 * this value is set to the time that the load event arrived so
	 * that we can sync the animation by that point in time.
	 *
	 * After we have used the value for synchronization, document
	 * begin set to a negative value (-1) again.
	 */
	void						SetDocumentBegin(double now) { m_begin_time = now; }
	double						GetDocumentBegin() { return m_begin_time; }

	/**
	 * Increase the SVGLoad counter for this SVG.
	 */
	void						IncSVGLoadCounter() { m_svg_load_counter++; }

	/**
	 * Decrease the SVGLoad counter for this SVG. Returns TRUE when
	 * the counter reaches zero.
	 */
	BOOL						DecSVGLoadCounter() { return (--m_svg_load_counter) == 0; }

	/**
	 * Return the SVGLoad counter for this document.
	 */
	int							CheckSVGLoadCounter() { return m_svg_load_counter; }

private:

	class ExternalProxyFrameElementItem : public Link, public ElementRef
	{
	public:
		URL							proxy_element_url;
		SVGWorkplace::LoadListener*	proxy_listener;

		// ElementRef
		virtual void OnDelete(FramesDocument *document) { Reset(); Out(); OP_DELETE(this); }
	};

	OP_STATUS QueueDelayedAction();
	void ProcessDiscard(HTML_Element* discard_target);
	void HandleInvalidationMessage();
	void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

	SVG_DOCUMENT_CLASS* m_doc;
	Head m_svg_images;
	Head m_svg_imagerefs;
	OpPointerSet<HTML_Element> m_pending_discards;
	OpPointerSet<HTML_Element> m_pending_mark_extra_dirty;

	BOOL m_listening;
	BOOL m_delayed_actions_pending;
	BOOL m_forced_reflow;
	OpTimer m_layout_pass_timer;
	double m_current_timer_fire_time;
	double m_lag; /**< How "late" was the last message. */
	int m_svg_load_counter; /**< The number of pending load event */
	double m_begin_time; 	/**< The begin time, set when the load event is sent */

	SVGDependencyGraph* m_depgraph;

	Head m_proxy_links;
	Head m_queued_inlines_changed; // queue for the Onloads we can't deal with during reflows
#ifdef SVG_THROTTLING
	double m_last_allowed_invalidation_timestamp;
	double m_next_allowed_invalidation_timestamp;
	double m_last_load_check_timestamp;
	BOOL m_throttling_needed;
	unsigned int m_current_throttling_fps;
	unsigned int m_last_adapt_step;
	BOOL IsThrottlingNeeded(double curr_time);
	BOOL IsInvalidationAllowed(double curr_time)
	{
		return (op_isnan(m_next_allowed_invalidation_timestamp) ||
               (curr_time >= m_next_allowed_invalidation_timestamp));
	}
	void UpdateTimestamps(double curr_time);
	BOOL AdaptFps(int adapt_interval);
#endif // SVG_THROTTLING
};

#endif // SVG_SUPPORT
#endif // SVG_WORKPLACE_IMPL_H
