/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef URL_COMMELM_H
#define URL_COMMELM_H

#include "modules/util/simset.h"

class Comm;
class SComm;

class CommWaitElm : public Link
{
friend class Comm;
public:
	CommWaitElm() : comm(0), status(0), time_set(0) {}

private:
  	Comm*				comm;
  	BYTE				status;
	time_t				time_set;

	void				Init(Comm* c, BYTE state);
};

class SCommWaitElm : public Link
{
friend class SComm;
public:
	SCommWaitElm() : comm(0), status(0) {}

private:
  	SComm*	comm;
  	BYTE	status;

	void  	Init(SComm* c, BYTE stat) { comm = c; status = stat; }
};

#endif // !URL_COMMELM_H
