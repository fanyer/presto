/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef OP_TABS_H
#define OP_TABS_H

#include "adjunct/quick_toolkit/widgets/OpToolbar.h"
#include "adjunct/quick_toolkit/windows/DesktopWindow.h"

/***********************************************************************************
**
**	OpTabs
**
***********************************************************************************/

class OpTabs : public OpToolbar, public SettingsListener
{
	protected:
								OpTabs();
	public:
		virtual					~OpTabs();

		static					OP_STATUS Construct(OpTabs** obj);

		OpButton*				AddTab(const uni_char* title, void* user_data = NULL);
		void					RemoveTab(INT32 pos);

		INT32					GetActiveTab()		{return GetSelected();}
		INT32					GetTabCount()		{return GetWidgetCount();}
		void*					GetTabUserData(INT32 index)	{return GetUserData(index);}
		INT32					FindTab(void* user_data);
		void 					UpdateTabStates(int selected_tab);

		virtual BOOL			SetSelected(INT32 index, BOOL invoke_listeners = FALSE, BOOL changed_by_mouse = FALSE);

		// Implementing the OpTreeModelItem interface

		virtual Type			GetType() {return WIDGET_TYPE_TABS;}

		// Settings listener

		virtual void				OnSettingsChanged(DesktopSettings* settings);

		// OpWidget

		virtual void			OnAdded();
		virtual void			OnRemoving();

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
		virtual Accessibility::ElementKind AccessibilityGetRole() const {return Accessibility::kElementKindRadioTabGroup;}
#ifdef MSWIN
		virtual OpAccessibleItem* GetAccessibleChildOrSelfAt(int x, int y);
#endif //MSWIN
#endif

	private:
		/**
		 * An input context inserted into the context tree just under the dialog
		 * context (if there is one) to "steal" tab cycling actions.
		 */
		class TabCycleContext : public OpInputContext
		{
		public:
			explicit TabCycleContext(OpTabs& tabs) : m_tabs(&tabs), m_child(NULL) {}
			virtual BOOL OnInputAction(OpInputAction* action);
			void Insert();
			void Remove();
		private:
			OpTabs* m_tabs;
			OpInputContext* m_child;
		} m_tab_cycle_context;
};

#endif // OP_TABS_H
