/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Espen Sand
*/

#ifndef __X11_SESSION_MANAGER_H__
#define __X11_SESSION_MANAGER_H__

#include "platforms/utilix/x11_all.h"
#include "platforms/posix/posix_selector.h"
#include "modules/hardcore/timer/optimer.h"
#include "modules/util/adt/oplisteners.h"

class X11SessionManager;

class X11SessionManagerListener
{
public:
	/**
	 * To be implemented when it is necessary to save data when the
	 * system session manager requests it. The system session manager 
	 * may close the application after this function.
	 *
	 * Note that the application will be closed gracefully even if this
	 * function is not implemented.
	 *
	 * The implemnation can use @ref AllowUserInteraction(), @ref IsShutdown(),
	 * @ref Accept() and @ref Cancel()
	 */
	virtual void OnSaveYourself(X11SessionManager& sm) = 0;

	/**
	 * Called when the system session manager decided to cancel an ongoing shutdown
	 * This can happen because another application decided to cancel it
	 */
	virtual void OnShutdownCancelled() = 0;
};


class X11SessionManager : public PosixSelectListener, public OpTimerListener
{
public:
	enum InteractUrgency
	{
		InteractNormal = 0,
		InteractError
	};

private:
	enum InteractState
	{
		InteractNone = 0,
		InteractWaiting,
		InteractAccepted,
	};

	enum SaveYourselfState
	{
		SaveYourselfNone = 0,
		SaveYourselfWaiting
	};

public:
	/**
	 * Creates the one and only instance of the X11SessionManager object. Opens a connection 
	 * to the system session manager if such is running.
	 *
	 * @return OpStatus::OK on success, otherwise an error. Opera should not exit if there is 
	 *         an error. If there is no active system session manager OpStatus::OK is returned 
	 */
	static OP_STATUS Create();
	
	/**
	 * Closes connection to the system session manager and removes the one and only instance 
	 * of the X11SessionManager.
	 */
	static void Destroy();

	/**
	 * Returns a pointer to one and only instance of the X11SessionManager object.
	 * Always test for NULL as there might not be a system session manager running
	 */
	static X11SessionManager* Self();
	
	/**
	 * Sets session manager listener. There will not be an instance to connect to 
	 * if Create() failed.
	 *
	 * @return OpStatus::OK on success or if there is no instance to connect to
	 *         otherwise an error code
	 */
	static OP_STATUS AddX11SessionManagerListener(X11SessionManagerListener* listener); 

	/**
	 * Removed session manager listener. There will not be an instance to disconnect if 
	 * if Create() failed.
	 *
	 * @return OpStatus::OK on success or if there is no instance to connect to
	 *         otherwise an error code
	 */
	static OP_STATUS RemoveX11SessionManagerListener(X11SessionManagerListener* listener); 

	/**
	 * Returns TRUE if the system session manager is running
	 */
	static BOOL IsSystemSessionManagerActive();

	/**
	 * Asks the system session manager if a save yourself session can be blocked 
	 * by a dialog. The function should only be called from the listener function
	 * @ref OnSaveYourself() and is only needed when @ref IsShutdown() returns TRUE.
	 * Either ref @Accept() or ref @Cancel() must be called after the dialog closes.
	 *
	 * @param urgency NORMAL or ERROR. The system session manager may give higher priority
	 *        to ERROR mode. 
	 *
	 * @return TRUE if a dialog can be shown, otherwise FALSE
	 */
	BOOL AllowUserInteraction(InteractUrgency urgency);

	/**
	 * The function should only be called from the listener function @ref OnSaveYourself() 
	 * Returns TRUE if the system session manager is about to shut down when it requests
	 * a save yourself. Note that the system session manager may revert later and not exit
	 * after all.
	 */
	BOOL IsShutdown() const { return m_shutdown; }

	/**
	 * Retuns the perferred exit strategy. If TRUE we exit Opera as quickly as possible at
	 * the expence of violating the protocol. We should not prefer to do this
	 */
	BOOL UseFastExit() const { return FALSE; }

	/**
	 * Instructs the session manager that the blocking dialog is closed and that the shutdown
	 * sequence can continue. Call this function before saving data. The system session 
	 * manager will then be able to proceede with other applications while our application 
	 * is saving data. The function should only be called if @ref AllowUserInteraction has
	 * returned TRUE
	 */
	void Accept();

	/**
	 * Instructs the session manager that the blocking dialog is closed and that the shutdown
	 * sequence should stop. The function should only be called if @ref AllowUserInteraction has
	 * returned TRUE
	 */
	void Cancel();

private:
	X11SessionManager();
	~X11SessionManager();

	/**
	 * Opens a connection to the system session manager if such is running.
	 *
	 * @return OpStatus::OK on success, otherwise an error. If there is no active system 
	 * session manager OpStatus::OK is returned 
	 */
	OP_STATUS Init();

	/**
	 * Resets internal state
	 */
	void Reset();

	/**
	 * [PosixSelectListener]
	 * Called when the session manager writes data to the client
	 */
	virtual void OnReadReady(int fd);

	/**
	 * [PosixSelectListener]
	 * Not used at the moment
	 */
	virtual void OnDetach(int fd) {}

	/**
	 * [PosixSelectListener]
	 * Not used at the moment
	 */
	virtual void OnError(int fd, int err=0) {}

	/**
	 * [OpTimerListener]
	 * Called when timer expiers
	 */
	void OnTimeOut(OpTimer* timer);

	/**
	 * Returns the current connection handle to the system session manager
	 */
	SmcConn GetConnectionHandle() const { return m_connection;}

	/**
	 * Sends a flag property to the system session manager
	 *
	 * @param key Property name
	 * @param value Property value
	 */
	void SetProperty(const OpStringC8& key, INT8 value);

	/**
	 * Sends a string property to the system session manager
	 *
	 * @param key Property name
	 * @param value Property value
	 */
	void SetProperty(const OpStringC8& key, const OpStringC8& value);

	/**
	 * Sends a string property to the system session manager
	 *
	 * @param key Property name
	 * @param value List og property values for the key
	 *
	 * @return OpStatus::OK on success, otherwise an error code
	 */
	OP_STATUS SetProperty(const OpStringC8& key, OpVector<OpString8>& value);

	/**
	 * Creates the command line argument list that will be sent to the system 
	 * session manager which it will use to restart the application
	 *
	 * @param list On exit the command line arguments
	 *
	 * @return OpStatus::OK on success, otherwise an error code
	 */
	OP_STATUS SetRestartList(OpVector<OpString8>& list);

	/**
	 * Functions triggered by callback functions
	 */
	void OnSaveYourself(int save_type, X11Types::Bool shutdown ,int interact_style, X11Types::Bool fast);
	void OnSaveYourself();
	void OnDie();
	void OnShutdownCancelled();
	void OnSaveComplete();
	void OnInteractRequest();

private:
	/**
	 * Callback functions that will be triggered by the system session manager
	 */
	static void SM_save_yourself_CB(SmcConn connection, SmPointer client_data, int save_type, X11Types::Bool shutdown ,int interact_style, X11Types::Bool fast);
	static void SM_die_CB(SmcConn connection, SmPointer client_data);
	static void SM_shutdown_cancelled_CB(SmcConn connection, SmPointer client_data);
	static void SM_save_complete_CB(SmcConn connection, SmPointer client_data);
	static void SM_interact_request_CB(SmcConn connection, SmPointer client_data);

private:
	SmcConn m_connection;
	int m_save_type;
	int m_interact_style;
	InteractState m_interact_state;
	SaveYourselfState m_save_yourself_state;
	BOOL m_shutdown;
	BOOL m_cancelled;
	OpString8 m_id;
	OpString8 m_key;
	OpTimer m_timer;
	OpListeners<X11SessionManagerListener> m_session_manager_listener;

private:
	static X11SessionManager* m_manager;
	
};


#endif // __X11_SESSION_MANAGER_H__
