/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Marcin Zdun (mzdun)
 */

#ifdef AUTO_UPDATE_SUPPORT
#ifndef MAC_ASYNC_PROCESS_RUNNER_H
#define MAC_ASYNC_PROCESS_RUNNER_H

enum PIM_PROCESS_STATE
{
	PIM_PROCESS_STATE_QUERY_ERROR = -1,
	PIM_PROCESS_RUNNING,
	PIM_PROCESS_FINISHED
};

class PIM_MacAsyncProcessRunner:
	public PIM_AsyncProcessRunner,
	public OpTimerListener
{
public:
	PIM_MacAsyncProcessRunner();
	~PIM_MacAsyncProcessRunner();
	
	OP_STATUS	RunProcess(const OpStringC& a_app, const OpStringC& a_params);
	OP_STATUS	KillProcess();
	void		OnTimeOut(OpTimer* timer);
	
	class ProcessObserver
	{
	public:
		virtual ~ProcessObserver() {}
		virtual PIM_PROCESS_STATE hasFinished(int& return_value) = 0;
		virtual bool start() = 0;
		virtual int terminate() = 0;
	};
	
protected:
	void		KickTimer();
	int			Mount(const OpStringC& dmg);
	void		Umount();
	void		InitAppInstaller();
	OP_STATUS	InitPackageInstaller(const OpStringC& pkg_path);
	void		OnProcessFinished(int exit_code);
	int			PackageElevation(const OpStringC& pkg_name);

	enum STATE
	{
		IDLE,       //the START state
		MOUNTED,    //Mount happened - we can search for installer
		INSTALLING, //Installer seems to be running
		FINISHED    //Instaler stopped, the image can be Umounted
	};
	
	OpString	m_mount_name;
	OpTimer 	m_poll_timer;
	STATE		m_state;
	OpAutoPtr<ProcessObserver> m_process;
};

#endif //MAC_ASYNC_PROCESS_RUNNER_H
#endif //AUTO_UPDATE_SUPPORT