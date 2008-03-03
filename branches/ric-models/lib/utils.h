/***************************************************************************
 *   Copyright (C) 2006-2008 by Nicolas Botti                              *
 *   <rududu@laposte.net>                                                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#pragma once

#include <stdio.h>

namespace rududu {

typedef enum cmode {encode, decode};
typedef enum trans {cdf97 = 0, cdf53 = 1, haar = 2, cdf75 = 3};

#define TOP		1
#define UP		1
#define BOTTOM	2
#define DOWN	2
#define LEFT	4
#define RIGHT	8
#define TOP_LEFT	16
#define TOP_RIGHT	32

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

unsigned short inline U(short s)
{
	return (unsigned short)s;
}

unsigned int inline U(int s)
{
	return (unsigned int)s;
}

int inline abs(int s)
{
	int const m = s >> sizeof(int) * 8 - 1;
	return (s + m) ^ m;
}

// from http://graphics.stanford.edu/~seander/bithacks.html
int inline bitcnt(int v)
{
	v -= (v >> 1) & 0x55555555;
	v = (v & 0x33333333) + ((v >> 2) & 0x33333333);
	return ((v + (v >> 4) & 0xF0F0F0F) * 0x1010101) >> 24;
}

static const char log_int[32] =
{0,1,2,2,3,3,3,3,4,4,4,4,4,4,4,4,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5};

int inline bitlen(unsigned int v)
{
	int r = 0;
	while (v >= 32) {
		r += 5;
		v >>= 5;
	}
	return r + log_int[v];
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

