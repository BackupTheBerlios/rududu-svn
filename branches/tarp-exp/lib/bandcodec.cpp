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

#include "bandcodec.h"
#include "huffcodec.h"
#include "hufftables.h" // huffman tables

#ifdef GENERATE_HUFF_STATS
#include <math.h>
#include <string.h>
#include <iostream>
using namespace std;
#endif

namespace rududu {

CBandCodec::CBandCodec(void):
	CBand(),
	pRD(0)
{
	init_lut();
#ifdef ERR_SH
	q_err = 0;
#endif
}

CBandCodec::~CBandCodec()
{
	delete[] pRD;
}

void CBandCodec::init_lut(void)
{
	static bool init = false;
	if (init)
		return;
	for( unsigned int i = 0; i < sizeof(huff_0_dec) / sizeof(sHuffCan); i++){
		CHuffCodec::init_lut(&huff_0_dec[i], LUT_DEPTH);
	}
	for( unsigned int i = 0; i < sizeof(huff_X_dec) / sizeof(sHuffCan); i++){
		CHuffCodec::init_lut(&huff_X_dec[i], LUT_DEPTH);
	}
#ifdef GENERATE_HUFF_STATS
	memset(k_0_c00, 0, sizeof(k_0_c00));
	memset(k_0_c01, 0, sizeof(k_0_c01));
	memset(k_0_c02, 0, sizeof(k_0_c02));
	memset(k_0_c03, 0, sizeof(k_0_c03));
	memset(k_2_c00, 0, sizeof(k_2_c00));
	memset(k_3_c00, 0, sizeof(k_3_c00));
	memset(k_3_c01, 0, sizeof(k_3_c01));
	memset(k_4_c00, 0, sizeof(k_4_c00));
	memset(k_4_c01, 0, sizeof(k_4_c01));
	memset(k_X_c00, 0, sizeof(k_X_c00));
	memset(k_X_c01, 0, sizeof(k_X_c01));
#endif
	init = true;
}

#ifdef GENERATE_HUFF_STATS
static void print_hist(sHuffSym const * const * hist, uint cnt, uint ctx, const char * name)
{
	printf(">%s\n", name);
	for (uint j = 0; j < ctx; j++) {
		for (uint i = 0; i < cnt; i++)
			printf("%i	", *(uint*)&hist[j][i]);
		printf("\n");
	}
	printf("\n");
}
#endif

template <cmode mode, class C>
		void CBandCodec::pred(CMuxCodec * pCodec)
{
	C * pCur = (C*) pBand;
	const int stride = DimXAlign;
	static const unsigned char geo_init[GEO_CONTEXT_NB]
		= { 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 15 };
	CGeomCodec geoCodec(pCodec, geo_init);

	if (mode == encode)
		pCodec->tabooCode(s2u(pCur[0]));
	else
		pCur[0] = u2s(pCodec->tabooDecode());

	for( unsigned int i = 1; i < DimX; i++){
		if (mode == encode)
			geoCodec.code(s2u(pCur[i] - pCur[i - 1]), DEPRECATED_BIT_CONTEXT_NB - 1);
		else
			pCur[i] = pCur[i - 1] + u2s(geoCodec.decode(DEPRECATED_BIT_CONTEXT_NB - 1));
	}
	pCur += stride;

	for( unsigned int j = 1; j < DimY; j++){
		if (mode == encode)
			geoCodec.code(s2u(pCur[0] - pCur[-stride]), DEPRECATED_BIT_CONTEXT_NB - 1);
		else
			pCur[0] = pCur[-stride] + u2s(geoCodec.decode(DEPRECATED_BIT_CONTEXT_NB - 1));

		for( unsigned int i = 1; i < DimX; i++){
			int var = ABS(pCur[i - 1] - pCur[i - 1 - stride]) +
					ABS(pCur[i - stride] - pCur[i - 1 - stride]);
			var = min(DEPRECATED_BIT_CONTEXT_NB - 2, bitlen(var));
			if (mode == encode) {
				int pred = pCur[i] - pCur[i - 1] - pCur[i - stride] +
						pCur[i - 1 - stride];
				geoCodec.code(s2u(pred), var);
			} else
				pCur[i] = pCur[i - 1] + pCur[i - stride] -
						pCur[i - 1 - stride] + u2s(geoCodec.decode(var));
		}
		pCur += stride;
	}
}

template void CBandCodec::pred<encode, short>(CMuxCodec *);
template void CBandCodec::pred<decode, short>(CMuxCodec *);

template void CBandCodec::pred<encode, int>(CMuxCodec *);
template void CBandCodec::pred<decode, int>(CMuxCodec *);

//FIXME : what happen with int ?
#define INSIGNIF_BLOCK -0x8000

template <class C>
	void  CBandCodec::inSort(C ** pKeys, int len)
{
	for (int i = 1; i < len; i++) {
		C * tmp = pKeys[i];
		int j = i;
		while (j > 0 && U(pKeys[j - 1][0]) < U(tmp[0])) {
			pKeys[j] = pKeys[j - 1];
			j--;
		}
		pKeys[j] = tmp;
	}
}

const char CBandCodec::blen[BLK_SIZE * BLK_SIZE + 1] = {
// 		0, 11, 17, 22, 26, 29, 31, 32, 32, 32, 31, 29, 26, 22, 17, 11, 0 // theo
// 		10, 15, 22, 27, 31, 34, 37, 39, 39, 39, 38, 37, 34, 30, 25, 19, 10 // @q=9 for all bands
	20, 40, 55, 66, 75, 81, 85, 88, 89, 88, 85, 81, 75, 66, 55, 40, 20 // enum theo
};

int CBandCodec::clen(int coef, unsigned int cnt)
{
	static const unsigned char k[] =
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2};
	static const unsigned char lps_len[] =
	{3, 3, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
	static const unsigned char mps_len[] =
	{1, 1, 2, 2, 2, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5};

	cnt--;
	unsigned int l = (coef - 1) >> k[cnt];
	return (k[cnt] + 1 + l * lps_len[cnt]) * 5 + mps_len[cnt];
}

const char len_1[BLK_SIZE * BLK_SIZE] = {
	5, 5, 5, 6, 6, 6, 6, 6, 7, 7, 8, 10, 12, 14, 17, 20
};

template <class C>
	void CBandCodec::makeThres(C * thres, const C quant, const int lambda)
{
	for( int i = 0; i < BLK_SIZE * BLK_SIZE; i++){
		thres[i] = (quant + ((lambda * (blen[i + 1] - blen[i] + len_1[i]/*clen(1, i + 1)*/) + 8) >> 4)) & 0xFFFE;
		if (thres[i] > quant * 2) thres[i] = quant * 2;
		if (thres[i] < (quant & 0xFFFE)) thres[i] = quant & 0xFFFE;
	}
}

template <class C>
	int CBandCodec::tsuqBlock(C * pCur, int stride, C Quant, int iQuant,
	                          int lambda, C * rd_thres)
{
	int var_cnt = 0, cnt = 0, dist = 0, rate = 0;
	C T = Quant >> 1;
	C * var[BLK_SIZE * BLK_SIZE];
	int cor = (1 << 15);
#ifdef ERR_SH
	cor -= (q_err >> ERR_SH);
	if (cor > (1 << 15))
		cor = (1 << 15);
#endif
	for (int j = 0; j < BLK_SIZE; j++) {
		for ( int i = 0; i < BLK_SIZE; i++ ) {
			if (U(pCur[i] + T) <= U(2 * T)) {
				pCur[i] = 0;
			} else {
				pCur[i] = s2u_(pCur[i]);
				if (U(pCur[i]) < U(rd_thres[0]))
					var[var_cnt++] = pCur + i;
				else {
					cnt++;
					int tmp = U(pCur[i]) >> 1;
					int qtmp = (tmp * iQuant + cor) >> 16;
					if (qtmp <= 0) qtmp = 1;
					int diff = qtmp * Quant;
#ifdef ERR_SH
					if (qtmp > 1)
						q_err += diff - tmp;
#endif
					if (diff > tmp) diff = 2 * tmp - diff; // FIXME c'est quoi ce calcul de distorsion bidon ?
					dist += diff;
					pCur[i] = (qtmp << 1) | (pCur[i] & 1);
				}
			}
		}
		pCur += stride;
	}

	if (var_cnt > 0) {
		inSort(var, var_cnt);
		int i = var_cnt - 1;
		while( i >= 0 && *var[i] < rd_thres[i + cnt])
			*var[i--] = 0;
		cnt += i + 1;
		for (; i >= 0; i--) {
			int diff = Quant - (pCur[i] & (C)0xFFFFFFFE); // FIXME : pkoi x2 ?
			dist += diff;
			*var[i] = 2 | (*var[i] & 1);
		}
	}

	for (int j = 0; j < BLK_SIZE; j++) {
		pCur -= stride;
		for ( int i = 0; i < BLK_SIZE; i++ ) {
			if (pCur[i] != 0)
				rate += clen(pCur[i] >> 1, cnt);
		}
	}

	rate += blen[cnt] - blen[0];

	// FIXME : this part of the R/D doesn't work
// 	return dist * 16 - lambda * rate;
	return cnt;
}

template <bool high_band, class C>
	void CBandCodec::buildTree(const C Quant, const int lambda)
{
	C rd_thres[BLK_SIZE * BLK_SIZE];
	int lbda = (int) (lambda / Weight);
	C Q = (short) (Quant / Weight);
	if (Q == 0) Q = 1;
	int iQuant = (1 << 16) / Q;
	makeThres(rd_thres, Q, lbda);
	C * pCur = (C*) pBand;
	unsigned int * pChild[2] = {0, 0};
	unsigned int child_stride = 0;

	if (this->pRD == 0)
		this->pRD = new unsigned int [((DimX + BLK_SIZE - 1) / BLK_SIZE) * ((DimY + BLK_SIZE - 1) / BLK_SIZE)];
	unsigned int * pRD = this->pRD;
	unsigned int rd_stride = (DimX + BLK_SIZE - 1) / BLK_SIZE;

	if (! high_band) {
		pChild[0] = ((CBandCodec*)this->pChild)->pRD;
		pChild[1] = pChild[0] + (this->pChild->DimX + BLK_SIZE - 1) / BLK_SIZE;
		child_stride = ((this->pChild->DimX + BLK_SIZE - 1) / BLK_SIZE) * 2;
	}

	for( unsigned int j = 0, i, k; j <= DimY - BLK_SIZE; j += BLK_SIZE){
		for( i = 0, k = 0; i <= DimX - BLK_SIZE; i += BLK_SIZE, k++){
			int rate = 0;
			long long dist = tsuqBlock(pCur + i, DimXAlign, Q, iQuant, lbda, rd_thres);
			if (! high_band) {
				dist += pChild[0][2*k] + pChild[0][2*k + 1] + pChild[1][2*k] + pChild[1][2*k + 1];
				rate += 4 * 5;
			}
			if (dist <= 0 /*lambda * rate*/) {
				pCur[i] = INSIGNIF_BLOCK;
				pRD[k] = 0;
			} else
				pRD[k] = MIN(dist/* - lambda * rate*/, 0xFFFFFFFFu);
		}
		pCur += DimXAlign * BLK_SIZE;
		pRD += rd_stride;
		pChild[0] += child_stride;
		pChild[1] += child_stride;
	}

	if (pParent != 0) {
		if (pParent->type == sshort)
			((CBandCodec*)pParent)->buildTree<false, short>(Quant, lambda);
		else if (pParent->type == sint)
			((CBandCodec*)pParent)->buildTree<false, int>(Quant, lambda);
	}
}

template void CBandCodec::buildTree<true, short>(short, int);
template void CBandCodec::buildTree<true, int>(int, int);

template <int block_size, class C>
	int CBandCodec::maxLen(C * pBlock, int stride)
{
	if (block_size == 1)
		return bitlen(ABS(pBlock[0]));

	C max = 0;
	for( int j = 0; j < block_size; j++) {
		for( int i = 0; i < block_size; i++)
			max = MAX(max, pBlock[i]);
		pBlock += stride;
	}
	return bitlen(U(max) >> 1);
}

static const char k0_lim[] = {16, 18, 22, 30, 37};
static const char k0_lut[10] = {2, 0, 0, 1, 1, 1, 2, 2, 2, 2};

static void sum_cnt_encode(const int sum, const int cnt, const int idx, CMuxCodec * pCodec)
{
	int k_cnt, n = 0, ctx;

	if (idx > 9)
		ctx = 3;
	else
		ctx = k0_lut[idx];

	if (sum < 16) {
		k_cnt = 8 * (sum - 1) + cnt - 1;
		if (cnt > 8)
			k_cnt = 8 * 14 + 15 - k_cnt;
	} else {
		while (k0_lim[++n] <= sum) {}
		k_cnt = 15 * 8 + (((n - 1) << 4) | (cnt - 1));
	}

	pCodec->huffCode(huff_0_enc[ctx][k_cnt]);
	if (n > 0)
		pCodec->maxCode(sum - k0_lim[n - 1], k0_lim[n] - k0_lim[n - 1] - 1);
}

static void sum_cnt_decode(int & sum, int & cnt, const int idx, CMuxCodec * pCodec)
{
	int k_cnt, n = 0, ctx;

	if (idx > 9)
		ctx = 3;
	else
		ctx = k0_lut[idx];

	k_cnt = pCodec->huffDecode(&huff_0_dec[ctx]);

	if (k_cnt < 15 * 8) {
		sum = (k_cnt >> 3) + 1;
		cnt = (k_cnt & 7) + 1;
		if (unlikely(cnt > sum)) {
			cnt = 17 - cnt;
			sum = 16 - sum;
		}
	} else {
		k_cnt -= 15 * 8;
		cnt = (k_cnt & 0xF) + 1;
		n = (k_cnt >> 4) + 1;
	}

	if (n > 0)
		sum = pCodec->maxDecode(k0_lim[n] - k0_lim[n - 1] - 1) + k0_lim[n - 1];
}

#define AVG_SHIFT 5
#define AVG_DECAY 3

template <cmode mode, bool high_band, class C>
	unsigned int CBandCodec::block_enum(C * pBlock, int stride, CMuxCodec * pCodec,
										CBitCodec<ZBLOCK_CTX_NB + ZK_CTX_NB + MORE_CTX_NB + 1> & bitCodec, int sum_)
{
	static uint k_avg[64] = {128, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 64, 64, 72, 72, 80, 88, 88, 96, 104, 112, 120, 128, 128, 136, 144, 152, 160, 168, 176, 184, 192, 200, 200, 208, 216, 224, 232, 240, 248, 248, 256, 264, 272, 280, 288, 296, 296, 304, 312, 320, 328, 336, 344, 344, 352, 360, 368, 376, 384}; // FIXME static is bad here
	static const uint k_max[K_2_CTX + K_3_CTX + K_4_CTX] = {2, 3, 3, 4, 4};
	uint k = 0;
	short enu[16];
	C tmp[16];
	int sum = 0, cnt = 0;

	int tst = sum_;
	if (tst > 3)
		tst = log2i(sum_) - 8;

	if (mode == encode) {
		for (int j = 0; j < 4; j++) {
			for (C * pEnd = pBlock + 4; pBlock < pEnd; pBlock++) {
				tmp[k++] = pBlock[0];
				sum += pBlock[0] >> 1;
				if (pBlock[0] != 0)
					cnt++;
			}
			pBlock += stride - 4;
		}

		if (!high_band && bitCodec.code(sum == 0, tst >= ZBLOCK_CTX_NB ? ZBLOCK_CTX_NB - 1 : tst))
			return 0;

		k = 0;
		if (sum - cnt >= MAX_P) {
			cnt = 0;
			do {
				sum = 0;
				k++;
				for (int i = 0; i < 16; i++) {
					int coef = tmp[i] >> (k + 1);
					sum += coef;
				}
			} while (sum >= MAX_P);
		}

		if (bitCodec.code(k == 0, tst >= ZK_CTX_NB ? ZBLOCK_CTX_NB + ZK_CTX_NB - 1 : ZBLOCK_CTX_NB + tst)) {
			unsigned short ones = 0;
			for (int i = 0, j = 0; i < 16; i++) {
				int coef = tmp[i] >> (k + 1);
				ones <<= 1;
				if (coef != 0) {
					ones |= 1;
					enu[j++] = coef - 1;
				}
			}
			sum_cnt_encode(sum, cnt, tst, pCodec);
			if (cnt < 16) pCodec->enumCode<16>(ones, cnt);
			pCodec->enumCode(enu, sum - cnt, cnt);
			for (int i = 0; i < 16; i++) {
				if (tmp[i] != 0)
					pCodec->bitsCode(tmp[i] & 1, 1);
			}
		} else {
			if (tst != 0) {
				int ctx = (k_avg[tst] >> (AVG_SHIFT - 1)) - 2, more = 0;
				if (ctx < 0) ctx = 0;

				sum -= 5;
				if (sum < 0) {
					more = -sum;
					sum = 0;
				}

				if (ctx < K_2_CTX + K_3_CTX + K_4_CTX) {
					if (k <= k_max[ctx]) {
						pCodec->huffCode(huff_X_enc[ctx][((k - 1) << 4) + sum]);
					} else {
						pCodec->huffCode(huff_X_enc[ctx][k_max[ctx] * 16 + sum]);
						for (uint i = k_max[ctx] + 1; i < k; i++)
							bitCodec.code(1, ZBLOCK_CTX_NB + ZK_CTX_NB + MORE_CTX_NB);
						bitCodec.code(0, ZBLOCK_CTX_NB + ZK_CTX_NB + MORE_CTX_NB);
					}
				} else {
					uint k_est = (k_avg[tst] + 16) >> (AVG_SHIFT - 1);
					ctx = (k_est & 1) + K_2_CTX + K_3_CTX + K_4_CTX;
					k_est >>= 1;
					if (k < k_est - 1 || k > k_est + 1) {
						if (k < k_est) {
							pCodec->huffCode(huff_X_enc[ctx][sum]);
						} else {
							pCodec->huffCode(huff_X_enc[ctx][4 * 16 + sum]);
						}
						for (int i = 2; i < abs(k - k_est); i++)
							bitCodec.code(1, ZBLOCK_CTX_NB + ZK_CTX_NB + MORE_CTX_NB);
						bitCodec.code(0, ZBLOCK_CTX_NB + ZK_CTX_NB + MORE_CTX_NB);
					} else {
						pCodec->huffCode(huff_X_enc[ctx][(k - k_est + 2) * 16 + sum]);
					}
				}

				if (sum == 0) {
					int i = 0;
					while (i < more)
						bitCodec.code(1, ZBLOCK_CTX_NB + ZK_CTX_NB + i++);
					if (more < 2)
						bitCodec.code(0, ZBLOCK_CTX_NB + ZK_CTX_NB + i);
				}
				sum += 5 - more;

				k_avg[tst] += (((int)k << AVG_SHIFT) - (int)k_avg[tst]) >> AVG_DECAY;
			} else {
				pCodec->tabooCode(k - 1);
				pCodec->maxCode(sum - 3, 17);
			}
			for (int i = 0; i < 16; i++)
				enu[i] = tmp[i] >> (k + 1);
			pCodec->enumCode(enu, sum, 16);
			uint mask = ((1 << k) - 1);
			for (int i = 0; i < 16; i++) {
				pCodec->bitsCode((tmp[i] >> 1) & mask, k);
				if (tmp[i] != 0)
					pCodec->bitsCode(tmp[i] & 1, 1);
			}
		}
	} else {
		if (!high_band && bitCodec.decode(tst >= ZBLOCK_CTX_NB ? ZBLOCK_CTX_NB - 1 : tst))
			return 0;
		if (bitCodec.decode(tst >= ZK_CTX_NB ? ZBLOCK_CTX_NB + ZK_CTX_NB - 1 : ZBLOCK_CTX_NB + tst)) {
			k = 0;
			sum_cnt_decode(sum, cnt, tst, pCodec);
			unsigned short ones = 0xFFFF;
			if (cnt < 16) ones = pCodec->enumDecode<16>(cnt);
			pCodec->enumDecode(enu, sum - cnt, cnt);
			for (int i = 0, j = 0; i < 16; i++) {
				tmp[i] = 0;
				if ((ones << i) & (1 << 15)) // FIXME maybe something simpler
					tmp[i] = ((enu[j++] + 1) << 1) | pCodec->bitsDecode(1);
			}
		} else {
			if (tst != 0) {
				int ctx = (k_avg[tst] >> (AVG_SHIFT - 1)) - 2;
				if (ctx < 0) ctx = 0;

				if (ctx < K_2_CTX + K_3_CTX + K_4_CTX) {
					int sym = pCodec->huffDecode(&huff_X_dec[ctx]);
					k = (sym >> 4) + 1;
					sum = sym & 0xF;
					if (k > k_max[ctx]) {
						while (bitCodec.decode(ZBLOCK_CTX_NB + ZK_CTX_NB + MORE_CTX_NB))
							k++;
					}
				} else {
					uint k_est = (k_avg[tst] + 16) >> (AVG_SHIFT - 1);
					ctx = (k_est & 1) + K_2_CTX + K_3_CTX + K_4_CTX;
					k_est >>= 1;
					int sym = pCodec->huffDecode(&huff_X_dec[ctx]);
					k = (sym >> 4) + k_est - 2;
					sum = sym & 0xF;
					if (k == k_est - 2) {
						while (bitCodec.decode(ZBLOCK_CTX_NB + ZK_CTX_NB + MORE_CTX_NB))
							k--;
					}
					if (k == k_est + 2) {
						while (bitCodec.decode(ZBLOCK_CTX_NB + ZK_CTX_NB + MORE_CTX_NB))
							k++;
					}
				}

				if (sum == 0) {
					int i = 0;
					while (sum > -2 && bitCodec.decode(ZBLOCK_CTX_NB + ZK_CTX_NB + i++))
						sum--;
				}

				sum += 5;

				k_avg[tst] += (((int)k << AVG_SHIFT) - (int)k_avg[tst]) >> AVG_DECAY;
			} else {
				k = pCodec->tabooDecode() + 1;
				sum = pCodec->maxDecode(17) + 3;
			}
			pCodec->enumDecode(enu, sum, 16);
			for (int i = 0; i < 16; i++) {
				tmp[i] = ((enu[i] << k) | pCodec->bitsDecode(k)) << 1;
				if (tmp[i] != 0)
					tmp[i] |= pCodec->bitsDecode(1);
			}
		}
		for (int j = 0, i = 0; j < 4; j++) {
			for (C * pEnd = pBlock + 4; pBlock < pEnd; pBlock++) {
				pBlock[0] = tmp[i++];
			}
			pBlock += stride - 4;
		}
	}

	return k;
}

#define TREE_CTX_NB	16

template <cmode mode, bool high_band, class C, class P>
	void CBandCodec::tree(CMuxCodec * pCodec)
{
	C * pCur1 = (C*) pBand;
	C * pCur2 = (C*) pBand + DimXAlign * (BLK_SIZE >> 1);
	int diff = DimXAlign * BLK_SIZE;
	P * pPar = 0;
	int diff_par = 0;

	if (pParent != 0) {
		pPar = (P*) pParent->pBand;
		diff_par = pParent->DimXAlign * (BLK_SIZE >> 1);
	}

	if (mode == decode) Clear(false);

	CBitCodec<TREE_CTX_NB> treeCodec(pCodec);
	CBitCodec<ZBLOCK_CTX_NB + ZK_CTX_NB + MORE_CTX_NB + 1> bitCodec(pCodec);

	// TODO : pb rate-distortion : c'est quoi cette courbe ?
	// TODO : utiliser le prédicteur comme un predicteur (coder l'erreur) ?
	// TODO : diminuer nb de contexts pour ajouter ctx variance
	// TODO : tester joindre les ctx k et 16-k -> semble pas possible à cause 0
	// TODO : tester block à 0/16 ? => puis arbre à 0 (ou != 0 ?)

	for( uint j = 0; j <= DimY - BLK_SIZE; j += BLK_SIZE){
		int i = 0, bs = BLK_SIZE;
// 		if (j & BLK_SIZE) {
// 			bs = -bs;
// 			i = (DimX & (-1 << BLK_PWR)) + bs;
// 		}
		for( ; (uint)i <= DimX - BLK_SIZE; i += bs){
			int ctx = 15, k = i >> 1;
			if (pPar) ctx = pPar[k];

			if (ctx == INSIGNIF_BLOCK) {
				pPar[k] = 0;
				if (!high_band)
					pCur1[i] = pCur1[i + (BLK_SIZE >> 1)] = pCur2[i] = pCur2[i + (BLK_SIZE >> 1)] = INSIGNIF_BLOCK;
			} else {
				if (pPar) ctx = maxLen<(BLK_SIZE >> 1)>(&pPar[k], pParent->DimXAlign);
				if ((high_band || ctx < 2) &&
					((mode == encode && treeCodec.code(pCur1[i] == INSIGNIF_BLOCK, ctx))
					|| (mode == decode && treeCodec.decode(ctx)))) {
					if (!high_band)
						pCur1[i] = pCur1[i + (BLK_SIZE >> 1)] = pCur2[i] = pCur2[i + (BLK_SIZE >> 1)] = INSIGNIF_BLOCK;
				} else {
					int est = 0, sum = 0;
					if (likely(i != 0)) {
						sum += (pCur1[-1 + i] >> 1) + (pCur1[DimXAlign - 1 + i] >> 1) + (pCur2[-1 + i] >> 1) + (pCur2[DimXAlign - 1 + i] >> 1);
						est++;
					}
					if (likely(j != 0)) {
						sum += (pCur1[-DimXAlign + i] >> 1) + (pCur1[-DimXAlign + 1 + i] >> 1) + (pCur1[-DimXAlign + 2 + i] >> 1) + (pCur1[-DimXAlign + 3 + i] >> 1);
						est++;
					}
					if (likely(est == 2))
						est = sum + 1;
					else if (est == 1)
						est = 2 * sum + 1;

					pCur1[i] &= ~INSIGNIF_BLOCK;

					block_enum<mode, high_band>(&pCur1[i], DimXAlign, pCodec, bitCodec, est);
				}
			}
		}
		pCur1 += diff;
		pCur2 += diff;
		pPar += diff_par;
	}
#ifdef GENERATE_HUFF_STATS
	if (high_band) {
		static int cnt = 0;
		if (cnt++ == 2) {
// 			printf("--> (more : %i)\n", more_bits);
			print_hist(huff_0_enc, K_0_CNT, K_0_CTX, "k_0_");
			print_hist(huff_X_enc, K_2_CNT, K_2_CTX, "k_2_");
			print_hist(&huff_X_enc[K_2_CTX], K_3_CNT, K_3_CTX, "k_3_");
			print_hist(&huff_X_enc[K_2_CTX + K_3_CTX], K_4_CNT, K_4_CTX, "k_4_");
			print_hist(&huff_X_enc[K_2_CTX + K_3_CTX + K_4_CTX], K_X_CNT, K_X_CTX, "k_X_");
// 			print_hist((uint *)k_est_cnt, 16, 64, "h_hist_");
// 			for (int i = 0; i < 64; i++) {
// 				int cnt = 0, sum = 0;
// 				for( int j = 0; j < 16; j++) {
// 					cnt += k_est_cnt[j][i];
// 					sum += k_est_cnt[j][i] * (j + 1);
// 				}
// 				int tmp = cnt >> 1;
// 				int j = 0;
// 				do {
// 					tmp -= k_est_cnt[j++][i];
// 				} while (tmp > 0);
// 				printf("%i\t", j);
// 				k_est_cnt[0][i] = cnt;
// 				k_est_cnt[1][i] = sum;
// 			}
// 			printf("\n");
// 			for (int i = 0; i < 64; i++)
// 				printf("%f\t", (float) k_est_cnt[1][i] / k_est_cnt[0][i]);
// 			printf("\n");
		}
	}
#endif
}

template void CBandCodec::tree<encode, true, short, short>(CMuxCodec * );
template void CBandCodec::tree<encode, false, short, short>(CMuxCodec * );
template void CBandCodec::tree<decode, true, short, short>(CMuxCodec * );
template void CBandCodec::tree<decode, false, short, short>(CMuxCodec * );

template void CBandCodec::tree<encode, true, short, int>(CMuxCodec * );
template void CBandCodec::tree<encode, false, short, int>(CMuxCodec * );
template void CBandCodec::tree<decode, true, short, int>(CMuxCodec * );
template void CBandCodec::tree<decode, false, short, int>(CMuxCodec * );

template void CBandCodec::tree<encode, true, int, int>(CMuxCodec * );
template void CBandCodec::tree<encode, false, int, int>(CMuxCodec * );
template void CBandCodec::tree<decode, true, int, int>(CMuxCodec * );
template void CBandCodec::tree<decode, false, int, int>(CMuxCodec * );
}
