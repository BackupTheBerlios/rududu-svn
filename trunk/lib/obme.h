/***************************************************************************
 *   Copyright (C) 2007 by Nicolas Botti   *
 *   rududu@laposte.net   *
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

#include <obmc.h>

namespace rududu {

typedef struct {
	sMotionVector MV;
	unsigned char ref; /// reference frame
	unsigned char bitCost; /// bit cost of the motion vector in 1/8 bit unit
	unsigned short dist; /// distortion of this MV
} sFullMV;

class COBME : public COBMC
{
public:
	COBME(unsigned int dimX, unsigned int dimY);

	~COBME();

	void EPZS(CImageBuffer & Images);

protected:
	unsigned short * pDist;

private:
	char * pData;

	template <unsigned int size> static unsigned short SAD(const short * pSrc, const short * pDst, const int stride);
	static inline void DiamondSearch(int cur_x, int cur_y, int im_x, int im_y, int stride, short ** pIm, sFullMV & MVBest);
	static sFullMV EPZS(int cur_x, int cur_y, int im_x, int im_y, int stride, short ** pIm, sFullMV * MVPred, int setB, int setC, int thres);
	template <int level> static void subpxl(int cur_x, int cur_y, int im_x, int im_y, int stride, short * pRef, short ** pSub, sFullMV & MVBest);

};

}
