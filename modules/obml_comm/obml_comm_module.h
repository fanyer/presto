/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2007-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_OBML_COMM_MODULE_H
#define MODULES_OBML_COMM_MODULE_H

#if defined(WEB_TURBO_MODE)

class OBML_Storage;
class OBML_Config;
class OBML_Id_Manager;

class ObmlCommModule : public OperaModule
{
public:
	OBML_Config  *config;
	OBML_Id_Manager	*id_manager;
	
public:
	ObmlCommModule() :
		config(NULL)
		,id_manager(NULL)
	{ }

	virtual void InitL(const OperaInitInfo& info);
	void InitConfigL();

	virtual void Destroy();
	virtual ~ObmlCommModule(){};

private:
	void CreateBypassFiltersL();
};

#define OBML_COMM_MODULE_REQUIRED

#define g_obml_storage					g_opera->obml_comm_module.storage
#define g_obml_config					g_opera->obml_comm_module.config
#define g_obml_id_manager				g_opera->obml_comm_module.id_manager

#endif // WEB_TURBO_MODE

#endif // MODULES_OBML_COMM_MODULE_H
