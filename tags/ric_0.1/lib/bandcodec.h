/***************************************************************************
 *   Copyright (C) 2007-2008 by Nicolas Botti                              *
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

#include <utils.h>
#include <band.h>
#include <geomcodec.h>
#include <bitcodec.h>

#define BLK_SIZE	4
// #define GENERATE_HUFF_STATS

namespace rududu {

class CBandCodec : public CBand
{
public:
	CBandCodec();
	virtual ~CBandCodec();
	static void init_lut(void);

	template <cmode mode, class C> void pred(CMuxCodec * pCodec);
	template <bool high_band, class C>void buildTree(C Quant, int lambda);
	template <cmode mode, bool high_band, class C, class P> void tree(CMuxCodec * pCodec);

#ifdef GENERATE_HUFF_STATS
	static unsigned int histo_l[17][17];
	static unsigned int histo_h[17][16];
#endif

private :
	template <class C> int tsuqBlock(C * pCur, int stride, C Quant, int iQuant,
	                                 int lambda, C * rd_thres);

	template <int block_size, cmode mode, class C>
		static inline int maxLen(C * pBlock, int stride);
	template <cmode mode, bool high_band, class C>
		static unsigned int block_enum(C * pBlock, int stride, CMuxCodec * pCodec,
		                       CGeomCodec & geoCodec, int idx);
	static inline int clen(int coef, unsigned int cnt);
	template <class C>
		static void inSort (C ** pKeys, int len);
	template <class C>
		static void makeThres(C * thres, const C quant, const int lambda);

	unsigned int * pRD;

	static const sHuffSym * huff_lk_enc[17];
	static const sHuffSym * huff_hk_enc[16];
	static sHuffCan huff_lk_dec[17];
	static sHuffCan huff_hk_dec[16];
	static const char blen[BLK_SIZE * BLK_SIZE + 1];
};

}

