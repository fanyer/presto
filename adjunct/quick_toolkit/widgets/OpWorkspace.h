/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef OP_WORKSPACE_H
#define OP_WORKSPACE_H

#include "modules/widgets/OpWidget.h"
#include "modules/util/adt/opvector.h"
#include "adjunct/desktop_util/treemodel/optreemodel.h"
#include "adjunct/quick_toolkit/windows/DesktopWindowListener.h"
#include "adjunct/quick/models/DesktopGroupModelItem.h"

class OpWindow;
class DesktopOpWindow;
class DocumentDesktopWindow;
class MailDesktopWindow;
class ChatDesktopWindow;
class WidgetContainer;
class DesktopWindowCollectionItem;
class DesktopWindowModelItem;
class DesktopGroupModelItem;

/***********************************************************************************
 **
 **	OpWorkspace
 **
 ***********************************************************************************/

class OpWorkspace : public OpWidget,
					public DesktopWindowListener,
					public OpTreeModel::Listener,
					public DesktopGroupListener
{
	public:

		class WorkspaceListener
		{
			public:
			virtual ~WorkspaceListener() {}

				virtual void		OnDesktopWindowAdded(OpWorkspace* workspace, DesktopWindow* desktop_window) = 0;
				virtual void		OnDesktopWindowRemoved(OpWorkspace* workspace, DesktopWindow* desktop_window) = 0;
				virtual void		OnDesktopWindowActivated(OpWorkspace* workspace, DesktopWindow* desktop_window, BOOL activate) = 0;
				virtual void		OnDesktopWindowAttention(OpWorkspace* workspace, DesktopWindow* desktop_window, BOOL attention) = 0;
				virtual void		OnDesktopWindowChanged(OpWorkspace* workspace, DesktopWindow* desktop_window) = 0;
				virtual void		OnDesktopWindowClosing(OpWorkspace* workspace, DesktopWindow* desktop_window, BOOL user_initiated) = 0;
				virtual void		OnDesktopWindowOrderChanged(OpWorkspace* workspace) = 0;
				virtual void		OnDesktopGroupAdded(OpWorkspace* workspace, DesktopGroupModelItem& group) = 0;
				virtual void		OnDesktopGroupCreated(OpWorkspace* workspace, DesktopGroupModelItem& group) = 0;
				virtual void		OnDesktopGroupRemoved(OpWorkspace* workspace, DesktopGroupModelItem& group) = 0;
				virtual void		OnDesktopGroupDestroyed(OpWorkspace* workspace, DesktopGroupModelItem& group) = 0;
				virtual void		OnDesktopGroupChanged(OpWorkspace* workspace, DesktopGroupModelItem& group) = 0;
				virtual void		OnWorkspaceEmpty(OpWorkspace* workspace,BOOL has_minimized_window) = 0;
				virtual void		OnWorkspaceAdded(OpWorkspace* workspace) = 0;
				virtual void		OnWorkspaceDeleted(OpWorkspace* workspace) = 0;
		};

							OpWorkspace(DesktopWindowModelItem* model_item = NULL);

		DesktopOpWindow*	GetOpWindow() {return m_window;}

		OP_STATUS			AddDesktopWindow(DesktopWindow* desktop_window);

		DesktopWindow*		GetDesktopWindowFromStack(INT32 pos) {return m_desktop_window_stack.Get(pos);}
		INT32				GetDesktopWindowCount() { return m_desktop_window_stack.GetCount(); }

		/** Get all desktop windows maintained by this workspace
		  * @param windows Windows maintained by this workspace
		  */
		OP_STATUS			GetDesktopWindows(OpVector<DesktopWindow>& windows, Type type = WINDOW_TYPE_UNKNOWN);

		/** @return Number of tabs this workspace maintains */
		UINT32				GetItemCount();

		/** @return tree model item that corresponds to this workspace (if it exists) */
		DesktopWindowModelItem* GetModelItem() { return m_model_item; }

		/** Executed when a group of tabs gets added to workspace
		  * @param group Group that has been added
		  */
		void				OnGroupCreated(DesktopGroupModelItem& group);
		void				OnGroupAdded(DesktopGroupModelItem& group);
		void				OnGroupRemoved(DesktopGroupModelItem& group);

		DesktopWindow*		GetActiveDesktopWindow() {return m_active_desktop_window;}
		DesktopWindow*		GetActiveNonHotlistDesktopWindow();

		DocumentDesktopWindow*	GetActiveDocumentDesktopWindow();
		MailDesktopWindow*	GetActiveMailDesktopWindow();
#ifdef IRC_SUPPORT
	    ChatDesktopWindow*  GetActiveChatDesktopWindow();
#endif

		INT32 				GetPosition(DesktopWindow* desktop_window);

		/** Looks for the window position in the parent group.
		  * @param desktop_window Window which position in the group should be found.
		  * @return Position of the window in the parent group or -1 if window is not grouped.
		  */
		INT32				GetPositionInGroup(DesktopWindow* desktop_window);

		INT32 				GetStackPosition(DesktopWindow* desktop_window) {return m_desktop_window_stack.Find(desktop_window);}
	
		/**
		  * Looks for the group identified by group_no and returns it.
		  * @param group_no Identifier of the group.
		  * @return Group or NULL if group not found.
		  */
		DesktopGroupModelItem*	GetGroup(INT32 group_no);

		OP_STATUS			AddListener(WorkspaceListener* listener) {return m_listeners.Add(listener);}
		void				RemoveListener(WorkspaceListener* listener) {m_listeners.RemoveByItem(listener);}

		void				CascadeAll();
		void				TileAll(BOOL vertically);
		void				CloseAll(BOOL immediately = FALSE);
		void				MaximizeAll();
		void				MinimizeAll();
		void				RestoreAll();
		void 				SetOpenInBackground( BOOL state );

		// == Hooks ======================

		void				OnDeleted();
		void				OnAdded();
		void				OnLayout();
		void				OnRelayout() {}
		void				OnShow(BOOL show);
		void				OnMove() {Relayout();}
		void				OnFocus(BOOL focus,FOCUS_REASON reason);
		void				OnWindowActivated(BOOL activate);

		virtual const char*	GetInputContextName() {return "Workspace";}

		// Implementing the DesktopWindowListener interface

		virtual void		OnDesktopWindowChanged(DesktopWindow* desktop_window);
		virtual void		OnDesktopWindowActivated(DesktopWindow* desktop_window, BOOL active);
		virtual void		OnDesktopWindowLowered(DesktopWindow* desktop_window);
		virtual void		OnDesktopWindowAttention(DesktopWindow* desktop_window, BOOL attention);
		virtual void		OnDesktopWindowClosing(DesktopWindow* desktop_window, BOOL user_initiated);
		virtual void		OnDesktopWindowParentChanged(DesktopWindow* desktop_window);
		virtual void		OnDesktopWindowStatusChanged(DesktopWindow* desktop_window, DesktopWindowStatusType type);
		virtual void		OnDesktopWindowResized(DesktopWindow* desktop_window, INT32 width, INT32 height);

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
		// Implementing the OpAccessibilityExtensionListener interface
		virtual Accessibility::ElementKind AccessibilityGetRole() const { return Accessibility::kElementKindWorkspace; }
		virtual int	GetAccessibleChildrenCount();
		virtual OpAccessibleItem* GetAccessibleChild(int);
		virtual int GetAccessibleChildIndex(OpAccessibleItem* child);
		virtual OpAccessibleItem* GetAccessibleChildOrSelfAt(int x, int y);
		virtual OpAccessibleItem* GetAccessibleFocusedChildOrSelf();

#endif //ACCESSIBILITY_EXTENSION_SUPPORT

		// Implementing the OpTreeModelItem interface

		virtual Type		GetType() {return WIDGET_TYPE_WORKSPACE;}

		INT32				GetParentWindowID() { return m_parent_window_id; };
		void				SetParentWindowID(INT32 wndid) { m_parent_window_id = wndid; };

		// OpTreeModel::Listener
		virtual void OnItemAdded(OpTreeModel* tree_model, INT32 item) {}
		virtual void OnItemChanged(OpTreeModel* tree_model, INT32 item, BOOL sort) {}
		virtual void OnItemRemoving(OpTreeModel* tree_model, INT32 pos) {}
		virtual void OnSubtreeRemoving(OpTreeModel* tree_model, INT32 parent, INT32 subtree_root, INT32 subtree_size) {}
		virtual void OnSubtreeRemoved(OpTreeModel* tree_model, INT32 parent, INT32 subtree_root, INT32 subtree_size) {}
		virtual void OnTreeChanging(OpTreeModel* tree_model) {}
		virtual void OnTreeChanged(OpTreeModel* tree_model);
		virtual void OnTreeDeleted(OpTreeModel* tree_model) {}

		// DesktopGroupListener
		virtual void OnCollapsedChanged(DesktopGroupModelItem* group, bool collapsed);
		virtual void OnActiveDesktopWindowChanged(DesktopGroupModelItem* group, DesktopWindow* active);
		virtual void OnGroupDestroyed(DesktopGroupModelItem* group);

		// OpWidgetListener
		virtual BOOL OnContextMenu(OpWidget* widget, INT32 child_index, const OpPoint& menu_point, const OpRect* avoid_rect, BOOL keyboard_invoked);
		virtual void OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks);
#if defined(_X11_SELECTION_POLICY_)
		virtual void OnDragMove(OpWidget* widget, OpDragObject* drag_object, INT32 pos, INT32 x, INT32 y);
		virtual void OnDragDrop(OpWidget* widget, OpDragObject* drag_object, INT32 pos, INT32 x, INT32 y);
#endif

		void 				EnableTransparentSkins(BOOL enable);
		BOOL 				IsTransparentSkinsEnabled(){return m_transparent_skins_enabled;}
	private:

		virtual OP_STATUS	Init();
		void 				GetWindowIdList(OpINT32Vector& list);
		void				UpdateHiddenStatus();

		DesktopWindow*		GetNextWindowToActivate(DesktopWindow* closing_desktop_window);

		OpVector<WorkspaceListener>		m_listeners;
		DesktopOpWindow*				m_window;
#ifdef MOUSE_GESTURE_ON_WORKSPACE
		// The only purpose of this widgetcontainer is to have a target 
		// to perform mouse gesture on, otherwise in a empty work space 
		// there is no target to receive the input
		// Why not a normal MDE_View? In that case we need to handle 
		// mouse message to set a proper mouse context and also deal with the
		// captured widget etc, WidgetContainer automatically do this.
		WidgetContainer*				m_widget_container;
#endif
		DesktopWindow*					m_active_desktop_window;
		OpVector<DesktopWindow>			m_desktop_window_stack;
		BOOL 							m_transparent_skins_enabled;
		INT32							m_parent_window_id;
		DesktopWindowModelItem*			m_model_item;
};

#endif // OP_WORKSPACE_H
