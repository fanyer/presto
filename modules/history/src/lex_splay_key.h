/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 * psmaas - Patricia Aas
 */

#ifndef LEX_SPLAY_KEY_H
#define LEX_SPLAY_KEY_H

//#define HISTORY_DEBUG

#ifdef HISTORY_SUPPORT

class LexSplayKey
{
public :
	/**
       @return pointer to start of key buffer
    */
    virtual const uni_char * GetKey() const = 0; 

    /**

    */
	LexSplayKey();

    /**

    */
	virtual ~LexSplayKey() {}

    /**
       Decrements the reference counter
    */
    void Decrement();

    /**
       Increments the reference counter
    */
    void Increment();

    /**
       @return TRUE if the address is still in use
    */
    BOOL InUse();

#ifdef HISTORY_DEBUG
	INT NumberOfReferences() { return m_ref_counter; }
#endif //HISTORY_DEBUG

private:

    INT m_ref_counter;	
};

#endif // HISTORY_SUPPORT
#endif // LEX_SPLAY_KEY_H
