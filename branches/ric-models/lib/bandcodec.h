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

	template <cmode mode> void pred(CMuxCodec * pCodec);
	template <bool high_band, int block_size> void buildTree(short Quant, float Thres, int lambda);
	template <cmode mode, bool high_band, int block_size> void tree(CMuxCodec * pCodec);

#ifdef GENERATE_HUFF_STATS
	static unsigned int histo_l[17][17];
	static unsigned int histo_h[17][16];
#endif

private :
	template <int block_size>
			unsigned int tsuqBlock(short * pCur, int stride, short Quant, short iQuant, short T, int lambda);

	template <int block_size, cmode mode>
		static inline int maxLen(short * pBlock, int stride);
	template <cmode mode, bool high_band>
		static unsigned int block_enum(short * pBlock, int stride, CMuxCodec * pCodec,
		                       CGeomCodec & geoCodec, int idx);
	unsigned int * pRD;

	static const sHuffSym * huff_lk_enc[17];
	static const sHuffSym * huff_hk_enc[16];
	static sHuffCan huff_lk_dec[17];
	static sHuffCan huff_hk_dec[16];
};

}

