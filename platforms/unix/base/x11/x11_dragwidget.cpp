/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Espen Sand
*/

#include "core/pch.h"

#include "platforms/unix/base/x11/x11_dragwidget.h"

#include "platforms/unix/base/x11/x11_mdebuffer.h"
#include "platforms/unix/base/x11/x11_mdescreen.h"
#include "platforms/unix/base/x11/x11_opdragmanager.h"
#include "platforms/unix/product/x11quick/iconsource.h"
#include "adjunct/desktop_util/image_utils/iconutils.h"


X11DragWidget::X11DragWidget(X11OpDragManager* drag_manager, DesktopDragObject* drag_object,const OpPoint& hotspot)
	:m_drag_manager(drag_manager)
	,m_drag_object(drag_object)
	,m_hotspot(hotspot)
	,m_drag_color(FALSE)
	,m_color(0)
{
}


X11DragWidget::X11DragWidget(UINT32 color, const OpPoint& hotspot)
	:m_drag_manager(0)
	,m_drag_object(0)
	,m_hotspot(hotspot)
	,m_drag_color(TRUE)
	,m_color(color)
{
}


void X11DragWidget::Move(int x, int y)
{
	X11Widget::Move(m_hotspot.x+x, m_hotspot.y+y);
}


void X11DragWidget::InstallIcon()
{
	OpAutoPtr<OpBitmap> bitmap_owner; // For auto destruction when needed
	OpBitmap* bitmap = 0;
	UINT8 opacity = 0xFF;

	if (m_drag_color)
	{
		OpBitmap::Create(&bitmap, 12, 12, FALSE, TRUE, 0, 0, TRUE);
		if (bitmap)
		{
			bitmap_owner = bitmap;
			OpPainter* painter = bitmap->GetPainter();
			if (painter)
			{
				OpRect rect(0, 0, bitmap->Width(), bitmap->Height());
				painter->SetColor(0x00, 0x00, 0x00, 0xFF);
				painter->FillRect(rect);
				painter->SetColor(OP_GET_R_VALUE(m_color), OP_GET_G_VALUE(m_color), OP_GET_B_VALUE(m_color), 0xFF);
				painter->FillRect(rect.InsetBy(1,1));
				bitmap->ReleasePainter();
			}
		}
	}
	else if (m_drag_object && m_drag_object->GetBitmap() && m_drag_object->GetType() != OpTypedObject::DRAG_TYPE_LINK)
	{
		bitmap = const_cast<OpBitmap*>(m_drag_object->GetBitmap()); // We need a non-const bitmap. It is not modified
		opacity = 0xC0;
	}
	else
	{
		UINT8* buffer;
		UINT32 size;
		if (IconSource::GetDragAndDropIcon(buffer, size) )
		{
			bitmap = IconUtils::GetBitmap(buffer, size, -1, -1);
			bitmap_owner = bitmap;
		}
	}

	if (bitmap)
	{
		UINT32 width  = bitmap->Width();
		UINT32 height = bitmap->Height();
		if (width > 0 && height > 0)
		{
			Resize(width, height);
			if (OpStatus::IsSuccess(CreateMdeBuffer()))
			{
				GetMdeBuffer()->SetBitmap(*bitmap);
				if (opacity != 0xFF)
					GetMdeBuffer()->LowerOpacity(opacity);
			}
		}
	}
}


void X11DragWidget::Repaint(const OpRect& rect)
{
	if (GetMdeBuffer())
		GetMdeBuffer()->Repaint(rect);
}


bool X11DragWidget::HandleEvent(XEvent *event)
{
	switch (event->type)
	{
		case Expose:
		{
			OpRect rect;
			rect.x = event->xexpose.x;
			rect.y = event->xexpose.y;
			rect.width  = event->xexpose.width;
			rect.height = event->xexpose.height;
			Repaint(rect);
			return true;
		}
		break;
	};

	return X11Widget::HandleEvent(event);
}
