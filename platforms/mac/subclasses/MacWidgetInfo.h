#ifndef MAC_WIDGET_INFO_H
#define MAC_WIDGET_INFO_H

#include "modules/skin/IndpWidgetInfo.h"

typedef enum {
	kOperaScrollbarStyleFull,
	kOperaScrollbarStyleSimplified
} OperaScrollbarStyle;

class MacWidgetInfo : public IndpWidgetInfo
{
public:

	/** Se OpWidget.h for parameter details. */
	virtual void GetPreferedSize(OpWidget* widget, OpTypedObject::Type type, INT32* w, INT32* h, INT32 cols, INT32 rows);

	/** Shrinks the rect with the border size */
	virtual void AddBorder(OpWidget* widget, OpRect *rect);

	virtual void GetItemMargin(INT32 &left, INT32 &top, INT32 &right, INT32 &bottom);

	virtual INT32 GetDropdownButtonWidth(OpWidget* widget);

	virtual INT32 GetScrollbarFirstButtonSize();

	virtual INT32 GetScrollbarSecondButtonSize();

	virtual SCROLLBAR_ARROWS_TYPE GetScrollbarArrowStyle();

	virtual UINT32 GetSystemColor(OP_SYSTEM_COLOR color);

	/**
	  Return which direction a scrollbar button points to. Could be used to make scrollbars with f.eks. no button in
	  the top and 2 at the bottom.
	  pos is the mousepos relative to the buttons x position (for a horizontal scrollbar) or y position (for a vertical).
	  Return values should be: 0 for UP (or LEFT),
							   1 for DOWN (or RIGHT)
	*/
	virtual BOOL GetScrollbarButtonDirection(BOOL first_button, BOOL horizontal, INT32 pos);

	virtual BOOL GetScrollbarPartHitBy(OpWidget *widget, const OpPoint &pt, SCROLLBAR_PART_CODE &hitPart);
};

#endif // MAC_WIDGET_INFO_H
