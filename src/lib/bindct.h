/***************************************************************************
 *   Copyright (C) 2007-2008 by Nicolas Botti   *
 *   rududu@laposte.net   *
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

#define BFLY(a,b)	{short tmp = a; a += b; b = tmp - b;}
#define BFLY_FWD(a,b)	b = a - b; a -= b >> 1;
#define BFLY_INV(a,b)	a += b >> 1; b = a - b;

// DCT from
// http://thanglong.ece.jhu.edu/Tran/Pub/binDCT.pdf

// binDCT L3
#define P1(a)	((a >> 1) - (a >> 4))	// 7/16
#define U1(a)	((a >> 1) - (a >> 3))	// 3/8
#define P2(a)	(a >> 2)	// 1/4
#define U2(a)	((a >> 1) + (a >> 4))	// 9/16
#define P3(a)	((a >> 2) + (a >> 4))	// 5/16
#define P4(a)	(a >> 3)	// 1/8
#define U3(a)	((a >> 2) - (a >> 4))	// 3/16
#define P5(a)	((a >> 3) - (a >> 5))	// 3/32

// binDCT L4
// #define P1(a)	((a >> 1) - (a >> 3))	// 3/8
// #define U1(a)	(a >> 2)	// 1/4
// #define P2(a)	(a >> 2)	// 1/4
// #define U2(a)	(a >> 1)	// 1/2
// #define P3(a)	(a >> 2)	// 1/4
// #define P4(a)	(a >> 3)	// 1/8
// #define U3(a)	((a >> 2) - (a >> 4))	// 3/16
// #define P5(a)	((a >> 3) - (a >> 5))	// 3/32

// binDCT L5
// #define P1(a)	(a >> 1)	// 1/2
// #define U1(a)	(a >> 1)	// 1/2
// #define P2(a)	(a >> 2)	// 1/4
// #define U2(a)	(a >> 1)	// 1/2
// #define P3(a)	(a >> 2)	// 1/4
// #define P4(a)	(a >> 3)	// 1/8
// #define U3(a)	(a >> 2)	// 1/4
// #define P5(a)	(a >> 3)	// 1/8

// binDCT L6
// #define P1(a)	(a >> 1)	// 1/2
// #define U1(a)	(a >> 1)	// 1/2
// #define P2(a)	0
// #define U2(a)	(a >> 1)	// 1/2
// #define P3(a)	(a >> 2)	// 1/4
// #define P4(a)	0
// #define U3(a)	(a >> 2)	// 1/4
// #define P5(a)	0
