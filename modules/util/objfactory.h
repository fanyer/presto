/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

/** OpObjectFactory<> is a generic object factory template that is backed
  * by a cache to handle out-of-memory situations.
  *
  * Objects are normally allocated from the common heap, but are taken from
  * the cache if allocation from the heap fails.  If an object is taken from
  * the cache then a message is posted to refill the cache "soon".  The idea
  * is for the cache "never" to run out of objects.
  *
  * Objects must all be subclass of Link, but it is easy to fix that
  * restriction (just use a local array of pointers in this class).
  *
  * The cache has a fixed capacity, for the time being.
  */

#ifndef MODULES_UTIL_OBJFACTORY_H
#define MODULES_UTIL_OBJFACTORY_H

#define OBJFACTORY_REFILL_INTERVAL		100 	///< Delay period for refill, in milliseconds

#include "modules/hardcore/mh/messobj.h"
#include "modules/util/simset.h"

template <class OBJECT>
class OpObjectFactory : public MessageObject
{
private:
	Head	cache;					///< A list of objects
	size_t	size;					///< How many of them
	size_t	capacity;				///< Size of cache
	int		id;						///< ID of factory (globally, for messaging)
	BOOL	refill_failed;			///< TRUE if we failed to post a cache-fill msg
	BOOL	oom_condition_raised;	///< TRUE if we exhausted the cache and have yet to refill it fully
    BOOL    post_critical_section;	///< TRUE if we're in the middle of posting a refill message

	void	PostRefill();
		/**< Post a cache refill message. */

public:
	OpObjectFactory();
		/**< Allocate a factory. */

	virtual ~OpObjectFactory();

	void	ConstructL( size_t requested_capacity );
		/**< Initialize the factory by allocating the required number of
		     elements and setting up the refill logic.  Leaves on OOM.
			 */

	virtual void	HandleCallback( OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2 );
		/**< Used internally for cache refills. */

	OBJECT  *Allocate();
		/**< Allocate an object.  If the cache is empty, then NULL is
			 returned and an OOM situation is flagged globally.

			 The client *must* handle the NULL but may choose not to
			 propagate the error.
			 */
};

#endif // !MODULES_UTIL_OBJFACTORY_H

/* eof */
