/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Manuela Hutter (manuelah)
 */

#ifndef FAKE_FEATURE_SETUP_WIZARD_H
#define FAKE_FEATURE_SETUP_WIZARD_H

#include "adjunct/quick/Application.h"
#include "adjunct/quick/controller/selftest/src/FakeFeatureController.h"
#include "adjunct/quick/controller/selftest/src/FakeFeatureSettingsContext.h"
#include "adjunct/quick/view/FeatureSetupWizard.h"

#include "adjunct/quick_toolkit/widgets/OpGroup.h"

class OperaAccountContext;

class FakeFeatureDialog : public FeatureDialog
{
friend class DialogProxy;

public:
	// constructors/destructors
	FakeFeatureDialog(OperaAccountContext* context = NULL) :
	  FeatureDialog(g_desktop_account_manager, context, TRUE),
		m_has_opened_license(FALSE) {}
	~FakeFeatureDialog() { /* TODO necessary? g_application->GetDesktopWindowCollection().RemoveDesktopWindow(this); */ }

	virtual	const char*		GetHelpAnchor() {return "fakefeature.html";}

	// make member functions publicly accessible
	using	Dialog::GetCurrentPage;
#ifdef QUICKTK_CAP_HAS_DIALOG_PAGE_COUNT
	using	Dialog::GetPageCount;
#endif // QUICKTK_CAP_HAS_DIALOG_PAGE_COUNT

	// others
	virtual void	ShowLicenseDialog()
	{
		m_has_opened_license = TRUE;
		FeatureDialog::ShowLicenseDialog();
	}

	// selftest helpers
	BOOL	HasOpenedLicense()	{ return m_has_opened_license; }

	virtual const char*		GetFeatureSettingsProgressIconName() const	{ return ""; } // stub
	virtual const char*		GetFeatureSettingsProgressLabelName() const	{ return ""; } // stub
	virtual const char*		GetFeatureSettingsErrorLabelName() const	{ return ""; } // stub
	virtual BOOL			IsFeatureEnabled() const					{ return TRUE; } // stub

protected:
	virtual FeatureController*		GetFeatureController()			{ return &m_feature_controller;}
	virtual FeatureSettingsContext* GetPresetFeatureSettings()		{ return &m_feature_settings; }
	virtual FeatureSettingsContext*	ReadCurrentFeatureSettings()	{ return &m_feature_settings; } // stub

	virtual void		InitIntroPage()		{}
	virtual void		InitSettingsPage()	{}

	virtual const char*	GetFeatureIntroPageName()		const	{ return "fakefeature_intro_page"; }
	virtual const char*	GetFeatureSettingsPageName()	const	{ return "fakefeature_settings_page"; }

	BOOL				HasValidSettings()	{ return TRUE; }


private: 
	BOOL						m_has_opened_license;
	FakeFeatureController		m_feature_controller;
	FakeFeatureSettingsContext	m_feature_settings;
};

#endif // FAKE_FEATURE_SETUP_WIZARD_H
