/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef WIDGETS_CUSTOM_DROPDOWN_WINDOW
#include "platforms/mac/subclasses/CocoaDropDownWindow.h"
#include "platforms/mac/quick_support/CocoaInternalQuickMenu.h"
#include "platforms/mac/pi/CocoaOpWindow.h"
#include "modules/display/vis_dev.h"
#ifdef WIDGETS_CAP_HAS_BOTH_CUSTOM_AND_DEFAULT_DROPDOWN
#include "modules/widgets/DropDownWindow.h"
#endif // WIDGETS_CAP_HAS_BOTH_CUSTOM_AND_DEFAULT_DROPDOWN
#include "platforms/mac/model/CocoaOperaListener.h"
#include "platforms/mac/util/macutils.h"

@interface DropDownDelegate : NSObject
{
	CocoaDropDownWindow* listener;
}
-(void)popupChanged:(id)sender;
@end

@implementation DropDownDelegate
-(id)initWithListener:(CocoaDropDownWindow*)win
{
	self = [super init];
	listener = win;
	return self;
}
-(void)popupChanged:(id)sender
{
	listener->SetActiveItem([((NSPopUpButton*)sender) indexOfSelectedItem]);
}
@end

// Sometimes the plugin message pump (CocoaOperaListener::OnRunSlice) will force-feed the delayed deletion to happen before the "m_button mouseDown:event" completes
// This is not a nice solution, but it solves the crash.
BOOL CocoaDropDownWindow::s_was_deleted = FALSE;

OP_STATUS OpDropDownWindow::Create(OpDropDownWindow **w, OpDropDown* dropdown)
{
	OpDropDownWindow *win = NULL;
	OP_STATUS status = OpStatus::ERR;
	
#ifdef WIDGETS_CAP_HAS_BOTH_CUSTOM_AND_DEFAULT_DROPDOWN
	if (dropdown->edit)
		win = (OpDropDownWindow*)OP_NEW(DropDownWindow, ());
	else
#endif // WIDGETS_CAP_HAS_BOTH_CUSTOM_AND_DEFAULT_DROPDOWN
		win = (OpDropDownWindow*)OP_NEW(CocoaDropDownWindow, (dropdown));
	
	if (win == NULL)
		return OpStatus::ERR_NO_MEMORY;

#ifdef WIDGETS_CAP_HAS_BOTH_CUSTOM_AND_DEFAULT_DROPDOWN
	if (dropdown->edit)
		status = ((DropDownWindow *)win)->Construct(dropdown);
	else
#endif // WIDGETS_CAP_HAS_BOTH_CUSTOM_AND_DEFAULT_DROPDOWN
		status = OpStatus::OK;

	if (OpStatus::IsError(status))
	{
		OP_DELETE(win);
		win = NULL;
		return status;
	}

	*w = win;
	return OpStatus::OK;
}

CocoaDropDownWindow::CocoaDropDownWindow(OpDropDown* dropdown)
: m_dropdown(dropdown),
  m_button(nil),
  m_selected(-1),
  m_is_closed(FALSE),
  m_show_timer(NULL)
{
	m_delegate = [[DropDownDelegate alloc] initWithListener:this];
	m_dummy_listwidget = new OpWidget;
	UpdateMenu(TRUE);
}

CocoaDropDownWindow::~CocoaDropDownWindow()
{
	s_was_deleted = TRUE;
	OP_DELETE(m_show_timer);
	if(m_dropdown->GetListener())
	{
		m_dropdown->GetListener()->OnDropdownMenu(m_dropdown, FALSE);
	}
	m_dropdown->m_dropdown_window = NULL;
	if(m_button)
	{
		[m_button removeFromSuperviewWithoutNeedingDisplay];
		[m_button setTarget:nil];
		[m_button setAction:nil];
		[m_button release];
		m_button = nil;
	}
	[m_delegate release];
}

OpRect CocoaDropDownWindow::CalculateRect()
{
	return OpRect(0,0,0,0);
}

void CocoaDropDownWindow::UpdateMenu(BOOL items_changed)
{
	if (!m_button)
	{
		NSRect frame = NSMakeRect(0,0,0,0);
		m_button = [[OperaNSPopUpButton alloc] initWithFrame:frame pullsDown:NO];
		[m_button setTarget:m_delegate];
		[m_button setAction:@selector(popupChanged:)];
		[m_button setAutoenablesItems:NO];
	}
	[m_button removeAllItems];
	NSMenu* menu = [m_button menu];
	while ([menu numberOfItems])
		[menu removeItemAtIndex:0];
	if (m_dropdown) {
		ItemHandler* ih = &m_dropdown->ih;
		for (int i = 0; i < ih->CountItemsAndGroups(); i++) {
			OpStringItem* stringitem = ih->GetItemAtIndex(i);
			NSString *title = [NSString stringWithCharacters:(unichar*)stringitem->string.Get() length:uni_strlen(stringitem->string.Get())];
			NSMenuItem* item = [[NSMenuItem alloc] initWithTitle:title action:nil keyEquivalent:@""];
			if (!stringitem->IsEnabled())
				[item setEnabled:NO];
			[menu addItem:item];
			[item release];
		}
		SelectItem(m_dropdown->GetSelectedItem(), TRUE);
	}
}

BOOL CocoaDropDownWindow::HasClosed() const
{
	return m_is_closed;
}

void CocoaDropDownWindow::SetClosed(BOOL closed)
{
	m_is_closed = closed;
}

void CocoaDropDownWindow::ClosePopup()
{
	OP_DELETE(this);
}

void CocoaDropDownWindow::SetVisible(BOOL vis, BOOL default_size)
{
	CocoaOpWindow *cocoa_window = (CocoaOpWindow *)m_dropdown->GetVisualDevice()->GetOpView()->GetRootWindow();
	if (!cocoa_window)
		return;

	if (vis) {
		m_show_timer = OP_NEW(OpTimer, ());
		m_show_timer->SetTimerListener(this);
		m_show_timer->Start(0);
	} else {
		OP_DELETE(m_show_timer);
		m_show_timer = NULL;
		// Reset the popup menu
		SendSetActiveNSPopUpButton(NULL);

		NSEvent* event = [NSEvent mouseEventWithType:NSRightMouseUp
											location:[[cocoa_window->GetContentView() window] convertScreenToBase:[NSEvent mouseLocation]]
									   modifierFlags:0 timestamp:[NSDate timeIntervalSinceReferenceDate]
										windowNumber:[[cocoa_window->GetContentView() window] windowNumber]
											 context:[[cocoa_window->GetContentView() window] graphicsContext]
										 eventNumber:0
										  clickCount:1
											pressure:0.0];
		[m_button mouseUp:event];
	}
}

void CocoaDropDownWindow::SelectItem(INT32 nr, BOOL selected)
{
	if (selected && nr >= 0) {
		[m_button selectItemAtIndex:m_dropdown->ih.FindItemIndex(nr)];
		m_selected = nr;
	}
}

INT32 CocoaDropDownWindow::GetFocusedItem()
{
	return m_selected;
}

void CocoaDropDownWindow::ResetMouseOverItem()
{
}

BOOL CocoaDropDownWindow::HasHotTracking()
{
	return FALSE;
}

BOOL CocoaDropDownWindow::IsListWidget(OpWidget* widget)
{
	return m_dummy_listwidget == widget;
}

void CocoaDropDownWindow::SetHotTracking(BOOL hot_track)
{
}

OpPoint CocoaDropDownWindow::GetMousePos()
{
	return OpPoint(0,0);
}

void CocoaDropDownWindow::OnMouseDown(OpPoint pt, MouseButton button, INT32 nclicks)
{
}

void CocoaDropDownWindow::OnMouseUp(OpPoint pt, MouseButton button, INT32 nclicks)
{
}

void CocoaDropDownWindow::OnMouseMove(OpPoint pt)
{
}

BOOL CocoaDropDownWindow::OnMouseWheel(INT32 delta, BOOL vertical)
{
	return FALSE;
}

void CocoaDropDownWindow::ScrollIfNeeded()
{
}

OP_STATUS CocoaDropDownWindow::GetItemAbsoluteVisualBounds(INT32 index, OpRect &rect)
{
	return OpStatus::ERR;
}

BOOL CocoaDropDownWindow::SendInputAction(OpInputAction* action)
{
	return FALSE;
}

void CocoaDropDownWindow::HighlightFocusedItem()
{
}

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
OP_STATUS CocoaDropDownWindow::GetAbsolutePositionOfList(OpRect &rect)
{
	if (m_button)
	{
		// FIXME
	}
	return OpStatus::ERR;
}
#endif

void CocoaDropDownWindow::SetActiveItem(int item)
{
	m_selected = m_dropdown->ih.FindItemNr(item);
	if (m_selected >= 0)
		m_dropdown->OnMouseEvent(m_dummy_listwidget, m_selected, 0, 0, MOUSE_BUTTON_1, FALSE, 1);
}

void CocoaDropDownWindow::PopupClosed()
{
	// Reset the popup menu
	SendSetActiveNSPopUpButton(NULL);

	m_dropdown->m_dropdown_window = NULL;
	OP_DELETE(this);
}

void CocoaDropDownWindow::OnTimeOut(OpTimer* timer)
{
	CocoaOpWindow *cocoa_window = (CocoaOpWindow *)m_dropdown->GetVisualDevice()->GetOpView()->GetRootWindow();
	if (!cocoa_window)
		return;

	[cocoa_window->GetContentView() addSubview:m_button];
	[m_button setHidden:NO];
	[m_button setEnabled:YES];
	NSEvent* event = [NSEvent mouseEventWithType:NSRightMouseDown
										location:[[cocoa_window->GetContentView() window] convertScreenToBase:[NSEvent mouseLocation]]
								   modifierFlags:0 timestamp:[NSDate timeIntervalSinceReferenceDate]
									windowNumber:[[cocoa_window->GetContentView() window] windowNumber]
										 context:[[cocoa_window->GetContentView() window] graphicsContext]
									 eventNumber:0
									  clickCount:1
										pressure:0.0];

	UINT32 width, height;
	((OpWindow*)cocoa_window)->GetInnerSize(&width, &height);

	INT32 xpos,ypos;
	((OpWindow*)cocoa_window)->GetInnerPos(&xpos, &ypos);

	// What we want to do
	OpRect r = m_dropdown->GetScreenRect();

	m_selected = -1;

	// Set the pop-menu active
	SendSetActiveNSPopUpButton(m_button);

	NSRect frame = NSMakeRect(r.x - xpos, height - r.y + ypos - r.height, r.width, r.height);
	[m_button setFrame:frame];
	s_was_deleted = FALSE;
	CocoaOperaListener tracker(TRUE); // Magic BS to keep the timer pumping
	[m_button mouseDown:event];
}

#endif // WIDGETS_CUSTOM_DROPDOWN_WINDOW
