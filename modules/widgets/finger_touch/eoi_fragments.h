/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef EOI_FRAGMENTS_H
#define EOI_FRAGMENTS_H

#include "modules/display/FontAtt.h"
#include "modules/img/image.h"
#include "modules/widgets/OpWidget.h"

struct EoiPaintInfo
{
	EoiPaintInfo(OpWidget *root, COLORREF text_color, VisualDevice* vis_dev, OpPoint anchor_origin, OpRect paint_rect) :
	root(root),
	text_color(text_color),
	vis_dev(vis_dev),
	anchor_origin(anchor_origin),
	paint_rect(paint_rect) { }

	OpWidget *root;
	COLORREF text_color;
	VisualDevice* vis_dev;
	OpPoint anchor_origin;
	OpRect paint_rect;
};

class TextAnchorFragment;

/** Anchor fragment. Used with AnchorRegion.
 *
 * One AnchorFragment represents a fragment of the anchor, and it is either one
 * image, or one piece of text.
 */
class AnchorFragment : public Link
{
protected:
	OpRect rect;
	int x;
	int y;

public:
	AnchorFragment() : x(0), y(0) {}
	virtual ~AnchorFragment() { }

	AnchorFragment* Suc() const { return (AnchorFragment*) Link::Suc(); }
	AnchorFragment* Pred() const { return (AnchorFragment*) Link::Pred(); }

	void ExpandRect(const OpRect& new_rect) { rect.UnionWith(new_rect); }
	const OpRect& GetRect() const { return rect; }
	virtual void SetRect(const OpRect& r, int internal_y) { rect = r; }
	virtual void SetPos(int x, int y) { this->x = x; this->y = y; }
	virtual void OffsetPosBy(int dx, int dy) { this->x += dx; this->y += dy; }

	virtual int GetWidth() { return rect.width; }
	virtual int GetHeight() { return rect.height; }

	virtual OP_STATUS Paint(EoiPaintInfo& paint_info) = 0;

	virtual TextAnchorFragment *GetTextAnchorFragment() { return NULL; }
	virtual BOOL IsImageFragment() { return FALSE; }
};

/** Text fragment of an anchor. */
class TextAnchorFragment : public AnchorFragment
{
private:
	OpWidgetString widgetstring;
	OpMultilineEdit *multi_edit;
	OpString text;
	int max_width;

public:
	TextAnchorFragment() : multi_edit(NULL), max_width(0) {}

	OP_STATUS AppendText(const uni_char* txt, size_t length) { return text.Append(txt, length); }
	const uni_char* GetText() { return text.CStr(); }

	virtual void SetRect(const OpRect& r, int internal_y);
	virtual void OffsetPosBy(int dx, int dy);

	OP_STATUS UpdateWidgetString(OpWidget *widget, int max_width, BOOL wrap);

	virtual int GetWidth();
	virtual int GetHeight();

	virtual OP_STATUS Paint(EoiPaintInfo& paint_info);

	virtual TextAnchorFragment *GetTextAnchorFragment() { return this; }
};

/** Widget in an anchor. */
class WidgetAnchorFragment : public AnchorFragment
{
private:

public:
	WidgetAnchorFragment(FormObject *form_obj) { }

	virtual OP_STATUS Paint(EoiPaintInfo& paint_info) { return OpStatus::OK; }
};

/** Image in an anchor. */
class ImageAnchorFragment : public AnchorFragment
{
private:
	Image img;

public:
	ImageAnchorFragment(const Image& img) : img(img) { }

	virtual OP_STATUS Paint(EoiPaintInfo& paint_info);
	virtual BOOL IsImageFragment() { return TRUE; }
};

#endif // EOI_FRAGMENTS_H
