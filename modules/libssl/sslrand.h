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

#ifndef _SSL_RAND_H_
#define _SSL_RAND_H_

#ifdef _NATIVE_SSL_SUPPORT_

class SSL_varvector32;

void SSL_SEED_RND(byte *source,uint32 len);
void SSL_RND(byte *,uint32 len);
void SSL_RND(SSL_varvector32 &target,uint32 len);

#endif // _NATIVE_SSL_SUPPORT_
#endif // _SSL_RAND_H_
