/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef EXTENSION_UTILS_H
#define	EXTENSION_UTILS_H

#include "modules/util/opstring.h"
#include "adjunct/widgetruntime/GadgetUtils.h"

class BitmapOpWindow;
class OpGadget;
class OpGadgetClass;

namespace ExtensionUtils
{
	/**
	 * Core's function IsThisAnExtensionGadgetPath can answer two questions
	 * 'Is this an extension zip archive?' or 'Is this an extension directory?'.
	 * Besides the zip archive question we need to answer the question
	 * 'Is this an extension config.xml file?'. This function when provided
	 * with path which ends in config.xml calls core's function with directory
	 * containing the config.xml file. If not config.xml at the end then
	 * OpGadgetManager::IsThisAnExtensionGadgetPath is called.
	 *
	 * @param some_path Some path to check if it's extension's path.
	 * @param[out] is_extension_path TRUE iff given path identifies
	 * an extension.
	 * @return Status.
	 */
	OP_STATUS IsExtensionPath(const OpStringC& some_path,
		BOOL& is_extension_path);

	/**
	 * Developer mode extensions are extensions installed via d'n'd
	 * of config.xml onto browser. We can recognize them in CORE 
	 * by checking if gadget's path is a directory, and that is the
	 * way we actually distinguish them.
	 *
	 * @param gclass Gadget's class.
	 * @param[out] in_dev_mode Filled with TRUE iff gadget is in dev mode.
	 * @return Status.
	 */
	OP_STATUS IsDevModeExtension(OpGadgetClass& gclass, BOOL& in_dev_mode);

	/**
	 * Check if extension is in dev mode
	 *
	 * @param wuid local extension identifier (as per widgets.dat)
	 * @param in_dev_mode set to @c TRUE iff extension is in dev mode
	 * @return statur
	 */
	OP_STATUS IsDevModeExtension(const OpStringC& wuid, BOOL& in_dev_mode);

	/**
	 * Find extension which is a non-developer extension.
	 *
	 * @param gid Gadget/Extension id.
	 * @param gadget[out] NULL if not found, else filled with
	 * @param starting_pos [in/out] in -starting pos for gadgets in extesnion manager, out - found pos or the end
	 * @return Status.
	 *
	 */
	OP_STATUS FindNormalExtension(const OpStringC& gid, OpGadget** gadget, UINT& starting_pos);

	/** Check if extension represented by installation file (*.oex) is installed.
	 *
	 * @param path full path to oex file
	 * @param result variable when result will be placed
	 * @return status of the operation
	 */
	OP_STATUS IsExtensionInstalled(const OpStringC& path, BOOL &result);

	/** Check if extension given by a path is present in installation package.
	 *
	 * @param path full path to oex file
	 * @param result variable when result will be placed
	 * @return status of the operation
	 */
	OP_STATUS IsExtensionInPackage(const OpStringC& path, BOOL &result);

	/**
	 * Extension can have userJS files included, regarding of this we
	 * show different strings in installer/privacy dialogs informing
	 * user about what extension can/can't do.
	 *
	 * @param gclass Gadget's class.
	 * @param has_userjs_includes Filled with TRUE iff extension has any
	 * userJS included.
	 * @return Status.
	 */
	OP_STATUS HasExtensionUserJSIncludes(OpGadgetClass* gclass,
			BOOL& has_userjs_includes);

	/**
	 * Obtains generic Opera extension icon from current skin.
	 * @param[out] icon receives gadget icon
	 * @param[in] size Get icon with designated size; If not given icon with size
	 *        16px will be returned
	 * @return status
	 */
	OP_STATUS GetGenericExtensionIcon(Image& icon, unsigned int size = 0);

    typedef enum {
        TYPE_SIMPLE = 0,
        TYPE_CHECKBOXES,
        TYPE_FULL			// not supported yet, see ExtensionUtils::AccessWindowType
    } AccessWindowType;
    
    AccessWindowType GetAccessWindowType(OpGadgetClass *gclass);
    
    /**
     * Retrieves access level string informing about extension access level
     *
     * @param[in] gclass Extension class access we're checking
     * @param[out] access_list Extension access level string
     *
     * @return status
     */
    
    OP_STATUS GetAccessLevelString(OpGadgetClass *gclass, OpString& access_list);
    /**
     * Checks access level of given gadget
     * @param[in]  gclass Gadget class access we're checking
     * @param[out] has_access_list Does gadget have access list defined
     * @param[out] access_list     Formatted access list
     * @param[out] has_userjs_includes Does given gadget have UserJS includes
     *
     * @return status
     */
    OP_STATUS GetAccessLevel(OpGadgetClass *gclass, BOOL& has_access_list, 
                             OpString& access_list, BOOL& has_userjs_includes);
    
	/**
	 * Provides standard fallback icon for extensions.
	 */
	class FallbackExtensionIconProvider : public GadgetUtils::FallbackIconProvider
	{
		unsigned int m_size;
	public:
		FallbackExtensionIconProvider(unsigned int size) : m_size(size) {}
		OP_STATUS GetIcon(Image& icon) { return GetGenericExtensionIcon(icon, m_size); }
	};

	/**
	 * Gets an extension icon for given OpGadgetClass.
	 * Specialized version of GadgetUtils::GetIcon that uses generic extension icon as fallback
	 * and allows icons to be scaled down only (caller is expected to center smaller icons).
	 * @param[out] img receives the icon image
	 * @param[in] gadget_class the gadget class
	 * @param[in] desired_width desired width
	 * @param[in] desired_height desired height
	 * @return status
	 */
	OP_STATUS GetExtensionIcon(Image& img,
		 const OpGadgetClass* gadget_class,
		 UINT32 desired_width = 64,
  		 UINT32 desired_height = 64);

	/**
	 * Checks if an extension is a Speed Dial extension.
	 *
	 * @param wuid local extension identifier (as per widgets.dat)
	 * @return @c true iff @a wuid identifies a Speed Dial extension
	 */
	bool RequestsSpeedDialWindow(const OpStringC& wuid);

	/**
	 * Checks if an extension is a Speed Dial extension.
	 *
	 * @param gadget_class extension class
	 * @return @c true iff this is a Speed Dial extension
	 */
	bool RequestsSpeedDialWindow(const OpGadgetClass& gadget_class);

	/**
	* Creates extension class based on file pointed by path
	*
	* @param[in]   path full path to the extension oex file
	* @param[out]  extension_class object for gadted class
	* @return[out] status
	*/
	OP_STATUS CreateExtensionClass(const OpStringC& path,OpGadgetClass** extension_class);

	/**
	 * Gets the bitmap window of a Speed Dial extension.
	 *
	 * @param wuid local extension identifier (as per widgets.dat)
	 * @return the bitmap window pointer or @c NULL if the extension is not
	 * 		installed or enabled
	 */
	BitmapOpWindow* GetExtensionBitmapWindow(const OpStringC& wuid);

	/**
	 * Determines if an extension is installed and enabled.
	 *
	 * @param wuid local extension identifier (as per widgets.dat)
	 * @return @c true iff @a wuid identifies an enabled extension
	 */
	bool IsExtensionRunning(const OpStringC& wuid);


	inline const uni_char* GetExtensionsExtension() { return UNI_L("oex"); }

}

#endif // EXTENSION_UTILS_H
