/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#ifndef  _URL_TRACE_H_
#define _URL_TRACE_H_

#ifdef URL_ENABLE_TRACER
class URL_Tracer
{
private:
	char *tracer;
protected:
	URL_Tracer():tracer(NULL){}
	URL_Tracer(URL_Tracer &old):tracer(NULL){}
	URL_Tracer(URL_Tracer *old):tracer(NULL){}
	~URL_Tracer(){Release();}

	void Release(){OP_DELETEA(tracer);tracer = NULL;}
	void SetTracker();
};

class URL_InUse_Tracer
{
private:
	char *tracer;

protected:
	URL_InUse_Tracer():tracer(NULL){}
	URL_InUse_Tracer(URL_InUse_Tracer &old):tracer(NULL){}
	URL_InUse_Tracer(URL_InUse_Tracer *old):tracer(NULL){}
	~URL_InUse_Tracer(){Release();}

	void Release(){OP_DELETEA(tracer);tracer = NULL;}
	void SetTracker();
};

#define TRACER_RELEASE() Release()
#define TRACER_SETTRACKER(x) do{if(x != EmptyURL_Rep){SetTracker();}} while(0)
#define TRACER_INUSE_SETTRACKER(x) do{if(x.IsValid()){SetTracker();}} while(0)
#else
#define TRACER_RELEASE() 
#define TRACER_SETTRACKER(x)
#define TRACER_INUSE_SETTRACKER(x)
#endif

#endif // _URL_TRACE_H_
