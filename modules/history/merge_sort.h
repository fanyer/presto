/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * arjanl
 */

#ifndef CORE_HISTORY_MERGE_SORT_H
#define CORE_HISTORY_MERGE_SORT_H

template<class T>
class MergeSorting
{
public:

    /**
     * Helper function for MergeSort: insert value at the correct (sorted)
     * place in the sorted vector in_out_vector[in_begin .. in_end)
     *
     * @param in_out_vector
     * @param in_begin
     * @param in_end
     * @param value
     * @param gt_function Function that returns whether item1 should be considered
     *        greater than item2
     *
     * @return OP_STATUS
     */
	static OP_STATUS MergeSortInsert(OpVector<T>& in_out_vector, 
									 int in_begin, 
									 int in_end, 
									 T& value,
									 BOOL (*gt_function)(const T& item1, const T& item2))
		{
		while(in_begin + 1 != in_end && gt_function(*(in_out_vector.Get(in_begin + 1)), value))
		{
			RETURN_IF_ERROR(Swap(in_out_vector, in_begin, in_begin + 1));
			in_begin++;
		}
		RETURN_IF_ERROR(in_out_vector.Replace(in_begin, &value));
		
		return OpStatus::OK;
    }

	/**
     * Helper function for MergeSort: merge the sorted vectors in_out_vector[in_begin .. in_begin_right) 
	 * and in_out_vector[in_begin_right .. in_end) into a sorted vector
     *
     * @param in_out_vector Partly sorted vector
     * @param in_begin Start of first range of sorted items
     * @param in_begin_right End of first range of sorted items, start of second range
     * @param in_end End of second range of sorted items
     * @param gt_function Function that returns whether item1 should be considered
     *        greater than item2
     *
     * @return OP_STATUS
     */
	static OP_STATUS MergeSortMerge(OpVector<T>& in_out_vector,
									int in_begin,
									int in_begin_right,
									int in_end,
									BOOL (*gt_function)(const T& item1, const T& item2))
	{
	    for(; in_begin < in_begin_right; in_begin++) 
	    {
			T* begin_item       = in_out_vector.Get(in_begin);
			T* begin_right_item = in_out_vector.Get(in_begin_right);
			
			if (gt_function(*begin_right_item, *begin_item))
			{
				RETURN_IF_ERROR(in_out_vector.Replace(in_begin, in_out_vector.Get(in_begin_right)));
				RETURN_IF_ERROR(MergeSortInsert(in_out_vector, in_begin_right, in_end, *begin_item, gt_function));
			}
	    }
		
	    return OpStatus::OK;
	}

    /**
     * Sorts the items in in_out_vector according to gt_function
     * (greatest item first)
     *
     * @param in_out_vector Vector to sort
     * @param in_begin Sort indexes from here (inclusive)
     * @param in_end Sort indexes until here (inclusive)
     * @param gt_function Function that returns whether item1 should be considered
     *        greater than item2
     *
     * @return OP_STATUS
     */
	static OP_STATUS MergeSort(OpVector<T>& in_out_vector,
							   int in_begin,
							   int in_end,
							   BOOL (*gt_function)(const T& item1, const T& item2))
	{
	    int size = in_end - in_begin;
	    
	    if(size < 2)
			return OpStatus::OK;
	    
	    int begin_right = in_begin + size / 2;
	    RETURN_IF_ERROR(MergeSort(in_out_vector, in_begin, begin_right, gt_function));
	    RETURN_IF_ERROR(MergeSort(in_out_vector, begin_right, in_end, gt_function));
	    RETURN_IF_ERROR(MergeSortMerge(in_out_vector, in_begin, begin_right, in_end, gt_function));
	    
	    return OpStatus::OK;
	}

    /**
     * Helper function for MergeSort: Swap the values of indices in_item1
     * and in_item2 in in_out_vector
     *
     * @param in_out_vector
     * @param in_item1
     * @param in_item2
     *
     * @return OP_STATUS
     */
	static OP_STATUS Swap(OpVector<T>& in_out_vector, UINT32 in_item1, UINT32 in_item2)
	{
	    if(in_item1 == in_item2)
			return OpStatus::OK;
	    
	    T* item1 = in_out_vector.Get(in_item1);
	    T* item2 = in_out_vector.Get(in_item2);
	    
	    RETURN_IF_ERROR(in_out_vector.Replace(in_item1, item2));
	    RETURN_IF_ERROR(in_out_vector.Replace(in_item2, item1));
	    
	    return OpStatus::OK;
	}

private:
	// Private constructor to prevent this class from being instantiated
	MergeSorting(){}
};

#endif //CORE_HISTORY_MERGE_SORT_H
