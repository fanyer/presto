#ifndef _OPBINARYSEARCH_H_
#define _OPBINARYSEARCH_H_

#include "adjunct/m2/src/include/defs.h"

/* Code example for OpBinarySearch

        //Search for a given number in an array with dirty entries (dirty entries are in this example defined as 0)
        OpBinarySearch binary_search;
        int numbers[] = {1, 2, 0, 7, 0, 13, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        int number_to_find = 7;
        INT32 item_count = sizeof(numbers)/sizeof(numbers[0]);
        INT32 current_position = binary_search.GetStartPosition(item_count);
        while (current_position!=-1)
        {
            if (numbers[current_position]==0)
            {
                current_position=binary_search.Unknown();
            }
            else if (numbers[current_position]>number_to_find)
            {
                current_position=binary_search.TooHigh();
            }
            else if (numbers[current_position]<number_to_find)
            {
                current_position=binary_search.TooLow();
            }
            else // if (numbers[current_position]==number_to_find)
            {
                break; //Number found
            }
        }
        if (current_position==-1)
        {
            //Number not found
        }
*/

class OpBinarySearch
{
public:

    /**

    */
    OpBinarySearch();


    /**

    */
    ~OpBinarySearch();


    /**

    */
    INT32 GetStartPosition(UINT32 item_count);


    /**

    */
    INT32 TooHigh();


    /**

    */
    INT32 TooLow();


    /**

    */
    INT32 Unknown();

private:

    INT32 m_item_count;
    INT32 m_confirmed_current_position;
    INT32 m_unconfirmed_current_position;
    INT32 m_low_limit;
    INT32 m_high_limit;
};


#endif
