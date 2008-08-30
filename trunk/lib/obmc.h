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
	template <cmode mode> void bt(CMuxCodec * inCodec);
	void toppm(char * file_name);

	static int median_mv(sMotionVector * v, int n)
	{
		if (n <= 1)
			return -1;
		if (n <= 2) {
			int d = abs(v[0].x - v[1].x) + abs(v[0].y - v[1].y);
			d += d + d;
			return d;
		}
		int dist[n], idx = 0;
		dist[0] = 0;
		for( int i = 1; i < n; i++){
			dist[i] = 0;
			for( int j = 0; j < i; j++){
				int d = abs(v[i].x - v[j].x) + abs(v[i].y - v[j].y);
				dist[i] += d;
				dist[j] += d;
			}
		}
		for( int i = 1; i < n; i++){
			if (dist[i] < dist[idx])
				idx = i;
		}
		sMotionVector tmp = v[idx];
		v[idx] = v[0];
		v[0] = tmp;
		if (n == 3)
			return dist[idx] + (dist[idx] >> 1);
		return dist[idx];
	}

protected:
	unsigned int dimX;
	unsigned int dimY;
	static const short window[8][16];

	sMotionVector * pMV;
	unsigned char * pRef;

	float a11, a12, a21, a22, mx, my; /// parameters for the (unused) global motion compensation

	sMotionVector median_mv(sMotionVector v1, sMotionVector v2, sMotionVector v3)
	{
		sMotionVector ret;
		ret.x = median(v1.x, v2.x, v3.x);
		ret.y = median(v1.y, v2.y, v3.y);
		return ret;
	}

// 	sMotionVector median_mv(sMotionVector * v, int n)
// 	{
// 		unsigned int x = 0;
// 		unsigned int y = 0;
// 		for( int i = 0; i < n; i++){
// 			x += v[i].x;
// 			y += v[i].y;
// 		}
//
// 		int idx = 0;
// 		int dist = 0x7FFFFFFF;
// 		for( int i = 0; i < n; i++){
// 			int curdist = abs(v[i].x * n - x) + abs(v[i].y * n - y);
// 			if (curdist < dist) {
// 				idx = i;
// 				dist = curdist;
// 			}
// 		}
// 		return v[idx];
// 	}

private:
	char * pData;

	static inline void obmc_block(const short * pSrc, short * pDst, const int src_stride, const int dst_stride);
	template <int flags>
		static void obmc_block(const short * pSrc, short * pDst,
		                       const int src_stride, const int dst_stride);
	template <int flags>
		static void obmc_block_intra(short * pDst, const int dst_stride, const short value);
	template <int flags>
		short get_block_mean(short * pSrc, const int src_stride);

	static inline int get_pos(const sMotionVector mv, const unsigned int i,
	                   const unsigned int j, const unsigned int im_x,
	                   const unsigned int im_y, const int stride);
};

}

