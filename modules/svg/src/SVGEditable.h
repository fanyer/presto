/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2007-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.	It may not be distributed
** under any circumstances.
*/

#ifndef SVG_EDITABLE_H
#define SVG_EDITABLE_H

#ifdef SVG_SUPPORT

#include "modules/svg/svg_number.h"
#include "modules/svg/src/SVGRect.h"
#include "modules/hardcore/timer/optimer.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/inputmanager/inputcontext.h"
#include "modules/layout/traverse/traverse.h"
#include "modules/dragdrop/clipboard_manager.h"

class OpClipboard;
class SVGEditable;
class HTML_Element;
class SVGCanvas;
class SVGTextSelection;
struct SVGLineInfo;

struct SVGEditPoint
{
	SVGEditPoint() : elm(NULL), ofs(0) {}

	BOOL IsValid() const { return elm != NULL; }
	BOOL IsText() const { return elm && elm->Type() == HE_TEXT; }
	BOOL IsValidTextOffset() const
	{
		OP_ASSERT(IsText());
		return ofs >= 0 && ofs <= elm->GetTextContentLength();
	}

	HTML_Element* elm;
	int ofs;
};

struct SVGCaretPoint
{
	SVGCaretPoint() : elm(NULL), ofs(0) {}
	SVGCaretPoint(const SelectionBoundaryPoint& tsp) :
		elm(tsp.GetElement()),
		ofs(tsp.GetElementCharacterOffset()) {}

	BOOL IsValid() const { return elm != NULL; }
	BOOL IsText() const { return elm && elm->Type() == HE_TEXT; }

	HTML_Element* elm;
	int ofs;
};

class SVGEditableCaret : public OpTimerListener
{
public:
	SVGEditableCaret(SVGEditable *edit);
	~SVGEditableCaret();

	enum
	{
		NORMAL_WIDTH = 1,
		OVERSTRIKE_WIDTH = 10
	};

	enum Placement
	{
		PLACE_START,
		PLACE_END,
		PLACE_LINESTART,
		PLACE_LINEEND,
		PLACE_LINEPREVIOUS,
		PLACE_LINENEXT
	};

	OP_STATUS Init(BOOL create_line_if_empty, HTML_Element* edit_root);

	void Invalidate();

	/** Lock or unlock the possibility for the caret to update its
		drawing position. Can be used to optimize, by avoiding to
		update it several times during a operation. */
	void LockUpdatePos(BOOL lock);

	/** Update the drawing position. Returns TRUE if the update is
		done. Returns FALSE if it is postponed. F.ex if it was locked
		or if the document was dirty.  This is done automatically from
		the Place functions. */
	BOOL UpdatePos(BOOL prefer_first = FALSE);

	/** Move the caret forward or back in the document in the logical
		position */
	void Move(BOOL forward, BOOL force_stop_on_break = FALSE);

	/** Move the caret to the next/previous word in the document in the logical
		position */
	void MoveWord(BOOL forward);

	void Place(Placement place);
	void Place(const SVGCaretPoint& cp, BOOL allow_snap = TRUE, BOOL snap_forward = FALSE);

	void PlaceFirst(HTML_Element* edit_root = NULL);

	void Set(const SVGCaretPoint& cp);

	void StickToPreceding();
	void StickToDummy();
	void Paint(SVGCanvas* canvas);

#if 0
	void ToggleOverstrike();
	void ToggleBold();
	void ToggleItalic();
	void ToggleUnderline();
#endif
	void BlinkNow();
	void RestartBlink();
	void StopBlink();

	void FindBoundrary(SVGLineInfo* lineinfo, SVGNumber boundrary_max);

	virtual void OnTimeOut(OpTimer* timer);

public:
	BOOL CheckElementOffset(SVGEditPoint& ep, BOOL forward);

	OpTimer m_timer;
	SVGEditable* m_edit;

	SVGCaretPoint m_point;

	SVGRect m_pos;
	SVGRect m_glyph_pos;
	int m_line;
	BOOL m_on;

	int m_update_pos_lock;
	BOOL m_update_pos_needed;

	BOOL m_prefer_first;
};

class SVGEditableListener
{
public:
	virtual ~SVGEditableListener() {}

	virtual void OnCaretMoved() {}
	virtual void OnTextChanged() {}
};

class SVGEditable : public OpInputContext
#ifdef USE_OP_CLIPBOARD
	, public ClipboardListener
#endif // USE_OP_CLIPBOARD
{
public:
	SVGEditable();
	virtual ~SVGEditable();

	static OP_STATUS		Construct(SVGEditable** obj, HTML_Element* root_elm);

	OP_STATUS				Init(HTML_Element* root_elm);
	OP_STATUS				Clear();										///< Clear all text.

	/** Paint stuff related to editing. Caret and resize-handles for
		boxes/tables etc.  Should be done after document is fully
		painted. */
	void					Paint(SVGCanvas* canvas);
#if 0
	BOOL					HandleMouseEvent(HTML_Element* helm, DOM_EventType event, int x, long y, MouseButton button);
#endif // 0
#ifdef SVG_SUPPORT_TEXTSELECTION
	void					Copy();
	void					Cut();
#endif // SVG_SUPPORT_TEXTSELECTION
	void					Paste();
#ifdef USE_OP_CLIPBOARD
	void					OnPaste(OpClipboard* clipboard);
	void					OnCut(OpClipboard* clipboard);
	void					OnCopy(OpClipboard* clipboard);
#endif // USE_OP_CLIPBOARD

	/* Set if the tab-key should have any effect or not while editing. */
	void					SetWantsTab(BOOL wants_tab) { m_wants_tab = wants_tab; }

	BOOL					SetListener(SVGEditableListener* listener)
	{ m_listener = listener; return TRUE; }
	SVGEditableListener*	GetListener() { return m_listener; }

	HTML_Element*			GetEditRoot() { return m_root; }
	BOOL					IsMultiLine()
	{ return m_root && m_root->Type() == Markup::SVGE_TEXTAREA; }

	/** Inserts linebreak at current caret position. If break_list is
		TRUE, new listitems may be created or deleted depending of
		where the break is inserted.  if new_paragraph is TRUE a new
		paragraph is created if apropriate (not in list etc.),
		Otherwise a BR will be created. */
	void					InsertBreak(BOOL break_list = TRUE, BOOL new_paragraph = FALSE);

	/** Inserts helm at current caret position. */
	void					InsertElement(HTML_Element* helm);

	/** Insert text at current caret position. If allow_append is
		TRUE, the text may be appended to the last event in the
		undo&redo-stack. (But it only will, if it follows the last
		event precisely.) */
	OP_STATUS				InsertText(const uni_char* text, int len, BOOL allow_append = FALSE);

	/** Create a text-element where the caret can be placed */
	OP_STATUS				CreateTemporaryCaretElement();

	/** Scroll to ensure caret is visible. */
	void					ScrollIfNeeded();

	void					ReportOOM();
	void					ReportIfOOM(OP_STATUS status);

#if 0
	/** Check if the logical tree has changed (F.ex. after DOM has
		removed or added elements). If this has happened we have to
		update the caret and the undoredostack. */
	void					CheckLogTreeChanged(BOOL caused_by_user = FALSE);
#endif // 0

	virtual void			OnFocus(BOOL focus, FOCUS_REASON reason);

	/** Should be called when a element is removed from the editable
		document. */
	void					OnElementRemoved(HTML_Element* elm);

#if 0
	/** Should be called when a element is inserted in the editable
		document. */
	void					OnElementInserted(HTML_Element* elm);

	/** Should be called when a element is deleted. */
	void					OnElementDeleted(HTML_Element* elm);

	/** Should be called when scalefactor has been changed, to update
		caret position. */
	void					OnScaleChanged();

	/** Prepare root element for editing */
	void					InitEditableRoot(HTML_Element* root);

	/** Unactivate editing on this element */
	void					UninitEditableRoot(HTML_Element* root);
#endif // 0

	/** Set focus to the editable element (element should be editable) */
	void					FocusEditableRoot(HTML_Element* helm, FOCUS_REASON reason);

	SVG_DOCUMENT_CLASS*		GetDocument();

#ifdef SVG_SUPPORT_TEXTSELECTION
	SVGTextSelection*		GetTextSelection();
	void					SelectToCaret(SVGCaretPoint& cp);
	void					SelectNothing();
#endif // SVG_SUPPORT_TEXTSELECTION
	BOOL					HasSelectedText();

	// == Input =======================
	void					EditAction(OpInputAction* action);
	virtual BOOL			OnInputAction(OpInputAction* action);
	virtual const char*		GetInputContextName() { return "SVG Editable"; }
	virtual void			OnKeyboardInputGained(OpInputContext* new_input_context, OpInputContext* old_input_context, FOCUS_REASON reason);
	virtual void			OnKeyboardInputLost(OpInputContext* new_input_context, OpInputContext* old_input_context, FOCUS_REASON reason);

#if 0 //def WIDGETS_IME_SUPPORT
	virtual IM_WIDGETINFO OnStartComposing(OpInputMethodString* imstring, IM_COMPOSE compose);
	virtual IM_WIDGETINFO OnCompose();
	virtual IM_WIDGETINFO GetWidgetInfo();
	virtual void OnCommitResult();
	virtual void OnStopComposing(BOOL cancel);
#endif

	// == Misc. helpers ========================
	static BOOL IsXMLSpacePreserve(HTML_Element* element);

	SVGCaretPoint ToCaret(const SVGEditPoint& ep);
	SVGEditPoint ToEdit(const SVGCaretPoint& cp);

	static int CalculateLeadingWhitespace(HTML_Element* elm);

	void Select(SelectionBoundaryPoint *startpoint, SelectionBoundaryPoint *endpoint);
	void Invalidate();

	HTML_Element* FindElementBetweenOfType(HTML_Element* start, HTML_Element* end,
										   int type, NS_Type ns);
	HTML_Element* FindElementBeforeOfType(HTML_Element* helm, HTML_ElementType type);
	HTML_Element* FindElementAfterOfType(HTML_Element* helm, Markup::Type type);

	// == Element handling
	HTML_Element* NewTextElement(const uni_char* text, int len);
	HTML_Element* NewElement(Markup::Type type);

	/** Delete helm in a safe manner */
	void DeleteElement(HTML_Element* helm, BOOL check_caret = TRUE)
	{ DeleteElement(helm, this, check_caret); }
	static void DeleteElement(HTML_Element* helm, SVGEditable* edit, BOOL check_caret = TRUE);

	/** Split a (text)element in 2 if ofs is not 0 or at the last
		character. Returns TRUE if the split was done. */
	BOOL SplitElement(SVGCaretPoint& cp);

	BOOL IsStandaloneElement(HTML_Element* helm);
	BOOL KeepWhenTidy(HTML_Element* helm);

	BOOL IsEnclosedBy(HTML_Element* helm, HTML_Element* start, HTML_Element* stop);
	BOOL ContainsTextBetween(HTML_Element* start, HTML_Element* stop);
	void RemoveContent(SVGEditPoint& start, SVGEditPoint& stop);
	void RemoveContentCaret(SVGCaretPoint& start, SVGCaretPoint& stop, BOOL aggressive = TRUE);

	// == Text element handling
	void SetElementText(HTML_Element* helm, const uni_char* text);
	BOOL DeleteTextInElement(SVGEditPoint& ep, int len);

	// == Caret related
	HTML_Element* FindEditableElement(HTML_Element* from_helm, BOOL forward,
									  BOOL include_current, BOOL text_only = FALSE);
	BOOL GetNearestCaretPos(HTML_Element* from_helm, SVGEditPoint& nearest_ep,
							BOOL forward, BOOL include_current, BOOL text_only = FALSE);
	BOOL FindWordStartAndOffset(SVGEditPoint& ep, const uni_char* &word, int& word_ofs,
								BOOL snap_forward, BOOL ending_whitespace = TRUE);
	SelectionBoundaryPoint GetTextSelectionPoint(HTML_Element* helm, int character_offset, BOOL prefer_first = FALSE);

	/**
	 * Shows the caret in given element at given offset.
	 *
	 * @param element The element the caret should be shown in.
	 * @param offset The character offset the caret should be shown at.
	 */
	void ShowCaret(HTML_Element* element, int offset);

	/** Hides the caret. */
	void HideCaret();

public:
	HTML_Element*			m_root;
	SVGEditableCaret		m_caret;
	
#if 0 //def WIDGETS_IME_SUPPORT
	IM_WIDGETINFO GetIMInfo();
	HTML_Element* BuildInputMethodStringElement();
	const OpInputMethodString* m_imstring;
	BOOL im_is_composing;
#endif

private:
	BOOL m_wants_tab;
	SVGEditableListener*	m_listener;
};

#endif // SVG_SUPPORT
#endif // SVG_EDITABLE_H
