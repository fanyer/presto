#ifndef MODULES_WIDGETS_OPWIDGETSTRINGDRAWER
#define MODULES_WIDGETS_OPWIDGETSTRINGDRAWER

#include "modules/widgets/OpWidget.h"
#include "modules/display/vis_dev.h" // VD_TEXT_HIGHLIGHT_TYPE

class OpWidgetStringDrawer
{
public:
	OpWidgetStringDrawer();

	void Draw(OpWidgetString* widget_string, const OpRect& rect, VisualDevice* vis_dev, UINT32 color);

	void Draw(OpWidgetString* widget_string, const OpRect& rect, VisualDevice* vis_dev, UINT32 color,
		ELLIPSIS_POSITION ellipsis_position, INT32 x_scroll);

private:
	VD_TEXT_HIGHLIGHT_TYPE selection_highlight_type;
	INT32 caret_pos;
	ELLIPSIS_POSITION ellipsis_position;
	BOOL underline;
	INT32 x_scroll;
	BOOL caret_snap_forward;
	BOOL only_text;

public:
	VD_TEXT_HIGHLIGHT_TYPE GetSelectionHighlightType() const { return selection_highlight_type; }
	void SetSelectionHighlightType(VD_TEXT_HIGHLIGHT_TYPE selection_highlight_type) { this->selection_highlight_type = selection_highlight_type; }

	INT32 GetCaretPos() const { return caret_pos; }
	void SetCaretPos(INT32 caret_pos) { this->caret_pos = caret_pos; }

	ELLIPSIS_POSITION GetEllipsisPosition() const { return ellipsis_position; }
	void SetEllipsisPosition(ELLIPSIS_POSITION ellipsis_position) { this->ellipsis_position = ellipsis_position; }

	BOOL IsUnderline() const { return underline; }
	void SetUnderline(BOOL underline) { this->underline = underline; }

	INT32 GetXScroll() const { return x_scroll; }
	void SetXScroll(INT32 x_scroll) { this->x_scroll = x_scroll; }

	BOOL IsCaretSnapForward() const { return caret_snap_forward; }
	void SetCaretSnapForward(BOOL caret_snap_forward) { this->caret_snap_forward = caret_snap_forward; }

	BOOL IsOnlyText() const { return only_text; };
	void SetOnlyText(BOOL only_text) { this->only_text = only_text; };

};

#endif // MODULES_WIDGETS_OPWIDGETSTRINGDRAWER
