/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef EXTENSION_INSTALL_CONTROLLER_H
#define EXTENSION_INSTALL_CONTROLLER_H

#include "adjunct/desktop_util/adt/opproperty.h"
#include "adjunct/quick/extensions/ExtensionUtils.h"
#include "adjunct/quick_toolkit/contexts/OkCancelDialogContext.h"

class ExtensionInstaller;
class OpGadgetClass;

/**
 *
 *	ExtensionInstallGenericController
 *
 *  Base class for extensions dialog controllers:
 *  install, update, preferences.
 *
 */
class ExtensionInstallGenericController : public OkCancelDialogContext
{
public:
	ExtensionInstallGenericController(OpGadgetClass& gclass, bool use_expand = true);

protected:
	virtual void InitL();
	void InitControlsL();
	OP_STATUS SetWarningAccessLevel();

	const TypedObjectCollection* m_widgets;
    

	OpGadgetClass* m_gclass;
	Image m_icon_img;
    ExtensionUtils::AccessWindowType m_dialog_mode;
	OpProperty<bool>   m_private_mode;
	OpProperty<bool>   m_secure_mode;
	bool			   m_use_expand;
};



/**
 *
 *	ExtensionInstallController
 *
 *  Class controlls dialog which shows up if the user requested to install 
 *  non dev-mode extension.
 *
 */
class ExtensionInstallController : public ExtensionInstallGenericController
{
public:
	ExtensionInstallController(ExtensionInstaller* installer,OpGadgetClass& gclass);

	virtual void OnOk();
	virtual void OnCancel();
	
private:
    ExtensionInstaller* m_installer;
};



/**
 *
 *	ExtensionPrivacyController
 *
 *  Class controlls dialog for setting up extensions privacy data
 *
 */
class ExtensionPrivacyController : public ExtensionInstallGenericController
{
public:

	ExtensionPrivacyController(OpGadgetClass& gclass,const uni_char* ext_wuid, bool private_access, bool secure_access);
	virtual void InitL();
	virtual void OnOk();
	
private:

    const OpStringC    m_ext_wuid;
};


/**
 *
 *	ExtensionUpdateController
 *
 *  Class controlls dialog which shows up if the there is  
 *  update for extension available and user presses "update" button 
 *  in extensions manager.
 *
 */
class ExtensionUpdateController : public ExtensionInstallGenericController
{
public:
	ExtensionUpdateController(OpGadgetClass& gclass,const OpStringC& gadget_id);

	virtual void OnOk();
	virtual void InitL();

private:

	OP_STATUS UpdateControls();
	
	OpGadgetClass* m_gclass_new;
	OpString m_gadget_id;
};

#endif //EXTENSION_INSTALL_CONTROLLER_H
