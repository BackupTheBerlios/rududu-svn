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

#define __STDC_LIMIT_MACROS
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
	for (unsigned int j = 0; j < size; j++) {
		for (unsigned int i = 0; i < size; i++) {
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
	int x = cur_x + mv.x;	\
	int y = cur_y + mv.y;	\
	if (x < -7) x = -7;	\
	if (x >= im_x) x = im_x - 1;	\
	if (y < -7) y = -7;	\
	if (y >= im_y) y = im_y - 1;	\
	src_pos = x + y * stride; \
}

#define BEST_MV(mv_result, mv_test)	\
{	\
	int src_pos;	\
	CHECK_MV(mv_test.MV);	\
	mv_test.dist = SAD<8>(pIm[1] + src_pos, pCur, stride);	\
	if (mv_result.dist > mv_test.dist)	\
		mv_result = mv_test;	\
}

void COBME::DiamondSearch(int cur_x, int cur_y, int im_x, int im_y, int stride,
                          short ** pIm, sFullMV & MVBest)
{
	unsigned int LastMove = 0, Last2Moves = 0, CurrentMove = 0;
	short * pCur = pIm[0] + cur_x + cur_y * stride;
	static const short x_mov[4] = {0, 0, -1, 2};
	static const short y_mov[4] = {-1, 2, -1, 0};
	static const unsigned int tst[4] = {DOWN, UP, RIGHT, LEFT};
	static const unsigned int step[4] = {UP, DOWN, LEFT, RIGHT};

	do {
		sFullMV MVTemp = MVBest;
		for (int i = 0; i < 4; i++) {
			MVTemp.MV.x += x_mov[i];
			MVTemp.MV.y += y_mov[i];
			if (!(Last2Moves & tst[i])){
				int src_pos;
				CHECK_MV(MVTemp.MV);
				MVTemp.dist = SAD<8>(pIm[1 + MVTemp.ref] + src_pos, pCur, stride);
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

#define THRES_A	256
#define THRES_B (thres + (thres >> 2))
#define THRES_C (thres + (thres >> 2))

sFullMV COBME::EPZS(int cur_x, int cur_y, int im_x, int im_y, int stride,
                 short ** pIm, sFullMV * MVPred, int setB, int setC, int thres)
{
	short * pCur = pIm[0] + cur_x + cur_y * stride;
	// test predictors
	sFullMV MVBest = MVPred[0];
	BEST_MV(MVBest, MVBest);
	if (MVBest.dist < THRES_A)
		return MVBest;

	for( int i = 1; i < setB + 1; i++){
		BEST_MV(MVBest, MVPred[i]);
	}
	if (MVBest.dist < THRES_B)
		return MVBest;

	for( int i = setB + 1; i < setB + 1 + setC; i++){
		BEST_MV(MVBest, MVPred[i]);
	}
	if (MVBest.dist < THRES_C)
		return MVBest;

	DiamondSearch(cur_x, cur_y, im_x, im_y, stride, pIm, MVBest);
	return MVBest;
}

#define MAX_PREDS	16

void COBME::EPZS(int im_x, int im_y, int stride, short ** pIm)
{
	sFullMV MVPred[MAX_PREDS];
	sMotionVector * pCurMV = pMV;
	unsigned char * pCurRef = pRef;
	unsigned short * pCurDist = pDist;

	for( unsigned int j = 0; j < dimY; j++){
		for( unsigned int i = 0; i < dimX; i++){
			int n = 1;
			MVPred[0].MV.all = 0;
			if (j == 0) {
				if (i == 0)
					MVPred[0].MV.all = 0;
				else
					MVPred[0].MV = pCurMV[i - 1];
			} else {
				if (i == 0 || i == dimX -1)
					MVPred[0].MV = pCurMV[i - dimX];
				else {
					MVPred[0].MV = median_mv(pCurMV[i - 1], pCurMV[i - dimX], pCurMV[i - dimX + 1]);
					MVPred[n++].MV = pCurMV[i - 1];
					MVPred[n++].MV = pCurMV[i - dimX];
					MVPred[n++].MV = pCurMV[i - dimX + 1];
				}
			}

			MVPred[n++].MV = pCurMV[i];

			for( int k = 0; k < n; k++){
				MVPred[k].ref = 0;
				MVPred[k].dist = UINT16_MAX;
			}

			sFullMV MVBest = EPZS(8 * i, 8 * j, im_x, im_y, stride, pIm, MVPred, n - 1, 0, 0);
			pCurMV[i] = MVBest.MV;
			pCurRef[i] = MVBest.ref;
			pCurDist[i] = MVBest.dist;
		}
		pCurMV += dimX;
		pCurRef += dimX;
		pCurDist += dimX;
	}
}

}