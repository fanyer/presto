/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Morten Stenshorne
*/

#ifndef DLMACROS_H
#define DLMACROS_H

#define DECLARE_SYMBOL(ret, name, args) typedef ret (*name##_t)args; extern name##_t name
#define DEFINE_SYMBOL(ret, name, args) typedef ret (*name##_t)args; name##_t name
#define LOAD_SYMBOL(f, handle, name) name = (name##_t)f(handle, #name)

#endif // DLMACROS_H
