/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT

//#define ACCESSIBILITY_RADIO_LABEL_HACK
//#define _ACCESSIBILITY_DEBUG_LOGGING

#include "platforms/mac/pi/CocoaOpWindow.h"
#include "platforms/mac/pi/MacOpSystemInfo.h"
#include "modules/accessibility/AccessibilityManager.h"
#include "modules/accessibility/opaccessibleitem.h"

// Last due to YES/NO type redefining.
#include "platforms/mac/pi/CocoaOpAccessibilityAdapter.h"

#define ADAPTER_FOR_ITEM(a)  a?static_cast<CocoaOpAccessibilityAdapter*>\
	(AccessibilityManager::GetInstance()->GetAdapterForAccessibleItem(a)):NULL

#ifdef _ACCESSIBILITY_DEBUG_LOGGING
void DumpObject(id obj) {
	if (!obj)
		printf("nil");
	else if ([obj respondsToSelector:@selector(getCharacters:)]) {
		OpString t;
		OpString8 t8;
		t.Reserve([obj length]+1);
		[obj getCharacters:(unichar*)t.CStr()];
		t[[obj length]] = 0;
		t8.Set(t.CStr());
		printf("\"%s\"", t8.CStr());
	}
	else if ([obj respondsToSelector:@selector(sizeValue)])
		printf("|%f x %f|", [obj sizeValue].width, [obj sizeValue].height);
	else if ([obj respondsToSelector:@selector(rangeValue)])
		printf("[%d-%d]", [obj rangeValue].location, [obj rangeValue].location+[obj rangeValue].length);
	else if ([obj respondsToSelector:@selector(objectAtIndex:)]) {
		unsigned count = [obj count];
		printf("{");
		for (unsigned i=0; i<count; i++) {
			if (i) printf(", ");
			DumpObject([obj objectAtIndex:i]);
		}
		printf("}");
	}
	else if ([obj respondsToSelector:@selector(floatValue)])
		printf("%f", [obj floatValue]);
	else if ([obj respondsToSelector:@selector(accessibleItem)]) {
		OpString8 n8;
		OpAccessibleItem* item = [obj accessibleItem];
		if (item) {
			OpString name;
			item->AccessibilityGetText(name);
			if (name.Length())
				n8.SetUTF8FromUTF16(name.CStr());
		}
		if (n8.Length())
			printf("%p (\"%s\")", obj, n8.CStr());
		else {
			printf("%p", obj);
		}
	}
	else
		printf("%p", obj);
}
#endif

//static
OP_STATUS OpAccessibilityAdapter::Create(OpAccessibilityAdapter** adapter, OpAccessibleItem* accessible_item)
{
	id root = nil;
	OpAccessibleItem* parent = accessible_item->GetAccessibleParent();
	if (parent) {
		CocoaOpAccessibilityAdapter* parent_acc = ADAPTER_FOR_ITEM(parent);
		root = parent_acc->GetAccessibleRoot();
	} else {
		CocoaOpWindow* win = static_cast<CocoaOpWindow*>(accessible_item->AccessibilityGetWindow());
		if (win)
			root = win->GetContentView();
	}
	*adapter = OP_NEW(CocoaOpAccessibilityAdapter, (accessible_item, root));
	return OpStatus::OK;
}

//static
OP_STATUS OpAccessibilityAdapter::SendEvent(OpAccessibleItem* sender, Accessibility::Event evt)
{
	if (sender && sender->GetAccessibleParent() && ((MacOpSystemInfo*)g_op_system_info)->IsScreenReaderRunning()) {
		CocoaOpAccessibilityAdapter *adapter = static_cast<CocoaOpAccessibilityAdapter*>(AccessibilityManager::GetInstance()->GetAdapterForAccessibleItem(sender, FALSE));
		if (adapter)
			return adapter->SendEvent(evt);
	}
	return OpStatus::OK;
}

CocoaOpAccessibilityAdapter::CocoaOpAccessibilityAdapter(OpAccessibleItem* accessible_item, id master)
	: m_accessible_item(accessible_item), m_master(master)
{
	m_object = [[OperaAccessibility alloc] initWithItem:m_accessible_item root:master];
#ifdef _ACCESSIBILITY_DEBUG_LOGGING
printf("Created: %p\n", m_object);
#endif
}

CocoaOpAccessibilityAdapter::~CocoaOpAccessibilityAdapter()
{
	if (m_object) {
#ifdef _ACCESSIBILITY_DEBUG_LOGGING
printf("Deleted: %p\n", m_object);
#endif
		[m_object release];
	}
}

OpAccessibleItem* CocoaOpAccessibilityAdapter::GetAccessibleItem()
{
	return m_accessible_item;
}

void CocoaOpAccessibilityAdapter::AccessibleItemRemoved()
{
#ifdef _ACCESSIBILITY_DEBUG_LOGGING
printf("Removed: %p\n", m_object);
#endif
	[m_object itemDeleted];
	delete this;
}

OP_STATUS CocoaOpAccessibilityAdapter::SendEvent(Accessibility::Event evt)
{
	if (m_object) {
#ifdef _ACCESSIBILITY_DEBUG_LOGGING
OpString8 n8;
OpString name;
GetAccessibleItem()->AccessibilityGetText(name);
n8.SetUTF8FromUTF16(name.CStr());
#endif
		if (evt.is_state_event) {
			if (evt.event_info.state_event & Accessibility::kAccessibilityStateFocused) {
				if (GetAccessibleItem()->AccessibilityGetState() & Accessibility::kAccessibilityStateFocused) {
#ifdef _ACCESSIBILITY_DEBUG_LOGGING
printf("FocusedUIElementChanged: %p (\"%s\")\n", m_object, n8.CStr());
#endif
					NSAccessibilityPostNotification(m_object, NSAccessibilityFocusedUIElementChangedNotification);
				}
				else {
					// The element's focus state changed, and the element doesn't itself have focus...
					// Normally that should mean it just lost focus (something we want to ignore).
					// But let's just check if it has passed focus on to a child. This happens on
					// for instance list boxes and treeviews.
					// If that has happened, send the event from the REAL new focus, instead.
					OpAccessibleItem* cand = GetAccessibleItem()->GetAccessibleFocusedChildOrSelf();
					do {
						OpAccessibleItem*child = cand?cand->GetAccessibleFocusedChildOrSelf():NULL;
						if (child && child != cand) {
							cand = child;
						}
						else break;
					} while (true);
					if (cand && cand->AccessibilityGetState() & Accessibility::kAccessibilityStateFocused) {
						CocoaOpAccessibilityAdapter* child_acc = ADAPTER_FOR_ITEM(cand);
						if (child_acc && child_acc->m_object)
							NSAccessibilityPostNotification(child_acc->m_object, NSAccessibilityFocusedUIElementChangedNotification);
					}
				}
			}
			if ((evt.event_info.state_event & Accessibility::kAccessibilityStateCheckedOrSelected) ||
				(evt.event_info.state_event & Accessibility::kAccessibilityStateExpanded)) {
//				NSAccessibilityPostNotification(m_object, NSAccessibilityValueChangedNotification);
			}
		} else switch (evt.event_info.event_type) {
			case Accessibility::kAccessibilityEventMoved:
//				NSAccessibilityPostNotification(m_object, NSAccessibilityMovedNotification);
				break;
			case Accessibility::kAccessibilityEventResized:
//				NSAccessibilityPostNotification(m_object, NSAccessibilityResizedNotification);
				break;
//			case Accessibility::kAccessibilityEventTextChanged:
			case Accessibility::kAccessibilityEventTextChangedBykeyboard:
				NSAccessibilityPostNotification(m_object, NSAccessibilityTitleChangedNotification);
				break;
			case Accessibility::kAccessibilityEventSelectedTextRangeChanged:
			case Accessibility::kAccessibilityEventSelectionChanged:
				NSAccessibilityPostNotification(m_object, NSAccessibilityValueChangedNotification);
				break;
		}
	}
	return OpStatus::ERR;
}


@implementation OperaAccessibility

- (id)initWithItem:(OpAccessibleItem*)item root:(NSObject*)root
{
	self = [super init];
	_item = item;
	_root = root;
	_attr_names = nil;
	_action_names = nil;
	_attr_values = nil;
	return self;
}

- (OpAccessibleItem*)accessibleItem
{
	return _item;
}

- (void)itemDeleted
{
	_item = NULL;
}

- (void) dealloc
{
#ifdef _ACCESSIBILITY_DEBUG_LOGGING
printf("dealloc %p\n", self);
#endif
	if (_attr_names)
		[_attr_names release];
	if (_action_names)
		[_action_names release];
	if (_attr_values)
		[_attr_values release];
	[super dealloc];
}

- (NSArray *)accessibilityAttributeNames
{
	if (!_attr_names) {
		_attr_names = [[NSMutableArray alloc] init];
		[_attr_names addObject:NSAccessibilityRoleAttribute];
		Accessibility::ElementKind role = _item->AccessibilityGetRole();
		[_attr_names addObject:NSAccessibilityRoleDescriptionAttribute];
		switch (role)
		{
			case Accessibility::kElementKindView:
			case Accessibility::kElementKindToolbar:
			case Accessibility::kElementKindHorizontalScrollbar:
			case Accessibility::kElementKindVerticalScrollbar:
			case Accessibility::kElementKindScrollview:
			case Accessibility::kElementKindSlider:
			case Accessibility::kElementKindListRow:
				break;
			default:
				[_attr_names addObject:NSAccessibilityTitleAttribute];
		}
		switch (role)
		{
			case Accessibility::kElementKindView:
			case Accessibility::kElementKindToolbar:
			case Accessibility::kElementKindHorizontalScrollbar:
			case Accessibility::kElementKindVerticalScrollbar:
			case Accessibility::kElementKindScrollview:
			case Accessibility::kElementKindSlider:
			case Accessibility::kElementKindListRow:
				break;
			default:
				[_attr_names addObject:NSAccessibilityDescriptionAttribute];
		}
		switch (role)
		{
			case Accessibility::kElementKindView:
			case Accessibility::kElementKindToolbar:
			case Accessibility::kElementKindHorizontalScrollbar:
			case Accessibility::kElementKindVerticalScrollbar:
			case Accessibility::kElementKindScrollview:
			case Accessibility::kElementKindSlider:
			case Accessibility::kElementKindListRow:
				break;
			default:
				[_attr_names addObject:NSAccessibilityHelpAttribute];
		}

		[_attr_names addObject:NSAccessibilityParentAttribute];
		if (_item->GetAccessibleChildrenCount() > 0 || role == Accessibility::kElementKindScrollview ||
			role == Accessibility::kElementKindDropdown || role == Accessibility::kElementKindDropdownButtonPart)
			[_attr_names addObject:NSAccessibilityChildrenAttribute];
		if (role == Accessibility::kElementKindListRow)
			[_attr_names addObject:NSAccessibilityVisibleChildrenAttribute];
		[_attr_names addObject:NSAccessibilityWindowAttribute];
		[_attr_names addObject:NSAccessibilityTopLevelUIElementAttribute];
		[_attr_names addObject:NSAccessibilityEnabledAttribute];
		[_attr_names addObject:NSAccessibilityFocusedAttribute];
		[_attr_names addObject:NSAccessibilityPositionAttribute];
		[_attr_names addObject:NSAccessibilitySizeAttribute];
		switch (role)
		{
			case Accessibility::kElementKindLabel:
			case Accessibility::kElementKindCheckbox:
			case Accessibility::kElementKindRadioTabGroup:
			case Accessibility::kElementKindRadioButton:
			case Accessibility::kElementKindDropdown:
			case Accessibility::kElementKindDropdownButtonPart:
			case Accessibility::kElementKindHorizontalScrollbar:
			case Accessibility::kElementKindVerticalScrollbar:
			case Accessibility::kElementKindSlider:
			case Accessibility::kElementKindTab:
			case Accessibility::kElementKindStaticText:
			case Accessibility::kElementKindSinglelineTextedit:
			case Accessibility::kElementKindMultilineTextedit:
				[_attr_names addObject:NSAccessibilityValueAttribute];
				break;
		}
		switch (role)
		{
			case Accessibility::kElementKindHorizontalScrollbar:
			case Accessibility::kElementKindVerticalScrollbar:
			case Accessibility::kElementKindSlider:
				[_attr_names addObject:NSAccessibilityOrientationAttribute];
				[_attr_names addObject:NSAccessibilityMinValueAttribute];
				[_attr_names addObject:NSAccessibilityMaxValueAttribute];
				break;
			case Accessibility::kElementKindSinglelineTextedit:
			case Accessibility::kElementKindMultilineTextedit:
				[_attr_names addObject:NSAccessibilityMaxValueAttribute];
				[_attr_names addObject:NSAccessibilitySelectedTextAttribute];
				[_attr_names addObject:NSAccessibilitySelectedTextRangeAttribute];
				[_attr_names addObject:NSAccessibilityNumberOfCharactersAttribute];
				[_attr_names addObject:NSAccessibilityVisibleCharacterRangeAttribute];
				[_attr_names addObject:NSAccessibilityInsertionPointLineNumberAttribute];
				break;
			case Accessibility::kElementKindList:
			case Accessibility::kElementKindOutlineList:
			case Accessibility::kElementKindTable:
			case Accessibility::kElementKindGrid:
				[_attr_names addObject:NSAccessibilitySelectedChildrenAttribute];
				[_attr_names addObject:NSAccessibilityRowsAttribute];
				[_attr_names addObject:NSAccessibilityVisibleRowsAttribute];
				[_attr_names addObject:NSAccessibilitySelectedRowsAttribute];
				[_attr_names addObject:NSAccessibilityHeaderAttribute];
				break;
			case Accessibility::kElementKindListRow:
				[_attr_names addObject:NSAccessibilitySelectedAttribute];
				[_attr_names addObject:NSAccessibilityIndexAttribute];
				[_attr_names addObject:NSAccessibilityDisclosingAttribute];
				[_attr_names addObject:NSAccessibilityDisclosedRowsAttribute];
				[_attr_names addObject:NSAccessibilityDisclosedByRowAttribute];
				[_attr_names addObject:NSAccessibilityDisclosureLevelAttribute];
				[_attr_names addObject:NSAccessibilitySubroleAttribute];
				break;
			case Accessibility::kElementKindLink:
				[_attr_names addObject:NSAccessibilityURLAttribute];
				break;
			case Accessibility::kElementKindScrollview:
				[_attr_names addObject:NSAccessibilityHorizontalScrollBarAttribute];
				[_attr_names addObject:NSAccessibilityVerticalScrollBarAttribute];
				[_attr_names addObject:NSAccessibilityContentsAttribute];
				break;
			case Accessibility::kElementKindWebView:
				[_attr_names addObject:@"AXLinkUIElements"];
				break;
			case Accessibility::kElementKindScrollbarPartArrowUp:
			case Accessibility::kElementKindScrollbarPartArrowDown:
			case Accessibility::kElementKindScrollbarPartPageUp:
			case Accessibility::kElementKindScrollbarPartPageDown:
				[_attr_names addObject:NSAccessibilitySubroleAttribute];
				break;
			case Accessibility::kElementKindLabel:
				[_attr_names addObject:NSAccessibilityServesAsTitleForUIElementsAttribute];
				break;
		}
		if (_item->GetAccessibleLabelForControl())
		{
#ifdef ACCESSIBILITY_RADIO_LABEL_HACK
			switch (role)
			{
				case Accessibility::kElementKindCheckbox:
				case Accessibility::kElementKindRadioButton:
					break;
				default:
					[_attr_names addObject:NSAccessibilityTitleUIElementAttribute];
					break;
			}
#else
					[_attr_names addObject:NSAccessibilityTitleUIElementAttribute];
#endif
		}
#ifdef _ACCESSIBILITY_DEBUG_LOGGING
printf("%p accessibilityAttributeNames -> ", self);
DumpObject(_attr_names);
printf("\n");
#endif
	}
	return _attr_names;
}

- (id)accessibilityAttributeValue:(NSString *)attr_name
{
	OpRect bounds;
	BOOL visible_only = FALSE;
	BOOL selected_only = FALSE;
	BOOL rows_only = FALSE;
	BOOL min_value = true;
	if (!_attr_values)
		_attr_values = [[NSMutableDictionary alloc] init];
	id ret = [_attr_values objectForKey:attr_name];
	if (!_item)
		return nil;

#ifdef _ACCESSIBILITY_DEBUG_LOGGING
OpString t;
OpString8 t8;
t.Reserve([attr_name length]+1);
[attr_name getCharacters:(unichar*)t.CStr()];
t[[attr_name length]] = 0;
t8.Set(t.CStr());
printf("%p accessibilityAttributeValue:\"%s\" -> ", self, t8.CStr());
#endif

	if ([attr_name isEqualToString:NSAccessibilityPositionAttribute])
	{
		NSPoint pt;
		_item->AccessibilityGetAbsolutePosition(bounds);
#ifdef ACCESSIBILITY_RADIO_LABEL_HACK
		if (_item->AccessibilityGetState() & Accessibility::kAccessibilityStateReliesOnLabel)
		{
			switch (_item->AccessibilityGetRole())
			{
				case Accessibility::kElementKindCheckbox:
				case Accessibility::kElementKindRadioButton:
					{
						OpAccessibleItem* acc = _item->GetAccessibleLabelForControl();
						if (acc)
						{
							OpRect label_bounds;
							if (OpStatus::IsSuccess(acc->AccessibilityGetAbsolutePosition(label_bounds)))
							{
								bounds.UnionWith(label_bounds);
							}
						}
					}
					break;
			}
		}
#endif
		pt.x = bounds.x;
		pt.y = [[NSScreen mainScreen] frame].size.height-bounds.y-bounds.height;
		ret = [NSValue valueWithPoint:pt];
	}
	else if ([attr_name isEqualToString:NSAccessibilitySizeAttribute])
	{
		OpRect bounds;
		NSSize size;
		_item->AccessibilityGetAbsolutePosition(bounds);
#ifdef ACCESSIBILITY_RADIO_LABEL_HACK
		if (_item->AccessibilityGetState() & Accessibility::kAccessibilityStateReliesOnLabel)
		{
			switch (_item->AccessibilityGetRole())
			{
				case Accessibility::kElementKindCheckbox:
				case Accessibility::kElementKindRadioButton:
					{
						OpAccessibleItem* acc = _item->GetAccessibleLabelForControl();
						if (acc)
						{
							OpRect label_bounds;
							if (OpStatus::IsSuccess(acc->AccessibilityGetAbsolutePosition(label_bounds)))
							{
								bounds.UnionWith(label_bounds);
							}
						}
					}
					break;
			}
		}
#endif
		size.width = bounds.width;
		size.height = bounds.height;
		ret = [NSValue valueWithSize:size];
	}
	else if ([attr_name isEqualToString:NSAccessibilityRoleAttribute])
	{
		switch (_item->AccessibilityGetRole())
		{
			case Accessibility::kElementKindWebView:
//				ret = NSAccessibilityBrowserRole;
				ret = @"AXWebArea";
				break;
			case Accessibility::kElementKindButton:
			case Accessibility::kElementKindScrollbarPartArrowUp:
			case Accessibility::kElementKindScrollbarPartArrowDown:
			case Accessibility::kElementKindScrollbarPartPageUp:
			case Accessibility::kElementKindScrollbarPartPageDown:
				ret = NSAccessibilityButtonRole;
				break;
			case Accessibility::kElementKindCheckbox:
				ret = NSAccessibilityCheckBoxRole;
				break;
			case Accessibility::kElementKindRadioTabGroup:
				ret = NSAccessibilityRadioGroupRole;
				break;
			case Accessibility::kElementKindRadioButton:
				ret = NSAccessibilityRadioButtonRole;
				break;
			case Accessibility::kElementKindDropdown:
				{
					if (_item->GetAccessibleChildrenCount() > 2)
						ret = NSAccessibilityGroupRole;
					else
						ret = NSAccessibilityPopUpButtonRole;
				}
				break;
			case Accessibility::kElementKindDropdownButtonPart:
				ret = NSAccessibilityPopUpButtonRole;
				break;
			case Accessibility::kElementKindTab:
				ret = NSAccessibilityRadioButtonRole;
				break;
			case Accessibility::kElementKindStaticText:
			case Accessibility::kElementKindLabel:
				ret = NSAccessibilityStaticTextRole;
				break;
			case Accessibility::kElementKindSinglelineTextedit:
				ret = NSAccessibilityTextFieldRole;
				break;
			case Accessibility::kElementKindMultilineTextedit:
//				ret = NSAccessibilityTextAreaRole;
				ret = NSAccessibilityTextFieldRole;
				break;
			case Accessibility::kElementKindLink:
				ret = NSAccessibilityLinkRole;
				break;
			case Accessibility::kElementKindImage:
				ret = NSAccessibilityImageRole;
				break;
			case Accessibility::kElementKindMenu:
				ret = NSAccessibilityMenuRole;
				break;
			case Accessibility::kElementKindMenuItem:
				ret = NSAccessibilityMenuItemRole;
				break;
			case Accessibility::kElementKindWindow:
			case Accessibility::kElementKindView:
				ret = NSAccessibilityGroupRole;
				break;
			case Accessibility::kElementKindHorizontalScrollbar:
			case Accessibility::kElementKindVerticalScrollbar:
				ret = NSAccessibilityScrollBarRole;
				break;
			case Accessibility::kElementKindScrollview:
				ret = NSAccessibilityScrollAreaRole;
				break;
			case Accessibility::kElementKindSlider:
				ret = NSAccessibilitySliderRole;
				break;
			case Accessibility::kElementKindList:
				ret = NSAccessibilityListRole;
				break;
			case Accessibility::kElementKindOutlineList:
				ret = NSAccessibilityOutlineRole;
				break;
			case Accessibility::kElementKindTable:
				ret = NSAccessibilityTableRole;
				break;
			case Accessibility::kElementKindGrid:
				ret = NSAccessibilityTableRole; // FIXME
				break;
			case Accessibility::kElementKindListRow:
				ret = NSAccessibilityRowRole;
				break;
			case Accessibility::kElementKindScrollbarPartKnob:
				ret = NSAccessibilityValueIndicatorRole;
				break;
			case Accessibility::kElementKindToolbar:
			case Accessibility::kElementKindUnknown:
			default:
				ret = NSAccessibilityGroupRole;
//				ret = NSAccessibilityUnknownRole;
				break;
		}
	}
	else if ([attr_name isEqualToString:NSAccessibilityRoleDescriptionAttribute])
	{
		switch (_item->AccessibilityGetRole())
		{
			case Accessibility::kElementKindWebView:
				ret = @"web area";
				break;
			case Accessibility::kElementKindButton:
				ret = @"button";
				break;
			case Accessibility::kElementKindCheckbox:
				ret = @"checkbox";
				break;
			case Accessibility::kElementKindRadioTabGroup:
				ret = @"radio group";
				break;
			case Accessibility::kElementKindRadioButton:
				ret = @"radio button";
				break;
			case Accessibility::kElementKindDropdown:
				if (_item->GetAccessibleChildrenCount() <= 2)
					ret = @"pop up button";
				break;
			case Accessibility::kElementKindDropdownButtonPart:
				ret = @"pop up button";
				break;
			case Accessibility::kElementKindTab:
				ret = @"tab";
				break;
			case Accessibility::kElementKindStaticText:
			case Accessibility::kElementKindLabel:
				ret = @"text";
				break;
			case Accessibility::kElementKindSinglelineTextedit:
				ret = @"text field";
				break;
			case Accessibility::kElementKindMultilineTextedit:
				ret = @"text entry area";
				break;
			case Accessibility::kElementKindLink:
				ret = @"link";
				break;
			case Accessibility::kElementKindImage:
				ret = @"image";
				break;
			case Accessibility::kElementKindMenu:
				ret = @"menu";
				break;
			case Accessibility::kElementKindMenuItem:
				ret = @"menu item";
				break;
			case Accessibility::kElementKindHorizontalScrollbar:
			case Accessibility::kElementKindVerticalScrollbar:
				_item->AccessibilityGetAbsolutePosition(bounds);
				if (bounds.width > bounds.height)
					ret = @"scroll bar";
				else
					ret = @"scroll bar";
				break;
			case Accessibility::kElementKindSlider:
					ret = @"slider";
				break;
			case Accessibility::kElementKindList:
				ret = @"list";
				break;
			case Accessibility::kElementKindOutlineList:
				ret = @"outline";
				break;
			case Accessibility::kElementKindTable:
				ret = @"table";
				break;
			case Accessibility::kElementKindGrid:
				ret = @"grid";
				break;
			case Accessibility::kElementKindListRow:
				ret = @"table row";
//				ret = @"outline row";
				break;
			case Accessibility::kElementKindScrollbarPartArrowUp:
				ret = @"decrement page button";
				break;
			case Accessibility::kElementKindScrollbarPartArrowDown:
				ret = @"increment page button";
				break;
			case Accessibility::kElementKindScrollbarPartPageUp:
				ret = @"decrement arrow button";
				break;
			case Accessibility::kElementKindScrollbarPartPageDown:
				ret = @"increment arrow button";
				break;
			case Accessibility::kElementKindScrollbarPartKnob:
				ret = @"value indicator";
				break;
//			case Accessibility::kElementKindToolbar:
//			case Accessibility::kElementKindWindow:
//			case Accessibility::kElementKindView:
//			case Accessibility::kElementKindUnknown:
//			default:
//				ret = @"";
//				break;
		}
	}
	else if ([attr_name isEqualToString:NSAccessibilityTitleAttribute])
	{
		OpString text;
		switch (_item->AccessibilityGetRole())
		{
			case Accessibility::kElementKindStaticText:
			case Accessibility::kElementKindLabel:
			case Accessibility::kElementKindSinglelineTextedit:
			case Accessibility::kElementKindMultilineTextedit:
			case Accessibility::kElementKindDropdown:
			case Accessibility::kElementKindImage:
				break;
			default:
				_item->AccessibilityGetText(text);
#ifdef ACCESSIBILITY_RADIO_LABEL_HACK
				if (text.IsEmpty() && (_item->AccessibilityGetState() & Accessibility::kAccessibilityStateReliesOnLabel))
				{
					switch (_item->AccessibilityGetRole())
					{
						case Accessibility::kElementKindCheckbox:
						case Accessibility::kElementKindRadioButton:
							{
								OpAccessibleItem* acc = _item->GetAccessibleLabelForControl();
								if (acc)
								{
									acc->AccessibilityGetText(text);
								}
							}
							break;
					}
				}
#endif
				break;
		}
		if (text.Length())
		{
			ret = [NSString stringWithCharacters:(unichar*)text.CStr() length:text.Length()];
		}
	}
	else if ([attr_name isEqualToString:NSAccessibilityDescriptionAttribute])
	{
		OpString text;
		_item->AccessibilityGetDescription(text);
		if (_item->AccessibilityGetRole() == Accessibility::kElementKindImage && !text.Length())
		{
			_item->AccessibilityGetText(text);
		}
		if (text.Length())
		{
			ret = [NSString stringWithCharacters:(unichar*)text.CStr() length:text.Length()];
		}
	}
	else if ([attr_name isEqualToString:NSAccessibilityParentAttribute])
	{
		OpAccessibleItem *parent = _item->GetAccessibleParent();
		CocoaOpAccessibilityAdapter* acc = ADAPTER_FOR_ITEM(parent);
		if (acc)
		{
			ret = acc->GetAccessibleObject();
		}
		if (!ret)
			ret = [_root window];
	}
	else if ([attr_name isEqualToString:NSAccessibilityEnabledAttribute])
	{
		Accessibility::State state = _item->AccessibilityGetState();
		NSBOOL enabled = !(state & Accessibility::kAccessibilityStateDisabled);
		ret = [NSNumber numberWithBool:enabled];
	}
	else if ([attr_name isEqualToString:NSAccessibilityFocusedAttribute])
	{
		Accessibility::State state = _item->AccessibilityGetState();
		NSBOOL focused = !!(state & Accessibility::kAccessibilityStateFocused);
		ret = [NSNumber numberWithBool:focused];
	}
	else if ([attr_name isEqualToString:NSAccessibilitySelectedAttribute])
	{
		Accessibility::State state = _item->AccessibilityGetState();
		NSBOOL selected = !!(state & Accessibility::kAccessibilityStateCheckedOrSelected);
		ret = [NSNumber numberWithBool:selected];
	}
	else if ([attr_name isEqualToString:NSAccessibilityWindowAttribute] || 
			 [attr_name isEqualToString:NSAccessibilityTopLevelUIElementAttribute])
	{
		OpAccessibleItem *parent = _item->GetAccessibleParent();
		while (!_root && parent) {
			CocoaOpAccessibilityAdapter* acc = ADAPTER_FOR_ITEM(parent);
			if (acc) {
				_root = acc->GetAccessibleRoot();
			}
			parent = parent->GetAccessibleParent();
		}
		if (_root && [_root respondsToSelector:@selector(accessibilityAttributeValue:)])
		{
			ret = [_root performSelector:@selector(accessibilityAttributeValue:) withObject:attr_name];
		}
	}
	else if ([attr_name isEqualToString:NSAccessibilityValueAttribute])
	{
		int i_value;
		OpString s_value;
		switch (_item->AccessibilityGetRole())
		{
			case Accessibility::kElementKindCheckbox:
			case Accessibility::kElementKindRadioButton:
			case Accessibility::kElementKindTab:
				_item->AccessibilityGetValue(i_value);
				ret = [NSNumber numberWithInt:i_value];
				break;
			case Accessibility::kElementKindHorizontalScrollbar:
			case Accessibility::kElementKindVerticalScrollbar:
			case Accessibility::kElementKindSlider:
				int max_val;
				double real_val;
				_item->AccessibilityGetValue(i_value);
				_item->AccessibilityGetMaxValue(max_val);
				real_val = max_val ? (((double)i_value) / ((double)max_val)) : 0.0;
				ret = [NSNumber numberWithDouble:real_val];
				break;
			case Accessibility::kElementKindStaticText:
			case Accessibility::kElementKindLabel:
			case Accessibility::kElementKindSinglelineTextedit:
			case Accessibility::kElementKindMultilineTextedit:
			case Accessibility::kElementKindDropdown:
				_item->AccessibilityGetText(s_value);
				ret = [NSString stringWithCharacters:(unichar*)s_value.CStr() length:s_value.Length()];
				break;
			case Accessibility::kElementKindRadioTabGroup:
				{
					int accChildrenCount = _item->GetAccessibleChildrenCount();
					OpAccessibleItem* child;
					CocoaOpAccessibilityAdapter* child_acc;
					for (int i=0; i<accChildrenCount; i++) {
						child = _item->GetAccessibleChild(i);
						if (child) {
							child->AccessibilityGetValue(i_value);
							if (i_value) {
								child_acc = ADAPTER_FOR_ITEM(child);
								ret = child_acc->GetAccessibleObject();
								break;
							}
						}
					}
				}
				break;
		}
	}
	else if ((min_value = ([attr_name isEqualToString:NSAccessibilityMinValueAttribute])) ||
		[attr_name isEqualToString:NSAccessibilityMaxValueAttribute])
	{
		int i_value = min_value ? 0 : 1;
		switch (_item->AccessibilityGetRole())
		{
			case Accessibility::kElementKindHorizontalScrollbar:
			case Accessibility::kElementKindVerticalScrollbar:
			case Accessibility::kElementKindSlider:
			{
				ret = [NSNumber numberWithInt:i_value];
			}
		}
	}
	else if (([attr_name isEqualToString:NSAccessibilityChildrenAttribute]) ||
			 (visible_only = ([attr_name isEqualToString:NSAccessibilityVisibleChildrenAttribute])) ||
			 (selected_only = ([attr_name isEqualToString:NSAccessibilitySelectedChildrenAttribute])) ||
			 (rows_only = (([attr_name isEqualToString:NSAccessibilityRowsAttribute]) ||
			 (visible_only = ([attr_name isEqualToString:NSAccessibilityVisibleRowsAttribute])) ||
			 (selected_only = ([attr_name isEqualToString:NSAccessibilitySelectedRowsAttribute])) )) )
	{
		int accChildrenCount = _item->GetAccessibleChildrenCount();
		NSMutableArray* accChildren = nil;
		if (ret) {
			accChildren = ret;
			[accChildren removeAllObjects];
		} else {
			ret = accChildren = [[NSMutableArray alloc] init];
			[_attr_values setObject:[ret autorelease] forKey:attr_name];
		}
		OpAccessibleItem* child = NULL;
		CocoaOpAccessibilityAdapter* child_acc;
		id child_ax;

		Accessibility::ElementKind role = _item->AccessibilityGetRole();

		BOOL is_dropdown = (role == Accessibility::kElementKindDropdown);

		if (accChildren)
		{
			int child_i, array_i;
			array_i = 0;
			OpRect parent_rect;
			if ((role == Accessibility::kElementKindList) || (role == Accessibility::kElementKindOutlineList))
				rows_only = true;
			if (visible_only)
			{
				OpAccessibleItem* parent = NULL;
				if ((role == Accessibility::kElementKindList) || (role == Accessibility::kElementKindOutlineList))
				{
					parent = _item->GetAccessibleParent();
					if (parent) {
						parent->AccessibilityGetAbsolutePosition(parent_rect);
					}
				}
				else
				{
					_item->AccessibilityGetAbsolutePosition(parent_rect);
					parent = _item->GetAccessibleParent();
				}
				while (parent)
				{
					OpRect super_rect;
					parent->AccessibilityGetAbsolutePosition(super_rect);
					if (!super_rect.IsEmpty())
						parent_rect.IntersectWith(super_rect);
					parent = parent->GetAccessibleParent();
				}
			}
			for (child_i = 0; child_i < accChildrenCount; child_i++)
			{
				if (is_dropdown && ((accChildrenCount == 2) || (child_i == (accChildrenCount-1))))
					continue;
				child_ax = nil;
				child = _item->GetAccessibleChild(child_i);
				Accessibility::State state = child->AccessibilityGetState();
				if (state & Accessibility::kAccessibilityStateInvisible)
					continue;
				if (selected_only && !(state & Accessibility::kAccessibilityStateCheckedOrSelected))
					continue;
				if (visible_only)
				{
					OpRect child_rect;
					child->AccessibilityGetAbsolutePosition(child_rect);
					if (child_rect.IsEmpty() || !parent_rect.Intersecting(child_rect))
						continue;
				}
				if (rows_only)
				{
					Accessibility::ElementKind role = child->AccessibilityGetRole();
					if (role != Accessibility::kElementKindListRow)
						continue;
				}
				child_acc = ADAPTER_FOR_ITEM(child);
				if (child_acc)
				{
					child_ax = child_acc->GetAccessibleObject();
					if (child_ax) {
						[accChildren addObject:child_ax];
					}
				}
			}
		}
	}
	else if ([attr_name isEqualToString:NSAccessibilitySelectedTextRangeAttribute])
	{
		int start, end;
		OpString s_value;
		if (OpStatus::IsSuccess(_item->AccessibilityGetSelectedTextRange(start, end)))
		{
			NSRange range;
			// Sanity check needed... sadly.
			_item->AccessibilityGetText(s_value);
			int len = s_value.Length();
			if (start < 0) start = 0;
			if (end < 0) end = 0;
			if (start > len) start = len;
			if (end > len) end = len;
			if (end < start) end = start;
			range.location = start;
			range.length = (end - start);
			ret = [NSValue valueWithRange:range];
		}
	}
	else if ([attr_name isEqualToString:NSAccessibilitySelectedTextAttribute])
	{
		int start = 0, end = 0;

		if (OpStatus::IsSuccess(_item->AccessibilityGetSelectedTextRange(start, end)))
		{
			OpString s_value;
			int len;
			switch (_item->AccessibilityGetRole())
			{
				case Accessibility::kElementKindSinglelineTextedit:
				case Accessibility::kElementKindMultilineTextedit:
					_item->AccessibilityGetText(s_value);
					len = s_value.Length();
					// Sanity check needed... sadly.
					if (start < 0) start = 0;
					if (end < 0) end = 0;
					if (start > len) start = len;
					if (end > len) end = len;
					if (end < start) end = start;
					ret = [NSString stringWithCharacters:(unichar*)s_value.CStr() + start length:end - start];
			}
		}
	}
	else if ([attr_name isEqualToString:NSAccessibilityVisibleCharacterRangeAttribute])
	{
		OpString s_value;
		NSRange range;
		switch (_item->AccessibilityGetRole())
		{
			case Accessibility::kElementKindSinglelineTextedit:
			case Accessibility::kElementKindMultilineTextedit:
				_item->AccessibilityGetText(s_value);
				range.location = 0;
				range.length = s_value.Length();
				ret = [NSValue valueWithRange:range];
		}
	}
	else if ([attr_name isEqualToString:NSAccessibilityNumberOfCharactersAttribute])
	{
		OpString s_value;
		int len;
		switch (_item->AccessibilityGetRole())
		{
			case Accessibility::kElementKindSinglelineTextedit:
			case Accessibility::kElementKindMultilineTextedit:
				_item->AccessibilityGetText(s_value);
				len = s_value.Length();
				ret = [NSNumber numberWithInt:len];
		}
	}
	else if ([attr_name isEqualToString:NSAccessibilityURLAttribute])
	{
		OpString text;
		_item->AccessibilityGetURL(text);
		ret = [NSString stringWithCharacters:(unichar*)text.CStr() length:text.Length()];
	}
	else if ([attr_name isEqualToString:NSAccessibilityOrientationAttribute])
	{
		OpRect rect;
		_item->AccessibilityGetAbsolutePosition(rect);
		if (rect.height < rect.width) {
			ret = NSAccessibilityHorizontalOrientationValue;
		}
		else {
			ret = NSAccessibilityVerticalOrientationValue;
		}
	}
	else if ([attr_name isEqualToString:NSAccessibilityHorizontalScrollBarAttribute])
	{
		if (_item->AccessibilityGetRole() == Accessibility::kElementKindScrollview)
		{
			int count = _item->GetAccessibleChildrenCount();
			for (int i = 0; i < count; i++)
			{
				OpAccessibleItem* child = _item->GetAccessibleChild(i);
				CocoaOpAccessibilityAdapter* child_acc = ADAPTER_FOR_ITEM(child);
				if (child_acc && child->AccessibilityGetRole() == Accessibility::kElementKindHorizontalScrollbar)
				{
					ret = child_acc->GetAccessibleObject();
				}
			}
		}
	}
	else if ([attr_name isEqualToString:NSAccessibilityVerticalScrollBarAttribute])
	{
		if (_item->AccessibilityGetRole() == Accessibility::kElementKindScrollview)
		{
			int count = _item->GetAccessibleChildrenCount();
			for (int i = 0; i < count; i++)
			{
				OpAccessibleItem* child = _item->GetAccessibleChild(i);
				CocoaOpAccessibilityAdapter* child_acc = ADAPTER_FOR_ITEM(child);
				if (child_acc && child->AccessibilityGetRole() == Accessibility::kElementKindVerticalScrollbar)
				{
					ret = child_acc->GetAccessibleObject();
				}
			}
		}
	}
	else if ([attr_name isEqualToString:NSAccessibilityContentsAttribute])
	{
		if (_item->AccessibilityGetRole() == Accessibility::kElementKindScrollview)
		{
			int count = _item->GetAccessibleChildrenCount();
			for (int i = 0; i < count; i++)
			{
				OpAccessibleItem* child = _item->GetAccessibleChild(i);
				CocoaOpAccessibilityAdapter* child_acc = ADAPTER_FOR_ITEM(child);
				if (child_acc)
				{
					Accessibility::ElementKind role = child->AccessibilityGetRole();
					if (role != Accessibility::kElementKindHorizontalScrollbar && role != Accessibility::kElementKindVerticalScrollbar)
					{
						id child_ax = child_acc->GetAccessibleObject();
						ret = [NSArray arrayWithObject:child_ax];
						break;
					}
				}
			}
		}
	}
	else if ([attr_name isEqualToString:@"AXLinkUIElements"])
	{
		if (_item->AccessibilityGetRole() == Accessibility::kElementKindWebView)
		{
			NSMutableArray* accChildren = nil;
			if (ret) {
				accChildren = ret;
				[accChildren removeAllObjects];
			} else {
				ret = accChildren = [[NSMutableArray alloc] init];
				[_attr_values setObject:[ret autorelease] forKey:attr_name];
			}
			OpVector<OpAccessibleItem> links;
			if (OpStatus::IsSuccess(_item->GetAccessibleLinks(links)))
			{
				for (unsigned i = 0; i < links.GetCount(); i++)
				{
					OpAccessibleItem* child = links.Get(i);
					CocoaOpAccessibilityAdapter* child_acc = ADAPTER_FOR_ITEM(child);
					if (child_acc && child->AccessibilityGetRole() == Accessibility::kElementKindLink)
					{
						OpString text;
						child->AccessibilityGetText(text);
						if (text.Length()) {
							id child_ax = child_acc->GetAccessibleObject();
							if (child_ax)
								[accChildren addObject:child_ax];
						}
					}
				}
			}
		}
	}
	else if ([attr_name isEqualToString:NSAccessibilitySubroleAttribute])
	{
		switch (_item->AccessibilityGetRole())
		{
			case Accessibility::kElementKindScrollbarPartArrowUp:
				ret = NSAccessibilityDecrementArrowSubrole;
				break;
			case Accessibility::kElementKindScrollbarPartArrowDown:
				ret = NSAccessibilityIncrementArrowSubrole;
				break;
			case Accessibility::kElementKindScrollbarPartPageUp:
				ret = NSAccessibilityDecrementPageSubrole;
				break;
			case Accessibility::kElementKindScrollbarPartPageDown:
				ret = NSAccessibilityIncrementPageSubrole;
				break;
			case Accessibility::kElementKindListRow:
//				switch (_item->GetAccessibleParent()->AccessibilityGetRole())
//				{
//					case Accessibility::kElementKindOutlineList:
//						ret = @"AXOutlineRow";
//						break;
//					case Accessibility::kElementKindTable:
//					default:
						ret = NSAccessibilityTableRowSubrole;
//						break;
//				}
				break;
		}
	}
	else if ([attr_name isEqualToString:NSAccessibilityTitleUIElementAttribute])
	{
		CocoaOpAccessibilityAdapter* acc = ADAPTER_FOR_ITEM(_item->GetAccessibleLabelForControl());
#ifdef ACCESSIBILITY_RADIO_LABEL_HACK
		switch (_item->AccessibilityGetRole())
		{
			case Accessibility::kElementKindCheckbox:
			case Accessibility::kElementKindRadioButton:
				acc = 0;
		}
#endif
		if (acc)
		{
			ret = acc->GetAccessibleObject();
		}
	}
	else if ([attr_name isEqualToString:NSAccessibilityServesAsTitleForUIElementsAttribute])
	{
		CocoaOpAccessibilityAdapter* acc = ADAPTER_FOR_ITEM(_item->GetAccessibleControlForLabel());
		if (acc)
		{
			id child_ax = acc->GetAccessibleObject();
			ret = [NSArray arrayWithObject:child_ax];
		}
	}
	else if ([attr_name isEqualToString:NSAccessibilityIndexAttribute])
	{
		int index = _item->GetAccessibleParent()->GetAccessibleChildIndex(_item);
		ret = [NSNumber numberWithInt:index];
	}
	else if ([attr_name isEqualToString:NSAccessibilityDisclosingAttribute])
	{
		Accessibility::State state = _item->AccessibilityGetState();
		NSBOOL expanded = !!(state & Accessibility::kAccessibilityStateExpanded);
		ret = [NSNumber numberWithBool:expanded];
	}
	else if ([attr_name isEqualToString:NSAccessibilityDisclosedRowsAttribute])
	{
		OpAccessibleItem* parent = _item->GetAccessibleParent();
		if (parent)
		{
			if (_item->AccessibilityGetState() & Accessibility::kAccessibilityStateExpanded)
			{
				int accChildrenCount = parent->GetAccessibleChildrenCount();
				OpAccessibleItem* child;
				CocoaOpAccessibilityAdapter* child_acc;
				int i;
				for (i = 0; i < accChildrenCount; i++)
				{
					child = parent->GetAccessibleChild(i);
					if (child == _item)
					{
						int depth;
						NSMutableArray* accChildren = nil;
						if (ret) {
							accChildren = ret;
							[accChildren removeAllObjects];
						} else {
							ret = accChildren = [[NSMutableArray alloc] init];
							[_attr_values setObject:[ret autorelease] forKey:attr_name];
						}
						i++;
						id child_ax;
						if (OpStatus::IsSuccess(_item->AccessibilityGetValue(depth)))
						{
							for (; i < accChildrenCount; i++)
							{
								int child_depth = 0;
								child = parent->GetAccessibleChild(i);
								child_acc = ADAPTER_FOR_ITEM(child);
								if (child_acc && OpStatus::IsSuccess(child->AccessibilityGetValue(child_depth)) && (child_depth > depth))
								{
									child_ax = child_acc->GetAccessibleObject();
									if (child_ax && (child_depth == (depth + 1)))
									{
										[accChildren addObject:child_ax];
									}
								}
								else
									break;
							}
						}
						break;
					}
				}
			}
		}
	}
	else if ([attr_name isEqualToString:NSAccessibilityDisclosedByRowAttribute])
	{
		OpAccessibleItem* parent = _item->GetAccessibleParent();
		if (parent)
		{
			int depth;
			if (OpStatus::IsSuccess(_item->AccessibilityGetValue(depth)))
			{
				int accChildrenCount = parent->GetAccessibleChildrenCount();
				OpAccessibleItem* child;
				CocoaOpAccessibilityAdapter* child_acc;
				int i;
				int parent_depth;
				CocoaOpAccessibilityAdapter* parent_candidate = NULL;
				for (i = 0; i < accChildrenCount; i++)
				{
					child = parent->GetAccessibleChild(i);
					child_acc = ADAPTER_FOR_ITEM(child);
					if (child_acc && OpStatus::IsSuccess(child->AccessibilityGetValue(parent_depth)) && (parent_depth == (depth-1)))
					{
						parent_candidate = child_acc;
					}
					if (child == _item && parent_candidate)
					{
						ret = parent_candidate->GetAccessibleObject();
						break;
					}
				}
			}
		}
	}
	else if ([attr_name isEqualToString:NSAccessibilityDisclosureLevelAttribute])
	{
		int depth;
		if (OpStatus::IsSuccess(_item->AccessibilityGetValue(depth)))
		{
			ret = [NSNumber numberWithInt:depth];
		}
	}
	else if ([attr_name isEqualToString:NSAccessibilityHeaderAttribute])
	{
		switch (_item->AccessibilityGetRole())
		{
			case Accessibility::kElementKindList:
			case Accessibility::kElementKindOutlineList:
			case Accessibility::kElementKindTable:
				OpAccessibleItem* child = _item->GetAccessibleChild(_item->GetAccessibleChildrenCount() - 1);
				CocoaOpAccessibilityAdapter* child_acc = ADAPTER_FOR_ITEM(child);
				if (child_acc && child->AccessibilityGetRole() == Accessibility::kElementKindHeaderList)
				{
					ret = child_acc->GetAccessibleObject(); 
				}
				break;
		}
	}
#ifdef _ACCESSIBILITY_DEBUG_LOGGING
DumpObject(ret);
printf("\n");
#endif
	return ret;
}

- (id)accessibilityAttributeValue:(NSString *)attr_name forParameter:(id)parameter
{
	BOOL with_attributes = FALSE;
	id ret = nil;
	if (!_item)
		return nil;

#ifdef _ACCESSIBILITY_DEBUG_LOGGING
OpString t;
OpString8 t8;
t.Reserve([attr_name length]+1);
[attr_name getCharacters:(unichar*)t.CStr()];
t[[attr_name length]] = 0;
t8.Set(t.CStr());
printf("%p accessibilityAttributeValue:\"%s\" forParameter:", self, t8.CStr());
DumpObject(parameter);
#endif

	if ([attr_name isEqualToString:NSAccessibilityStringForRangeParameterizedAttribute] ||
		(with_attributes = [attr_name isEqualToString:NSAccessibilityAttributedStringForRangeParameterizedAttribute]))
	{
		OpString s_value;
		NSString* mac_s_value;
		NSRange range = [parameter rangeValue];
		int start, end, len;
		_item->AccessibilityGetText(s_value);
		len = s_value.Length();
		start = range.location;
		end = start + range.length;

		if ((start >= 0) && (start <= len) && (end >= start) && (end <= len))
		{
			mac_s_value = [NSString stringWithCharacters:(unichar*)s_value.CStr() + start length:end - start];
			if (with_attributes)
			{
				ret = [[NSAttributedString alloc] initWithString:mac_s_value];
			}
			else
			{
				ret = mac_s_value;
			}
		}
	}
#ifdef _ACCESSIBILITY_DEBUG_LOGGING
printf(" -> ");
DumpObject(ret);
printf("\n");
#endif
	return ret;
}

- (NSBOOL)accessibilityIsAttributeSettable:(NSString *)attr_name
{
	if (!_item)
		return FALSE;

#ifdef _ACCESSIBILITY_DEBUG_LOGGING
OpString t;
OpString8 t8;
t.Reserve([attr_name length]+1);
[attr_name getCharacters:(unichar*)t.CStr()];
t[[attr_name length]] = 0;
t8.Set(t.CStr());
printf("%p accessibilityIsAttributeSettable:\"%s\"\n", self, t8.CStr());
#endif

	if ([attr_name isEqualToString:NSAccessibilityValueAttribute])
	{
		switch (_item->AccessibilityGetRole())
		{
			case Accessibility::kElementKindButton:
//			case Accessibility::kElementKindCheckbox:
//			case Accessibility::kElementKindRadioButton:
			case Accessibility::kElementKindHorizontalScrollbar:
			case Accessibility::kElementKindVerticalScrollbar:
			case Accessibility::kElementKindSlider:
			case Accessibility::kElementKindSinglelineTextedit:
			case Accessibility::kElementKindMultilineTextedit:
				return !(_item->AccessibilityGetState() & Accessibility::kAccessibilityStateReadOnly);
		}
	}
	else if ([attr_name isEqualToString:NSAccessibilityFocusedAttribute])
	{
		return TRUE;
	}
	else if ([attr_name isEqualToString:NSAccessibilitySelectedAttribute])
	{
		switch (_item->AccessibilityGetRole())
		{
			case Accessibility::kElementKindListRow:
//			case Accessibility::kElementKindCheckbox:
//			case Accessibility::kElementKindRadioButton:
				return TRUE;
		}
	}
	else if ([attr_name isEqualToString:NSAccessibilitySelectedChildrenAttribute] ||
			[attr_name isEqualToString:NSAccessibilitySelectedRowsAttribute])
	{
		switch (_item->AccessibilityGetRole())
		{
			case Accessibility::kElementKindList:
			case Accessibility::kElementKindOutlineList:
			case Accessibility::kElementKindTable:
				return TRUE;
		}
	}
	else if ([attr_name isEqualToString:NSAccessibilityDisclosingAttribute])
	{
		Accessibility::State state = _item->AccessibilityGetState();
		return !!(state & Accessibility::kAccessibilityStateExpandable);
	}
	return FALSE;
}

- (void)accessibilitySetValue:(id)value forAttribute:(NSString *)attr_name
{
	int max_val = 0;
	CFIndex len;
	uni_char* buffer;
	if (!_item)
		return;

#ifdef _ACCESSIBILITY_DEBUG_LOGGING
OpString t;
OpString8 t8;
t.Reserve([attr_name length]+1);
[attr_name getCharacters:(unichar*)t.CStr()];
t[[attr_name length]] = 0;
t8.Set(t.CStr());
printf("%p accessibilitySetValue:", self);
DumpObject(value);
printf(" forAttribute:\"%s\"\n", t8.CStr());
#endif

	if (_item->AccessibilityGetState() & Accessibility::kAccessibilityStateReadOnly)
		return;

	if ([attr_name isEqualToString:NSAccessibilityValueAttribute])
	{
		switch (_item->AccessibilityGetRole())
		{
			case Accessibility::kElementKindButton:
			case Accessibility::kElementKindCheckbox:
			case Accessibility::kElementKindRadioButton:
				_item->AccessibilitySetValue([value intValue]);
				break;
			case Accessibility::kElementKindHorizontalScrollbar:
			case Accessibility::kElementKindVerticalScrollbar:
			case Accessibility::kElementKindSlider:
				_item->AccessibilityGetMaxValue(max_val);
				_item->AccessibilitySetValue(([value floatValue] * ((double)max_val)));
				break;
			case Accessibility::kElementKindSinglelineTextedit:
			case Accessibility::kElementKindMultilineTextedit:
				len = [value length];
				buffer = new uni_char[len+1];
				[value getCharacters:(unichar*)buffer];
				buffer[len] = 0;
				_item->AccessibilitySetText(buffer);
				delete [] buffer;
				break;
		}
	}
	else if ([attr_name isEqualToString:NSAccessibilityFocusedAttribute])
	{
		_item->AccessibilitySetFocus();
	}
	else if ([attr_name isEqualToString:NSAccessibilitySelectedAttribute])
	{
		OpAccessibleItem* parent = _item->GetAccessibleParent();
		switch (_item->AccessibilityGetRole())
		{
			case Accessibility::kElementKindListRow:
				if (parent)
				{
					if ([value boolValue])
						parent->AccessibilityChangeSelection(Accessibility::kSelectionSetAdd, _item);
					else
						parent->AccessibilityChangeSelection(Accessibility::kSelectionSetRemove, _item);
				}
				break;
			case Accessibility::kElementKindCheckbox:
			case Accessibility::kElementKindRadioButton:
				_item->AccessibilitySetValue([value boolValue]);
				break;
		}
	}
	else if ([attr_name isEqualToString:NSAccessibilitySelectedChildrenAttribute] ||
			[attr_name isEqualToString:NSAccessibilitySelectedRowsAttribute])
	{
		switch (_item->AccessibilityGetRole())
		{
			case Accessibility::kElementKindList:
			case Accessibility::kElementKindOutlineList:
			case Accessibility::kElementKindTable:
				_item->AccessibilityChangeSelection(Accessibility::kSelectionDeselectAll, NULL);
				for (int i = 0; i < _item->GetAccessibleChildrenCount(); i++)
				{
					OpAccessibleItem* child = _item->GetAccessibleChild(i);
					CocoaOpAccessibilityAdapter* child_acc = ADAPTER_FOR_ITEM(child);
					id child_ax = child_acc ? child_acc->GetAccessibleObject() : nil;
					if (child_ax && [value containsObject:child_ax])
						_item->AccessibilityChangeSelection(Accessibility::kSelectionSetAdd, child);
				}
				break;
		}
	}
	else if ([attr_name isEqualToString:NSAccessibilityDisclosingAttribute])
	{
		_item->AccessibilitySetExpanded([value boolValue]);
	}
	return;
}

- (NSBOOL)accessibilityIsIgnored {
	if (!_item)
		return YES;
	
	switch (_item->AccessibilityGetRole())
	{
		case Accessibility::kElementKindView:
		case Accessibility::kElementKindWindow:
		case Accessibility::kElementKindWorkspace:
		case Accessibility::kElementKindUnknown:
			return YES;
	}
    return NO;
}

- (id)accessibilityHitTest:(NSPoint)point {
	id ret = nil;
	OpAccessibleItem* cand = _item;
	if (_item) {
		do {
			OpAccessibleItem*child = cand->GetAccessibleChildOrSelfAt(point.x, [[NSScreen mainScreen] frame].size.height-point.y);
			if (child && child != cand)
				cand = child;
			else break;
		} while (true);
		if (cand && cand != _item) {
			CocoaOpAccessibilityAdapter* child_acc = ADAPTER_FOR_ITEM(cand);
			ret = child_acc ? child_acc->GetAccessibleObject() : nil;
		}
	}
#ifdef _ACCESSIBILITY_DEBUG_LOGGING
printf("%p accessibilityHitTest(%f, %f) -> ", self, point.x, point.y);
DumpObject(ret);
printf("\n");
#endif
	return ret;
}

- (id)accessibilityFocusedUIElement
{
	id ret = nil;
	OpAccessibleItem* cand = _item;
	if (_item) {
		do {
			OpAccessibleItem*child = cand->GetAccessibleFocusedChildOrSelf();
			if (child && child != cand)
				cand = child;
			else break;
		} while (true);
		if (cand) {
			CocoaOpAccessibilityAdapter* child_acc = ADAPTER_FOR_ITEM(cand);
			ret = child_acc ? child_acc->GetAccessibleObject() : nil;
		}
	}
#ifdef _ACCESSIBILITY_DEBUG_LOGGING
printf("%p accessibilityFocusedUIElement -> ", self);
DumpObject(ret);
printf("\n");
#endif
	return ret;
}

- (NSArray *)accessibilityActionNames
{
	if (!_action_names)
	{
		_action_names = [[NSMutableArray alloc] init];
		Accessibility::ElementKind role = _item->AccessibilityGetRole();
		switch (role)
		{
			case Accessibility::kElementKindView:
			case Accessibility::kElementKindWindow:
			case Accessibility::kElementKindWorkspace:
			case Accessibility::kElementKindUnknown:
			case Accessibility::kElementKindScrollview:
			case Accessibility::kElementKindStaticText:
				break;
			default:
				[_action_names addObject:NSAccessibilityPressAction];
		}
		switch (role)
		{
			case Accessibility::kElementKindDropdown:
				if (_item->GetAccessibleChildrenCount() > 2)
					break;
			case Accessibility::kElementKindDropdownButtonPart:
				[_action_names addObject:NSAccessibilityShowMenuAction];
		}
	}
	return _action_names;
}

- (NSString *)accessibilityActionDescription:(NSString *)action
{
	if ([action isEqualToString:NSAccessibilityPressAction])
	{
		return @"press";
	}
	else if ([action isEqualToString:NSAccessibilityShowMenuAction])
	{
		return @"show menu";
	}
	return nil;
}

- (void)accessibilityPerformAction:(NSString *)action
{
	if (!_item)
		return;

	if ([action isEqualToString:NSAccessibilityPressAction] || [action isEqualToString:NSAccessibilityShowMenuAction])
	{
		_item->AccessibilityClicked();
	}
}

- (NSUInteger)accessibilityArrayAttributeCount:(NSString *)attribute
{
	if ([attribute isEqualToString:NSAccessibilityChildrenAttribute])
	{
		int accChildrenCount = _item->GetAccessibleChildrenCount();
		BOOL is_dropdown = (_item->AccessibilityGetRole() == Accessibility::kElementKindDropdown);
		unsigned long realChildrenCount = 0;
		for (int child_i = 0; child_i < accChildrenCount; child_i++)
		{
			if (is_dropdown && ((accChildrenCount == 2) || (child_i == (accChildrenCount-1))))
				continue;
			OpAccessibleItem* child = _item->GetAccessibleChild(child_i);
			Accessibility::State state = child->AccessibilityGetState();
			if (state & Accessibility::kAccessibilityStateInvisible)
				continue;
			realChildrenCount++;
		}
#ifdef _ACCESSIBILITY_DEBUG_LOGGING
printf("%p accessibilityArrayAttributeCount -> %lu\n", self, realChildrenCount);
OP_ASSERT(realChildrenCount == [[self accessibilityAttributeValue:attribute] count]);
#endif
		return realChildrenCount;
	}
	else
	{
		return [[self accessibilityAttributeValue:attribute] count];
	}
}

- (NSUInteger)accessibilityIndexOfChild:(id)seek_child
{
	int accChildrenCount = _item->GetAccessibleChildrenCount();
	BOOL is_dropdown = (_item->AccessibilityGetRole() == Accessibility::kElementKindDropdown);
	unsigned long realChildrenCount = 0;
	unsigned long res = NSNotFound;
	for (int child_i = 0; child_i < accChildrenCount; child_i++)
	{
		if (is_dropdown && ((accChildrenCount == 2) || (child_i == (accChildrenCount-1))))
			continue;
		OpAccessibleItem* child = _item->GetAccessibleChild(child_i);
		Accessibility::State state = child->AccessibilityGetState();
		if (state & Accessibility::kAccessibilityStateInvisible)
			continue;
		if ([seek_child accessibleItem] == child) {
			res = realChildrenCount;
			break;
		}
		realChildrenCount++;
	}
#ifdef _ACCESSIBILITY_DEBUG_LOGGING
printf("%p accessibilityIndexOfChild:", self);
DumpObject(seek_child);
printf(" -> %lu\n", res);
OP_ASSERT(res == [[self accessibilityAttributeValue:NSAccessibilityChildrenAttribute] indexOfObject:seek_child]);
#endif
	return res;
}

- (NSArray *)accessibilityArrayAttributeValues:(NSString *)attribute index:(NSUInteger)index maxCount:(NSUInteger)maxCount
{
#ifdef _ACCESSIBILITY_DEBUG_LOGGING
OpString t;
OpString8 t8;
t.Reserve([attribute length]+1);
[attribute getCharacters:(unichar*)t.CStr()];
t[[attribute length]] = 0;
t8.Set(t.CStr());
printf("%p accessibilityArrayAttributeValues:\"%s\" index:%lu maxCount:%lu -> ", self, t8.CStr(), index, maxCount);
#endif
	id res = nil;
	if ([attribute isEqualToString:NSAccessibilityChildrenAttribute])
	{
		int accChildrenCount = _item->GetAccessibleChildrenCount();
		BOOL is_dropdown = (_item->AccessibilityGetRole() == Accessibility::kElementKindDropdown);
		unsigned long realChildrenCount = 0;
		NSMutableArray* accChildren = [[NSMutableArray alloc] init];
		for (int child_i = 0; child_i < accChildrenCount; child_i++)
		{
			if (is_dropdown && ((accChildrenCount == 2) || (child_i == (accChildrenCount-1))))
				continue;
			OpAccessibleItem* child = _item->GetAccessibleChild(child_i);
			if (child)
			{
				Accessibility::State state = child->AccessibilityGetState();
				if (state & Accessibility::kAccessibilityStateInvisible)
					continue;
				if (realChildrenCount>=index && realChildrenCount<index+maxCount) {
					CocoaOpAccessibilityAdapter* child_acc = ADAPTER_FOR_ITEM(child);
					if (child_acc)
					{
						id child_ax = child_acc->GetAccessibleObject();
						if (child_ax) {
							[accChildren addObject:child_ax];
						}
					}
				}
				realChildrenCount++;
				if (realChildrenCount>=index+maxCount)
					continue;
			}
		}
		res = accChildren;
	}
	else
	{
		res = [self accessibilityAttributeValue:attribute];
		if (res && index >= [res count])
			res = nil;
		if (res) {
			res = [res subarrayWithRange:NSMakeRange(index, MIN(maxCount, [res count]-index))];
		}
	}
#ifdef _ACCESSIBILITY_DEBUG_LOGGING
DumpObject(res);
printf("\n");
#endif
	return res;
}
@end

#endif // ACCESSIBILITY_EXTENSION_SUPPORT
