#ifndef AZ_FIXPOINT_H
#define AZ_FIXPOINT_H

typedef int AZLevel;

/** the number of bits used to store decimal data/fractions */
#define AZ_FIX_DECBITS 8
/** 0.5 as AZLevel */
#define AZ_FIX_HALF (1 << (AZ_FIX_DECBITS-1))
/** converts from integer to AZLevel */
#ifdef _DEBUG
AZLevel AZ_INTTOFIX(int v);
#else // _DEBUG
#define AZ_INTTOFIX(v) ((AZLevel)((v)<<(AZ_FIX_DECBITS)))
#endif // _DEBUG

/** converts from AZLevel to integer */
#define AZ_FIXTOINT(v) ((int)((v)>>(AZ_FIX_DECBITS)))

/** multiplies x and y 
    @param x first multiplicand
    @param y second multiplicand
    @return the product
 */
AZLevel AZMul(AZLevel x, AZLevel y);
/** divides x and y
    @param x the dividend
    @param y the divisor
    @return the quotient
*/
AZLevel AZDiv(AZLevel x, AZLevel y);

#endif // AZ_FIXPOINT_H
