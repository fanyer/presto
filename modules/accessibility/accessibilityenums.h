/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef OPACCESSIBILITYENUMS_H
#define OPACCESSIBILITYENUMS_H

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT

namespace Accessibility
{
	const int NoSuchChild = -1;

	enum {
		kSelectionSetSelect   = 1 << 0, // Set selection to child, (click)
		kSelectionSetAdd      = 1 << 1, // Add child to selection, (ctrl-click)
		kSelectionSetRemove   = 1 << 2, // Remove child from selection (ctrl-click)
		kSelectionSetExtend   = 1 << 3, // Extend selection to child (shift-click)
		kSelectionSelectAll   = 1 << 4, // Select everything. Parameter ignored.
		kSelectionDeselectAll = 1 << 5  // Deselect everything. Parameter ignored.
	};
	typedef UINT32 SelectionSet; 

	typedef enum {
		kScrollToTop,
		kScrollToBottom,
		kScrollToAnywhere,
	} ScrollTo;

	typedef enum {
		kElementKindApplication,			// A window or web page that behaves like a stand-alone application. Widgets could be an example of this.
		kElementKindWindow,					// Windows are usually supplied with all relevant information by the system, but this can be used for additional (help) information that isn't endemic to the native window object.
		kElementKindAlert,					// A simple alert box/area, an example would be the yellow warning area on top of pages in IE. Only used by alerts written in DOM using ARIA
		kElementKindAlertDialog,			// A simple alert dialog. Quick dialogs are automatically labled as such, so this is only used by dialogs written in DOM using ARIA
		kElementKindDialog,					// A dialog. Quick dialogs are automatically labled as such, so this is only used by dialogs written in DOM using ARIA.
		kElementKindView,					// View controls normally have no real functionality (mostly just containers), and implementer is free to ignore them.
		kElementKindWebView,				// A browser area... A ViualDevice, in our case. This can help the user locate the "interesting" information.
		kElementKindToolbar,
		kElementKindButton,					// If it can be clicked, and doesn't fall into any other category it is a button.
		kElementKindCheckbox,
		kElementKindDropdown,
		kElementKindRadioTabGroup,			// Important to properly put radio buttons/tabs in a group rather that rely on visual hints.
		kElementKindRadioButton,			// The parent of a radio button IS ALWAYS a radio tab group, even if there is only one radio in that group.
		kElementKindTab,					// The parent of a tab is always a radio tab group
		kElementKindStaticText,				// Explanatory text in dialog as well as the most important type inside the document. A paragraph of text is the most that should be fit in one text element.
		kElementKindSinglelineTextedit,
		kElementKindMultilineTextedit,
		kElementKindHorizontalScrollbar,
		kElementKindVerticalScrollbar,
		kElementKindScrollview,
		kElementKindSlider,
		kElementKindProgress,				// Progress indicator. For indeterminate progress bars max value is -1, otherwise max is positive and 0 <= value <= max value.
		kElementKindLabel,					// Labels functions as description for controls that have no title of their own.
		kElementKindDescriptor,				// A descriptor for an object. Sort of like labels, but needs to be mapped to other relations in ATK/IAccessible2
		kElementKindLink,					// Links usually either have some text of their own, or contain one or more elements of type kElementKindStaticText or kElementKindImage.
		kElementKindImage,					// All images in a document.
		kElementKindMenuBar,				// A Menu bar.
		kElementKindMenu,					// A must-have for the various things Opera displays that are intended to function/look like menus, such as form select elements & Autocomplete drop-down.
		kElementKindMenuItem,
		kElementKindList,					// Simple list of rows, non-expandable, one column
		kElementKindGrid,					// A "list" of items in grid formation (like many OS'es display files). Not a table, because "row" and "column" are unimportant, basically this is a list randomly ordered in 2 dimensions.
		kElementKindOutlineList,			// List of expandable rows.
		kElementKindTable,					// Table with columns and rows, usually non-expandable
		kElementKindHeaderList,				// List of header buttons
		kElementKindListRow,				// A row in a table. Contains one item per column of the table. Parent of list rows is always a list, outline list, table or grid.
		kElementKindListHeader,				// Column header (on rare occations a row header)
		kElementKindListColumn,				// A column in a table. Contains one item per row. Parent of list rows are always a list, outline list, table or grid. NOTE that the parents of these children is the row, not the column!
		kElementKindListCell,				// A cell in a table or grid. Lists and outline lists don't have cells, they only have rows.
		kElementKindScrollbarPartArrowUp,	// The "line up" button in a scrollbar
		kElementKindScrollbarPartArrowDown,	// The "line down" button in a scrollbar
		kElementKindScrollbarPartPageUp,	// The part of the track above/before the knob. Page up.
		kElementKindScrollbarPartPageDown,	// The part of the track below/after the knob. Page down.
		kElementKindScrollbarPartKnob,		// The bit you grab to scroll to a specific spot.
		kElementKindDropdownButtonPart,		// The "arrow down" bit.
		kElementKindDirectory,
		kElementKindDocument,
		kElementKindLog,
		kElementKindPanel,
		kElementKindWorkspace,
		kElementKindSplitter,
		kElementKindSection,				// A section of a document or a div
		kElementKindParagraph,				// A paragraph of a document
		kElementKindForm,
		kElementKindUnknown
	} ElementKind;


	enum {
		kAccessibilityStateNone					= 0,
		kAccessibilityStateFocused				= 1 << 0,
		kAccessibilityStateDisabled				= 1 << 1,
		kAccessibilityStateInvisible			= 1 << 2,
		kAccessibilityStateOffScreen			= 1 << 3,
		kAccessibilityStateCheckedOrSelected	= 1 << 4,
		kAccessibilityStateGrayed				= 1 << 5,
		kAccessibilityStateAnimated				= 1 << 6,
		kAccessibilityStateExpanded				= 1 << 7,
		kAccessibilityStatePassword				= 1 << 8,
		kAccessibilityStateReadOnly				= 1 << 9,
		kAccessibilityStateTraversedLink		= 1 << 10,
		kAccessibilityStateDefaultButton		= 1 << 11,
		kAccessibilityStateCancelButton			= 1 << 12,
		kAccessibilityStateExpandable			= 1 << 13,
		kAccessibilityStateReliesOnLabel		= 1 << 14,
		kAccessibilityStateReliesOnDescriptor	= 1 << 15,
		kAccessibilityStateMultiple				= 1 << 16,
		kAccessibilityStateBusy					= 1 << 17,
		kAccessibilityStateRequired				= 1 << 18,
		kAccessibilityStateInvalid				= 1 << 19
	};
	typedef UINT32 State;

	typedef enum {
		kAccessibilityEventMoved,						//The widget has moved relatively to its parent
		kAccessibilityEventResized,						//The size of the widget has changed
		kAccessibilityEventTextChanged,					//The visual title of the widget has changed (or the title it would have if the display of the text was turned on)
		kAccessibilityEventTextChangedBykeyboard,		//Same as previous event, except that the text has been explicitly changed by the user
		kAccessibilityEventDescriptionChanged,			//The description (tooltip) of the widget has changed
		kAccessibilityEventURLChanged,					//Usually only useful for kElementKindLink
		kAccessibilityEventSelectedTextRangeChanged,
		kAccessibilityEventSelectionChanged,
		kAccessibilityEventSetLive,						//The feedback level/rudeness of the widget/area has changed. The screen-reader should pay attention to this area
		kAccessibilityEventStartDragAndDrop,
		kAccessibilityEventEndDragAndDrop,
		kAccessibilityEventStartScrolling,
		kAccessibilityEventEndScrolling,
		kAccessibilityEventReorder
	} EventType;

	//Stores an event that can either be one of the EventType constants
	//or a state mask indicating which states have changed (have been set or unset)
	typedef struct _Event
	{
		union {
			State state_event;
			EventType event_type;
		} event_info;

		BOOL is_state_event;

		_Event(State state_event)
		{
			event_info.state_event = state_event;
			is_state_event = TRUE;
		}

		_Event(EventType event_type)
		{
			event_info.event_type = event_type;
			is_state_event = FALSE;
		}
	} Event;
}

#endif //OPACCESSIBILITYENUMS_H
#endif //ACCESSIBILITY_EXTENSION_SUPPORT
