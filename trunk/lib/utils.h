/***************************************************************************
 *   Copyright (C) 2006 by Nicolas BOTTI <rududu@laposte.net>              *
 *                                                                         *
 * This software is a computer program whose purpose is to compress        *
 * images.                                                                 *
 *                                                                         *
 * This software is governed by the CeCILL v2 license under French law and *
 * abiding by the rules of distribution of free software.  You can  use,   *
 * modify and/ or redistribute the software under the terms of the         *
 * CeCILL v2 license as circulated by CEA, CNRS and INRIA at the following *
 * URL "http://www.cecill.info".                                           *
 *                                                                         *
 * As a counterpart to the access to the source code and  rights to copy,  *
 * modify and redistribute granted by the license, users are provided only *
 * with a limited warranty  and the software's author,  the holder of the  *
 * economic rights,  and the successive licensors  have only  limited      *
 * liability.                                                              *
 *                                                                         *
 * In this respect, the user's attention is drawn to the risks associated  *
 * with loading,  using,  modifying and/or developing or reproducing the   *
 * software by the user in light of its specific status of free software,  *
 * that may mean  that it is complicated to manipulate,  and  that  also   *
 * therefore means  that it is reserved for developers  and  experienced   *
 * professionals having in-depth computer knowledge. Users are therefore   *
 * encouraged to load and test the software's suitability as regards their *
 * requirements in conditions enabling the security of their systems       *
 * and/or data to be ensured and,  more generally, to use and operate it   *
 * in the same conditions as regards security.                             *
 *                                                                         *
 * The fact that you are presently reading this means that you have had    *
 * knowledge of the CeCILL v2 license and that you accept its terms.       *
 ***************************************************************************/

#pragma once

#include <stdio.h>

namespace rududu {

typedef enum cmode {encode, decode};
typedef enum trans {cdf97, cdf53, haar};

#define TOP		1
#define UP		1
#define BOTTOM	2
#define DOWN	2
#define LEFT	4
#define RIGHT	8

#define UNSIGN_CLIP(NbToClip,Value)	\
	((NbToClip) > (Value) ? (Value) : (NbToClip))

#define CLIP(NbToClip,ValueMin,ValueMax)	\
	((NbToClip) > (ValueMax) ? (ValueMax) : ((NbToClip) < (ValueMin) ? (ValueMin) : (NbToClip)))

#define ABS(Number)					\
	((Number) < 0 ? -(Number) : (Number))

#define MAX(a,b)	\
	(((a) > (b)) ? (a) : (b))

#define MIN(a,b)	\
	(((a) > (b)) ? (b) : (a))

template <class a>
a inline clip(const a nb, const a min, const a max)
{
	if (nb < min)
		return min;
	if (nb > max)
		return max;
	return nb;
}

template <class a>
a inline median(a nb1, a nb2, const a nb3)
{
	if (nb2 < nb1) {
		a tmp = nb1;
		nb1 = nb2;
		nb2 = tmp;
	}
	if (nb3 <= nb1)
		return nb1;
	if (nb3 <= nb2)
		return nb3;
	return nb2;
}

int inline s2u(int s)
{
	int u = -(2 * s + 1);
	u ^= u >> 31;
	return u;
}

int inline u2s(int u)
{
	return (u >> 1) ^ -(u & 1);
}

int inline s2u_(int s)
{
	int m = s >> 31;
	return (2 * s + m) ^ (m * 2);
}

int inline u2s_(int u)
{
	int m = (u << 31) >> 31;
	return ((u >> 1) + m) ^ m;
}

// from http://graphics.stanford.edu/~seander/bithacks.html
int inline bitcnt(int v)
{
	v -= (v >> 1) & 0x55555555;
	v = (v & 0x33333333) + ((v >> 2) & 0x33333333);
	return ((v + (v >> 4) & 0xF0F0F0F) * 0x1010101) >> 24;
}

static const char log[32] =
{0,1,2,2,3,3,3,3,4,4,4,4,4,4,4,4,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5};

int inline bitlen(unsigned int v)
{
	int r = 0;
	while (v >= 32) {
		r += 5;
		v >>= 5;
	}
	return r + log[v];
}

template <class a, int x, int y, bool add>
	void inline copy(a * src, a * dst, const int src_stride, const int dst_stride)
{
	for (int j = 0; j < y; j++) {
		for (int i = 0; i < x; i++) {
			if (add)
				dst[i] += src[i];
			else
				dst[i] = src[i];
		}
		src += src_stride;
		dst += dst_stride;
	}
}

}

