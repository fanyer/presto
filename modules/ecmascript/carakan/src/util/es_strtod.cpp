/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) Opera Software ASA  2001-2004
 *
 * Contains high-precision non-radix-10 number-to-string conversion
 */

#include "core/pch.h"

#include "modules/ecmascript/carakan/src/es_pch.h"

JString*
ecma_numbertostring(ES_Context *context, double x, int radix)
{
    // Integers, radix != 10.
    double ix = op_truncate(x);

    TempBuffer string;
    ANCHOR(TempBuffer, string);
    static const char digits[] = "0123456789abcdefghijklmnopqrstuvwxyz";  // lowercase important -- spec doesn't require it but sites depend on it, see #239699
/*    static const int iterations[] = { 0, 0, 52, 32, 26, 22, 20, 18, 17, 16,
                                      15, 15, 14, 14, 13, 13, 13, 12, 12, 12,
                                      12, 11, 11, 11, 11, 11, 11, 10, 10, 10,
                                      10, 10, 10, 10, 10, 10, 10 };*/
    /* Should be derivable from (but isn't): Math.ceil(Math.log(Math.pow(2,53)) / Math.log(base)) */
    static const int iterations[] = { 0, 0, 53, 34, 26, 23, 21, 19, 18, 18,
                                      15, 15, 15, 14, 15, 14, 13, 14, 13, 13,
                                      13, 13, 12, 13, 12, 12, 12, 12, 12, 12,
                                      12, 11, 11, 12, 11, 12, 11 };

    int iterations_left = iterations[radix];
    double remaining = ix;
    int sign = 0;
    if (remaining < 0.0) {
        sign = 1;
        remaining = -remaining;
    }

	if (remaining <= INT_MAX)
	{
		// Fast and safe case
		int r = DOUBLE2INT32(remaining);
		while (r > 0)
		{
			string.AppendL(&digits[r%radix], 1);
			r /= radix;
            --iterations_left;
		}
	}
	else
	{
		// Hard case: the number doesn't fit in an int
#if 1
		// This isn't right; it is dangerous to use floating-point
		// operations here. The code can't be trusted for large x
		// (> 2^53).
		double mult = 1;
		while (mult <= ix)
		{
			double old_mult = mult;
			mult *= radix;
			int dig = DOUBLE2INT32(op_fmod(ix, mult)/old_mult);
			string.AppendL(&digits[dig], 1);
			--iterations_left;
		}
#else
		// Largest radix is 36 so largest divisor is 6 bits.
		// Largest exponent is 2^1023 so largest dividend is
		// 1024 bits (?), most of them zero...  I don't really
		// feel like implementing high-precision division today,
		// but it's not clear that there are any tricks that apply.
#endif
	}
	if (string.Length() == 0)
		string.AppendL("0");


    if (sign)
        string.AppendL("-");

    double fx = op_fabs(x);
    fx = fx - op_truncate(fx);

    int slen = 0;
    int curr_pos = string.Length();

    if (fx != 0.0 && iterations_left > 0)
    {
        string.AppendL(".");

        int dig = 0;
        BOOL seen_decpt = FALSE;

        do
        {
            fx *= radix;
            dig = DOUBLE2INT32(op_truncate(fx));
            fx = fx - op_truncate(fx);
            string.AppendL(&digits[dig], 1);
            if (seen_decpt || (seen_decpt = (dig != 0)))
                --iterations_left;
        }
        while (fx != 0.0 && iterations_left > 0);

        if (fx != 0.0)
        {
            // Round last digit up if needed.
            // Will not work if we need to change more than one digit.
            fx *= radix;
            if (fx >= radix / 2.0)
                if (dig < radix - 1)
                    string.GetStorage()[string.Length() - 1] = digits[dig+1];
                else
                    string.Append(&digits[radix/2], 1);
        }
        // Remove all trailing zeros.
        int len = string.Length();
        while (string.GetStorage()[len - 1] == '0')
        {
            --len;
            slen = len;
        }
    }

    if (slen == 0)
        slen = string.Length();

    JString *final_string = JString::Make(context, slen);
    uni_char *storage = Storage(context, final_string);

    for (int i = curr_pos - 1; i >= 0; --i)
        *storage++ = string.GetStorage()[i];

    op_memcpy(storage, string.GetStorage() + curr_pos, (slen - curr_pos) * sizeof(uni_char));

	return Finalize(context, final_string);
}

// class used to suspend the above function:
class ES_SuspendedNumberToString : public ES_SuspendedCall
{
public:
    ES_SuspendedNumberToString(ES_Execution_Context *context, double x, int radix)
        : x(x), radix(radix), result(NULL), status(OpStatus::OK)
    {
        context->SuspendedCall(this);
        if (OpStatus::IsMemoryError(status))
            context->AbortOutOfMemory();
    }

    virtual void DoCall(ES_Execution_Context *context)
    {
        TRAP(status, result = ecma_numbertostring(context, x, radix));
        if (result == NULL)
            result = context->rt_data->idents[ESID_NaN];
    }

    operator JString* ()
    {
        return result;
    }

private:
    double                x;
    int                   radix;
    JString              *result;
    OP_STATUS             status;
};

JString*
ecma_suspended_numbertostring(ES_Execution_Context *context, double x, int radix)
{
    return ES_SuspendedNumberToString(context, x, radix);
}
