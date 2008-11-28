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

template <unsigned int size>
static unsigned int SAD(const short * pSrc, const short * pDst, const int stride)
{
	unsigned int ret = 0;
	for (unsigned int j = 0; j < size; j++) {
		for (unsigned int i = 0; i < size; i++)
			ret += abs(pDst[i] - pSrc[i]);
		pDst += stride;
		pSrc += stride;
	}
	return ret;
}

template <unsigned int size>
static unsigned int SAD(const short * pSrc1, const short * pSrc2,
                        const short * pDst, const int stride)
{
	unsigned int ret = 0;
	for (unsigned int j = 0; j < size; j++) {
		for (unsigned int i = 0; i < size; i++)
			ret += abs(pDst[i] - ((pSrc1[i] + pSrc2[i]) >> 1));
		pDst += stride;
		pSrc1 += stride;
		pSrc2 += stride;
	}
	return ret;
}

#ifdef __MMX__

typedef unsigned short v4hi __attribute__ ((vector_size (8)));
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

template <>
static unsigned int SAD<8>(const short * pSrc, const short * pDst, const int stride)
{
	unsigned int ret = 0;
	union vect4us sum;
	sum.v = __builtin_ia32_pxor(sum.v, sum.v);

 	for (unsigned int j = 0; j < 8; j++) {
	 	v4hi * s1 = (v4hi*)pSrc, * s2 = (v4hi*)pDst;
	 	sum.v = _SAD(s1++, s2++, sum.v);
	 	sum.v = _SAD(s1, s2, sum.v);

		pDst += stride;
		pSrc += stride;
	}

	ret = (unsigned int) sum.s[0] + sum.s[1] + sum.s[2] + sum.s[3];

	return ret;
}

template <>
static unsigned int SAD<16>(const short * pSrc, const short * pDst, const int stride)
{
	unsigned int ret = 0;
	union vect4us sum[2];
	sum[0].v = __builtin_ia32_pxor(sum[0].v, sum[0].v);
	sum[1].v = __builtin_ia32_pxor(sum[1].v, sum[1].v);

	for (unsigned int j = 0; j < 16; j++) {
		v4hi * s1 = (v4hi*)pSrc, * s2 = (v4hi*)pDst;
		sum[0].v = _SAD(s1++, s2++, sum[0].v);
		sum[1].v = _SAD(s1++, s2++, sum[1].v);
		sum[0].v = _SAD(s1++, s2++, sum[0].v);
		sum[1].v = _SAD(s1, s2, sum[1].v);

		pDst += stride;
		pSrc += stride;
	}

	ret = (unsigned int) sum[0].s[0] + sum[0].s[1] + sum[0].s[2] + sum[0].s[3] +
		sum[1].s[1] + sum[1].s[2] + sum[1].s[2] + sum[1].s[3];

	return ret;
}

template <>
static unsigned int SAD<8>(const short * pSrc1, const short * pSrc2,
                           const short * pDst, const int stride)
{
	unsigned int ret = 0;
	union vect4us sum;
	sum.v = __builtin_ia32_pxor(sum.v, sum.v);

	for (unsigned int j = 0; j < 8; j++) {
		v4hi *s11 = (v4hi*)pSrc1, *s12 = (v4hi*)pSrc2, *s2 = (v4hi*)pDst;
		sum.v = _SAD(s11++, s12++, s2++, sum.v);
		sum.v = _SAD(s11, s12, s2, sum.v);

		pDst += stride;
		pSrc1 += stride;
		pSrc2 += stride;
	}

	ret = (unsigned int) sum.s[0] + sum.s[1] + sum.s[2] + sum.s[3];

	return ret;
}

template <>
static unsigned int SAD<16>(const short * pSrc1, const short * pSrc2,
                            const short * pDst, const int stride)
{
	unsigned int ret = 0;
	union vect4us sum[2];
	sum[0].v = __builtin_ia32_pxor(sum[0].v, sum[0].v);
	sum[1].v = __builtin_ia32_pxor(sum[1].v, sum[1].v);

	for (unsigned int j = 0; j < 16; j++) {
		v4hi *s11 = (v4hi*)pSrc1, *s12 = (v4hi*)pSrc2, *s2 = (v4hi*)pDst;
		sum[0].v = _SAD(s11++, s12++, s2++, sum[0].v);
		sum[1].v = _SAD(s11++, s12++, s2++, sum[1].v);
		sum[0].v = _SAD(s11++, s12++, s2++, sum[0].v);
		sum[1].v = _SAD(s11, s12, s2, sum[1].v);

		pDst += stride;
		pSrc1 += stride;
		pSrc2 += stride;
	}

	ret = (unsigned int) sum[0].s[0] + sum[0].s[1] + sum[0].s[2] + sum[0].s[3] +
		sum[1].s[1] + sum[1].s[2] + sum[1].s[2] + sum[1].s[3];

	return ret;
}


#endif

#define CHECK_MV(mv)	\
{	\
	int x = cur_x + mv.x;	\
	int y = cur_y + mv.y;	\
	if (x < 1 - (int)size) x = 1 - (int)size;	\
	if (x >= im_x) x = im_x - 1;	\
	if (y < 1 - (int)size) y = 1 - (int)size;	\
	if (y >= im_y) y = im_y - 1;	\
	src_pos = x + y * stride; \
}

#define BEST_MV(mv_result, mv_test, mv_pred)	\
{	\
	int src_pos;	\
	CHECK_MV(mv_test);	\
	unsigned int dist = SAD<size>(pIm[1] + src_pos, pCur, stride);	\
	unsigned int cost = mv_bit_estimate(mv_test, mv_pred); \
	if (mv_result.dist + lambda * mv_result.cost > dist + lambda * cost) {	\
		mv_result.MV = mv_test;	\
		mv_result.cost = cost; \
		mv_result.dist = dist; \
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
                          short ** pIm, sFullMV & MVBest, sMotionVector MVPred,
                          unsigned int lambda)
{
	unsigned int LastMove = 0, Last2Moves = 0, CurrentMove = 0;
	const short * pCur = pIm[0] + cur_x + cur_y * stride;
	static const short x_mov[4] = {0, 0, -1, 2};
	static const short y_mov[4] = {-1, 2, -1, 0};
	static const unsigned int tst[4] = {DOWN, UP, RIGHT, LEFT};
	static const unsigned int step[4] = {UP, DOWN, LEFT, RIGHT};

	do {
		sMotionVector MVTemp = MVBest.MV;
		for (int i = 0; i < 4; i++) {
			MVTemp.x += x_mov[i];
			MVTemp.y += y_mov[i];
			if (!(Last2Moves & tst[i])){
				int src_pos;
				CHECK_MV(MVTemp);
				unsigned int dist = SAD<size>(pIm[1] + src_pos, pCur, stride);
				unsigned int cost = mv_bit_estimate(MVTemp, MVPred);
				if (MVBest.dist + lambda * MVBest.cost > dist + lambda * cost) {
					MVBest.MV = MVTemp;
					MVBest.cost = cost;
					MVBest.dist = dist;
					CurrentMove = step[i];
				}
			}
		}
		Last2Moves = CurrentMove | LastMove;
		LastMove = CurrentMove;
		CurrentMove = 0;
	}while(LastMove);
}

template <int level>
static void subpxl(int cur_x, int cur_y, int im_x, int im_y, int stride,
                   short * pRef, short ** pSub, sFullMV & MVBest)
{
	const short * pCur = pRef + cur_x + cur_y * stride;
	static const short x_mov[8] = {1,  0,-1,-1, 0, 0, 1, 1};
	static const short y_mov[8] = {0, -1, 0, 0, 1, 1, 0, 0};
	const unsigned int size = 8;

	sFullMV MVTemp = MVBest;
	for (int i = 0; i < 8; i++) {
		MVTemp.MV.x += x_mov[i] << level;
		MVTemp.MV.y += y_mov[i] << level;

		int src_pos;
		int pic = ((MVTemp.MV.x & 3) << 2) | (MVTemp.MV.y & 3);
		sMotionVector tmp;
		tmp.x = MVTemp.MV.x >> 2;
		tmp.y = MVTemp.MV.y >> 2;
		CHECK_MV(tmp);
		MVTemp.dist = SAD<8>(pSub[pic] + src_pos, pCur, stride);
		if (MVBest.dist > MVTemp.dist) MVBest = MVTemp;
	}
}

static void halfpxl(int cur_x, int cur_y, int im_x, int im_y, int stride,
                    short * pRef, short ** pSub, sFullMV & MVBest)
{
	const short * pCur = pRef + cur_x + cur_y * stride;
	static const short x_mov[8] = {2,  0,-2,-2, 0, 0, 2, 2};
	static const short y_mov[8] = {0, -2, 0, 0, 2, 2, 0, 0};
	const unsigned int size = 8;

	sFullMV MVTemp = MVBest;
	for (int i = 0; i < 8; i++) {
		MVTemp.MV.x += x_mov[i];
		MVTemp.MV.y += y_mov[i];

		int src_pos;
		int pic = (MVTemp.MV.x & 2) | ((MVTemp.MV.y & 2) >> 1);
		sMotionVector tmp;
		tmp.x = MVTemp.MV.x >> 2;
		tmp.y = MVTemp.MV.y >> 2;
		CHECK_MV(tmp);
		MVTemp.dist = SAD<8>(pSub[pic] + src_pos, pCur, stride);
		if (MVBest.dist > MVTemp.dist) MVBest = MVTemp;
	}
}

static void quarterpxl(int cur_x, int cur_y, int im_x, int im_y, int stride,
                       short * pRef, short ** pSub, sFullMV & MVBest)
{
	const short * pCur = pRef + cur_x + cur_y * stride;
	static const short x_mov[8] = {1,  0,-1,-1, 0, 0, 1, 1};
	static const short y_mov[8] = {0, -1, 0, 0, 1, 1, 0, 0};
	const unsigned int size = 8;

	sFullMV MVTemp = MVBest;
	for (int i = 0; i < 8; i++) {
		MVTemp.MV.x += x_mov[i];
		MVTemp.MV.y += y_mov[i];

		int src_pos;
		sMotionVector tmp = MVTemp.MV, tmp2 = MVTemp.MV;
		int pic1 = (tmp.x & 2) | ((tmp.y & 2) >> 1);
		int pic2 = ((tmp.x + 1) & 2) | (((tmp.y + 1) & 2) >> 1);
		if ((pic1 ^ pic2) == 3 && (pic1 & 1) == (pic1 >> 1)) {
			// TODO : check if it's really usefull
			if (pic1 == 3) {
				tmp.x += 1;
				tmp2.y += 1;
			}
			pic1 = 1;
			pic2 = 2;
		} else {
			tmp2.x += 1;
			tmp2.y += 1;
		}
		tmp.x >>= 2;
		tmp.y >>= 2;
		CHECK_MV(tmp);
		short * src1 = pSub[pic1] + src_pos;
		tmp2.x >>= 2;
		tmp2.y >>= 2;
		CHECK_MV(tmp2);
		MVTemp.dist = SAD<8>(src1, pSub[pic2] + src_pos, pCur, stride);
		if (MVBest.dist > MVTemp.dist) MVBest = MVTemp;
	}
}

#define THRES_A	1024
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
 * @param setB
 * @param setC
 * @param thres
 * @param lambda
 * @return
 */
template <unsigned int size>
static sFullMV EPZS(int cur_x, int cur_y, int im_x, int im_y, int stride,
                    short ** pIm, sMotionVector * lst, int setB, int setC,
                    int thres, unsigned int lambda)
{
	const short * pCur = pIm[0] + cur_x + cur_y * stride;
	// test predictors
	sFullMV MVBest;
	sMotionVector MVPred = MVBest.MV = lst[0];
	MVBest.dist = 65535;
	MVBest.cost = 255;
	if (MVBest.MV.all == MV_INTRA)
		MVBest.MV.all = 0;
	BEST_MV(MVBest, MVBest.MV, MVPred);
	if (MVBest.dist < ((THRES_A * size * size) >> 6))
		return MVBest;

	for( int i = 1; i < setB + 1; i++){
		if (lst[i].all != MV_INTRA)
			BEST_MV(MVBest, lst[i], MVPred);
	}
	if (MVBest.dist < ((THRES_B * size * size) >> 6))
		return MVBest;

	for( int i = setB + 1; i < setB + 1 + setC; i++){
		if (lst[i].all != MV_INTRA)
			BEST_MV(MVBest, lst[i], MVPred);
	}
	if (MVBest.dist < ((THRES_C * size * size) >> 6))
		return MVBest;

	DiamondSearch<size>(cur_x, cur_y, im_x, im_y, stride, pIm, MVBest, MVPred, lambda);
	return MVBest;
}

#define MAX_PREDS	16

void COBME::EPZS(CImageBuffer & Images)
{
	sMotionVector MVPred[MAX_PREDS];
	sMotionVector * pCurMV = pMV;
	unsigned char * pCurRef = pRef;
	unsigned short * pCurDist = pDist;
	int im_x = Images[0][0]->dimX, im_y = Images[0][0]->dimY,
		stride = Images[0][0]->dimXAlign;
	short * pIm[2] = {Images[0][0]->pImage[0], Images[1][0]->pImage[0]};
	short * pSub[SUB_IMAGE_CNT];
	for( int i = 0; i < SUB_IMAGE_CNT; i++){
		pSub[i] = Images[1][i]->pImage[0];
	}

#define BMC_STEP 1

	for( unsigned int j = 0; j < dimY; j += BMC_STEP){
		for( unsigned int i = 0; i < dimX; i += BMC_STEP){
			int n = 1;
			MVPred[0].all = 0;
			if (j == 0) {
				if (i != 0)
					MVPred[0] = pCurMV[i - 1];
			} else {
				if (i == 0)
					MVPred[0] = pCurMV[i - dimX];
				else {
					MVPred[n++] = pCurMV[i - 1];
					MVPred[n++] = pCurMV[i - dimX];
					if (i >= dimX - BMC_STEP) {
						MVPred[0] = median_mv(pCurMV[i - 1], pCurMV[i - dimX], pCurMV[i - dimX - 1]);
						MVPred[n++] = pCurMV[i - dimX - 1];
					} else {
						MVPred[0] = median_mv(pCurMV[i - 1], pCurMV[i - dimX], pCurMV[i - dimX + BMC_STEP]);
						MVPred[n++] = pCurMV[i - dimX + BMC_STEP];
					}
				}
			}

			MVPred[n].x = (pCurMV[i].x + 2) >> 2;
			MVPred[n++].y = (pCurMV[i].y + 2) >> 2;

			MVPred[n++].all = 0;

			sFullMV MVBest = rududu::EPZS<8 * BMC_STEP>(8 * i, 8 * j, im_x, im_y, stride, pIm, MVPred, n - 2, 1, 0, 50);
#if BMC_STEP == 1
			pCurMV[i] = MVBest.MV;
			pCurRef[i] = 0;
			pCurDist[i] = MVBest.dist;
#else
			pCurMV[i] = pCurMV[i + 1] = pCurMV[i + dimX] = pCurMV[i + dimX + 1] = MVBest.MV;
			pCurRef[i] = pCurRef[i + 1] = pCurRef[i + dimX] = pCurRef[i + dimX + 1] = 0;
			pCurDist[i] = pCurDist[i + 1] = pCurDist[i + dimX] = pCurDist[i + dimX + 1] = MVBest.dist;
#endif
		}
		pCurMV += dimX * BMC_STEP;
		pCurRef += dimX * BMC_STEP;
		pCurDist += dimX * BMC_STEP;
	}

// 	pCurMV = pMV;
// 	pCurRef = pRef;
// 	pCurDist = pDist;
// 	for( uint j = 0; j < dimY; j++){
// 		for( uint i = 0; i < dimX; i++){
// 			int n = 0;
// 			if (i > 0)
// 				MVPred[n++] = pCurMV[i - 1];
// 			if (i + 1 < dimX)
// 				MVPred[n++] = pCurMV[i + 1];
// 			if (j > 0)
// 				MVPred[n++] = pCurMV[i - dimX];
// 			if (j + 1 < dimY)
// 				MVPred[n++] = pCurMV[i + dimX];
// 			sFullMV MVBest = rududu::EPZS<8>(8 * i, 8 * j, im_x, im_y, stride, pIm, MVPred, n - 2, 1, 0, 50);
// 			pCurMV[i] = MVBest.MV;
// 			pCurRef[i] = 0;
// 			pCurDist[i] = MVBest.dist;
// 		}
// 		pCurMV += dimX;
// 		pCurRef += dimX;
// 		pCurDist += dimX;
// 	}

	pCurMV = pMV;
	pCurRef = pRef;
	pCurDist = pDist;
	for( unsigned int j = 0; j < dimY; j++){
		for( unsigned int i = 0; i < dimX; i++){
			if (pCurDist[i] < THRES_D) {
				sFullMV MVBest = {pCurMV[i], pCurRef[i], 0, pCurDist[i]};
				MVBest.MV.x <<= 2;
				MVBest.MV.y <<= 2;
				halfpxl(8 * i, 8 * j, im_x, im_y, stride, pIm[0], pSub, MVBest);
				quarterpxl(8 * i, 8 * j, im_x, im_y, stride, pIm[0], pSub, MVBest);
				pCurMV[i] = MVBest.MV;
				pCurDist[i] = MVBest.dist;
			} else
				pCurMV[i].all = MV_INTRA;
		}
		pCurMV += dimX;
		pCurRef += dimX;
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
