/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#ifndef __SSLEXCEPT_H
#define __SSLEXCEPT_H

#ifdef _NATIVE_SSL_SUPPORT_
inline void AlertSet(SSL_Alert *target, OP_STATUS op_err);
inline void AlertSet(SSL_Alert &target, OP_STATUS op_err);
inline void ErrorSet(SSL_Error_Status *target, OP_STATUS op_err);
inline void ErrorSet(SSL_Error_Status &target, OP_STATUS op_err);

#define SSL_TRAP_AND_RAISE_ERROR(f, t) \
{\
	TRAPD(ssl_op_err, f);\
	if(OpStatus::IsError(ssl_op_err))\
		ErrorSet(t, ssl_op_err);  \
}

/*
#define SSL_TRAP_AND_RAISE_ERROR_ALERT(f, t) \
{\
	TRAPD(ssl_op_err, f);\
	if(OpStatus::IsError(ssl_op_err))\
		AlertSet(t,ssl_op_err);  \
}
*/
	
#define SSL_TRAP_AND_RAISE_ERROR_THIS(f) \
{\
	TRAPD(ssl_op_err, f);\
	if(OpStatus::IsError(ssl_op_err))\
		RaiseAlert(ssl_op_err);  \
}
#endif
#endif
