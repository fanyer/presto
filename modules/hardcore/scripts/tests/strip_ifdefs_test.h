// These are tests for the pike script (strip_ifdefs.pike) that removes ifdefs from a header file.
// After stripping there should be no FAIL in the output.

#pragma keep_conditionals TEST_IFDEFSTRIPPING_H SAVE_ME

#ifndef TEST_IFDEFSTRIPPING_H
#define TEST_IFDEFSTRIPPING_H

#warning alfa

// These pragmas should be kept
#pragma OK
#pragma OK OK

#if 0
// This pragma should not be visible
#pragma FAIL FAIL
#pragma FAIL
#endif

// 1.
#if 1
OK
#endif

// 2.
#if 0
FAIL
#endif
OK

// 3.
#if 1
OK
#else
FAIL
#endif

// 4.
#if 0
FAIL
#else
OK
#endif

// 5.
#if 1
OK
#elif 0
FAIL
#endif

// 6.
#if 0
FAIL
#elif 1
OK
#endif

// 7.
#if 1
OK
#else
FAIL
#endif

// 8.
#if 0
FAIL
#else
OK
#endif

// 9.
#if 1
OK
#elif 0
FAIL1
#else
FAIL2
#endif

// 10.
#if 0
FAIL1
#elif 1
OK
#else
FAIL2
#endif

// 11.
#if 0
FAIL1
#elif 0
FAIL2
#else
OK
#endif

// 12.
#if 1
OK1
# if 1
OK2
# else
FAIL
# endif
OK3
#endif

// 13.
#if 0
FAIL1
#elif 0
FAIL2
#elif 0
FAIL3
#elif 1
OK
#else
FAIL4
#endif

// 14.
#if 0
FAIL1
# if 0
FAIL2
# endif
#endif
OK

// 15.
#ifdef TRUE
OK
#endif

// 16.
#ifndef TRUE
FAIL
#endif
OK

// 17.
#ifdef MANDELMASSA
FAIL
#endif
OK

// 18.
#ifdef MANDELMASSA
FAIL
#else
OK
#endif

// 19.
#ifdef MANDELMASSA
FAIL1
#elif TRUE
OK
#else
FAIL2
#endif

// 20. ( The SAVE_ME check should not be touched)
#ifdef SAVE_ME
OK1
#else 
OK2
#endif
OK3

// 21.
#ifdef BARBAPAPA_WILL_RETURN
FAIL
#endif
#define BARBAPAPA_WILL_RETURN
#ifdef BARBAPAPA_WILL_RETURN
OK
#endif

// 22. Unhandled statement (You should get a warning from the next line)
#if 1 || B
OK
#endif

// 23. Commented out test that should just be untouched
/* #if AKAK */
/* #else */
/* AK */
/* #endif */

// 24. Same as test 19, but with comments
#ifdef MANDELMASSA
/* A little comment */
FAIL1 // another comment
#elif TRUE // a third comment
OK
/*#else*/ // one else is commented out
#else
FAIL2
#endif // and final comment

// 25. Handle of 'defined'
#if defined(TRUE)
OK
#else
FAIL
#endif

// 26. Handle of 'defined'
#if !defined(TRUE)
FAIL
#else
OK
#endif

// 27. Handle of 'defined'
#if defined(MANDELMASSA)
FAIL
#else
OK
#endif

// 28. Handle of '!defined'
#if !defined(TRUE)
FAIL
#else
OK
#endif

// 29. Handle of &&
#if defined(TRUE) && defined(FALSE)
OK
#else
FAIL
#endif

// 30. Handle of &&
#if defined(TRUE) && !defined(FALSE)
FAIL
#else
OK
#endif

// 31. Testing preset defines
#undef PINN
#define GREN
#ifdef PINN
FAIL
#endif
#ifdef GREN
OK
#endif

// 32. Testing defines set on commandline
#ifdef CMD1
OK1
#else
FAIL
#endif
#ifdef CMD2
FAIL
#else
OK2
#endif
#ifdef CMD3
OK3
#else
FAIL
#endif

// 33. Nested if 0's
#if 0
FAIL1
# if 0
FAIL2
# else
FAIL3
# endif
FAIL4
#endif
OK

// 34. Nested not defines 
#ifdef ASDF
FAIL1
# ifdef ASDF
FAIL2
# else
FAIL3
# endif
FAIL4
#endif
OK

// 35. Various nest comb I
#ifndef TRUE
FAIL1
# ifdef TRUE
FAIL2
# endif
FAIL3
#endif
OK

// 36. Various nest comb II
#ifndef TRUE
FAIL1
# ifdef TRUE
FAIL2
#  ifndef TRUE
FAIL3
#  endif
FAIL4
# endif
FAIL3
#endif
OK

// 37. Various nest comb III
#ifndef ASDF
OK
# ifdef TRUE
OK
#  ifndef TRUE
FAIL1
#  endif
OK
# endif
OK
#endif
OK

// 38. Mix 0 and not defined (1)
#ifdef ASDF
FAIL1
# if 0
FAIL2
# else
FAIL3
# endif
FAIL4
#endif
OK

// 39. Mix 0 and not defined (2)
#if 0
FAIL1
# ifdef ASDF
FAIL2
# else
FAIL3
# endif
FAIL4
#endif
OK

// 40. Nested double 1 inside if 0
#if 0
FAIL1
# if 1
FAIL2
#  if 1
FAIL3
#  else
FAIL4
#  endif
FAIL5
# else
FAIL6
# endif
FAIL7
#endif
OK

// 41. Elif inside nested if 0
#if 0
FAIL1
# if 0
FAIL2
# elif 1
FAIL3
# else
FAIL4
# endif
FAIL5
#endif
OK

// 42. The roots go deep my lord...
#if 0
FAIL1
# if 0
FAIL2
#  if 0
FAIL3
#   if 0
FAIL4
#   else
FAIL5
#   endif
FAIL6
#  elif 1
FAIL7
#   if 0
FAIL8
#    if 0
FAIL9
#    else
FAIL10
#    endif
FAIL11
#   endif
FAIL12
#  else
FAIL13
#  endif
FAIL14
# endif
FAIL15
#endif
OK

// 43. elif defined
#ifdef MANDELMASSA
FAIL1
#elif defined(CMD1)
OK
#else
FAIL2
#endif

// 44. Handling of ||
#if 1 || 0
OK
#else
FAIL
#endif

// 45. Handling of ||
#if 0 || 0
FAIL
#else
OK
#endif

// 46. Handling of ||
#if defined(CMD1) || 0
OK
#else
FAIL
#endif

// 47. Handle of define spanning several rows
#define ABRAKADABRA(a) \
    Simsalabim \
    sa \
    trollpackan \
    mim

A
B
C
D
E
F
G
H
I
J

#endif // TEST_IFDEFSTRIPPING_H
