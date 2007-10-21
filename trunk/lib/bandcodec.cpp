/***************************************************************************
 *   Copyright (C) 2007 by Nicolas Botti                                   *
 *   <rududu@laposte.net>                                                  *
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

#include "bandcodec.h"
#include "bitcodec.h"

namespace rududu {

CBandCodec::CBandCodec()
 : CBand()
{
}


CBandCodec::~CBandCodec()
{
}

template <cmode mode>
		void CBandCodec::pred(CMuxCodec * pCodec)
{
	short * pCur = pBand;
	int k = 6;
	const int stride = DimXAlign;

	if (mode == encode)
		pCodec->tabooCode(s2u(pCur[0]));
	else
		pCur[0] = u2s(pCodec->tabooDecode());

	for( unsigned int i = 1; i < DimX; i++){
		if (mode == encode)
			pCodec->golombCode(s2u(pCur[i] - pCur[i - 1]), k);
		else
			pCur[i] = pCur[i - 1] + u2s(pCodec->golombDecode(k));
	}
	pCur += stride;

	for( unsigned int j = 1; j < DimY; j++){
		if (mode == encode)
			pCodec->golombCode(s2u(pCur[0] - pCur[-stride]), k);
		else
			pCur[0] = pCur[-stride] + u2s(pCodec->golombDecode(k));

		for( unsigned int i = 1; i < DimX; i++){
			int var = ABS(pCur[i - 1] - pCur[i - 1 - stride]) +
					ABS(pCur[i - stride] - pCur[i - 1 - stride]);
			var -= var >> 2;
			if (var >= 32)
				var = k;
			else
				var = log_int[var];
			if (mode == encode) {
				int pred = pCur[i] - pCur[i - 1] - pCur[i - stride] +
						pCur[i - 1 - stride];
				pCodec->golombCode(s2u(pred), var);
			} else
				pCur[i] = pCur[i - 1] + pCur[i - stride] -
						pCur[i - 1 - stride] + u2s(pCodec->golombDecode(var));
		}
		pCur += stride;
	}
}

template void CBandCodec::pred<encode>(CMuxCodec *);
template void CBandCodec::pred<decode>(CMuxCodec *);

#define INSIGNIF_BLOCK -0x8000

template <bool high_band>
		void CBandCodec::buildTree(void)
{
	short * pCur1 = pBand;
	short * pCur2 = pBand + DimXAlign;
	short * pChild1 = 0, * pChild2 = 0;
	unsigned int child_stride = 0;

	if (this->pChild) {
		pChild1 = this->pChild->pBand;
		pChild2 = pChild1 + 2 * this->pChild->DimXAlign;
		child_stride = this->pChild->DimXAlign * 4;
	}

	for( unsigned int j = 0; j < DimY; j += 2){
		for( unsigned int i = 0; i < DimX; i += 2){
			if (high_band) {
				if (0 == (pCur1[i] | pCur1[i + 1] | pCur2[i] | pCur2[i + 1]))
					pCur1[i] = INSIGNIF_BLOCK;
			} else {
				if (0 == (pCur1[i] | pCur1[i + 1] | pCur2[i] | pCur2[i + 1]) &&
								INSIGNIF_BLOCK == pChild1[2*i] &&
								INSIGNIF_BLOCK == pChild1[2*i + 2] &&
								INSIGNIF_BLOCK == pChild2[2*i] &&
								INSIGNIF_BLOCK == pChild2[2*i + 2])
					pCur1[i] = INSIGNIF_BLOCK;
			}
		}
		pCur1 += DimXAlign * 2;
		pCur2 += DimXAlign * 2;
		pChild1 += child_stride;
		pChild2 += child_stride;
	}

	if (pParent != 0)
		((CBandCodec*)pParent)->buildTree<false>();
}

template void CBandCodec::buildTree<true>(void);

#define CODE_COEF(coef) { \
		short tmp = s2u_(coef); \
		int len = bitlen(tmp >> 1), k; \
		for(k = 0; k < len; k++) lenCodec.code0(k); \
		if (k < bits) lenCodec.code1(k); \
		if (len != 0) pCodec->bitsCode(tmp & ((1 << len) - 1), len); \
	}

#define DECODE_COEF(coef) \
	len = 0; coef = 0; \
	while( len < bits && lenCodec.decode(len) == 0 ) len++; \
	if (len != 0) coef = u2s_(pCodec->bitsDecode(len) | (1 << len));

template <cmode mode>
		void CBandCodec::tree(CMuxCodec * pCodec)
{
	int bits;
	if (mode == encode) {
		bits = MAX(Max, -Min);
		bits = bitlen((unsigned int) bits);
		pCodec->tabooCode(bits);
	} else
		bits = pCodec->tabooDecode();

	short * pCur1 = pBand;
	short * pCur2 = pBand + DimXAlign;
	short * pPar = 0;
	int diff_par = 0;
	int diff = DimXAlign << 1;

	if (pParent != 0) {
		pPar = pParent->pBand;
		diff_par = pParent->DimXAlign;
	}

	CBitCodec lenCodec(pCodec);
	CBitCodec treeCodec(pCodec);

	for( unsigned int j = 0; j < DimY; j += 2){
		for( unsigned int i = 0; i < (DimX >> 1); i++){
			int context = 0;
			if (pPar) context = pPar[i];

			if (context == INSIGNIF_BLOCK) {
				pPar[i] = 0;
				pCur1[2*i] = pCur1[2*i + 1] = pCur2[2*i] = pCur2[2*i + 1] = -(pChild != 0) & INSIGNIF_BLOCK;
				continue;
			}

			if (mode == encode) {
				if (pCur1[i*2] == INSIGNIF_BLOCK) {
					treeCodec.code1(bitlen(s2u_(context) >> 1));
					pCur1[2*i] = pCur1[2*i + 1] = pCur2[2*i] = pCur2[2*i + 1] = -(pChild != 0) & INSIGNIF_BLOCK;
				} else {
					treeCodec.code0(bitlen(s2u_(context) >> 1));
					CODE_COEF(pCur1[i*2]);
					CODE_COEF(pCur1[i*2 + 1]);
					CODE_COEF(pCur2[i*2]);
					CODE_COEF(pCur2[i*2 + 1]);
				}
			} else {
				context = bitlen(s2u_(context) >> 1);
				if (treeCodec.decode(context)) {
					pCur1[2*i] = pCur1[2*i + 1] = pCur2[2*i] = pCur2[2*i + 1] = -(pChild != 0) & INSIGNIF_BLOCK;
				} else {
					int len;
					DECODE_COEF(pCur1[i*2]);
					DECODE_COEF(pCur1[i*2 + 1]);
					DECODE_COEF(pCur2[i*2]);
					DECODE_COEF(pCur2[i*2 + 1]);
				}
			}
		}
		pCur1 += diff;
		pCur2 += diff;
		pPar += diff_par;
	}
}

template void CBandCodec::tree<encode>(CMuxCodec * );
template void CBandCodec::tree<decode>(CMuxCodec * );

}
