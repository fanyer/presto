/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef DESKTOP_BOOTSTRAP_H
#define DESKTOP_BOOTSTRAP_H

#include "modules/hardcore/opera/module.h"
#include "modules/util/opautoptr.h"
#include "modules/util/adt/opvector.h"

class Application;
class ManagerHolder;
class MessageObject;
class StartupMessageHandler;


#define g_desktop_bootstrap (&DesktopBootstrap::GetInstance())

/**
 * Manages the booting and shutting down of a Desktop product.  Maintains the
 * global Application object.
 *
 * All the supported products are "registered" in AddProducts().
 *
 * @author Wojciech Dzierzanowski (wdzierzanowski)
 */
class DesktopBootstrap
{
public:
	class Listener;

	/**
	 * Describes a Desktop product that can be booted.
	 *
 	 * @author Wojciech Dzierzanowski (wdzierzanowski)
	 */
	class Product
	{
	public:
		virtual ~Product()  {}

		virtual const char* GetName() const  { return ""; }

		/**
		 * @param boot_me should be set to @c TRUE iff the product is the one
		 *		that should be booted
		 */
		virtual OP_STATUS BootMe(BOOL& boot_me) const = 0;

		/**
		 * @param profile_name should receive the name of the Opera profile to
		 * 		be used for the product.  If left empty, the product shall use
		 * 		the default profile.
		 */
		virtual OP_STATUS GetOperaProfileName(OpString& profile_name) const = 0;
		
		virtual Listener* CreateBootstrapListener() const = 0;
	};


	/**
	 * Hooks for product-specific actions related to booting and shutting down.
	 *
 	 * @author Wojciech Dzierzanowski (wdzierzanowski)
	 */
	class Listener
	{
	public:
		enum StartupStepResult
		{
			CONTINUE,
			DONE,
		};

		virtual ~Listener()  {}

		/**
		 * It's time to create Application.  It will not be operational yet.
		 */
		virtual Application* OnCreateApplication() = 0;

		/**
		 * Start the Quick startup sequence, or move to the next sequence step.
		 * May be called repeatedly, depending on the value returned.
		 *
		 * @return @c StartupStepResult::DONE to indicate the startup sequence
		 * 		is now complete
		 */
		virtual StartupStepResult OnStepStartupSequence(MH_PARAM_1 par1,
				MH_PARAM_2 par2) = 0;

		/**
		 * It's time to initialize the Desktop managers, if and how required.
		 * Application and core globals are now initialized and usable. 
		 */
		virtual OP_STATUS OnInitDesktopManagers(
				ManagerHolder& manager_holder) = 0;

		/**
		 * It's time to perform any shut down actions required before
		 * Application is destroyed.
		 */
		virtual void OnShutDown() = 0;
	};

	static DesktopBootstrap& GetInstance();

	~DesktopBootstrap();

	/**
	 * Initializes the bootstrap object.  The platform must call it before the
	 * first time it needs the OperaInitInfo returned from GetInitInfo().
	 */
	OP_STATUS Init();

	/**
	 * Creates and initializes Application.  The platform must call it when it's
	 * ready to initialize Quick.
	 */
	OP_STATUS Boot();

	/**
	 * Triggers Application shut-down and destroys it.  The platform must call
	 * it to clean up Quick when terminating.
	 */
	void ShutDown();

	/**
	 * @return the OperaInitInfo initialized for the current boot.  Is @c NULL
	 * 		until after Init() is called.
	 */
	OperaInitInfo* GetInitInfo() const;

	/**
	 * @return the Application initialized for the current boot.  Is @c NULL
	 * 		until after Boot() is called.
	 */
	Application* GetApplication();
	const Application* GetApplication() const;


	/* Global settings.  These are typically set very early (often
	 * during command line parsing), not subsequently modified and
	 * they apply globally.
	 */

	/**
	 * Disable hardware acceleration.  Set when the early bootup
	 * sequence decides that it is too risky to use the hardware
	 * acceleration (e.g. during crash recovery).
	 */
	void DisableHWAcceleration() { m_disable_hwaccel = true; };

	/**
	 * @return Whether hardware acceleration should be avoided
	 * completely.
	 */
	bool GetHWAccelerationDisabled();


// The protected interface is meant for the selftests.
protected:
	DesktopBootstrap();

	/**
	 * @param product a product that can potentially be booted.  Ownership is
	 * 		transfered.
	 */
	OP_STATUS AddProduct(Product* product);

	Listener* GetListener();
	MessageObject* GetMessageObject();

private:
	/**
	 * Place AddProduct() calls here for all the Desktop products.
	 *
	 * When the time comes to boot, the products are asked in the order of
	 * registration if they should be booted.  The first product to answer "yes"
	 * is the one that is booted.
	 */
	OP_STATUS AddProducts();

	/**
	 * This method returns the product to be booted in 'ret_product.'
	 * This method will call AddProducts() to obtain the list of
	 * possible products.  See that method for details on the
	 * selection process.
	 *
	 * If no suitable product is found, OpStatus::ERR is returned.
	 */
	OP_STATUS ChooseProduct(Product ** ret_product);

	void CleanUp();

	OpAutoVector<Product> m_products;
	OpAutoPtr<OperaInitInfo> m_init_info;
	OpAutoPtr<Listener> m_listener;
	Application* m_application;
	ManagerHolder* m_manager_holder;
	StartupMessageHandler* m_startup_message_handler;

	bool m_disable_hwaccel;
};

#endif // DESKTOP_BOOTSTRAP_H
