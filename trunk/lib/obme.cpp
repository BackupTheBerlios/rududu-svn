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

#define __STDC_LIMIT_MACROS
#include <stdint.h>
#include <iostream>
using namespace std;

#include "obme.h"

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

#ifndef __MMX__

static inline uint SAD(const short * pSrc, const short * pDst,
                       const int stride, const uint size)
{
	uint ret = 0;
	for (uint j = 0; j < size; j++) {
		for (uint i = 0; i < size; i++)
			ret += abs(pDst[i] - pSrc[i]);
		pDst += stride;
		pSrc += stride;
	}
	return ret;
}

static inline uint SAD(const short * pSrc1, const short * pSrc2,
                       const short * pDst, const int stride, const uint size)
{
	uint ret = 0;
	for (uint j = 0; j < size; j++) {
		for (uint i = 0; i < size; i++)
			ret += abs(pDst[i] - ((pSrc1[i] + pSrc2[i]) >> 1));
		pDst += stride;
		pSrc1 += stride;
		pSrc2 += stride;
	}
	return ret;
}

#else

typedef short v4hi __attribute__ ((vector_size (8)));
union vect4us {
	v4hi v;
	unsigned short s[4];
};

static inline v4hi _SAD(v4hi * s1, v4hi * s2, v4hi sum)
{
	v4hi tmp[2];
	tmp[0] = __builtin_ia32_psubsw(s1[0], s2[0]);
	tmp[1] = __builtin_ia32_pxor(tmp[1], tmp[1]);
	tmp[1] = __builtin_ia32_pcmpgtw(tmp[1], tmp[0]);
	tmp[0] = __builtin_ia32_pxor(tmp[0], tmp[1]);
	tmp[0] = __builtin_ia32_psubw(tmp[0], tmp[1]);
	return __builtin_ia32_paddusw(sum, tmp[0]);
}

static inline v4hi _SAD(v4hi * s11, v4hi * s12, v4hi * s2, v4hi sum)
{
	v4hi tmp[2];
	tmp[0] = __builtin_ia32_paddsw(s11[0], s12[0]);
	tmp[0] = __builtin_ia32_psraw(tmp[0], 1);
	tmp[0] = __builtin_ia32_psubsw(tmp[0], s2[0]);
	tmp[1] = __builtin_ia32_pxor(tmp[1], tmp[1]);
	tmp[1] = __builtin_ia32_pcmpgtw(tmp[1], tmp[0]);
	tmp[0] = __builtin_ia32_pxor(tmp[0], tmp[1]);
	tmp[0] = __builtin_ia32_psubw(tmp[0], tmp[1]);
	return __builtin_ia32_paddusw(sum, tmp[0]);
}

static inline uint SAD(const short * pSrc, const short * pDst,
                       const int stride, const uint size)
{
	uint ret = 0, cnt = 0;
	union vect4us sum[2];

	sum[0].v = __builtin_ia32_pxor(sum[0].v, sum[0].v);
	sum[1].v = __builtin_ia32_pxor(sum[1].v, sum[1].v);

 	for (uint j = 0; j < size; j++) {
	 	v4hi * s1 = (v4hi*)pSrc, * s2 = (v4hi*)pDst;
		for (uint i = 0; i < size; i += 8) {
			sum[0].v = _SAD(s1++, s2++, sum[0].v);
			sum[1].v = _SAD(s1++, s2++, sum[1].v);
		}

		cnt += size;
		if (cnt >= 8*8) {
			ret += (uint) sum[0].s[0] + sum[0].s[1] + sum[0].s[2] + sum[0].s[3] +
					sum[1].s[1] + sum[1].s[2] + sum[1].s[2] + sum[1].s[3];
			sum[0].v = __builtin_ia32_pxor(sum[0].v, sum[0].v);
			sum[1].v = __builtin_ia32_pxor(sum[1].v, sum[1].v);
			cnt = 0;
		}

		pDst += stride;
		pSrc += stride;
	}

	ret += (uint) sum[0].s[0] + sum[0].s[1] + sum[0].s[2] + sum[0].s[3] +
			sum[1].s[1] + sum[1].s[2] + sum[1].s[2] + sum[1].s[3];

	return ret;
}

static inline uint SAD(const short * pSrc1, const short * pSrc2,
                       const short * pDst, const int stride, const uint size)
{
	uint ret = 0, cnt = 0;
	union vect4us sum[2];

	sum[0].v = __builtin_ia32_pxor(sum[0].v, sum[0].v);
	sum[1].v = __builtin_ia32_pxor(sum[1].v, sum[1].v);

	for (uint j = 0; j < size; j++) {
		v4hi *s11 = (v4hi*)pSrc1, *s12 = (v4hi*)pSrc2, *s2 = (v4hi*)pDst;
		for (uint i = 0; i < size; i += 8) {
			sum[0].v = _SAD(s11++, s12++, s2++, sum[0].v);
			sum[1].v = _SAD(s11++, s12++, s2++, sum[1].v);
		}

		cnt += size;
		if (cnt >= 8*8) {
			ret += (uint) sum[0].s[0] + sum[0].s[1] + sum[0].s[2] + sum[0].s[3] +
					sum[1].s[1] + sum[1].s[2] + sum[1].s[2] + sum[1].s[3];
			sum[0].v = __builtin_ia32_pxor(sum[0].v, sum[0].v);
			sum[1].v = __builtin_ia32_pxor(sum[1].v, sum[1].v);
			cnt = 0;
		}

		pDst += stride;
		pSrc1 += stride;
		pSrc2 += stride;
	}

	ret += (uint) sum[0].s[0] + sum[0].s[1] + sum[0].s[2] + sum[0].s[3] +
			sum[1].s[1] + sum[1].s[2] + sum[1].s[2] + sum[1].s[3];

	return ret;
}

#endif

#define BEST_MV(mv_result, mv_test, mv_pred, action)	\
{	\
	sMotionVector v1 = mv_test, v2 = mv_test; \
	int yx = (v1.x & 3) | ((v1.y & 3) << 2); \
	int pic1 = COBMC::qpxl_lut[yx].pic1; \
	int pic2 = COBMC::qpxl_lut[yx].pic2; \
	v1.x++; v2.y++; \
	int src_pos1 = COBMC::get_pos<size, 0>(v1, cur_x, cur_y, im_x, im_y, stride);	\
	uint dist; \
	if (pic2 == -1) { \
		dist = SAD(pSub[pic1] + src_pos1, pCur, stride, size);	\
	} else { \
		int src_pos2 = COBMC::get_pos<size, 0>(v2, cur_x, cur_y, im_x, im_y, stride);	\
		dist = SAD(pSub[pic1] + src_pos1, pSub[pic2] + src_pos2, pCur, stride, size);	\
	} \
	uint cost = mv_bit_estimate(mv_test, mv_pred); \
	if (mv_result.dist + lambda * mv_result.cost > dist + lambda * cost) {	\
		mv_result.MV = mv_test;	\
		mv_result.cost = cost; \
		mv_result.dist = dist; \
		action; \
	} \
}

static int mv_bit_estimate(sMotionVector mv, sMotionVector pred)
{
	// FIXME : this is a suboptimal bit length estimation
	int x = s2u(mv.x - pred.x) + 1;
	int y = s2u(mv.y - pred.y) + 1;
	int l = bitlen(x) + bitlen(y);
	return l * 8; /* estimated bit length in 1/8 bit unit */
}

template <unsigned int size>
static void DiamondSearch(int cur_x, int cur_y, int im_x, int im_y, int stride,
                          short * pRef, short ** pSub, sFullMV & MVBest,
                          sMotionVector MVPred, uint lambda)
{
	const short * pCur = pRef + cur_x + cur_y * stride;
	static const short x_mov[4] = {0, 0, -1, 2};
	static const short y_mov[4] = {-1, 2, -1, 0};
	static const unsigned int tst[4] = {DOWN, UP, RIGHT, LEFT};
	static const unsigned int step[4] = {UP, DOWN, LEFT, RIGHT};
	static const short x_mov_diag[4] = {0, -2, 0, 2};
	static const short y_mov_diag[4] = {-1, 0, 2, 0};
	static const unsigned int step_diag[4] = {UP|RIGHT, UP|LEFT, DOWN|LEFT, DOWN|RIGHT};
	int shift = 3;
	sMotionVector MVTemp;

	MVTemp.x = (MVBest.MV.x + 2) & (~3);
	MVTemp.y = (MVBest.MV.y + 2) & (~3);

	for (; shift >= 0; shift--){
		uint LastMove = 0, Last2Moves = 0, CurrentMove = 0;
		do {
			for (int i = 0; i < 4; i++) {
				MVTemp.x += x_mov[i] << shift;
				MVTemp.y += y_mov[i] << shift;
				if (!(Last2Moves & tst[i]))
					BEST_MV(MVBest, MVTemp, MVPred, CurrentMove = step[i]);
			}
			if (CurrentMove == 0) {
				for (int i = 0; i < 4; i++) {
					MVTemp.x += x_mov_diag[i] << shift;
					MVTemp.y += y_mov_diag[i] << shift;
					BEST_MV(MVBest, MVTemp, MVPred, CurrentMove = LastMove = step_diag[i]);
				}
			}
			Last2Moves = CurrentMove | LastMove;
			LastMove = CurrentMove;
			CurrentMove = 0;
			MVTemp = MVBest.MV;
		}while(LastMove);
	}
}

#define THRES_A	0
#define THRES_B (thres + (thres >> 2))
#define THRES_C (thres + (thres >> 2))
#define THRES_D 65535

/**
 *
 * @param cur_x
 * @param cur_y
 * @param im_x
 * @param im_y
 * @param stride
 * @param pIm
 * @param lst the first vector in the list will be used for rate estimation
 * @param setB number of vectors in set B (set A has 1 vector)
 * @param setC number of vectors in set C
 * @param thres
 * @param lambda
 * @return
 */
template <unsigned int size>
static sFullMV EPZS(int cur_x, int cur_y, int im_x, int im_y, int stride,
                    short * pRef, short ** pSub, sMotionVector * lst, int setB,
                    int setC, int thres, uint lambda)
{
	const short * pCur = pRef + cur_x + cur_y * stride;
	// test predictors
	sFullMV MVBest;
	sMotionVector MVPred = MVBest.MV = lst[0];
	MVBest.dist = 65535;
	MVBest.cost = 255;
	if (MVBest.MV.all == MV_INTRA)
		MVBest.MV.all = 0;
	BEST_MV(MVBest, lst[0], MVPred,);
	if (MVBest.dist < ((THRES_A * size * size) >> 6))
		return MVBest;

	for( int i = 1; i <= setB; i++){
		if (lst[i].all != MV_INTRA)
			BEST_MV(MVBest, lst[i], MVPred,);
	}
	if (MVBest.dist < ((THRES_B * size * size) >> 6))
		return MVBest;

	for( int i = setB + 1; i <= setB + setC; i++){
		if (lst[i].all != MV_INTRA)
			BEST_MV(MVBest, lst[i], MVPred,);
	}
	if (MVBest.dist < ((THRES_C * size * size) >> 6))
		return MVBest;

	DiamondSearch<size>(cur_x, cur_y, im_x, im_y, stride, pRef, pSub, MVBest, MVPred, lambda);
	return MVBest;
}

#define MAX_PREDS	16
#define BMC_STEP 2
#define APPLY_EPZS(mv, dst) \
	COBMC::median_mv(lst, n); \
	lst[n++] = mv; \
	sFullMV MVBest = rududu::EPZS<8>(8 * i, 8 * j, im_x, im_y, stride, pIm, pSub, lst, n - 1, 0, 0, lambda); \
	mv = MVBest.MV; \
	dst = MVBest.dist;

void COBME::EPZS(CImageBuffer & Images)
{
	sMotionVector lst[MAX_PREDS];
	sMotionVector * pCurMV = pMV;
	unsigned char * pCurRef = pRef;
	unsigned short * pCurDist = pDist;
	int im_x = Images[0][0]->dimX, im_y = Images[0][0]->dimY,
		stride = Images[0][0]->dimXAlign;
	short * pIm = Images[0][0]->pImage[0];
	short * pSub[SUB_IMAGE_CNT];
	for (int i = 0; i < SUB_IMAGE_CNT; i++)
		pSub[i] = Images[1][i]->pImage[0];

	for( unsigned int j = 0; j < dimY; j += BMC_STEP){
		for( unsigned int i = 0; i < dimX; i += BMC_STEP){
			int n = 1, lambda = 0;
			lst[0].all = 0;
			if (i > 0)
				lst[n++] = lst[0] = pCurMV[i - 1];
			if (j > 0) {
				lst[n++] = pCurMV[i - dimX];
				if (i != 0) {
					int s = BMC_STEP;
					if (i >= dimX - BMC_STEP)
						s = -1;
					lst[n++] = pCurMV[i - dimX + s];
					lst[0] = median_mv(lst[1], lst[2], lst[3]);
				} else
					lst[0] = lst[--n];
			}

			if (n == 1) lambda = 0;
			else lst[n++].all = 0;
			lst[n++] = pCurMV[i];

			sFullMV MVBest = rududu::EPZS<8 * BMC_STEP>(8 * i, 8 * j, im_x, im_y, stride, pIm, pSub, lst, n - 2, 1, 0, lambda);

#if BMC_STEP == 1
			pCurMV[i] = MVBest.MV;
			pCurRef[i] = 0;
			pCurDist[i] = MVBest.dist;
#else
			pCurMV[i] = pCurMV[i + 1] = pCurMV[i + dimX] = pCurMV[i + dimX + 1] = MVBest.MV;
			pCurRef[i] = pCurRef[i + 1] = pCurRef[i + dimX] = pCurRef[i + dimX + 1] = 0;
			pCurDist[i] = pCurDist[i + 1] = pCurDist[i + dimX] = pCurDist[i + dimX + 1] = MVBest.dist >> 2;
#endif
		}
		pCurMV += dimX * BMC_STEP;
		pCurRef += dimX * BMC_STEP;
		pCurDist += dimX * BMC_STEP;
	}

	uint step = 8, lambda_max = 256, lambda;
	uint line_step = step * dimX;
	pCurMV = pMV + line_step - dimX;
	pCurDist = pDist + line_step - dimX;

	lambda = lambda_max / (step * step * 2);
	for( uint j = step - 1; j < dimY; j += step) {
		for( uint i = 0; i < dimX; i += step) {
			int n = 0;
			if (i > 0)
				lst[n++] = pCurMV[i - step];
			if (j >= step) {
				lst[n++] = pCurMV[i - line_step];
				if (i < dimX - step)
					lst[n++] = pCurMV[i - line_step + step];
				if (i > 0)
					lst[n++] = pCurMV[i - line_step - step];
			}
			APPLY_EPZS(pCurMV[i], pCurDist[i]);
		}
		pCurMV += line_step;
		pCurDist += line_step;
	}

	for( ; step >= 2; step >>= 1){
		lambda = lambda_max / (step * step);
		uint step_2 = step >> 1;
		for( uint j = step_2 - 1; j < dimY; j += step){
			for( uint i = step_2; i < dimX; i += step){
				int n = 0;
				if (j >= step_2) {
					if (i >= step_2)
						lst[n++] = pMV[(j - step_2) * dimX + i - step_2];
					if (i + step_2 < dimX)
						lst[n++] = pMV[(j - step_2) * dimX + i + step_2];
				}
				if (j + step_2 < dimY) {
					if (i >= step_2)
						lst[n++] = pMV[(j + step_2) * dimX + i - step_2];
					if (i + step_2 < dimX)
						lst[n++] = pMV[(j + step_2) * dimX + i + step_2];
				}
				APPLY_EPZS(pMV[j * dimX + i], pDist[j * dimX + i]);
			}
		}
		lambda = lambda_max * 2 / (step * step);
		uint istart = 0;
		for( uint j = step_2 - 1; j < dimY; j += step_2){
			for( uint i = istart; i < dimX; i += step){
				int n = 0;
				if (i >= step_2)
					lst[n++] = pMV[j * dimX + i - step_2];
				if (i + step_2 < dimX)
					lst[n++] = pMV[j * dimX + i + step_2];
				if (j >= step_2)
					lst[n++] = pMV[(j - step_2) * dimX + i];
				if (j + step_2 < dimX)
					lst[n++] = pMV[(j + step_2) * dimX + i];
				APPLY_EPZS(pMV[j * dimX + i], pDist[j * dimX + i]);
			}
			istart = step_2 - istart;
		}
	}

	pCurMV = pMV;
	pCurDist = pDist;
	for (unsigned int j = 0; j < dimY; j++) {
		for (unsigned int i = 0; i < dimX; i++) {
			if (pCurDist[i] >= THRES_D)
				pCurMV[i].all = MV_INTRA;
		}
		pCurMV += dimX;
		pCurDist += dimX;
	}

#ifdef __MMX__
	__builtin_ia32_emms();
#endif
}

void COBME::global_motion(void)
{
	sMotionVector const * pCurMV = pMV + dimX;
	int mean_x = 0, mean_y = 0, cnt = 0;
	for( unsigned int j = 1; j < dimY - 1; j++){
		for( unsigned int i = 1; i < dimX - 1; i++){
			if (pCurMV[i].all != MV_INTRA) {
				mean_x += pCurMV[i].x;
				mean_y += pCurMV[i].y;
				cnt++;
			}
		}
		pCurMV += dimX;
	}

	mx = (float)mean_x / cnt;
	my = (float)mean_y / cnt;

	pCurMV = pMV + dimX;
	double xVx = 0, xVy = 0, yVx = 0, yVy = 0, x2 = 0, y2 = 0, xy = 0;
	for( unsigned int j = 1; j < dimY - 1; j++){
		double l = (double) j - (dimY >> 1);
		for( unsigned int i = 1; i < dimX - 1; i++){
			if (pCurMV[i].all != MV_INTRA) {
				double x = pCurMV[i].x - mx;
				double y = pCurMV[i].y - my;
				double k = (double)i - (dimX >> 1);
				xVx += x * k;
				xVy += y * k;
				yVx += x * l;
				yVy += y * l;
				x2 += k * k;
				y2 += j * l;
				xy += k * l;
			}
		}
		pCurMV += dimX;
	}

	float det = (float) (x2 * y2 - xy * xy);

	a11 = ((float)xVx * y2 + yVx * -xy) / det;
	a12 = ((float)xVx * -xy + yVx * x2) / det;
	a21 = ((float)xVy * y2 + yVy * -xy) / det;
	a22 = ((float)xVy * -xy + yVy * x2) / det;

	// TODO : add ransac refinement
	// voir aussi Low-Complexity Global Motion Estimation from P-Frame Motion Vectors for MPEG-7 Applications
}

}
