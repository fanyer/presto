/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/desktop_util/boot/DesktopBootstrap.h"
#include "adjunct/desktop_util/resources/ResourceUtils.h"
#include "adjunct/desktop_util/filelogger/desktopfilelogger.h"
#include "adjunct/desktop_pi/DesktopGlobalApplication.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick/application/BrowserProduct.h"
#include "adjunct/quick/managers/ManagerHolder.h"
#include "adjunct/quick/managers/opsetupmanager.h"
#include "adjunct/widgetruntime/GadgetProduct.h"
#include "modules/hardcore/mh/messobj.h"

class StartupMessageHandler : public MessageObject
{
public:
	typedef DesktopBootstrap::Listener Listener;

	StartupMessageHandler(Listener& listener, Application& application,
			ManagerHolder& manager_holder)
		: m_listener(&listener), m_application(&application),
		  m_manager_holder(&manager_holder)
	{
		OpMessage msg_list[] = {
			MSG_QUICK_APPLICATION_START,
			MSG_QUICK_APPLICATION_START_CONTINUE
		};
		g_main_message_handler->SetCallBackList(
				this, 0, msg_list, ARRAY_SIZE(msg_list));
	}

	~StartupMessageHandler()
	{
		g_main_message_handler->UnsetCallBacks(this);
	}

	//
	// MessageObject
	//
	virtual void HandleCallback(
			OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
	{
		if (msg == MSG_QUICK_APPLICATION_START)
		{
			OP_PROFILE_METHOD("Startup sequence 1 completed");

			// Init managers, they might be used in the following sequence
			{
				OP_PROFILE_METHOD("Desktop managers initialized");
				if (OpStatus::IsError(m_listener->OnInitDesktopManagers(*m_manager_holder)))
				{
					g_desktop_global_application->Exit();
					return;
				}
			}

			// Shall have been posted by the platform when they want the
			// desktop application (quick) to begin its StartupSequence
			StepStartupSequence(par1, par2);
		}
		else if (msg == MSG_QUICK_APPLICATION_START_CONTINUE)
		{
			OP_PROFILE_METHOD("Startup sequence 2 completed");
			// Posted by the StartupSequence to drive it forward, or by one
			// of the dialogs it uses.
			StepStartupSequence(par1, par2);
		}
	}

private:
	void StepStartupSequence(MH_PARAM_1 par1, MH_PARAM_2 par2)
	{
		if (Listener::DONE == m_listener->OnStepStartupSequence(par1, par2))
		{
			OP_PROFILE_METHOD("Application start completed");

			m_application->Start();
		}
	}

	Listener* m_listener;
	Application* m_application;
	ManagerHolder* m_manager_holder;
};


DesktopBootstrap& DesktopBootstrap::GetInstance()
{
	static DesktopBootstrap instance;
	return instance;
}


DesktopBootstrap::DesktopBootstrap()
	: m_application(NULL), m_manager_holder(NULL), m_startup_message_handler(NULL),
	  m_disable_hwaccel(false)
{
}


OP_STATUS DesktopBootstrap::Init()
{
	const BOOL initialized =
			NULL != m_listener.get() || NULL != m_init_info.get();
	OP_ASSERT(!initialized || !"Already initialized");
	if (initialized)
	{
		return OpStatus::ERR;
	}

	Product * product = 0;
	RETURN_IF_ERROR(ChooseProduct(&product));
	OP_ASSERT(product != 0);


	OP_PROFILE_METHOD("Boot the product");
	m_listener = product->CreateBootstrapListener();
	RETURN_OOM_IF_NULL(m_listener.get());

	OpString profile_name;
	RETURN_IF_ERROR(product->GetOperaProfileName(profile_name));

	m_init_info = OP_NEW(OperaInitInfo, ());
	RETURN_OOM_IF_NULL(m_init_info.get());
	RETURN_IF_ERROR(ResourceUtils::Init(m_init_info.get(),
					profile_name.HasContent() ? profile_name.CStr() : NULL));

	return OpStatus::OK;
};



OP_STATUS DesktopBootstrap::AddProducts()
{
#ifdef WIDGET_RUNTIME_SUPPORT
	RETURN_IF_ERROR(AddProduct(OP_NEW(GadgetProduct, ())));
#endif
	RETURN_IF_ERROR(AddProduct(OP_NEW(BrowserProduct, ())));
	return OpStatus::OK;
}


OP_STATUS DesktopBootstrap::ChooseProduct(Product ** ret_product)
{
	RETURN_IF_ERROR(AddProducts());
	for (UINT32 i = 0; i < m_products.GetCount(); i++)
	{
		/* NOTE: OP_PROFILE_METHOD may access its parameter when it
		 * goes out of scope, so don't modify prof_name_product!
		 */
		char prof_name_product[20];
		op_snprintf(prof_name_product, 20, "Boot product %d?", i);
		OP_PROFILE_METHOD(prof_name_product);

		Product * candidate_product = m_products.Get(i);
		BOOL boot_this_product = FALSE;
		/* despite the name of this method, "BootMe" does not boot
		 * anything, but only sets "boot_this_product" to TRUE if the
		 * product detects that it is the product that should be
		 * booted.
		 */
		RETURN_IF_ERROR(candidate_product->BootMe(boot_this_product));
		if (boot_this_product)
		{
			*ret_product = candidate_product;
			return OpStatus::OK;
		};
	};
	return OpStatus::ERR;
};



DesktopBootstrap::~DesktopBootstrap()
{
	OP_ASSERT(NULL == m_application
			|| !"Application was not destroyed.  Missing call to ShutDown()?");
	CleanUp();
}


void DesktopBootstrap::CleanUp()
{
	OP_DELETE(m_startup_message_handler);
	m_startup_message_handler = NULL;

	OP_DELETE(m_manager_holder);
	m_manager_holder = NULL;

	OP_DELETE(m_application);
	m_application = NULL;

	OpSetupManager::Destroy();
}


OP_STATUS DesktopBootstrap::AddProduct(Product* product)
{
	return m_products.Add(product);
}


OP_STATUS DesktopBootstrap::Boot()
{
	OpAutoPtr<Application> application(m_listener->OnCreateApplication());
	RETURN_OOM_IF_NULL(application.get());

	OpAutoPtr<ManagerHolder> manager_holder(OP_NEW(ManagerHolder, ()));
	RETURN_OOM_IF_NULL(manager_holder.get());

	OpAutoPtr<StartupMessageHandler> startup_sequence(
			OP_NEW(StartupMessageHandler,
					(*m_listener, *application, *manager_holder)));
	RETURN_OOM_IF_NULL(startup_sequence.get());

	m_application = application.release();
	m_manager_holder = manager_holder.release();
	m_startup_message_handler = startup_sequence.release();
	return OpStatus::OK;
}


void DesktopBootstrap::ShutDown()
{
	OP_PROFILE_MSG("Shutdown sequence starting");

	m_listener->OnShutDown();

	CleanUp();

	m_listener.reset();
}


OperaInitInfo* DesktopBootstrap::GetInitInfo() const
{
	return m_init_info.get();
}


Application* DesktopBootstrap::GetApplication()
{
	return m_application;
}

const Application* DesktopBootstrap::GetApplication() const
{
	return m_application;
}


DesktopBootstrap::Listener* DesktopBootstrap::GetListener()
{
	return m_listener.get();
}


MessageObject* DesktopBootstrap::GetMessageObject()
{
	return m_startup_message_handler;
}

bool DesktopBootstrap::GetHWAccelerationDisabled()
{
#ifdef HIDE_HW_DEVICES
	return true;
#else
	return m_disable_hwaccel;
#endif
}
