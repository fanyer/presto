/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_UTIL_OPSTRLST_H
#define MODULES_UTIL_OPSTRLST_H

#ifdef USE_UTIL_OPSTRING_LIST

/**
 *	Variable array of string objects
 */

class OpString_list{
private:
    unsigned long length;
    OpString **vectorlist;
 
    unsigned long alloclength;
	
    OpString_list(const OpString_list &old); // NonExistent
    OpString_list &operator =(const OpString_list &old); // NonExistent

	void Clear();
	
public:

	/**
	 *  Default constructor
	 *
	 *  array is presently non-existent
	 */
    OpString_list(): length(0), vectorlist(NULL), alloclength(0){};

	/**
	 *  Destructor
	 */
    ~OpString_list();

	/**
	 *  Resize operation
	 *
	 *  Configure the array to be nlength items long
	 *	If necessary destroy items that are no longer needed
	 *
	 *  @param	nlength	the new length of the arrat
	 */
    OP_STATUS Resize(unsigned long nlength);
 
	/**
	 *  Count : return the number of items in the array
	 *
	 *  @return	the number of items presently allocated in the array.
	 */
    unsigned long Count() const{return length;};


	/**
	 *  Item : return a reference to the indicated item in the array
	 *  If the item is outside the array a reference to a dummy item is returned
	 *
	 *  @param	index	the number of the item to be retrieved. should be between 0 and Count()-1, inclusive
	 *  @return	A reference to the indicated item in the array
	 */
    OpString &Item(unsigned long index);

	/**
	 *  Item : return a const reference to the indicated item in the array
	 *  If the item is outside the array a reference to a dummy item is returned
	 *
	 *  @param	index	the number of the item to be retrieved. should be between 0 and Count()-1, inclusive
	 *  @return	A const reference to the indicated item in the array
	 */
    const OpString &Item(unsigned long index) const;
 
	
	/**
	 *  Array Operator : return a reference to the indicated item in the array
	 *  If the item is outside the array a reference to a dummy item is returned
	 *
	 *  @param index The number of the item to be retrieved. should be between 0 and Count()-1, inclusive
	 *  @return	A reference to the indicated item in the array
	 */
	OpString &operator [](unsigned long index){return Item(index);};

	/**
	 *  Array Operator : return a const reference to the indicated item in the array
	 *  If the item is outside the array a reference to a dummy item is returned
	 *
	 *  @param index The number of the item to be retrieved. should be between 0 and Count()-1, inclusive
	 *  @return	A const reference to the indicated item in the array
	 */
    const OpString &operator [](unsigned long index) const{return Item(index);};
};

#endif // USE_UTIL_OPSTRING_LIST

#endif // !MODULES_UTIL_OPSTRLST_H
