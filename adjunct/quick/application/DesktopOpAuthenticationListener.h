#ifndef DESKTOP_OP_AUTHENTICATION_LISTENER_H
#define DESKTOP_OP_AUTHENTICATION_LISTENER_H

#include "modules/windowcommander/OpWindowCommanderManager.h"

class DesktopOpAuthenticationListener
	: public OpAuthenticationListener
{
public:
	virtual ~DesktopOpAuthenticationListener() {}

	virtual void OnAuthenticationRequired(OpAuthenticationCallback* callback) = 0;

	virtual void OnAuthenticationCancelled(const OpAuthenticationCallback* callback) = 0;

	virtual void OnAuthenticationRequired(OpAuthenticationCallback* callback, OpWindowCommander *commander, DesktopWindow* parent_window) = 0 ;
};
#endif // DESKTOP_OP_AUTHENTICATION_LISTENER_H
