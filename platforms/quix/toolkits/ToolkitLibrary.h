/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef TOOLKIT_LIBRARY_H
#define TOOLKIT_LIBRARY_H

#include "platforms/quix/toolkits/NativeSkinElement.h"
#include "platforms/utilix/x11types.h"

class ToolkitUiSettings;
class ToolkitColorChooser;
class ToolkitFileChooser;
class ToolkitMainloopRunner;
class ToolkitWidgetPainter;
class ToolkitPrinterIntegration;

/** @brief Represents external toolkit library that can communicate with Opera
  * Functionality that can be handled differently depending on toolkit should
  * be implemented here, such as getting different UI elements or using native
  * dialogs.
  */
class ToolkitLibrary
{
public:
	struct PopupMenuLayout
	{
		/** LeftMargin <IMAGE> ImageMargin <TEXT> TextMargin <SHORTCUT> <SUBMENU> RightMargin */
		unsigned int left_margin;
		unsigned int image_margin;
		unsigned int text_margin;
		unsigned int right_margin;
		unsigned int arrow_width;
		/** Thickness of frame around the menu window */
		unsigned int frame_thickness;
		/** If > 0 the submenu arrow will start inside shortcut column and grow # pixels further */
		unsigned int arrow_overlap;
		/** Number of pixels a sub menu is moved to left and up wrt to the parent menu entry. */
		unsigned int sub_menu_x_offset;
		unsigned int sub_menu_y_offset;
		/** When too close to the right edge of the screen, place pop-up menu
		  * to the left of the anchor (reflect).  If @c false, align with the
		  * right edge.  Ignored for submenus (they are always reflected).
		  */
		bool vertical_screen_edge_reflects;
	};

	struct MenuBarLayout
	{
		// Thickness of frame around the menu bar
		unsigned int frame_thickness;
		// Spacing between menu elements
		unsigned int spacing;
		// Margin between left/right border and menu item
		unsigned int h_margin;
		// Margin between top/bottom border and menu item
		unsigned int v_margin;
		// Extra margin that can be used to tune toolkit appearance
		unsigned int bottom_margin;
		// The help button should be drawn at the very right (or left)
		bool separate_help;
	};

public:
	virtual ~ToolkitLibrary() {}

	/** Initialize this library
	  * @param display X display to use
	  * @return true on success, false on failure
	  */
	virtual bool Init(X11Types::Display* display) = 0;

	/** @return A handler for a specific element type or NULL if not available
	  * if non-NULL, the caller is responsible for the deletion of this object
	  */
	virtual NativeSkinElement* GetNativeSkinElement(NativeSkinElement::NativeType type) = 0;

	/** @return An object that can be used to paint complete widgets, or 0 if not supported
	  * The object returned is owned by the ToolkitLibrary
	  */
	virtual ToolkitWidgetPainter* GetWidgetPainter() = 0;

	/** @return An object that can be used to determine settings for the UI
	  * The object returned is owned by the ToolkitLibrary
	  */
	virtual ToolkitUiSettings* GetUiSettings() = 0;

	/** @return A color chooser object for this toolkit
	  * FIXME
	  */
	virtual ToolkitColorChooser* CreateColorChooser() = 0;

	/** @return A file chooser object for this toolkit
	  * if non-NULL, the caller is responsible for the deletion of this object
	  */
	virtual ToolkitFileChooser* CreateFileChooser() = 0;

	/** Set an Opera mainloop runner that can be used to run a slice
	  * @param runner Mainloop runner that can be used by this toolkit to run the Opera main loop if necessary
	  */
	virtual void SetMainloopRunner(ToolkitMainloopRunner* runner) = 0;

	/** @return An object that can be used to print pages, or 0 if not supported
	  * if non-NULL, the caller is responsible for the deletion of this object
	  */
	virtual ToolkitPrinterIntegration* CreatePrinterIntegration() = 0;

	/** @return true if the toolkit supports different styles and the style has changed -
	  * if the toolkit has no support for different styles this function should return false
	  */
	virtual bool IsStyleChanged() = 0;

	/** @return true or false depending if the toolkit implementation requires
	 *  opera to block input events while a modal dialog is shown.
	 */
	virtual bool BlockOperaInputOnDialogs() = 0;

	/**
	 * @returns the name of the toolkit and the version (if possible)
	 * Format: "name (version)". The string is owned by the toolkit.
	 */
	virtual const char* ToolkitInformation() = 0;
	
	/**
	 * Called by Opera when it is about to start shutdown
	 */
	virtual void OnOperaShutdown() { SetMainloopRunner(0); }

	/**
	 * Called when Opera has changed color scheme
	 *
	 * @param mode 0: No modification,
	 *             1: Blend with system color,
	 *             2: Blend with @ref colorize_color
	 * @param colorize_color Blend color when  @ref mode is 2 
	 */
	virtual void SetColorScheme(uint32_t mode, uint32_t colorize_color) = 0;

	/**
	 * Returns true if toolkit is used for skinning
	 */
	virtual bool HasToolkitSkinning() { return true; }

	/**
	 * Allows the toolkit to tune the popup menu layout. The layout
	 * arrives prefilled, so values that do not need to be changed
	 * can be ignored.
	 *
	 * @param layout The layout structure
	 */
	virtual void GetPopupMenuLayout(PopupMenuLayout& layout) {}

	/**
	 * Allows the toolkit to tune the menu bar layout. The layout
	 * arrives prefilled, so values that do not need to be changed
	 * can be ignored.
	 *
	 * @param layout The layout structure
	 */
	virtual void GetMenuBarLayout(MenuBarLayout& layout) {}

};

// Declarations that the library should implement
extern "C" ToolkitLibrary* CreateToolkitLibrary();

#endif // TOOLKIT_LIBRARY_H
