/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef MODULES_WAND_WAND_MODULE_H
#define MODULES_WAND_WAND_MODULE_H

#ifdef WAND_SUPPORT

#include "modules/hardcore/opera/module.h"
#include "modules/util/adt/opvector.h"

class ObfuscatedPassword;

class WandManager;
class WandPassword;

class WandModule : public OperaModule
{
public:
	WandModule()
		: wand_manager(NULL),
		  wand_encryption_password(NULL),
		  wand_obfuscation_password(NULL),
		  m_wand_ssl_setup_count(0),
		  m_wand_ssl_setup_is_sync(FALSE)
#ifdef HAS_COMPLEX_GLOBALS
# ifdef WAND_ECOMMERCE_SUPPORT
		  , m_eCom_items(NULL)
#  ifdef WAND_GUESS_COMMON_FORMFIELD_MEANING
		  , m_common_items(NULL)
#  endif // WAND_GUESS_COMMON_FORMFIELD_MEANING
# endif // WAND_ECOMMERCE_SUPPORT
#endif // HAS_COMPLEX_GLOBALS
		{}

	virtual void InitL(const OperaInitInfo& info);
	virtual void Destroy();

	WandManager* wand_manager;
	const char* obfuscation_password;

	ObfuscatedPassword* wand_encryption_password;
	ObfuscatedPassword* wand_obfuscation_password;

	OpVector<WandPassword> preleminary_dataitems;
	/**
	 * Number of nested setups of SSL. This used to be a BOOL, but since we use nested setup we have to count them so that we don't clear passwords until we've close all usages of SSL.
	 */
	int m_wand_ssl_setup_count;

	/**
	 * If SSL Setup has managed to do a synchronous crypto operation so we
	 * don't have to check it anymore.
	 */
	BOOL m_wand_ssl_setup_is_sync;

#ifdef WAND_ECOMMERCE_SUPPORT
	struct ECOM_ITEM
	{
		const char* field_name;
		unsigned short pref_id;
	};

#define ECOM_ITEMS_LEN 45
	/**
	 * Array of connections between ecommerce field names and prefs values from the user info section.
	 */
#ifdef HAS_COMPLEX_GLOBALS
	const ECOM_ITEM* m_eCom_items;
#else
	ECOM_ITEM m_eCom_items[ECOM_ITEMS_LEN];
#endif // HAS_COMPLEX_GLOBALS

# ifdef WAND_GUESS_COMMON_FORMFIELD_MEANING

# define ECOM_COMMON_ITEMS_LEN 6
	/**
	 * Array of connections between common field names and prefs values from the user info section.
	 */
#  ifdef HAS_COMPLEX_GLOBALS
	const ECOM_ITEM* m_common_items;
#  else
	ECOM_ITEM m_common_items[ECOM_COMMON_ITEMS_LEN];
#  endif // HAS_COMPLEX_GLOBALS
# endif // WAND_GUESS_COMMON_FORMFIELD_MEANING
#endif // WAND_ECOMMERCE_SUPPORT
};

#define g_wand_manager g_opera->wand_module.wand_manager

#define g_wand_encryption_password  g_opera->wand_module.wand_encryption_password
#define g_wand_obfuscation_password g_opera->wand_module.wand_obfuscation_password

#define WAND_MODULE_REQUIRED

#endif // WAND_SUPPORT

#endif // !MODULES_WAND_WAND_MODULE_H
