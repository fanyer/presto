#ifndef OP_TEXTCOLLECTION_H
#define OP_TEXTCOLLECTION_H

#include "modules/widgets/optextfragmentlist.h"
#include "modules/widgets/OpWidget.h"
#include "modules/widgets/UndoRedoStack.h"
#ifdef INTERNAL_SPELLCHECK_SUPPORT
#include "modules/spellchecker/opspellcheckerapi.h"
#endif // INTERNAL_SPELLCHECK_SUPPORT
#include "modules/display/vis_dev.h"

class OpTextCollection;
class OpTCBlock;
class VisualDevice;

#ifdef MULTILABEL_RICHTEXT_SUPPORT
/**
 * Bit flags of style combinations possible in MultiEdit
 */
enum {
	OP_TC_STYLE_NORMAL    = 0,
	OP_TC_STYLE_BOLD      = 1,
	OP_TC_STYLE_ITALIC    = 2,
	OP_TC_STYLE_UNDERLINE = 4,
	OP_TC_STYLE_HEADLINE  = 8,
	OP_TC_STYLE_LINK      = 16,
	OP_TC_STYLE_SMALL     = 32
};
#endif // MULTILABEL_RICHTEXT_SUPPORT

/** Return status for the OpTCBlockTraverser. Makes it possible to skip traversing when we don't need it. */

enum OP_TC_BT_STATUS {
	OP_TC_BT_STATUS_STOP_ALL,		///< All traversing can be stopped. Both current line and following lines.
	OP_TC_BT_STATUS_STOP_LINE,		///< The current line can be stopped.
	OP_TC_BT_STATUS_CONTINUE		///< The current line must be continued.
};

/** baseclass for traversing through a OpTCBlock. */

class OpTCBlockTraverser
{
public:
	OpTCBlockTraverser() {}
	virtual ~OpTCBlockTraverser() {}

	/** Handle word. If collapsed is TRUE, the word should not be visible and not take up any space on the line. */
	virtual OP_TC_BT_STATUS HandleWord(OP_TCINFO* info, OP_TEXT_FRAGMENT* frag, BOOL collapsed) { return OP_TC_BT_STATUS_CONTINUE; }

	/** Handle line. Called at the end of a line.
		Called for both wrapped lines, or terminated. Might not be called, if HandleWord returns a STOP status.
		@param first_fragment_idx The index of the first fragment in the line.
		@param num_fragments The number of fragments on the line. */
	virtual void HandleLine(OP_TCINFO* info, int first_fragment_idx, int num_fragments) { }
public:
	OpTCBlock* block;
	int translation_x;	///< The current x position during traverse, relative to the block, including justify.
	int translation_y;	///< The current y position during traverse, relative to the block.
	int block_width;	///< Width of the block, including justify. Only reliable when traversing is finished.
	int block_height;	///< Height of the block. Only reliable when traversing is finished.
};

#ifdef MULTILABEL_RICHTEXT_SUPPORT
/** Style struct used to inform about applied style to part of the text in OpTextCollection. */
struct OP_TCSTYLE {
	UINT32    position;  ///< Position of the text where the style should be applied from
	UINT32    style;     ///< Style to be applied
	uni_char *param;	 ///< Alternative string parameter of the "style". Available for OP_TC_STYLE_LINK
	int       param_len; ///< Length of the param parameter;
};
#endif // MULTILABEL_RICHTEXT_SUPPORT

/** Info about the OpTextCollection. Passed as parameter to many functions to minimize parameter count. */

struct OP_TCINFO {
	OpTextCollection* tc;			///< Pointer to the OpTextCollection.
	VisualDevice* vis_dev;			///< Pointer to the VisualDevice that the OpTextCollection is being used in.
	BOOL wrap;						///< If the lines should wrap.
    BOOL aggressive_wrap; ///< True if text should be broken mid-word to avoid scrolling
	BOOL rtl;						///< If the widget is in RIGHT-TO-LEFT direction.
	BOOL readonly;					///< If the widget is readonly
	BOOL overstrike;				///< If the caret should overwrite following characters when typing.
	BOOL selectable;				///< If the text should be selectable.
	BOOL show_selection;			///< If selected text should be shown selected (or just like normal unselected text)
	BOOL show_caret;				///< If the caret should be shown.
	UINT32 font_height;				///< The height of the font in pixels
#ifdef COLOURED_MULTIEDIT_SUPPORT
	BOOL coloured;					///< If the widget should be syntax highlighted in different colors.
#endif
#ifdef MULTILABEL_RICHTEXT_SUPPORT
	OP_TCSTYLE **styles;			///< Array of styles that should be applied to given text
	UINT32       style_count;		///< Count of items in styles array
#endif // MULTILABEL_RICHTEXT_SUPPORT
#ifdef WIDGETS_IME_SUPPORT
	int ime_start_pos;				///< Global offset for the start of the IME string when composing.
	OpInputMethodString* imestring;	///< Pointer to the OpInputMethodString if composing or NULL.
#endif
	JUSTIFY justify;				///< The current justify of the text content.
	FramesDocument* doc;
	int original_fontnumber;		///< The font number that should be used as base (fontswitching might select other fonts that look like this font)
    WritingSystem::Script preferred_script;

    BOOL use_accurate_font_size; ///< if TRUE, use font at actual zoom level to measure text
};

/** A listener for OpTextCollection. This is the connection between the widget and the textcollection. */

class OpTCListener
{
public:
	virtual ~OpTCListener() {}
	/** Called when the content has changed size, as a result of a content change. */
	virtual void TCOnContentResized() = 0;
	/** Called before the content is about to change. */
	virtual void TCOnContentChanging() = 0;
	/** Called when a area needs to be repainted. */
	virtual void TCInvalidate(const OpRect& rect) = 0;
	/** Called when everything needs to be repainted */
	virtual void TCInvalidateAll() = 0;
	/** Called when the OpTextCollection need information from the widget. */
	virtual OP_TCINFO* TCGetInfo() = 0;
	/** Called when direction change is detected as a result of a content change if automatic detection
		is enabled (See OpTextCollection::SetAutodetectDirection) */
	virtual void TCOnRTLDetected(BOOL is_rtl) {}
#ifdef INTERNAL_SPELLCHECK_SUPPORT
	/** Called when spellcheck has changed content. */
	virtual void TCOnTextCorrected() = 0;
#endif
};

/** Offset in a OpTextCollection (Such as caret and selectionstart/stop). Uses a offset relative to a OpTCBlock. */

class OpTCOffset
{
public:
	OpTCOffset() : block(NULL), ofs(0) {}
	OpTCOffset(OpTCBlock* block, int ofs) : block(block), ofs(ofs) {}

	int GetGlobalOffset(OP_TCINFO* info) const;
	void SetGlobalOffset(OP_TCINFO* info, int newofs);

	/* The normalized versions count CRLF line terminators as a single
	   character, both when reporting offset and when setting a new
	   'normalized' offset. Needed to support the script-level
	   view of a textarea's value as being LF-normalized. */
	int GetGlobalOffsetNormalized(OP_TCINFO* info) const;
	void SetGlobalOffsetNormalized(OP_TCINFO* info, int newofs);

	OpPoint GetPoint(OP_TCINFO* info, BOOL snap_forward = FALSE);
//	void SetPoint(OP_TCINFO* info, const OpPoint& point);

	int IsRTL(OP_TCINFO* info, BOOL snap_forward);

	BOOL operator == (const OpTCOffset& other) const;

	/**
	 * Clears this object block pointer and sets ofs to 0
	 */
	void Clear(){ block = NULL; ofs = 0; }
public:
	OpTCBlock* block;
	int ofs;
};

/** A OpTCBlock is a block of text in a OpTextCollection.
	It may present several lines of text if wrapping is on.
	It handles layout of the text with bidi/fontswitching etc.
	The content can be traversed with the different versions of OpTCBlockTraverser. */

class OpTCBlock : public Link
{
public:
	OpTCBlock();
	~OpTCBlock();

	/** Remove text from this block and update layout. */
	OP_STATUS RemoveText(int pos, int len, OP_TCINFO* info);
	/** Set text on this block and update layout. */
	OP_STATUS SetText(const uni_char* newtext, int len, OP_TCINFO* info);
	/** Insert text at the given position in this block and update layout. If the
		text contains new lines, new blocks will be created following this block. */
	OP_STATUS InsertText(int pos, const uni_char* newtext, int len, OP_TCINFO* info);

	/** Split this block at the given position and update layout, so a new block will follow this block with the second half of the content. */
	OP_STATUS Split(int pos, OP_TCINFO* info);
	/** Merge this block with the following block and update layout. */
	OP_STATUS Merge(OP_TCINFO* info);

	/** Update all textfragments for this block and call Layout. */
	OP_STATUS UpdateAndLayout(OP_TCINFO* info, BOOL propagate_height);

	/** Relayout this block. If propagate_height is TRUE, the following blocks will be updated with
		new position and invalidated if this block changed its height. */
	void Layout(OP_TCINFO* info, int virtual_width, BOOL propagate_height);

	/** Traverse this block with the given traverse object.
	 * @param visual_order TRUE if the traverse should be in visual order (differs from logical order with BIDI content)
	 * @param layout TRUE only if it is a layout traverse.
	 * @param line For optimization you can specify a line to start traverse from. Or just specify 0.
	 */
	void Traverse(OP_TCINFO* info, OpTCBlockTraverser* traverser, BOOL visual_order, BOOL layout, int line);

	/** Invalidate and update position of blocks. If propagate_height is TRUE, the following blocks will be updated with
		new position and invalidated if this block changed its height. */
	void UpdatePosition(OP_TCINFO* info, int new_w, int new_h, BOOL propagate_height);

	/** Return string pointed for this block starting at the given offset. */
	const uni_char* CStr(int ofs = 0);

	/** Get start line for traverse from offset (relative to this block), or 0 if there where no cached info. */
	int GetStartLineFromOfs(OP_TCINFO* info, int ofs);

	/** Get start line for traverse from y position (relative to this block), or 0 if there where no cached info. */
	int GetStartLineFromY(OP_TCINFO* info, int y);
#ifdef MULTILABEL_RICHTEXT_SUPPORT
protected:
	virtual void SetStyle(OP_TCINFO* info, OP_TEXT_FRAGMENT* frag);
public:
	const uni_char* GetLinkForOffset(OP_TCINFO* info, int ofs);
#endif // MULTILABEL_RICHTEXT_SUPPORT
public:
	int y;
	int block_width;
	int block_height;
	int text_len;
	OpString text;
	OpTextFragmentList fragments;
	struct LINE_CACHE_INFO {
		INT32 start;
		INT32 width;
	};
	LINE_CACHE_INFO* line_info; // Which fragment each line is started with.
	short num_lines;
private:
	OP_STATUS InsertTextInternal(int pos, const uni_char* newtext, int len, OP_TCINFO* info);
};

/**
	Handles layout and editing of text over multiple lines.
	This is not a OpWidget, but the main part of the OpMultilineEdit widget (and other multiline widgets).
*/

#ifdef INTERNAL_SPELLCHECK_SUPPORT

class TextCollectionWordIterator : public OpSpellCheckerWordIterator
{
public:
	TextCollectionWordIterator(OpTextCollection *collection, BOOL used_by_session) { m_collection = collection; m_used_by_session = used_by_session; Reset(); }
	virtual ~TextCollectionWordIterator() {}
	void Reset();
	// Sets the range of the iterator. NOTE: SetRange() leaves the word iterator active on success. It is the callers responsibility to ensure that CheckText() is called with the iterator.
	void SetRange(OpTCBlock *first, OpTCBlock *last, BOOL goto_first_word = TRUE);
	// Sets the iterator to the word at location. NOTE: SetAtWord() leaves the word iterator active on success. It is the callers responsibility to ensure that CheckText() is called with the iterator.
	void SetAtWord(OpTCOffset location);
	BOOL Active() { return m_active; }

	// Returns true if the current word has been invalidated. I.e the range of OnBlocksInvalidated() covered the current word, so it is no longer valid.
	// Note that the spellcheck might still be working with the invalidated word. (But once ContinueIteration() has been called it will get into a valid range again).
	BOOL WordInvalid() { return m_word_invalid; }
	OpTCOffset CurrentStart() { return m_current_start; }
	OpTCOffset CurrentStop() { return m_current_stop; }

	void OnBlocksInvalidated(OpTCBlock *start, OpTCBlock *stop);

	// <<<OpSpellCheckerWordIterator>>>
	virtual const uni_char *GetCurrentWord() { return m_current_string.IsEmpty() ? UNI_L("") : m_current_string.CStr(); }
	virtual BOOL ContinueIteration();

private:
	OpTextCollection *m_collection;
	BOOL m_active;
	BOOL m_single_word;
	BOOL m_used_by_session;
	BOOL m_word_invalid;

	OpTCOffset m_current_start, m_current_stop;
	OpTCBlock *m_first, *m_last;
	OpString m_current_string;
};

struct TextCollectionWordIteratorSet
{
	TextCollectionWordIteratorSet(OpTextCollection *collection)
		: word_iterator(collection, TRUE), replace_word(collection, FALSE), background_updater(collection, FALSE) {}
	TextCollectionWordIterator word_iterator; ///< Used by spellchecker to peform inline spellchecking
	TextCollectionWordIterator replace_word; ///< Used to record the word which should possibly be replaced
	TextCollectionWordIterator background_updater; ///< Used for updating correct/misspelled words after words have been added/removed from dictionary
};

#endif // INTERNAL_SPELLCHECK_SUPPORT

class OpTextCollection
#ifdef INTERNAL_SPELLCHECK_SUPPORT
	: public SpellUIHandler
#endif // INTERNAL_SPELLCHECK_SUPPORT
{
public:
	OpTextCollection(OpTCListener* listener);
	~OpTextCollection();

	/** Set caret offset from a point (relative to the first block) */
	BOOL SetCaretPos(const OpPoint& pos, BOOL bounds_check_pos_y = TRUE);
	/** Set caret offset from a OpTCOffset */
	void SetCaret(OpTCOffset ofs);
	/** Set caret offset relative to its current block */
	void SetCaretOfs(int ofs);
	/** Set caret offset relative to start of first block */
	void SetCaretOfsGlobal(int global_ofs);
	/** Set caret offset relative to start of first block, but
	    the offset being a line-terminator normalized offset. */
	void SetCaretOfsGlobalNormalized(int global_ofs);

	/** Moves caret between elements */
	void MoveCaret(BOOL right, BOOL visual_order);
	void MoveToNextWord(BOOL right, BOOL visual_order);
	void MoveToStartOrEndOfWord(BOOL right, BOOL visual_order);
	void MoveToStartOrEndOfLine(BOOL start, BOOL visual_order);

	/** Move one of the selectionpoints to new position (As when holding shift+arrows to select).
		The start or stop (whatever matches old_caret_pos) will move to new_caret_pos. */
	void SelectToCaret(INT32 old_caret_pos, INT32 new_caret_pos);
	/** Set selection start and stop from 2 OpTCOffset */
	void SetSelection(OpTCOffset ofs1, OpTCOffset ofs2);
	/** Set selection start and stop from 2 points */
	void SetSelection(OpPoint from, OpPoint to);

	/** Set selection start and stop from global offsets.
	    If you know that it is valid offsets, set check_boundary
	    to FALSE to optimize. line_normalized control the
	    interpretation of the offset values, TRUE if the offset
	    is assumed to be line terminator-normalized
	    (i.e., CRLF will count as one, not two.) */
	void SetSelection(INT32 char_start, INT32 char_stop, BOOL check_boundary = TRUE, BOOL line_normalized = FALSE);

	void SelectAll();
	void SelectNothing(BOOL invalidate);

	/** Set the visible size that the content should wrap within, if wrapping is enabled.
		It will also act as a clipping for some invalidation and stuff.
		No relayout or anything will be performed from this function.
		Returns TRUE if it changed so a reformat of the content is needed. */
	BOOL SetVisibleSize(INT32 width, INT32 height);

        /** for limited paragraph width */
        BOOL SetLayoutWidth(INT32 width);

	/** Set the text and place the caret at the last position.
	 * @param text             text to be set
	 * @param text_len         number of chars in text
	 * @param use_undo_stack   argument which tells if this call to
	 *                         SetText should generate a undo event
	 *                         which allows the user to undo this
	 *                         text entry later
	 */
	OP_STATUS SetText(const uni_char* text, INT32 text_len, BOOL use_undo_stack);

	/** Get the length of the text. If insert_linebreak is TRUE, a linebreak will be counted for every place where a line wraps. */
	INT32 GetTextLength(BOOL insert_linebreak) const;

	/** Get maximum max_len characters of text to buf, starting at offset.
		If insert_linebreak is TRUE, a linebreak will be inserted for every place where a line wraps. */
	INT32 GetText(uni_char* buf, INT32 max_len, INT32 offset, BOOL insert_linebreak) const;

        /** Compare contents of text collection to str
         @param str the string to compare to (used as second arg to uni_strcmp)
         @param max_len maximum number of characters to compare
         @param offset offset from start of contents of text collection
        */
	INT32 CompareText(const uni_char* str, INT32 max_len = -1, UINT32 offset = 0) const;

	/**
	 * Inserts text in the current text position.
	 */
	OP_STATUS InsertText(const uni_char* text, INT32 text_len, BOOL use_undo_stack = TRUE, BOOL can_append_undo = TRUE);

	/**
	 * Reformat the content. If update_fragment_list is TRUE, the textfragmentlist will do a full update. That
	 * is only needed if the bidi-direction has changed, or the font or text has changed.
	*/
	OP_STATUS Reformat(BOOL update_fragment_list = FALSE);

	void Paint(UINT32 col, UINT32 fg_col, UINT32 bg_col, UINT32 link_col, VD_TEXT_HIGHLIGHT_TYPE selection_highlight_type, const OpRect& rect, bool paint_styled = false);

	/** Remove the selected text. Put is in the undostack if use_undo_stack is TRUE.
		reformat_block is currently unused but should be TRUE. */
	OP_STATUS RemoveSelection(BOOL use_undo_stack, BOOL reformat_block);

#ifdef WIDGETS_UNDO_REDO_SUPPORT
	OP_STATUS SaveSelectionOnUndoStack();
#endif // WIDGETS_UNDO_REDO_SUPPORT
	uni_char* GetSelectionText();			///< YOU must free the data when you are done.

	INT32 GetSelectionTextLen();
	BOOL HasSelectedText();

	/** BeginChange should be called before changes to the content are being made, if invalidate_spell_data==TRUE,
		then should the blocks where the selction or caret currently is be spellchecked. */
	void BeginChange(BOOL invalidate_spell_data = FALSE);

	/** EndChange should be called after changes to the content has been made, spell_data_invalidated should only
		be TRUE when BeginChange was called with invalidate_spell_data == TRUE. */
	void EndChange(BOOL spell_data_invalidated = FALSE);

	/** Invalidate block */
	void InvalidateBlock(OpTCBlock* block);
	/** Invalidate all blocks from (and including) first to (and including) last */
	void InvalidateBlocks(OpTCBlock* first, OpTCBlock* last);
	/** Invalidate everything between the given offsets */
	void InvalidateDiff(OpTCOffset* ofs1, OpTCOffset* ofs2);

	/** Invalidate the caret at its current position */
	void InvalidateCaret();

	/** Update the the caret position. It will automatically invalidate the old and new position. */
	void UpdateCaretPos();

	/** Enable or Disable automatic detection of direction change. Direction change is signaled to OpTCListener::TCOnRTLDetected */
	void SetAutodetectDirection(BOOL autodetect) { autodetect_direction = autodetect; }

	OpTCBlock* FirstBlock() const { return (OpTCBlock*) blocks.First(); }
	OpTCBlock* LastBlock() const { return (OpTCBlock*) blocks.Last(); }

	/** Find the block that contains point */
	OpTCBlock* FindBlockAtPoint(const OpPoint& point);

	/** Get a OpTCOffset from the given point. If snap_forward is not NULL, it will be set to TRUE or FALSE depending
		of which fragment that is the closest (for offsets that share the same visual position). */
	OpTCOffset PointToOffset(const OpPoint& point, BOOL* snap_forward = NULL, BOOL bounds_check_point_y = TRUE);

	/** Called when the TextCollection receives or lose focus. */
	void OnFocus(BOOL focus);

#ifdef INTERNAL_SPELLCHECK_SUPPORT

	virtual void OnSessionReady(OpSpellCheckerSession *session);
	virtual void OnError(OpSpellCheckerSession *session, OP_STATUS error_status, const uni_char *error_string);
	virtual void OnMisspellingFound(OpSpellCheckerSession *session, OpSpellCheckerWordIterator *word, const uni_char **replacements);
	virtual void OnCorrectSpellingFound(OpSpellCheckerSession *session, OpSpellCheckerWordIterator *word);
	virtual void OnCheckingComplete(OpSpellCheckerSession *session);
	virtual OpSpellCheckerSession* GetSpellCheckerSession() { return m_spell_session; }
	virtual BOOL SetSpellCheckLanguage(const uni_char *lang);
	virtual void DisableSpellcheck();
	virtual OpSpellCheckerWordIterator* GetAllContentIterator();
	virtual OP_STATUS ReplaceWord(OpSpellCheckerWordIterator *word_iterator, const uni_char *str);

	/** If blocks has been changed (OnBlocksInvalidated() has been called), this will post a message causing the spellchecker to check the changed blocks */
	OP_STATUS RunPendingSpellcheck();

	/** Called when the contents between start and stop should change, will cause a spellchecking when RunPendingSpellcheck() is called */
	void OnBlocksInvalidated(OpTCBlock *start, OpTCBlock *stop);

	/** If spellchecking has been delayed (because we don't want red lines under a word the user currently types),
		this function will start the spellcheck if the caret has moved out of that word. */
	void OnCaretChange(OpTCOffset old_caret, OpTCOffset new_caret);

	/** Sets the WordInfo::is_misspelling flag of the text fragments in block to the value of misspelled, between character offsets start_ofs and stop_ofs */
	void MarkMisspellingInBlock(BOOL misspelled, OpTCBlock *block, int start_ofs, int stop_ofs);

	/** Enable the spellchecker */
	OP_STATUS EnableSpellcheckInternal(BOOL by_user, const uni_char *lang);
	/** Disables the spellchecker */
	void DisableSpellcheckInternal(BOOL by_user, BOOL force);

	BOOL SpellcheckByUser() { return m_by_user; }

	/** Creates a SpellUISession and records the word located at point, so this word could later be e.g. replaced with a correct word (if it was misspelled) */
	OP_STATUS CreateSpellUISessionInternal(const OpPoint &point, int &spell_session_id);
	OP_STATUS CreateSpellUISession(const OpPoint &point, int &spell_session_id);

#endif // INTERNAL_SPELLCHECK_SUPPORT

	/** returns true if artificial word wrapping (because of css
		property "overflow-wrap: break-word") has been applied. this is
		used from OpMultiEdit::TCOnContentResized() to determine if
		we need to update the text fragment list when reformatting
		the text collection, which is costly.
	*/
	BOOL HasBeenSplit();

	/** Gets a list of rectangles a text selection consist of (if any) and a rectangle being a union of all rectangles in the list.
	 *
	 * @param list - a pointer to the list to be filled in.
	 * @param union_rect - a union of all rectangles in the list.
	 * @param offset - an offset of the text.
	 * @param text_rect - a rectangle the selection should be search within (in edit's coordinates).
	 */
	OP_STATUS GetSelectionRects(OpVector<OpRect>* rects, OpRect& union_rect, const OpPoint& offset, const OpRect& text_rect);

	/** Checks if a given point is contained by any of selection's rectangle.
	 *
	 * @param point - a point to be checked.
	 * @param offset - an offset of the text.
	 * @param text_rect - a rectangle the selection should be search within (in edit's coordinates).
	 */
	BOOL IsWithinSelection(const OpPoint& point, const OpPoint& offset, const OpRect& text_rect);

public: // public, but READONLY!
	Head blocks;

	OpTCOffset sel_start;
	OpTCOffset sel_stop;
	OpTCOffset caret;

	/** The logical offset of the caret may have 2 different visual positions, in the case when it is at a edge of a RTL and LTR fragment.
		If this flag is TRUE, it is the second fragment that is the current position. */
	BOOL caret_snap_forward;
	OpPoint caret_pos;

	INT32 line_height;
	INT32 visible_width;
	INT32 visible_height;
	INT32 m_layout_width; ///< requested layout width for limit paragraph width
	INT32 total_width;
	INT32 max_block_width;
	INT32 total_height;
	INT32 total_width_old;
	INT32 total_height_old;

	BOOL autodetect_direction;

	OpTCListener* listener;

#ifdef WIDGETS_UNDO_REDO_SUPPORT
	UndoRedoStack undo_stack;
#endif

#ifdef INTERNAL_SPELLCHECK_SUPPORT
	UINT16 m_spell_change_count; ///< Used for not taking any action on nestled calls to BeginChange(invalidate_spell_data == TRUE) and EndChange(spell_data_invalidated == TRUE)
	unsigned short m_delayed_spell : 4; ///< Is set to TRUE when spellchecking is postponed because the user currently writes a word
	unsigned short m_has_spellchecked : 4; ///< Will be set to TRUE after any word has been marked as misspelled, for not having to clear all is_misspelling flags when disabling spellcheck (hmm, not so very necessary "optimization" perhaps...)
	unsigned short m_has_pending_spellcheck : 8; ///< Indicates that content has been changed so that we should start the spellchecker soon.
	TextCollectionWordIteratorSet *m_iterators;
	OpSpellCheckerSession *m_spell_session; ///< The session with the spellchecker when spellchecking is enabled.
	OpTCBlock *m_pending_spell_first, *m_pending_spell_last; ///< If m_has_pending_spellcheck == TRUE, indicates which blocks that should be spellchecked when RunPendingSpellcheck() is called.
	BOOL m_by_user;
#endif // INTERNAL_SPELLCHECK_SUPPORT
};

// == Some traversal objects =======================================

/** Resolves bidi-levels across all lines of the block. */
class LayoutTraverser : public OpTCBlockTraverser
{
public:
	void HandleLine(OP_TCINFO* info, int first_fragment_idx, int num_fragments);
};

/** Paints the block. */
class PaintTraverser : public OpTCBlockTraverser
{
#ifdef MULTILABEL_RICHTEXT_SUPPORT
protected:
	void SetStyle(OP_TCINFO* info, OP_TEXT_FRAGMENT* frag);
	BOOL IsLink(OP_TCINFO* info, OP_TEXT_FRAGMENT* frag);
	void DrawUnderline(OP_TCINFO* info, OP_TEXT_FRAGMENT* frag, int x, int y);
#endif // MULTILABEL_RICHTEXT_SUPPORT
public:
    PaintTraverser(const OpRect& rect, UINT32 col, UINT32 fg_col, UINT32 bg_col, UINT32 link_col, VD_TEXT_HIGHLIGHT_TYPE selection_highlight_type, UINT32 base_ascent);
	OP_TC_BT_STATUS HandleWord(OP_TCINFO* info, OP_TEXT_FRAGMENT* frag, BOOL collapsed);
	void HandleLine(OP_TCINFO* info, int first_fragment_idx, int num_fragments);

	OpRect rect;
	UINT32 col;
	UINT32 fg_col;
	UINT32 bg_col;
	UINT32 link_col;
	VD_TEXT_HIGHLIGHT_TYPE selection_highlight_type;
	int block_ofs;
	UINT32 base_ascent; ///< ascent of base font - used to align text to baseline
};

class SelectionTraverser : public OpTCBlockTraverser
{
public:
	SelectionTraverser(OpVector<OpRect>* list, OpRect* union_rect, const OpPoint& offset, const OpRect& rect)
		: m_status(OpStatus::OK)
		, m_list(list)
		, m_union_rect(union_rect)
		, m_offset(offset)
		, m_rect(rect)
		, m_contains_point(FALSE)
	{}

	SelectionTraverser(const OpPoint& point, const OpPoint& offset, const OpRect& rect)
		: m_status(OpStatus::OK)
		, m_list(NULL)
		, m_union_rect(NULL)
		, m_offset(offset)
		, m_rect(rect)
		, m_point(point)
		, m_contains_point(FALSE)
	{}
	OP_TC_BT_STATUS HandleWord(OP_TCINFO* info, OP_TEXT_FRAGMENT* frag, BOOL collapsed);
	void HandleLine(OP_TCINFO* info, int first_fragment_idx, int num_fragments);

	OP_STATUS m_status;
	OpVector<OpRect>* m_list;
	OpRect* m_union_rect;
	OpPoint m_offset;
	OpRect m_rect;
	OpPoint m_point;
	BOOL m_contains_point;
};

/** Get a OpPoint (relative to the block) from a OpTCOffset */
class OffsetToPointTraverser : public OpTCBlockTraverser
{
public:
	OffsetToPointTraverser(OpTCOffset* ofs, BOOL snap_forward);
	OP_TC_BT_STATUS HandleWord(OP_TCINFO* info, OP_TEXT_FRAGMENT* frag, BOOL collapsed);
	void HandleLine(OP_TCINFO* info, int first_fragment_idx, int num_fragments);

	OpPoint point;
	BOOL snap_forward;
	OpTCOffset* ofs;
};

/** Get information for the line where ofs is placed. */
class LineFinderTraverser : public OffsetToPointTraverser
{
public:
	LineFinderTraverser(OpTCOffset* ofs, BOOL snap_forward);
	OP_TC_BT_STATUS HandleWord(OP_TCINFO* info, OP_TEXT_FRAGMENT* frag, BOOL collapsed);
	void HandleLine(OP_TCINFO* info, int first_fragment_idx, int num_fragments);

	int line_nr;
	int line_first_fragment_idx;
	int line_num_fragments;
	BOOL found_word;
	BOOL found_line;
};

/** Get a OpTCOffset from a OpPoint (relative to the block) */
class PointToOffsetTraverser : public OpTCBlockTraverser
{
public:
	PointToOffsetTraverser(const OpPoint& point);
	OP_TC_BT_STATUS HandleWord(OP_TCINFO* info, OP_TEXT_FRAGMENT* frag, BOOL collapsed);
	void HandleLine(OP_TCINFO* info, int first_fragment_idx, int num_fragments);

	OpPoint point;
	OpTCOffset ofs;
	BOOL snap_forward;
	OP_TEXT_FRAGMENT* line_first_frag;
	OP_TEXT_FRAGMENT* line_last_frag;
};

/** Get text from the block into the given buf. Insert linebreaks for each line if insert_linebreak is TRUE.
	len will be the number of characters that was needed. buf can be NULL to only get the length. */
class GetTextTraverser : public OpTCBlockTraverser
{
public:
  /** creates a GetTextTraverser instance.
      @param buf (out) the target buffer. can be zero, in which case the length is computed but nothing is copied
      @param insert_linebreak if TRUE, linebreaks will be inserted for each line
      @param max_len the maximum number of characters to process.
      @note specifying a max_len that corresponds to the buffer size might still lead to buffer overflows, since writing is started at GetTextTraverser::len
      @param offset offset into the text where processing is to commence */
	GetTextTraverser(uni_char* buf, BOOL insert_linebreak, int max_len, int offset);
	/** process one word
	 @param currently unused
	 @param frag the text fragment to process
	 @param collapsed currently unused */
	OP_TC_BT_STATUS HandleWord(OP_TCINFO* info, OP_TEXT_FRAGMENT* frag, BOOL collapsed);
	void HandleLine(OP_TCINFO* info, int first_fragment_idx, int num_fragments);

	uni_char* buf;
	int len;
	BOOL insert_linebreak;
	int max_len, offset;
};

#endif // OP_TEXTCOLLECTION_H
