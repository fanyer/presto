/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef INPUT_MANAGER_DIALOG_H
#define INPUT_MANAGER_DIALOG_H

#include "adjunct/quick_toolkit/widgets/Dialog.h"
#include "adjunct/desktop_util/treemodel/simpletreemodel.h"
#include "adjunct/quick/managers/opsetupmanager.h"
#include "modules/inputmanager/inputmanager.h"

class OpTreeView;
class PrefsFile;
class URL;

/***********************************************************************************
**
**	InputManagerDialog
**
***********************************************************************************/

class InputManagerDialog : public Dialog
{
	public:
			
								InputManagerDialog(INT32 index, OpSetupType type) :
									m_input_treeview(NULL),
									m_input_prefs(NULL),
									m_input_index(index),
									m_input_model(2),
									m_input_type(type) {}
		virtual					~InputManagerDialog();

		DialogType				GetDialogType()			{return TYPE_OK_CANCEL;}
		Type					GetType()				{return DIALOG_TYPE_BOOKMARK_MANAGER;}
		const char*				GetWindowName()			{return "Input Manager Dialog";}
		const char*				GetHelpAnchor();

		virtual void			OnInit();
		virtual UINT32			OnOk();
		void					OnCancel();
		void					OnInitVisibility();

		void					OnChange(OpWidget *widget, BOOL changed_by_mouse);
		virtual BOOL			OnInputAction(OpInputAction* action);
		virtual void			OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks);
		virtual void			OnItemEdited(OpWidget *widget, INT32 pos, INT32 column, OpString& text);

		enum ItemType
		{
			ITEM_CATEGORY,
			ITEM_SECTION,
			ITEM_ENTRY
		};

		static void WriteShortcutSections(OpSetupType input_type, URL& url);
		static void WriteShortcutSections(OpSetupType input_type, const char** shortcut_section, URL& url);

	private:

		class Item
		{
			public:

								Item(ItemType type, const char* section) : m_item_type(type), m_is_changed(FALSE)
								{
									m_section.Set(section);
								}

				OpString8&		GetSection()	{return m_section;}
				void			SetChanged(BOOL changed) {m_is_changed = changed;}
				BOOL			IsChanged()		{return m_is_changed;}
				BOOL			IsCategory()	{return m_item_type == ITEM_CATEGORY;}
				BOOL			IsSection()		{return m_item_type == ITEM_SECTION;}
				BOOL			IsEntry()		{return m_item_type == ITEM_ENTRY;}

				void			MakeSectionString(OpString8& string, BOOL defaults)
				{
					string.Set(m_section);

					if (defaults)
					{
						string.Append("    (defaults)");
					}
					else if (m_is_changed)
					{
						string.Append("    (changed)");
					}
				}

			private:

				ItemType		m_item_type;
				BOOL			m_is_changed;
				OpString8		m_section;
		};

		void					AddShortcutSection(const char* shortcut_section, INT32 root, INT32 parent_index,BOOL force_default);
		void					AddShortcutSections(const char** shortcut_section, INT32 root);
		void					OnItemChanged(INT32 model_pos);

		static const char*	m_shortcut_sections[];		
		static const char*	m_advanced_shortcut_sections[];		

		OpTreeView*				m_input_treeview;
		PrefsFile*				m_input_prefs;
		INT32					m_input_index;
		AutoTreeModel<Item>		m_input_model;
		OpSetupType				m_input_type;
		INT32					m_old_file_index;				//used in setupmanager
		INT32					m_new_file_index;				//used in setupmanager
};

#endif //INPUT_MANAGER_DIALOG_H
