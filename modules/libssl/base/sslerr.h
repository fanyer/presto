/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2000-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Yngve Pettersen
 */
#ifndef __SSLERR_H
#define __SSLERR_H

#ifdef _NATIVE_SSL_SUPPORT_
#include "modules/util/twoway.h"

class SSL_Error_Status_Base: public TwoWayPointer_Target, public TwoWayAction_Target
{
public:
	virtual void RaiseAlert(const SSL_Error_Status_Base *)=0;
};

class SSL_Error_Status: 
	public SSL_Error_Status_Base
{
private:
    SSL_Alert_Base status;
	TwoWayPointer_WithAction<SSL_Error_Status_Base> forwardto;
	
public:
    BOOL ErrorRaisedFlag;

	enum {
		RESET_ERROR_STATUS = TWOWAYPOINTER_ACTION_USER,
		ERROR_STATUS_ACTIONBASE
	};
    
private:
    void ForwardAlert();
	
protected:
    SSL_Error_Status();
    SSL_Error_Status(const SSL_Error_Status& old);
    virtual ~SSL_Error_Status();
    
    virtual BOOL HandleAlert(SSL_Alert_Base &);
	
public:
    virtual BOOL Error(SSL_Alert_Base *msg = NULL) const;
	virtual BOOL Valid(SSL_Alert *msg=NULL) const;

    virtual void ResetError();
	OP_STATUS GetOPStatus();
    
    void ForwardTo(SSL_Error_Status *other);
	void ForwardToThis(SSL_Error_Status *other1, SSL_Error_Status *other2);
	void ForwardToThis(SSL_Error_Status &other1, SSL_Error_Status &other2);
	void ForwardToThis(DataStream *other1){};// Dummy, needed in some situations
	void ForwardToThis(DataStream &other1){};// Dummy, needed in some situations
    void RaiseAlert(SSL_AlertLevel, SSL_AlertDescription);
    void RaiseAlert(SSL_AlertLevel, SSL_AlertDescription, SSL_Alert *alert);
    void RaiseAlert(const SSL_Alert &);
    void RaiseAlert(const SSL_Error_Status &);
    void RaiseAlert(const SSL_Error_Status *);
    virtual void RaiseAlert(const SSL_Error_Status_Base *);
	void RaiseAlert(OP_STATUS);

	virtual OP_STATUS SetReason(const OpStringC &text){return status.SetReason(text);}
	virtual OP_STATUS SetReason(Str::LocaleString str){return status.SetReason(str);}

	SSL_Error_Status *GetForwardErrorToTarget(){return (SSL_Error_Status *) forwardto.get();}	

	virtual void TwoWayPointerActionL(TwoWayPointer_Target *action_source, uint32 action_val);
};

//inline void AlertSet(SSL_Alert *target, OP_STATUS op_err){if(target) target->Set(op_err);}
//inline void AlertSet(SSL_Alert &target, OP_STATUS op_err){target.Set(op_err);}
inline void ErrorSet(SSL_Error_Status *target, OP_STATUS op_err){if(target) target->RaiseAlert(op_err);}
inline void ErrorSet(SSL_Error_Status &target, OP_STATUS op_err){target.RaiseAlert(op_err);}

#endif
#endif
