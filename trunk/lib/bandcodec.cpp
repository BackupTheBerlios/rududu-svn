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

bool CBandCodec::checkBlock(short ** pCur, int i, int block_x, int block_y)
{
	short res = 0;
	for (int l = 0; l < block_y; l++)
		for (int k = i; k < i + block_x; k++)
			res |= pCur[l][k];
	return res == 0;
}

template <bool high_band, int block_size>
		void CBandCodec::buildTree(void)
{
	short * pCur[block_size] = {pBand};
	short * pChild[2] = {0, 0};
	unsigned int child_stride = 0;
	
	for (int i = 1; i < block_size; i++)
		pCur[i] = pCur[i - 1] + DimXAlign;

	if (! high_band) {
		pChild[0] = this->pChild->pBand;
		pChild[1] = pChild[0] + block_size * this->pChild->DimXAlign;
		child_stride = this->pChild->DimXAlign * 2 * block_size;
	}

	for( unsigned int j = 0; j < DimY; j += block_size){
		for( unsigned int i = 0; i < DimX; i += block_size){
			if (checkBlock(pCur, i, block_size, block_size) && (high_band ||
							(INSIGNIF_BLOCK == pChild[0][2*i] &&
							INSIGNIF_BLOCK == pChild[0][2*i + block_size] &&
							INSIGNIF_BLOCK == pChild[1][2*i] &&
							INSIGNIF_BLOCK == pChild[1][2*i + block_size])))
				pCur[0][i] = INSIGNIF_BLOCK;
		}
		for (int k = 0; k < block_size; k++)
			pCur[k] += DimXAlign * block_size;
		pChild[0] += child_stride;
		pChild[1] += child_stride;
	}

	if (pParent != 0)
		((CBandCodec*)pParent)->buildTree<false, block_size>();
}

template void CBandCodec::buildTree<true, 2>(void);

template <int block_size>
	int CBandCodec::maxLen(short * pBlock, int stride)
{
	if (block_size == 1)
		return bitlen(ABS(pBlock[0]));

	short max = 0, min = 0;
	for( int j = 0; j < block_size; j++){
		for( int i = 0; i < block_size; i++){
			max = MAX(max, pBlock[i]);
			min = MIN(min, pBlock[i]);
		}
		pBlock += stride;
	}
	min = ABS(min);
	max = MAX(max, min);
	return bitlen(max);
}

template <cmode mode, int block_size>
	void CBandCodec::block(short * pBlock, int stride, CMuxCodec * pCodec,
	                       CBitCodec & lenCodec, int max_len)
{
	for( int j = 0; j < block_size; j++){
		for( int i = 0; i < block_size; i++){
			if (mode == encode) {
				short tmp = s2u_(pBlock[i]);
				int len = bitlen(tmp >> 1), k;
				for(k = 0; k < len; k++) lenCodec.code0(k);
				if (k < max_len) lenCodec.code1(k);
				if (len != 0) pCodec->bitsCode(tmp & ((1 << len) - 1), len);
			} else {
				int len = 0;
				pBlock[i] = 0;
				while( len < max_len && lenCodec.decode(len) == 0 ) len++;
				if (len != 0) pBlock[i] = u2s_(pCodec->bitsDecode(len) | (1 << len));
			}
		}
		pBlock += stride;
	}
}

template <cmode mode, int block_size>
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
	short * pCur2 = pBand + DimXAlign * (block_size >> 1);
	int diff = DimXAlign * block_size;
	short * pPar = 0;
	int diff_par = 0;

	if (pParent != 0) {
		pPar = pParent->pBand;
		diff_par = pParent->DimXAlign * (block_size >> 1);
	}

	CBitCodec lenCodec(pCodec);
	CBitCodec treeCodec(pCodec);

	for( unsigned int j = 0; j < DimY; j += block_size){
		for( unsigned int i = 0, k = 0; i < DimX; i += block_size, k += block_size >> 1){
			int context = 0;
			if (pPar) context = pPar[k];

			if (context == INSIGNIF_BLOCK) {
				pPar[k] = 0;
				pCur1[i] = pCur1[i + (block_size >> 1)] = pCur2[i] = pCur2[i + (block_size >> 1)] = -(pChild != 0) & INSIGNIF_BLOCK;
				continue;
			}

			if (pPar) context = maxLen<block_size >> 1>(&pPar[k], pParent->DimXAlign);
			if (mode == encode) {
				if (pCur1[i] == INSIGNIF_BLOCK) {
					treeCodec.code1(context);
					pCur1[i] = pCur1[i + (block_size >> 1)] = pCur2[i] = pCur2[i + (block_size >> 1)] = -(pChild != 0) & INSIGNIF_BLOCK;
				} else {
					treeCodec.code0(context);
					block<mode, block_size>(&pCur1[i], DimXAlign, pCodec, lenCodec, bits);
				}
			} else {
				if (treeCodec.decode(context))
					pCur1[i] = pCur1[i + (block_size >> 1)] = pCur2[i] = pCur2[i + (block_size >> 1)] = -(pChild != 0) & INSIGNIF_BLOCK;
				else
					block<mode, block_size>(&pCur1[i], DimXAlign, pCodec, lenCodec, bits);
			}
		}
		pCur1 += diff;
		pCur2 += diff;
		pPar += diff_par;
	}
}

template void CBandCodec::tree<encode, 2>(CMuxCodec * );
template void CBandCodec::tree<decode, 2>(CMuxCodec * );

}
