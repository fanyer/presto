/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#include "core/pch.h"

#include "adjunct/quick_toolkit/widgets/WidgetTypeMapping.h"
#include "adjunct/desktop_util/string/hash.h"


namespace WidgetTypeMapping
{
	const struct
	{
		unsigned name_hash;
		OpTypedObject::Type widget_type;
	}
	s_mapping[] =
	{
		{ HASH_LITERAL("Button"), OpTypedObject::WIDGET_TYPE_BUTTON },
		{ HASH_LITERAL("Radiobutton"), OpTypedObject::WIDGET_TYPE_RADIOBUTTON },
		{ HASH_LITERAL("Checkbox"), OpTypedObject::WIDGET_TYPE_CHECKBOX },
		{ HASH_LITERAL("Listbox"), OpTypedObject::WIDGET_TYPE_LISTBOX },
		{ HASH_LITERAL("Treeview"), OpTypedObject::WIDGET_TYPE_TREEVIEW },
		{ HASH_LITERAL("Dropdown"), OpTypedObject::WIDGET_TYPE_DROPDOWN },
		{ HASH_LITERAL("Edit"), OpTypedObject::WIDGET_TYPE_EDIT },
		{ HASH_LITERAL("MultilineEdit"), OpTypedObject::WIDGET_TYPE_MULTILINE_EDIT },
		{ HASH_LITERAL("Label"), OpTypedObject::WIDGET_TYPE_DESKTOP_LABEL },
		{ HASH_LITERAL("Image"), OpTypedObject::WIDGET_TYPE_IMAGE },
		{ HASH_LITERAL("WebImage"), OpTypedObject::WIDGET_TYPE_WEBIMAGE },
		{ HASH_LITERAL("Filechooser"), OpTypedObject::WIDGET_TYPE_FILECHOOSER_EDIT },
		{ HASH_LITERAL("Folderchooser"), OpTypedObject::WIDGET_TYPE_FOLDERCHOOSER_EDIT },
		{ HASH_LITERAL("Toolbar"), OpTypedObject::WIDGET_TYPE_TOOLBAR },
		{ HASH_LITERAL("Group"), OpTypedObject::WIDGET_TYPE_GROUP },
		{ HASH_LITERAL("Splitter"), OpTypedObject::WIDGET_TYPE_SPLITTER },
		{ HASH_LITERAL("Browser"), OpTypedObject::WIDGET_TYPE_BROWSERVIEW },
		{ HASH_LITERAL("Progress"), OpTypedObject::WIDGET_TYPE_PROGRESSBAR },
		{ HASH_LITERAL("Tabs"), OpTypedObject::WIDGET_TYPE_TABS },
		{ HASH_LITERAL("Pagebar"), OpTypedObject::WIDGET_TYPE_PAGEBAR },
		{ HASH_LITERAL("Personalbar"), OpTypedObject::WIDGET_TYPE_PERSONALBAR },
		{ HASH_LITERAL("Status"), OpTypedObject::WIDGET_TYPE_STATUS_FIELD },
		{ HASH_LITERAL("ProgressField"), OpTypedObject::WIDGET_TYPE_PROGRESS_FIELD },
		{ HASH_LITERAL("Address"), OpTypedObject::WIDGET_TYPE_ADDRESS_DROPDOWN },
		{ HASH_LITERAL("Search"), OpTypedObject::WIDGET_TYPE_SEARCH_DROPDOWN },
		{ HASH_LITERAL("SearchEdit"), OpTypedObject::WIDGET_TYPE_SEARCH_EDIT },
		{ HASH_LITERAL("Zoom"), OpTypedObject::WIDGET_TYPE_ZOOM_DROPDOWN },
		{ HASH_LITERAL("Account"), OpTypedObject::WIDGET_TYPE_ACCOUNT_DROPDOWN },
		{ HASH_LITERAL("Scrollbar"), OpTypedObject::WIDGET_TYPE_SCROLLBAR },
		{ HASH_LITERAL("Quickfind"), OpTypedObject::WIDGET_TYPE_QUICK_FIND },
		{ HASH_LITERAL("Bookmarks"), OpTypedObject::WIDGET_TYPE_BOOKMARKS_VIEW },
		{ HASH_LITERAL("Contacts"), OpTypedObject::WIDGET_TYPE_CONTACTS_VIEW },
		{ HASH_LITERAL("Widgets"), OpTypedObject::WIDGET_TYPE_GADGETS_VIEW },
		{ HASH_LITERAL("Notes"), OpTypedObject::WIDGET_TYPE_NOTES_VIEW },
		{ HASH_LITERAL("Music"), OpTypedObject::WIDGET_TYPE_MUSIC_VIEW },
		{ HASH_LITERAL("Links"), OpTypedObject::WIDGET_TYPE_LINKS_VIEW },
		{ HASH_LITERAL("IdentifyField"), OpTypedObject::WIDGET_TYPE_IDENTIFY_FIELD },
		{ HASH_LITERAL("Hotlist"), OpTypedObject::WIDGET_TYPE_HOTLIST },
		{ HASH_LITERAL("Spacer"), OpTypedObject::WIDGET_TYPE_SPACER },
		{ HASH_LITERAL("DocumentEdit"), OpTypedObject::WIDGET_TYPE_DOCUMENT_EDIT },
		{ HASH_LITERAL("PanelSelector"), OpTypedObject::WIDGET_TYPE_PANEL_SELECTOR },
		{ HASH_LITERAL("MultilineLabel"), OpTypedObject::WIDGET_TYPE_MULTILINE_LABEL },
		{ HASH_LITERAL("Separator"), OpTypedObject::WIDGET_TYPE_SEPARATOR },
		{ HASH_LITERAL("Attachments"), OpTypedObject::WIDGET_TYPE_ATTACHMENT_LIST },
		{ HASH_LITERAL("Thumbnail"), OpTypedObject::WIDGET_TYPE_THUMBNAIL },
		{ HASH_LITERAL("Root"), OpTypedObject::WIDGET_TYPE_ROOT },
		{ HASH_LITERAL("SpeedDial"), OpTypedObject::WIDGET_TYPE_SPEEDDIAL },
		{ HASH_LITERAL("NumberEdit"), OpTypedObject::WIDGET_TYPE_NUMBER_EDIT },
		{ HASH_LITERAL("Slider"), OpTypedObject::WIDGET_TYPE_SLIDER },
		{ HASH_LITERAL("Calendar"), OpTypedObject::WIDGET_TYPE_CALENDAR },
		{ HASH_LITERAL("DateTime"), OpTypedObject::WIDGET_TYPE_DATETIME },
		{ HASH_LITERAL("HelpTooltip"), OpTypedObject::WIDGET_TYPE_HELP_TOOLTIP },
		{ HASH_LITERAL("SpeedDial Search"), OpTypedObject::WIDGET_TYPE_SPEEDDIAL_SEARCH },
		{ HASH_LITERAL("StatusImage"), OpTypedObject::WIDGET_TYPE_STATUS_IMAGE },
		{ HASH_LITERAL("Expand"), OpTypedObject::WIDGET_TYPE_EXPAND },
		{ HASH_LITERAL("NewPage"), OpTypedObject::WIDGET_TYPE_NEWPAGE_BUTTON },
		{ HASH_LITERAL("Icon"), OpTypedObject::WIDGET_TYPE_ICON },
		{ HASH_LITERAL("ComposeEdit"), OpTypedObject::WIDGET_TYPE_COMPOSE_EDIT },
		{ HASH_LITERAL("TrustAndSecurity Button"), OpTypedObject::WIDGET_TYPE_TRUST_AND_SECURITY_BUTTON },
		{ HASH_LITERAL("MinimizedUpdate"), OpTypedObject::WIDGET_TYPE_MINIMIZED_AUTOUPDATE_STATUS_BUTTON },
#ifdef WEBSERVER_SUPPORT
		{ HASH_LITERAL("UniteServicesView"), OpTypedObject::WIDGET_TYPE_UNITE_SERVICES_VIEW },
#endif // WEBSERVER_SUPPORT
		{ HASH_LITERAL("HotlistConfigButton"), OpTypedObject::WIDGET_TYPE_HOTLIST_CONFIG_BUTTON },
		{ HASH_LITERAL("Infobar"), OpTypedObject::WIDGET_TYPE_INFOBAR },
		{ HASH_LITERAL("DragScrollBar"), OpTypedObject::WIDGET_TYPE_DRAGSCROLLBAR },
		{ HASH_LITERAL("RichTextEditor"), OpTypedObject::WIDGET_TYPE_RICH_TEXT_EDITOR },
		{ HASH_LITERAL("ResizeSearchDropdown"), OpTypedObject::WIDGET_TYPE_RESIZE_SEARCH_DROPDOWN },
		{ HASH_LITERAL("StateButton"), OpTypedObject::WIDGET_TYPE_STATE_BUTTON },
		{ HASH_LITERAL("History"), OpTypedObject::WIDGET_TYPE_HISTORY_VIEW },
		{ HASH_LITERAL("FindTextBar"), OpTypedObject::WIDGET_TYPE_FINDTEXTBAR },
		{ HASH_LITERAL("AccordionItem"), OpTypedObject::WIDGET_TYPE_ACCORDION_ITEM },
		{ HASH_LITERAL("SearchWidgetItem"), OpTypedObject::WIDGET_TYPE_SEARCH_WIDGET_ITEM },
		{ HASH_LITERAL("SearchButtonItem"), OpTypedObject::WIDGET_TYPE_SEARCH_BUTTON_ITEM },
		{ HASH_LITERAL("SearchSuggestionItem"), OpTypedObject::WIDGET_TYPE_SEARCH_SUGGESTION_ITEM },
		{ HASH_LITERAL("SearchParentLabelItem"), OpTypedObject::WIDGET_TYPE_SEARCH_PARENT_LABEL_ITEM },
		{ HASH_LITERAL("SearchActionItem"), OpTypedObject::WIDGET_TYPE_SEARCH_ACTION_ITEM },
		{ HASH_LITERAL("SearchHistoryItem"), OpTypedObject::WIDGET_TYPE_SEARCH_HISTORY_ITEM },
		{ HASH_LITERAL("ZoomSlider"), OpTypedObject::WIDGET_TYPE_ZOOM_SLIDER },
		{ HASH_LITERAL("ZoomMenuButton"), OpTypedObject::WIDGET_TYPE_ZOOM_MENU_BUTTON },
		{ HASH_LITERAL("ButtonStrip"), OpTypedObject::WIDGET_TYPE_BUTTON_STRIP },
		{ HASH_LITERAL("MailSearchField"), OpTypedObject::WIDGET_TYPE_MAIL_SEARCH_FIELD },
		{ HASH_LITERAL("ExtensionButton"), OpTypedObject::WIDGET_TYPE_EXTENSION_BUTTON },
		{ HASH_LITERAL("ExtensionSet"), OpTypedObject::WIDGET_TYPE_EXTENSION_SET },
		{ HASH_LITERAL("RichTextLabel"), OpTypedObject::WIDGET_TYPE_RICHTEXT_LABEL },
		{ HASH_LITERAL("StackLayout"), OpTypedObject::WIDGET_TYPE_STACK_LAYOUT },
		{ HASH_LITERAL("GridLayout"), OpTypedObject::WIDGET_TYPE_GRID_LAYOUT },
		{ HASH_LITERAL("DynamicGridLayout"), OpTypedObject::WIDGET_TYPE_DYNAMIC_GRID_LAYOUT },
		{ HASH_LITERAL("PagingLayout"), OpTypedObject::WIDGET_TYPE_PAGING_LAYOUT },
		{ HASH_LITERAL("GroupBox"), OpTypedObject::WIDGET_TYPE_GROUP_BOX },
		{ HASH_LITERAL("ToggleButton"), OpTypedObject::WIDGET_TYPE_TOGGLE_BUTTON },
		{ HASH_LITERAL("ScrollContainer"), OpTypedObject::WIDGET_TYPE_SCROLL_CONTAINER },
		{ HASH_LITERAL("PasswordStrength"), OpTypedObject::WIDGET_TYPE_PASSWORD_STRENGTH },
		{ HASH_LITERAL("SkinElement"), OpTypedObject::WIDGET_TYPE_SKIN_ELEMENT },
		{ HASH_LITERAL("Composite"), OpTypedObject::WIDGET_TYPE_COMPOSITE },
		{ HASH_LITERAL("DropdownWindow"), OpTypedObject::WIDGET_TYPE_DROPDOWN_WITHOUT_EDITBOX },
		{ HASH_LITERAL("Empty"), OpTypedObject::WIDGET_TYPE_EMPTY },
		{ HASH_LITERAL("FlowLayout"), OpTypedObject::WIDGET_TYPE_FLOW_LAYOUT },
		{ HASH_LITERAL("PagingLayout"), OpTypedObject::WIDGET_TYPE_PAGING_LAYOUT },
		{ HASH_LITERAL("WrapLayout"), OpTypedObject::WIDGET_TYPE_WRAP_LAYOUT },
		{ HASH_LITERAL("Centered"), OpTypedObject::WIDGET_TYPE_CENTERED },
	};

	OpTypedObject::Type FromHash(unsigned hash)
	{
		for (size_t i = 0; i < ARRAY_SIZE(s_mapping); i++)
		{
			if (s_mapping[i].name_hash == hash)
				return s_mapping[i].widget_type;
		}
		return OpTypedObject::UNKNOWN_TYPE;
	};
};


OpTypedObject::Type WidgetTypeMapping::FromString(const OpStringC8& string)
{
	if (string.IsEmpty())
		return OpTypedObject::UNKNOWN_TYPE;

	return FromHash(Hash::String(string.CStr()));
}


OpTypedObject::Type WidgetTypeMapping::FromString(const OpStringC8& string, unsigned length)
{
	if (string.IsEmpty())
		return OpTypedObject::UNKNOWN_TYPE;

	return FromHash(Hash::String(string.CStr(), length));
}
