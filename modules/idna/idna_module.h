/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_IDNA_MODULE_H
#define MODULES_IDNA_MODULE_H

#define IDNA_TempBufSize 256 // [rfc2181] A full domain name is limited to 255 octets (including the separators)

class IdnaModule : public OperaModule
{
public:
	IdnaModule(){};
	virtual ~IdnaModule(){};

	virtual void InitL(const OperaInitInfo& info){};
	virtual void Destroy(){};

	uni_char IDNA_tembuf1[IDNA_TempBufSize+1]; /* ARRAY OK 2011-06-10 kswitalski */
	uni_char IDNA_tembuf2[IDNA_TempBufSize+1]; /* ARRAY OK 2011-06-10 kswitalski */
	uni_char IDNA_tembuf3[IDNA_TempBufSize+1]; /* ARRAY OK 2011-06-10 kswitalski */
	char     IDNA_tembuf4[IDNA_TempBufSize+1]; /* ARRAY OK 2011-06-10 kswitalski */
	uni_char IDNA_tembuf5[IDNA_TempBufSize+1]; /* ARRAY OK 2011-06-10 kswitalski */
	uni_char IDNA_tembuf6[IDNA_TempBufSize+1]; /* ARRAY OK 2011-06-10 kswitalski */
};

#define g_IDNA_tembuf1 g_opera->idna_module.IDNA_tembuf1
#define g_IDNA_tembuf2 g_opera->idna_module.IDNA_tembuf2
#define g_IDNA_tembuf3 g_opera->idna_module.IDNA_tembuf3
#define g_IDNA_tembuf4 g_opera->idna_module.IDNA_tembuf4
#define g_IDNA_tembuf5 g_opera->idna_module.IDNA_tembuf5
#define g_IDNA_tembuf6 g_opera->idna_module.IDNA_tembuf6

#define IDNA_MODULE_REQUIRED

#endif // !MODULES_IDNA_MODULE_H
