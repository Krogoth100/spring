/* See the import.pl script for potential modifications */
/* s_tanhf.c -- StreflopSimple version of s_tanh.c.
 * Conversion to StreflopSimple by Ian Lance Taylor, Cygnus Support, ian@cygnus.com.
 */

/*
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunPro, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice
 * is preserved.
 * ====================================================
 */

#if defined(LIBM_SCCS) && !defined(lint)
static char rcsid[] = "$NetBSD: s_tanhf.c,v 1.4f 1995/05/10 20:48:24 jtc Exp $";
#endif

#include "SMath.h"
#include "math_private.h"

namespace streflop_libm {
#ifdef __STDC__
static const StreflopSimple one=1.0f, two=2.0f, tiny = 1.0e-30f;
#else
static StreflopSimple one=1.0f, two=2.0f, tiny = 1.0e-30f;
#endif

#ifdef __STDC__
	StreflopSimple __tanhf(StreflopSimple x)
#else
	StreflopSimple __tanhf(x)
	StreflopSimple x;
#endif
{
	StreflopSimple t,z;
	int32_t jx,ix;

	GET_FLOAT_WORD(jx,x);
	ix = jx&0x7fffffff;

    /* x is INF or NaN */
	if(ix>=0x7f800000) {
	    if (jx>=0) return one/x+one;    /* tanh(+-inf)=+-1 */
	    else       return one/x-one;    /* tanh(NaN) = NaN */
	}

    /* |x| < 22 */
	if (ix < 0x41b00000) {		/* |x|<22 */
	    if (ix == 0)
		return x;		/* x == +-0 */
	    if (ix<0x24000000) 		/* |x|<2**-55 */
		return x*(one+x);    	/* tanh(small) = small */
	    if (ix>=0x3f800000) {	/* |x|>=1  */
		t = __expm1f(two*fabsf(x));
		z = one - two/(t+two);
	    } else {
	        t = __expm1f(-two*fabsf(x));
	        z= -t/(t+two);
	    }
    /* |x| > 22, return +-1 */
	} else {
	    z = one - tiny;		/* raised inexact flag */
	}
	return (jx>=0)? z: -z;
}
weak_alias (__tanhf, tanhf)
}
