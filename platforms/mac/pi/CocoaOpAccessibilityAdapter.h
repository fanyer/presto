/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef COCOA_ACCESSIBILITYADAPTER_H
#define COCOA_ACCESSIBILITYADAPTER_H

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT

#include "modules/pi/OpAccessibilityAdapter.h"

#define BOOL NSBOOL
#import <AppKit/NSAccessibility.h>
#import <AppKit/NSEvent.h>
#import <AppKit/NSScreen.h>
#import <AppKit/NSView.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSGeometry.h>
#import <Foundation/NSString.h>
#import <Foundation/NSAttributedString.h>
#undef BOOL

@interface OperaAccessibility : NSObject {
	OpAccessibleItem* _item;
	NSObject* _root;
	NSMutableArray* _attr_names;
	NSMutableArray* _action_names;
	NSMutableDictionary* _attr_values;
}

- (id)initWithItem:(OpAccessibleItem*)item root:(NSObject*)root;
- (OpAccessibleItem*)accessibleItem;
- (void)itemDeleted;
- (NSArray *)accessibilityAttributeNames;
- (id)accessibilityAttributeValue:(NSString *)attribute;
- (NSBOOL)accessibilityIsAttributeSettable:(NSString *)attribute;
- (void)accessibilitySetValue:(id)value forAttribute:(NSString *)attribute;
- (NSBOOL)accessibilityIsIgnored;
- (id)accessibilityHitTest:(NSPoint)point;
- (NSArray *)accessibilityActionNames;
- (void)accessibilityPerformAction:(NSString *)action;
@end


class CocoaOpAccessibilityAdapter : public OpAccessibilityAdapter
{
public:
	CocoaOpAccessibilityAdapter(OpAccessibleItem* accessible_item, id master = nil);
	virtual ~CocoaOpAccessibilityAdapter();

	/** Get the accessible item represented by this adapter
	 * 
	 * @return the accessible item
	 */
	virtual OpAccessibleItem* GetAccessibleItem();

	/** Signals the platform that an accessible item has been removed from the tree
	 * No value is returned because there is nothing to do if this fails
	 */
	virtual void AccessibleItemRemoved();

	/** Send an accessibility event to the system.
	 */
	OP_STATUS SendEvent(Accessibility::Event evt);

	id GetAccessibleObject() { return m_object; }
	id GetAccessibleRoot() { return m_master; }

private:
	OpAccessibleItem* m_accessible_item;
	id m_object;
	id m_master;
};

#endif // ACCESSIBILITY_EXTENSION_SUPPORT

#endif // COCOA_ACCESSIBILITYADAPTER_H
