/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DOCHAND_FDELM_H
#define DOCHAND_FDELM_H

#include "modules/dochand/docman.h"

#include "modules/util/tree.h"
#include "modules/doc/doctypes.h"
#include "modules/doc/css_mode.h"
#include "modules/windowcommander/OpWindowCommander.h" // For LayoutMode and NEARBY_INTERACTIVE_ITEM_DETECTION

#define DOCHAND_ATTR_SUB_WIN_ID Markup::DOCHANDA_SUB_WIN_ID

// Used in both fdelm.cpp and frm_doc.cpp.
#define FRAME_BORDER_SIZE 2
#define RELATIVE_MIN_SIZE 25

class HTML_Element;
class FormObject;
class VisualDevice;
class OpView;
class CoreView;

class ES_TerminatingThread;

#include "modules/doc/frm_doc.h"

class DocumentManager;
class FDEElmRef;

class FrameReinitData
{
public:
	FrameReinitData(int history_num, BOOL visible, LayoutMode old_layout_mode)
		:	m_history_num(history_num), m_visible(visible),
			m_old_layout_mode(old_layout_mode) {}

	int				m_history_num;
	BOOL			m_visible;
	LayoutMode		m_old_layout_mode;
};

class FramesDocElmAttr : public ComplexAttr
{
public:
	FramesDocElm*	m_fde;

	FramesDocElmAttr(FramesDocElm* fde) : m_fde(fde) {}

	// ComplexAttr methods
	virtual OP_STATUS	CreateCopy(ComplexAttr **copy_to) { return OpStatus::ERR_NOT_SUPPORTED; }
	virtual BOOL		MoveToOtherDocument(FramesDocument *old_document, FramesDocument *new_document) { return FALSE; }
};

class FramesDocElm
	: private Tree
{
public:

	class FDEElmRef : public ElementRef
	{
	public:
		FDEElmRef(FramesDocElm* fde) : m_fde(fde) {}

		FramesDocElm*		GetFramesDocElm() { return m_fde; }

		// ElementRef methods
		virtual	void		OnDelete(FramesDocument *document);
		virtual	void		OnRemove(FramesDocument *document) { return OnDelete(document); }
		virtual BOOL		IsA(ElementRefType type) { return type == ElementRef::FRAMESDOCELM; }

	private:
		FramesDocElm*	m_fde;
	};

private:

	union
	{
		struct
		{
			unsigned int row:1;
			unsigned int is_frameset:1;
			unsigned int is_inline:1;
			unsigned int is_secondary:1; ///< This FramesDocElm represents the same HTML_Element as its parent.
			unsigned int is_in_doc_coords:1;

			unsigned int frame_border:1;
			unsigned int frame_border_top:1;
			unsigned int frame_border_left:1;
			unsigned int frame_border_right:1;
			unsigned int frame_border_bottom:1;

			unsigned int frame_noresize:1;
			unsigned int hidden:1;

#ifdef SVG_SUPPORT
			unsigned int is_svg_resource:1;
#endif
			unsigned int nofity_parent_on_content_change:1;
			unsigned int is_being_deleted:1;

			unsigned int size_type:2;

			unsigned int is_stacked:1;
			unsigned int normal_row:1;

			unsigned int special_object:1;
			unsigned int special_object_layout_mode:3;

			unsigned int es_onload_called:1;
			unsigned int es_onload_ready:1;
			unsigned int is_deleted:1;
			unsigned int checked_unload_targets:1;
			unsigned int has_unload_targets:1;

//			unsigned int unused:4;
		} packed1; // 32 bits
		unsigned int packed1_init;
	};

	BYTE			frame_scrolling;
	int				frame_margin_width;
	int				frame_margin_height;
	int				frame_spacing;
	//int			frameset_border;

	DocumentManager*
					doc_manager;
	FramesDocument*	parent_frm_doc;

	/**
	 * Used for iframes only.
	 * Stores the OpInputContext associated with some layout object. This input context is a parent
	 * of this iframe's VisualDevice. When NULL, then the parent input context of this
	 * iframe's VisualDevice is the VisualDevice of the parent document.
	 * The layout takes care of notifying this iframe about parent context changes. */
	OpInputContext* parent_layout_input_ctx;

	FDEElmRef		m_helm;

#ifdef _PRINT_SUPPORT_
	HTML_Element*	m_print_twin_elm;
#endif // _PRINT_SUPPORT_

	AffinePos		pos; ///< Position relative to parent's rendering viewport

	int				width; ///< Rendering viewport width
	int				height; ///< Rendering viewport height

	int				normal_width; ///< Layout viewport width
	int				normal_height; ///< Layout viewport height

	int 			sub_win_id;

#ifndef MOUSELESS
	int				drag_val;
	int				drag_offset;
#endif // !MOUSELESS

	int				size_val;
	VisualDevice*	frm_dev;

	OpString		name;
	OpString		frame_id;

	FrameReinitData*
					reinit_data; ///< Used for asynchronous initialization of frames

	/**
	 * Convert the specified value from screen to document coordinates, unless
	 * IsInDocCoords() is TRUE.
	 */
	AffinePos		ToDocIfScreen(const AffinePos& val) const;

	/**
	 * Convert the specified value from screen to document coordinates, unless
	 * IsInDocCoords() is TRUE.
	 */
	int				ToDocIfScreen(int val, BOOL round_up) const;

	/**
	 * Convert the specified value from document to screen coordinates, unless
	 * IsInDocCoords() is TRUE.
	 */
	int				ToScreenIfScreen(int val, BOOL round_up) const;

	AffinePos		GetDocOrScreenAbsPos() const;
	int				GetDocOrScreenAbsX() const;
	int				GetDocOrScreenAbsY() const;

	/**
	 * Get position, relative to parent, in document coordinates
	 */
	AffinePos		GetPos() const;

	/**
	 * Get X position, relative to parent, in document coordinates
	 */
	int				GetX() const;

	/**
	 * Get Y position, relative to parent, in document coordinates
	 */
	int				GetY() const;

	/**
	 * Set X position, relative to parent, in document coordinates.
	 * Does not commit the change.
	 */
	void			SetX(int x);

	/**
	 * Set Y position, relative to parent, in document coordinates.
	 * Does not commit the change.
	 */
	void			SetY(int y);

#ifndef MOUSELESS
	int 			GetMaxX();
	int				GetMaxY();
	int				GetMinWidth();
	int				GetMinHeight();

	void			Reformat(int x, int y);
	BOOL			CanResize();
	void 			PropagateSizeChanges(BOOL left, BOOL right, BOOL top, BOOL bottom/*, int border*/);
#endif // !MOUSELESS

	enum
	{
		/** Value of frame_index indicating that frame is not currently stored in (i)frame root */
		FRAME_NOT_IN_ROOT = -1
	};

	/**
	 * Index of this frame element. It keeps track of the insertion order.
	 */
	int frame_index;

	/**
	 * Checks if given element is or belongs to current document's (i)frame root
	 */
	BOOL			IsFrameRoot(FramesDocElm *head);

	/**
	 * Checks if mouse events should be disabled for the CoreView
	 * associated with this frame, due to SVG interactivity rules.
	 * This function should be called whenever the frame is shown.
	 *
	 * When this function is called, mouse events will be disabled for
	 * the associated CoreView if:
	 *
	 *	- The SVG is loaded using an <img> tag.
	 *	- The SVG is loaded using an <object> tag, and interaction is
	 *    explicitly disabled with <param name="render" value="frozen">.
	 *	- The SVG is a plugin placeholder.
	 */
	void			SVGCheckWantMouseEvents();

public:

					FramesDocElm(int id, int x, int y, int w, int h, FramesDocument* frm_doc, HTML_Element* he, VisualDevice* vd, int type, int val, BOOL inline_frm, FramesDocElm* parent_frmset, BOOL secondary = FALSE);
					~FramesDocElm();

	void			SetSubWinId(int id) { sub_win_id = id; }

    OP_STATUS       Init(HTML_Element *helm, VisualDevice* vd, OpView* clipview); // must be called immediately after the constructor

	void			Reset(int type, int val, FramesDocElm* parent_frmset, HTML_Element *helm = NULL);

	VisualDevice*	GetVisualDevice() { return frm_dev; }

	/**
	 * // Geir: kunne ikke parent_frm_doc hentes ut fra doc_manager (GetDocManager()->GetParentDoc())?
	 *
	 * @returns The owner document, never NULL.
	 */
	FramesDocument*	GetParentFramesDoc() { return parent_frm_doc; };

	/**
	 * @returns The top FramesDocument. Never NULL.
	 *
	 * @see FramesDocument::GetTopFramesDoc().
	 */
	FramesDocument*	GetTopFramesDoc();

	BOOL			IsRow() { return packed1.row; }
	void			SetIsRow(BOOL is_row) { packed1.row = is_row; }
	void			CheckFrameEdges(BOOL& outer_top, BOOL& outer_left, BOOL& outer_right, BOOL& outer_bottom, BOOL top_checked = FALSE, BOOL left_checked = FALSE, BOOL right_checked = FALSE, BOOL bottom_checked = FALSE);
	BOOL			GetFrameBorder() { return packed1.frame_border; }
	void			SetFrameBorder(BOOL border) { packed1.frame_border = border; }
	int				GetFrameSpacing() { return frame_spacing; }
	//int				Border();
	BOOL			GetFrameTopBorder() { return packed1.frame_border_top; }
	BOOL			GetFrameLeftBorder() { return packed1.frame_border_left; }
	BOOL			GetFrameRightBorder() { return packed1.frame_border_right; }
	BOOL			GetFrameBottomBorder() { return packed1.frame_border_bottom; }
	void			SetFrameBorders(BOOL top, BOOL left, BOOL right, BOOL bottom)
						{ packed1.frame_border_top = top; packed1.frame_border_left = left; packed1.frame_border_right = right; packed1.frame_border_bottom = bottom; }

	/** @return TRUE if this element has been checked if it contains an
	 * embedded unload target.
	 */
	BOOL			GetCheckedUnloadHandler() { return packed1.checked_unload_targets; }

	/** Mark or unmark this element's cached unload target information.
	 *  Must be cleared on an element when one of its descendant frame
	 *  elements gains or loses an unload handler.
	 */
	void			SetCheckedUnloadHandler(BOOL flag) { packed1.checked_unload_targets = flag; }

	/** @return TRUE if this element has embedded unload handlers.
	 */
	BOOL			GetHasUnloadHandler() { return packed1.has_unload_targets; }

	/** Set if this element has embedded unload targets or not. If a
	 * frameset has this flag set, but no unload handlers directly
	 * on its element, it will still be considered an unload target.
	 */
	void			SetHasUnloadHandler(BOOL flag) { packed1.has_unload_targets = flag; }

	/** Determine if this element has any unload targets directly attached
	 * or below it in the element tree.
	 * @return TRUE if it contains any embedded unload targets,
	 * FALSE otherwise.
	 */
	BOOL			CheckForUnloadTarget();

	/** Update this element's unload handler information given that
	 * an unload target has been added or removed somewhere within
	 * its document. If that represents a change in overall status,
	 * clear the cached and now-invalid unload information on any
	 * of its ancestor elements.
	 *
	 * @param added Flag indicating if an unload handler was added
	 * or removed.
	 */
	void			UpdateUnloadTarget(BOOL added);

	OP_DOC_STATUS   BuildTree(FramesDocument* top_frm_doc, Head* existing_frames = NULL);

	/**
	 * @return TRUE if the xpos, ypos, width, height, normal_width and
	 * normal_height members and the SetRootSize() method represent or should
	 * represent the position and size in document coordinates, FALSE if the
	 * position and size is or should be represented in screen
	 * coordinates. FramesDocElm normally represent size and position in screen
	 * coordinates, but they need to be represented in document coordinates in
	 * TrueZoom, to avoid rounding errors.
	 */
	BOOL			IsInDocCoords() const;

	/**
	 * Get position relative to the root of the frameset to which this
	 * frame/frameset belongs, in document coordinates.
	 */
	AffinePos		GetAbsPos() const;

	/**
	 * Get X position relative to the root of the frameset to which this
	 * frame/frameset belongs, in document coordinates.
	 */
	int				GetAbsX() const;

	/**
	 * Get Y position relative to the root of the frameset to which this
	 * frame/frameset belongs, in document coordinates.
	 */
	int				GetAbsY() const;

	/**
	 * Get frame width, in document coordinates.
	 */
	int				GetWidth() const;

	/**
	 * Get frame height, in document coordinates.
	 */
	int				GetHeight() const;

	/**
	 * Get layout viewport width, in document coordinates.
	 */
	int				GetNormalWidth() const;

	/**
	 * Get layout viewport width, in document coordinates.
	 */
	int				GetNormalHeight() const;

	/**
	 * Set position, in document coordinates.
	 */
	void			SetPosition(const AffinePos& doc_pos);

	/**
	 * Set size, in document coordinates.
	 */
	void			SetSize(int w, int h);

	/**
	 * Set size and position, in document coordinates.
	 */
	void			SetGeometry(const AffinePos& doc_pos, int w, int h);

	/**
	 * Force new frame height, in document coordinates.
	 */
	void			ForceHeight(int h);

	/**
	 * Show all frames in this frameset.
	 */
	OP_STATUS		ShowFrames();

	/**
	 * Hide all frames in this frameset.
	 */
	void			HideFrames();

	/**
	 * Commit size and position changes, i.e. update VisualDevice.
	 *
	 * Makes sure that the VisualDevice size is up-to-date (see xpos,
	 * ypos, width and height members).
	 *
	 * Will never trigger reflow.
	 */
	void			UpdateGeometry();

	/**
	 * Set root frameset size, in screen or document coordinates, depending on IsInDocCoords().
	 * These values will be used by FormatFrames() as top-level document available size.
	 */
	void			SetRootSize(int width, int height);

	/**
	 * Distribute available space among sub-frames, according to framesets'
	 * COLS and ROWS attributes. If the size of a sub-frame changes here, the
	 * document in that frame will also be reflowed right away if that document
	 * defines another frameset; or, if it defines a regular HTML document
	 * (with BODY), it will be scheduled for reflow (marked dirty, etc.).
	 */
	OP_DOC_STATUS   FormatFrames(BOOL format_rows = TRUE, BOOL format_cols = TRUE);

	/**
	 * Initiate loading of the URL specified for each frame in this frameset.
	 *
	 * @param origin_thread The thread that inserted the frame.
	 */
	OP_STATUS       LoadFrames(ES_Thread *origin_thread = NULL);

	/**
	 * Reflow each document in this frameset brutally and thoroughly.
	 * This involves reloading CSS properties and regenerating all layout boxes.
	 */
	OP_DOC_STATUS   FormatDocs();

	FramesDocElm*	Parent() const;
	FramesDocElm*	Suc() const { return (FramesDocElm*)Tree::Suc(); };
	FramesDocElm*	Pred() const { return (FramesDocElm*)Tree::Pred(); };
	FramesDocElm*	FirstChild() const { return (FramesDocElm*)Tree::FirstChild(); };
	FramesDocElm*	LastChild() const { return (FramesDocElm*)Tree::LastChild(); };
    FramesDocElm*	Next() const { return (FramesDocElm*)Tree::Next(); }
    FramesDocElm*	Prev() const { return (FramesDocElm*)Tree::Prev(); }
    FramesDocElm*	FirstLeaf() { return (FramesDocElm*)Tree::FirstLeaf(); }
    FramesDocElm*	LastLeaf() { return (FramesDocElm*)Tree::LastLeaf(); }
    FramesDocElm*	NextLeaf() { return (FramesDocElm*)Tree::NextLeaf(); }
    FramesDocElm*	PrevLeaf() { return (FramesDocElm*)Tree::PrevLeaf(); }

	OP_STATUS       Undisplay(BOOL will_be_destroyed = FALSE);

	/**
	 * Shows the frame if it has VisualDevice. It is a shortcut to VisualDevice::Show(),
	 * which it calls, passing appropriate param.
	 * @returns OK, OOM or generic error if something went wrong in VisualDevice::Show().
	 */
	OP_STATUS		Show();

	/**
	 * Hides the frame if it has VisualDevice. Should be used instead of VisualDevice::Hide(),
	 * which it calls.
	 * @param free_views if TRUE, deletes the view and most other things of this frame's VisualDevice
	 * If FALSE, just hides the view of the VisualDevice.
	 * @see VisualDevice::Hide()
	 */
	void			Hide(BOOL free_views = FALSE);

#ifdef _PRINT_SUPPORT_
	void			Display(VisualDevice* vd, const RECT& rect);
#endif // _PRINT_SUPPORT_
	void			DisplayBorder(VisualDevice* vd);

#ifndef MOUSELESS
	/**
	 * Check something and return a FramesDocElm if something is the case.
	 *
	 * @param x Mouse X position in top-level document coordinates
	 * @param y Mouse Y position in top-level document coordinates
	 */
	FramesDocElm*	IsSeparator(int x, int y);

	/**
	 * Start moving frame separator.
	 *
	 * @param x Mouse X position in top-level document coordinates
	 * @param y Mouse Y position in top-level document coordinates
	 */
	void			StartMoveSeparator(int x, int y);

	/**
	 * Move frame separator.
	 *
	 * @param x Mouse X position in top-level document coordinates
	 * @param y Mouse Y position in top-level document coordinates
	 */
	void			MoveSeparator(int x, int y);

	/**
	 * Stop moving frame separator.
	 *
	 * @param x Mouse X position in top-level document coordinates
	 * @param y Mouse Y position in top-level document coordinates
	 */
	void			EndMoveSeparator(int x, int y);
#endif // !MOUSELESS

	OP_STATUS       ReactivateDocument();

	BOOL			IsLoaded(BOOL inlines_loaded = TRUE);

	void            SetInlinesUsed(BOOL used);

	void            Free(BOOL layout_only = FALSE, FramesDocument::FreeImportance importance = FramesDocument::FREE_NORMAL);

	OP_STATUS		SetMode(BOOL win_show_images, BOOL win_load_images, CSSMODE win_css_mode, CheckExpiryType check_expiry);

	OP_STATUS       SetAsCurrentDoc(BOOL state, BOOL visible_if_current = TRUE);

	void			SetCurrentHistoryPos(int n, BOOL parent_doc_changed, BOOL is_user_initiated);
	void			RemoveFromHistory(int from);
	void			RemoveUptoHistory(int to);
	void	    	RemoveElementFromHistory(int pos);

	void			CheckHistory(int decrement, int& minhist, int& maxhist);
    DocListElm*     GetHistoryElmAt(int pos, BOOL forward = FALSE);

	const URL&		GetCurrentURL() { return doc_manager->GetCurrentURL(); };

	BOOL			IsInlineFrame() { return packed1.is_inline; }
	static FramesDocElm*	GetFrmDocElmByHTML(HTML_Element*);

	int				GetSubWinId() { return sub_win_id; }

	int				GetSizeType() { return (unsigned int)packed1.size_type; }
	int				GetSizeVal() { return size_val; }
	HTML_Element*	GetHtmlElement();
	void			SetHtmlElement(HTML_Element *elm);
	void			DetachHtmlElement();

	/**
	 * Returns the name of the frame, i.e. the name attribute
	 * of the element creating this frame. This is typically
	 * used to identify the frame.
	 *
	 * <p>The used to have an internal fallback to the id if the
	 * name was missing, but that is gone and callers will have
	 * to check GetFrameId themselves.
	 *
	 * @returns The name or NULL.
	 *
	 * @see GetFrameId();
	 */
	const uni_char*	GetName();
	OP_STATUS       SetName(const uni_char* str);

	/**
	 * Returns the id string of the frame, i.e. the id of the
	 * element creating this frame. Typically the name
	 * is used to identify a frame but the id acts as a backup name.
	 *
	 * @returns The id string or NULL.
	 *
	 * @see GetName()
	 */
	const uni_char* GetFrameId() { return frame_id.CStr(); }

	/**
	 * Propagates the DocumentManager::StopLoading() call to the whole
	 * frame tree.
	 *
	 * @see DocumentManager::StopLoading()
	 */
	void			StopLoading(BOOL format = TRUE, BOOL abort = FALSE);

	OP_BOOLEAN      CheckSource();

	virtual BOOL    OnLoadCalled() const { return packed1.es_onload_called; }
	void			SetOnLoadCalled(BOOL the_truth) { packed1.es_onload_called = the_truth; }
	virtual BOOL    OnLoadReady() const { return packed1.es_onload_ready; }
	void			SetOnLoadReady(BOOL the_truth) { packed1.es_onload_ready = the_truth; }

	void			SetIsDeleted() { packed1.is_deleted = 1; }
	BOOL			IsDeleted() { return packed1.is_deleted; }

	Window*			GetWindow() const { return doc_manager->GetWindow(); }
	DocumentManager*GetDocManager() const { return doc_manager; }
	FramesDocument*	GetCurrentDoc() { return doc_manager->GetCurrentDoc(); }

	OP_STATUS       HandleLoading(OpMessage msg, URL_ID url_id, MH_PARAM_2 user_data);

	void			ReloadIfModified();

#ifdef _PRINT_SUPPORT_
	OP_STATUS		CopyFrames(FramesDocElm* new_frm);
	OP_STATUS		CreatePrintLayoutAllPages(PrintDevice* pd, FramesDocElm* print_root);
#endif // _PRINT_SUPPORT_
	int				CountPages();
#ifdef _PRINT_SUPPORT_
	OP_DOC_STATUS   PrintPage(PrintDevice* pd, int page_num, BOOL selected_only);
#endif // _PRINT_SUPPORT_

	void			UpdateSecurityState(BOOL include_loading_docs);

	BOOL			GetHidden() { return packed1.hidden; }
	void			SetHidden(BOOL val) { packed1.hidden = val; }

	BOOL			IsFrameset() { return packed1.is_frameset; };
	void			SetIsFrameset(BOOL the_truth) { packed1.is_frameset = the_truth; }
	BOOL			GetFrameNoresize() { return packed1.frame_noresize; };
	void			SetFrameNoresize(BOOL noresize) { packed1.frame_noresize = noresize; }
	BYTE			GetFrameScrolling() { return frame_scrolling; };
	void			SetFrameScrolling(BYTE scrolling);
	int				GetFrameMarginWidth() { return frame_margin_width; };
	int				GetFrameMarginHeight() { return frame_margin_height; };
	void			UpdateFrameMargins(HTML_Element *he);

	int				CountFrames();
	FramesDocElm*	GetFrameByNumber(int& num);

	FramesDocElm*	LastLeafActive();
	FramesDocElm*	ParentActive();
	FramesDocElm*	FirstChildActive();
	FramesDocElm*	LastChildActive();
	FramesDocElm*	PrevActive();
	FramesDocElm*	NextActive();

#ifdef SVG_SUPPORT
	/**
	 * Returns whether this is a SVG resource document. The SVG module
	 * should be notified when such documents has finished loading.
	 */
	BOOL			IsSVGResourceDocument() const { return packed1.is_svg_resource; }

	/**
	 * Mark this document as a SVG resource document. @see
	 * IsSVGResourceDocument().
	 */
	void			SetAsSVGResourceDocument() { packed1.is_svg_resource = 1; }
#endif

	/**
	 * Get the 'notify parent on content change' flag.  This is part of
	 * the -o-content-size feature.  Frames with width/height:
	 * -o-content-size will have this flag set and the parent document
	 * will adapt the size of the frame to the content document.
	 */
	BOOL			GetNotifyParentOnContentChange() const { return packed1.nofity_parent_on_content_change; }

	/**
	 * Set the 'notify parent on size change' flag.
	 */
	void			SetNotifyParentOnContentChange(BOOL notify = TRUE) { packed1.nofity_parent_on_content_change = notify; }

#ifdef _PRINT_SUPPORT_
	FramesDocument*       GetPrintDoc() { return doc_manager->GetPrintDoc(); }
	void			DeletePrintCopy();
	void			SetPrintTwinElm(HTML_Element *twin) { m_print_twin_elm = twin; }
#endif // _PRINT_SUPPORT_

	BOOL			IsBeingDeleted() { return packed1.is_being_deleted; }

	void			AppendChildrenToList(Head* list);

	void			CheckSmartFrames(BOOL on);
	void			ExpandFrameSize(int inc_width, int inc_height);

	void			CalculateFrameSizes(int frames_policy);

	void			SetNormalRow(BOOL value) { packed1.normal_row = value; }
	void			CheckFrameStacking(BOOL stack_frames);
	BOOL			HasExplicitZeroSize();

	void			CheckERA_LayoutMode();
	LayoutMode		GetLayoutMode() const;
	void			CheckSpecialObject(HTML_Element* he);

	BOOL			IsSpecialObject() const { return packed1.special_object; }

	OP_STATUS		OnlineModeChanged();

	void			OnRenderingViewportChanged(const OpRect &rendering_viewport);

	FrameReinitData*
					GetReinitData() { return reinit_data; }
	OP_STATUS		SetReinitData(int history_num, BOOL visible, LayoutMode frame_layout_mode);
	void			RemoveReinitData();

	// an interface of Tree class needed by other code parts
	void			Under(FramesDocElm* elm);
	void			Into(FramesDocElm* elm) { Under(elm); }

	// when any of the below versions is called we can skip checking whether the tree is a (i)frame root as we are sure it's not (it's not type of FramesDocElm).
	void			Under(Tree* tree) { Tree::Under(tree); }
	void			Into(Tree* tree) { Tree::Into(tree); }
	void			Into(Head *list) { Tree::Into(list); }
	void			Out();
	BOOL			Empty() { return Tree::Empty(); }
	/** Specifically designed for use with the list of deleted iframes in FramesDocument. */
	static FramesDocElm* GetFirstFramesDocElm(Tree* tree_root) { return static_cast<FramesDocElm*>(tree_root->FirstChild()); }

	/**
	 * Checks whether a frame is allowed to have any content.
	 */
	BOOL IsContentAllowed();

#ifdef NEARBY_INTERACTIVE_ITEM_DETECTION
	/**
	 * Helper method used to add frame's scrollbars to interactive items list.
	 * The method has the following assumptions:
	 * - this frame has current doc.
	 * - this frame is visible in the top document's visual viewport
	 * - the rect_of_interest is fully inside the part of this frame that is intersecting the top document's visual viewport
	 * @param[in/out] rect_of_interest rectangle in the coordinates of this frame's current document.
	 *								   Any scrollbar, which cover the document's area that intersect the
	 *								   rect of interest, will be added. After successful call the rect will be limited
	 *								   to the maximum part that does not intersect any scrollbar.
	 * @param list list to insert the scrollbars into.
	 * @return OpStatus::OK or OpStatus::ERR_NO_MEMORY. */
	OP_STATUS		AddFrameScrollbars(OpRect& rect_of_interest, List<InteractiveItemInfo>& list);
#endif // NEARBY_INTERACTIVE_ITEM_DETECTION

	/**
	 * Sets the current OpInputContext (associated with some layout object), which is the parent
	 * of this iframe or restores the parent OpInputContext of this iframe's VisualDevice
	 * to the VisualDevice of the parent document (if the context is NULL).
	 */
	void			SetParentLayoutInputContext(OpInputContext* context);
};

#endif // DOCHAND_FDELM_H
