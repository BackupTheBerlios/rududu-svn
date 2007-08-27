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

#include <stdint.h>

#include "obme.h"
#include "utils.h"

namespace rududu {

COBME::COBME(unsigned int dimX, unsigned int dimY)
 : COBMC(dimX, dimY)
{
	int allocated = (dimX * dimY + 1) * sizeof(unsigned short);
	pData = new char[allocated];

	pDist = (unsigned short*)(((intptr_t)pData + sizeof(unsigned short) - 1) & (-sizeof(unsigned short)));
}


COBME::~COBME()
{
	delete[] pData;
}

template <unsigned int size>
unsigned short COBME::SAD(const short * pSrc, const short * pDst, const int stride)
{
	unsigned int ret = 0;
	for (int j = 0; j < size; j++) {
		for (int i = 0; i < size; i++) {
			int tmp = pDst[i] - pSrc[i];
			ret += ABS(tmp);
		}
		pDst += stride;
		pSrc += stride;
	}
	return MIN(ret, 65535);
}

#define CHECK_MV(mv)	\
{	\
	int x = cur_x + mv.x - 4;	\
	int y = cur_y + mv.y - 4;	\
	if (x < -15) x = -15;	\
	if (x >= im_x) x = im_x - 1;	\
	if (y < -15) y = -15;	\
	if (y >= im_y) y = im_y - 1;	\
	src_pos = x + y * stride; \
}

void COBME::DiamondSearch(int cur_x, int cur_y, int im_x, int im_y, int stride,
                          short ** pIm, sFullMV & MVBest)
{
	unsigned int LastMove = 0, Last2Moves = 0, CurrentMove = 0;
	sFullMV MVTemp = MVBest;
	short * pCur = pIm[0] + cur_x + cur_y * stride;
	static const short x_mov[4] = {0, 0, -1, 2};
	static const short y_mov[4] = {-1, 2, -1, 0};
	static const short tst[4] = {DOWN, UP, RIGHT, LEFT};
	static const short step[4] = {UP, DOWN, LEFT, RIGHT};

	do {
		for (int i = 0; i < 4; i++) {
			MVTemp.MV.x += x_mov[i];
			MVTemp.MV.y += y_mov[i];
			if (!(Last2Moves & tst[i])){
				int src_pos;
				CHECK_MV(MVTemp.MV);
				MVTemp.dist = SAD<8>(pIm[1] + src_pos, pCur, stride);
				if (MVBest.dist > MVTemp.dist){
					MVBest = MVTemp;
					CurrentMove = step[i];
				}
			}
		}
		Last2Moves = CurrentMove | LastMove;
		LastMove = CurrentMove;
		CurrentMove = 0;
	}while(LastMove);
}


}
