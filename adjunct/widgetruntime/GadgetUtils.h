/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef GADGETUTILS_H
#define	GADGETUTILS_H

#ifdef WIDGET_RUNTIME_SUPPORT

#include "modules/img/image.h"
#include "modules/util/opstring.h"
#include "modules/locale/locale-enum.h"

class OpGadgetClass;


class GadgetUtils
{
public:
	/**
	 * Represents a validation result.
	 */
	struct Validation
	{
		enum Result
		{
			V_ERROR = 0,
			V_WARNING,
			V_OK,
		};

		Validation();

		Result m_result;

		/**
		 * If m_result is not V_OK, this should be the relevant explanation
		 * message.
		 */
		Str::LocaleString m_msg_id;
	};


	/**
	 * Starts the gadget installer in a new Opera process.
	 *
	 * @param gadget_path the @c .wgt file pathname
	 * @return status
	 */
	static OP_STATUS ExecuteGadgetInstaller(const OpStringC& gadget_path);

	/**
	 * Provides fallback icon if OpGadgetClass does not have suitable icons.
	 */
	class FallbackIconProvider
	{
	public:
		virtual ~FallbackIconProvider() {}
		virtual OP_STATUS GetIcon(Image& icon) = 0;
	};

	/**
	 * Provides standard fallback icon for gadgets.
	 */
	class FallbackGadgetIconProvider : public FallbackIconProvider
	{
		unsigned int m_size;
	public:
		FallbackGadgetIconProvider(unsigned int size) : m_size(size) {}
		OP_STATUS GetIcon(Image& icon) { return GetGenericGadgetIcon(icon, m_size); }
	};

	 /**
	  * Gets a gadget icon for given OpGadgetClass.
	  *
	  * @param img receives the icon image
	  * @param gadget_class the gadget class 
	  * @param desired_width desired width
	  * @param desired_height desired height
	  * @param effect_mask which effects should be applied
	  * @param effect_value "strength" of effects
	  * @param can_scale_up if TRUE and best icon is still smaller it will be scaled up
	  * @param can_scale_down if TRUE and best icon is still bigger it will be scaled down
	  * @param provider fallback used if gadget_class does not have its own icons
	  * @return status
	  */
	 static OP_STATUS GetGadgetIcon(Image& img,
			const OpGadgetClass* gadget_class,
			UINT32 desired_width,
			UINT32 desired_height,
			INT32 effect_mask,
			INT32 effect_value,
			BOOL can_scale_up,
			BOOL can_scale_down,
			FallbackIconProvider& provider);

	 /**
	  * Gets a gadget icon for given OpGadgetClass.
	  * Specialized version of GetIcon that uses generic gadget icon as fallback and allows
	  * icons to be resized both up and down.
	  */
	 static OP_STATUS GetGadgetIcon(Image& img,
		 const OpGadgetClass* gadget_class,
		 UINT32 desired_width = 32,
		 UINT32 desired_height = 32,
		 INT32 effect_mask = 0,
		 INT32 effect_value = 0)
	 {
		 FallbackGadgetIconProvider fallback_provider(desired_width);
		 return GetGadgetIcon(img, gadget_class, desired_width, desired_height, effect_mask, effect_value, TRUE, TRUE, fallback_provider);
	 }

	/**
	 * Obtains the generic Opera gadget icon from the current skin.
	 *
	 * @param[out] icon Receives the gadget icon.
	 * @param[in] size Get icon with designated size; If not given icon with size
	 *        16px will be returned
	 * @return Error status
	 */
	static OP_STATUS GetGenericGadgetIcon(Image& icon, unsigned int size = 0);

	/**
	 * Obtains <widgetname> element content from config.xml file.
	 *
	 * @param[in] conf_xml_path Path to widget's config.xml
	 * @param[out] widget_name Name of the widget from <widgetname> element
	 * @return Error status
	 */
	static OP_STATUS GetGadgetName(const OpStringC& conf_xml_path,
		OpString8& widget_name);

	/**
	 * Obtains number of gadgets installed in OS.
	 *
	 * @retval -1 indicates any error
	 */
	static int GetInstallationCount();
};


#endif // WIDGET_RUNTIME_SUPPORT

#endif	// !GADGETUTILS_H
