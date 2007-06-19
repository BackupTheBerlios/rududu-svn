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

#include "image.h"

namespace rududu {

typedef union {
	unsigned int All;
	struct {
		short	x;
		short	y;
	};
} SMotionVector;

class COBMC{
public:
	COBMC(unsigned int dimX, unsigned int dimY);

    ~COBMC();

	void apply_mv(CImage * pRefFrames, CImage & dstImage);

protected:
	unsigned int dimX;
	unsigned int dimY;
	static const short window[8][8];

	char * pData;
	SMotionVector * pMV;
	unsigned char * pRef;

private:
	static void obmc_block(const short * pSrc, short * pDst, const int stride);
	template <int flags> static void obmc_block(const short * pSrc, short * pDst, const int stride);
};

}

