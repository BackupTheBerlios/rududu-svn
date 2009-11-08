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

#include "utils.h"
#include "band.h"
#include "geomcodec.h"
#include "bitcodec.h"

#define BLK_PWR		2
#define BLK_SIZE	(1 << BLK_PWR)
#define GENERATE_HUFF_STATS

// define this to track the quantization error mean and try to make it don't
// go too far from zero
// #define ERR_SH 2

#define ZBLOCK_CTX_NB 16
#define ZK_CTX_NB 6
#define MORE_CTX_NB 16

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

private :
	template <class C> int tsuqBlock(C * pCur, int stride, C Quant, int iQuant,
	                                 int lambda, C * rd_thres);

	template <int block_size, class C>
		static inline int maxLen(C * pBlock, int stride);
	template <cmode mode, bool high_band, class C>
		static unsigned int block_enum(C * pBlock, int stride, CMuxCodec * pCodec,
									   CBitCodec<ZBLOCK_CTX_NB + ZK_CTX_NB + MORE_CTX_NB + 1> & zblockCodec, int idx);
	static inline int clen(int coef, unsigned int cnt);
	template <class C>
		static void inSort (C ** pKeys, int len);
	template <class C>
		static void makeThres(C * thres, const C quant, const int lambda);

	unsigned int * pRD;
#ifdef ERR_SH
	int q_err; /// sum of the quantization error (for quant bin > 1)
#endif
	
	static const char blen[BLK_SIZE * BLK_SIZE + 1];
};

}

