/***************************************************************************
 *   Copyright (C) 2007 by Nicolas Botti                                   *
 *   rududu@laposte.net                                                    *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
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

#include "muxcodec.h"
#include "imagebuffer.h"
#include "utils.h"

namespace rududu {

typedef union {
	unsigned int all;
	struct {
		short	x;
		short	y;
	};
} sMotionVector;

#define MV_INTRA	0x80008000

class COBMC{
public:
	COBMC(unsigned int dimX, unsigned int dimY);

    ~COBMC();

	void apply_mv(CImageBuffer & RefFrames, CImage & dstImage);
	template <bool pre>
		void apply_intra(CImage & srcImage, CImage & dstImage);
	void encode(CMuxCodec * inCodec);
	void decode(CMuxCodec * inCodec);

protected:
	unsigned int dimX;
	unsigned int dimY;
	static const short window[8][8];

	sMotionVector * pMV;
	unsigned char * pRef;

	sMotionVector median_mv(sMotionVector v1, sMotionVector v2, sMotionVector v3)
	{
		sMotionVector ret;
		ret.x = median(v1.x, v2.x, v3.x);
		ret.y = median(v1.y, v2.y, v3.y);
		return ret;
	}

private:
	char * pData;

	static void obmc_block(const short * pSrc, short * pDst, const int src_stride, const int dst_stride);
	template <int flags>
		static void obmc_block(const short * pSrc, short * pDst,
		                       const int src_stride, const int dst_stride);
	template <int flags>
		static void obmc_block_intra(short * pDst, const int dst_stride, const short value);

	static inline int get_pos(const sMotionVector mv, const unsigned int i,
	                   const unsigned int j, const unsigned int im_x,
	                   const unsigned int im_y, const int stride);
};

}

