/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef OP_GROUP_H
#define OP_GROUP_H

#include "modules/widgets/OpWidget.h"
#include "adjunct/quick_toolkit/widgets/OpLabel.h"

class OpButton;
class OpButtonGroup;

// == OpGroup ==================================================

class OpGroup : public OpWidget
{
	friend OP_STATUS QuickOpWidgetBase::Construct<OpGroup>(OpGroup**);

	protected:
		OpGroup();

	public:
		~OpGroup();

		virtual OP_STATUS		SetText(const uni_char* text);
		virtual OP_STATUS		GetText(OpString& text);

		void					SetTab(OpButton* tab) {m_tab = tab;}
		OpButton*				GetTab() { return m_tab; }

		// Implementing the OpTreeModelItem interface

		virtual Type		GetType() {return WIDGET_TYPE_GROUP;}

		virtual void OnDeleted();

		//To identify radio buttons children
		virtual void AddChild(OpWidget* child, BOOL is_internal_child = FALSE, BOOL first = FALSE);

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
		virtual Accessibility::ElementKind AccessibilityGetRole() const {return Accessibility::kElementKindView;}


		virtual int	GetAccessibleChildrenCount();
		virtual OpAccessibleItem* GetAccessibleChild(int child_nr);
		virtual int GetAccessibleChildIndex(OpAccessibleItem* child);
		virtual OpAccessibleItem* GetAccessibleChildOrSelfAt(int x, int y);
		virtual OpAccessibleItem* GetAccessibleFocusedChildOrSelf();
#ifdef PROPERTY_PAGE_CHILDREN_OF_TABS
		virtual OpAccessibleItem* GetAccessibleParent();
#endif //PROPERTY_PAGE_CHILDREN_OF_TABS
#endif // ACCESSIBILITY_EXTENSION_SUPPORT
		
		virtual void OnShow(BOOL show);

		virtual void OnFontChanged() { m_string.NeedUpdate(); }
		virtual void OnDirectionChanged() { OnFontChanged(); }

		// OpWidgetListener
		virtual void OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks);
		virtual BOOL OnContextMenu(OpWidget* widget, INT32 child_index, const OpPoint& menu_point, const OpRect* avoid_rect, BOOL keyboard_invoked);

		void OnMouseMove(const OpPoint &point){}
		void OnMouseLeave() {}
		void OnMouseDown(const OpPoint &point, MouseButton button, UINT8 nclicks){}
		void OnMouseUp(const OpPoint &point, MouseButton button, UINT8 nclicks);

	private:

		OpWidgetString		m_string;
		OpButtonGroup*		m_button_group;
		BOOL				m_fail_button_group;
		OpButton*			m_tab;
};

// == OpSpacer ==================================================

class OpSpacer : public OpWidget
{
	public:

		enum SpacerType
		{
			SPACER_FIXED,
			SPACER_DYNAMIC,
			SPACER_SEPARATOR,
			SPACER_WRAPPER
		};

		OpSpacer(SpacerType spacer_type = SPACER_FIXED);

		SpacerType GetSpacerType() const { return m_spacer_type; }

		virtual void Update();
		virtual void GetRequiredSize(INT32 &width, INT32 &height);
		virtual void GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows);
		virtual void OnLayout();
		virtual void OnAdded();

		void  SetFixedSize(INT32 size) { m_fixed_size = size; }
		INT32 GetFixedSize() const { return m_fixed_size; }

		// Implementing the OpTreeModelItem interface

		virtual Type		GetType() {return WIDGET_TYPE_SPACER;}
#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
		virtual Accessibility::ElementKind AccessibilityGetRole() const {return Accessibility::kElementKindView;}
		virtual int	GetAccessibleChildrenCount();
		virtual OpAccessibleItem* GetAccessibleChild(int);
		virtual OpAccessibleItem* GetAccessibleChildOrSelfAt(int x, int y);
		virtual OpAccessibleItem* GetAccessibleFocusedChildOrSelf();
#endif // ACCESSIBILITY_EXTENSION_SUPPORT

	private:

		SpacerType	m_spacer_type;
		OpLabel*	m_descr_label;
		BOOL		m_last_customize_state;
		INT32		m_fixed_size;
};

#endif // OP_GROUP_H
