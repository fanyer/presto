/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*- */

#ifndef ES_MATH_BUILTINS_SUPPORT_H
#define ES_MATH_BUILTINS_SUPPORT_H

class ES_MathBuiltinSupport
{
public:
    static double HandlePowSpecial(double r, double x, double y);
    /**< If op_pow() reports a NaN, check for and report the special values that the spec for Math.pow() mandates. */

    static double HandleAtan2Special(double r, double x, double y);
    /**< If op_atan2() reports a NaN, check for and report the special values that the spec for Math.atan2() mandates. */
};

#endif // ES_MATH_BUILTINS_SUPPORT_H
