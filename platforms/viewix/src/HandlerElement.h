/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Patricia Aas (psmaas)
 */

#ifndef __FILEHANDLER_HANDLER_ELEMENT_H__
#define __FILEHANDLER_HANDLER_ELEMENT_H__

class HandlerElement
{
 public:
    /**
       HandlerElement constructor
    */
    HandlerElement() : ask(FALSE) {}

    /**
       @param candidate
       @return
    */
    BOOL SupportsExtension(const OpString& candidate);

    /**
       @param candidate
    */
    OP_STATUS SetExtension(const OpStringC & candidate);

    /**
       @param candidate
    */
    OP_STATUS SetHandler(const OpStringC & candidate);

	BOOL HasHandler() { return handler.HasContent(); }

 public:
    BOOL ask;
    OpString extension;
    OpString handler;
    OpAutoVector<OpString> extension_list;
};

#endif //__FILEHANDLER_HANDLER_ELEMENT_H__
