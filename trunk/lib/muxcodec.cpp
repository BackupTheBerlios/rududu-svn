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

#include "muxcodec.h"
#include "utils.h"

namespace rududu {

CMuxCodec::CMuxCodec(unsigned char *pStream, unsigned short firstWord){
	initCoder(firstWord, pStream);
	initTaboo(2);
}

CMuxCodec::CMuxCodec(unsigned char *pStream){
	initDecoder(pStream);
	initTaboo(2);
}

void CMuxCodec::initCoder(unsigned short firstWord = 0,
						  unsigned char *pOutStream = 0){
	low = firstWord << 16;
	range = MIN_RANGE << 4;
	outCount = 0;
	nbBits = 0;
	pReserved = 0;
	if (pOutStream != 0){
		pStream = pOutStream + 4;
		pInitStream = pOutStream + 2;
		for( int i = 0; i < 4; i++)
			pLast[i] = pOutStream + i;
	}
}

void CMuxCodec::initDecoder(unsigned char *pInStream)
{
	range = MIN_RANGE << 4;
	nbBits = 0;
	if (pInStream){
		pInitStream = pInStream + 2;
		pStream = pInStream + 2;
		code = low = (pStream[0] << 8) | pStream[1];
		pStream += 2;
	}
}

void CMuxCodec::normalize_enc(void)
{
	flushBuffer<false>();
	do{
		*pLast[outCount++ & ROT_BUF_MASK] = low >> 24;
		if (((low + range - 1) ^ low) >= 0x01000000)
			range = -low & (MIN_RANGE - 1);
		pLast[(outCount + 3) & ROT_BUF_MASK] = pStream++;
		range <<= 8;
		low <<= 8;
	} while (range <= MIN_RANGE);
}

void CMuxCodec::normalize_dec(void){
	do{
		if (((code - low + range - 1) ^ (code - low)) >= 0x01000000)
			range = (low - code) & (MIN_RANGE - 1);
		low = (low << 8) | (*pStream);
		code = (code << 8) | (*pStream);
		pStream++;
		range <<= 8;
	} while (range <= MIN_RANGE);
}

unsigned char * CMuxCodec::endCoding(void)
{
	flushBuffer<true>();

	if (range <= MIN_RANGE)
		normalize_enc();

	int last_out = 0x200 | 'W';
	if ((low & (MIN_RANGE - 1)) > (last_out & (MIN_RANGE - 1)))
		low += MIN_RANGE;

	low = (low & ~(MIN_RANGE - 1)) | (last_out & (MIN_RANGE - 1));

	*pLast[outCount & ROT_BUF_MASK] = low >> 24;
	*pLast[(outCount + 1) & ROT_BUF_MASK] = low >> 16;
	*pLast[(outCount + 2) & ROT_BUF_MASK] = low >> 8;
	*pLast[(outCount + 3) & ROT_BUF_MASK] = low;

	return pStream;
}

unsigned int CMuxCodec::getSize(void)
{
	return pStream - pInitStream;
}

void CMuxCodec::initTaboo(unsigned int k)
{
	unsigned int i;
	nbTaboo[0] = 1;
	nTaboo = k;
	for( i = 1; i < k; i++)
		nbTaboo[i] = 1 << (i - 1);
	for( i = k; i < REG_SIZE; i++) {
		unsigned int j, accu = nbTaboo[i - k];
		for( j = i - k + 1; j < i; j++)
			accu += nbTaboo[j];
		nbTaboo[i] = accu;
	}
	sumTaboo[0] = nbTaboo[0];
	for( i = 1; i < REG_SIZE; i++)
		sumTaboo[i] = sumTaboo[i - 1] + nbTaboo[i];
}

const unsigned int CMuxCodec::nbFibo[32] =
{
	1, 2, 3, 5, 8, 13, 21, 34, 55, 89, 144, 233, 377, 610, 987, 1597, 2584,
	4181, 6765, 10946, 17711, 28657, 46368, 75025, 121393, 196418, 317811,
	514229, 832040, 1346269, 2178309, 3524578
};

void CMuxCodec::fiboCode(unsigned int nb)
{
	if ( nbBits >= 8 )
		emptyBuffer();

	int i = 1, t;
	for( ; nbFibo[i] <= nb; i++){
	}
	int l = i + 1;
	i--;
	nb -= nbFibo[i];

	register unsigned int r = 0xC0000000;
	t = i;
	i--;
	while( nb > 0 ){
		i--;
		if (nbFibo[i] <= nb){
			nb -= nbFibo[i];
			r >>= t-i;
			r |= 0x80000000;
			t = i;
			i--;
		}
	}
	buffer = (buffer << l) | (r >> (33 - l + i));
	nbBits += l;
}

unsigned int CMuxCodec::fiboDecode(void)
{
	if ( nbBits < 2 )
		fillBuffer(2);

	int l = 2;
	unsigned int t = 3 << (nbBits - l);
	while( (buffer & t) != t ){
		l++;
		if (l > (int)nbBits){
			fillBuffer(l);
			t <<= 8;
		}
		t >>= 1;
	}
	nbBits -= l;
	l -= 2;
	unsigned int nb = nbFibo[l];
	t = 1 << (nbBits + 2);
	l--;
	while( l > 0 ){
		l--;
		t <<= 1;
		if (buffer & t){
			nb += nbFibo[l];
			t <<= 1;
			l--;
		}
	}
	return nb;
}

/**
 * This is an implementation of taboo codes as described by Steven Pigeon in :
 * http://www.iro.umontreal.ca/~brassard/SEMINAIRES/taboo.ps
 * or for french speaking people in his phd thesis :
 * http://www.stevenpigeon.com/Publications/publications/phd.pdf
 *
 * the taboo length is set in initTaboo() which have to be called once before
 * using tabooCode() or tabooDecode()
 *
 * @param nb number >= 0 to encode
 */
void CMuxCodec::tabooCode(unsigned int nb)
{
	int i = 0, l;
	unsigned int r = 0;

	while (sumTaboo[i] <= nb) i++;

	if (i == 0) {
		bitsCode(0, nTaboo);
		return;
	}

	l = i;
	i--;
	nb -= sumTaboo[i];

	while (i > (int)nTaboo) {
		unsigned int k = i - nTaboo + 1, cnt = nbTaboo[k], j = 0;
		while (nb >= cnt)
			cnt += nbTaboo[k + ++j];
		nb -= cnt - nbTaboo[k + j];
		j = nTaboo - j;
		r = (r << j) | 1;
		i -= j;
	}

	if (i == (int)nTaboo) nb++;

	r = ((((r << i) | (nb & ((1 << i) - 1))) << 1) | 1) << nTaboo;
	bitsCode(r, l + nTaboo);
}

unsigned int CMuxCodec::tabooDecode(void)
{
	int i = 0, l = nTaboo;
	unsigned int nb = 0;

	if ( nbBits < nTaboo ) fillBuffer(nTaboo);

	unsigned int t = ((1 << nTaboo) - 1) << (nbBits - nTaboo);
	while( (~buffer & t) != t ){
		l++;
		if (l > (int)nbBits){
			fillBuffer(l);
			t <<= 8;
		}
		t >>= 1;
	}
	nbBits -= l;

	unsigned int cd = buffer >> (nbBits + nTaboo + 1);

	i = l - nTaboo;

	if (i > 0) {
		i--;
		nb += sumTaboo[i];
	}

	while (i > (int)nTaboo) {
		unsigned int j = 1;
		while (((cd >> (i - j)) & 1) == 0) j++;
		nb += sumTaboo[i - j] - sumTaboo[i - nTaboo];
		i -= j;
	}

	if (i == (int)nTaboo) nb -= 1;
	nb += cd & ((1 << i) - 1);

	return nb;
}

const unsigned short CMuxCodec::Cnk[8][16] =
{
	{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
	{0, 0, 1, 3, 6, 10, 15, 21, 28, 36, 45, 55, 66, 78, 91, 105},
	{0, 0, 0, 1, 4, 10, 20, 35, 56, 84, 120, 165, 220, 286, 364, 455},
	{0, 0, 0, 0, 1, 5, 15, 35, 70, 126, 210, 330, 495, 715, 1001, 1365},
	{0, 0, 0, 0, 0, 1, 6, 21, 56, 126, 252, 462, 792, 1287, 2002, 3003},
	{0, 0, 0, 0, 0, 0, 1, 7, 28, 84, 210, 462, 924, 1716, 3003, 5005},
	{0, 0, 0, 0, 0, 0, 0, 1, 8, 36, 120, 330, 792, 1716, 3432, 6435},
	{0, 0, 0, 0, 0, 0, 0, 0, 1, 9, 45, 165, 495, 1287, 3003, 6435}
};

const unsigned char CMuxCodec::CnkLen[16][8] =
{
	{0, 0, 0, 0, 0, 0, 0, 0},
	{1, 0, 0, 0, 0, 0, 0, 0},
	{2, 2, 0, 0, 0, 0, 0, 0},
	{2, 3, 2, 0, 0, 0, 0, 0},
	{3, 4, 4, 3, 0, 0, 0, 0},
	{3, 4, 5, 4, 3, 0, 0, 0},
	{3, 5, 6, 6, 5, 3, 0, 0},
	{3, 5, 6, 7, 6, 5, 3, 0},
	{4, 6, 7, 7, 7, 7, 6, 4},
	{4, 6, 7, 8, 8, 8, 7, 6},
	{4, 6, 8, 9, 9, 9, 9, 8},
	{4, 7, 8, 9, 10, 10, 10, 9},
	{4, 7, 9, 10, 11, 11, 11, 11},
	{4, 7, 9, 10, 11, 12, 12, 12},
	{4, 7, 9, 11, 12, 13, 13, 13},
	{4, 7, 10, 11, 13, 13, 14, 14}
};

const unsigned short CMuxCodec::CnkLost[16][8] =
{
	{0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0},
	{1, 1, 0, 0, 0, 0, 0, 0},
	{0, 2, 0, 0, 0, 0, 0, 0},
	{3, 6, 6, 3, 0, 0, 0, 0},
	{2, 1, 12, 1, 2, 0, 0, 0},
	{1, 11, 29, 29, 11, 1, 0, 0},
	{0, 4, 8, 58, 8, 4, 0, 0},
	{7, 28, 44, 2, 2, 44, 28, 7},
	{6, 19, 8, 46, 4, 46, 8, 19},
	{5, 9, 91, 182, 50, 50, 182, 91},
	{4, 62, 36, 17, 232, 100, 232, 17},
	{3, 50, 226, 309, 761, 332, 332, 761},
	{2, 37, 148, 23, 46, 1093, 664, 1093},
	{1, 23, 57, 683, 1093, 3187, 1757, 1757},
	{0, 8, 464, 228, 3824, 184, 4944, 3514}
};

/**
 * Codes upto 16 bits using enumerative coding
 * Be careful : 0 and n_max are not allowed for k
 * @param n_max number of bits to code
 * @param bits bits to code
 * @param k number of bits set
 */
template <unsigned int n_max>
	void CMuxCodec::enumCode(unsigned int bits, unsigned int k)
{
	unsigned int code = 0;
	const unsigned short * C = Cnk[0];
	unsigned int n = 0;

	if (k > ((n_max + 1) >> 1)) {
		k = n_max - k;
		bits ^= (1 << n_max) - 1;
	}

	do {
		if (bits & 1) {
			code += C[n];
			C += 16;
		}
		n++;
		bits >>= 1;
	} while(bits != 0);

	if (code < CnkLost[n_max - 1][k - 1])
		bitsCode(code, CnkLen[n_max - 1][k - 1] - 1);
	else
		bitsCode(code + CnkLost[n_max - 1][k - 1], CnkLen[n_max - 1][k - 1]);
}

template void CMuxCodec::enumCode<16>(unsigned int, unsigned int);

/**
 * Decode upto 16 bits using enumerative coding
 * @param n_max number of bits to decode
 * @param k number of bits set
 * @return decoded bits
 */
template <unsigned int n_max>
	unsigned int CMuxCodec::enumDecode(unsigned int k)
{
	unsigned int n = n_max - 1, bits = 0;

	if (k > ((n_max + 1) >> 1)) {
		k = n_max - k;
		bits ^= (1 << n_max) - 1;
	}

	const unsigned short * C = Cnk[k - 1];
	unsigned int code = bitsDecode(CnkLen[n_max - 1][k - 1] - 1);
	if (code >= CnkLost[n_max - 1][k - 1])
		code = ((code << 1) | bitsDecode(1)) - CnkLost[n_max - 1][k - 1];

	do {
		if (code >= C[n]) {
			bits ^= 1 << n;
			code -= C[n];
			C -= 16;
		}
		n--;
	} while(C >= Cnk[0]);

	return bits;
}

template unsigned int CMuxCodec::enumDecode<16>(unsigned int);

void CMuxCodec::golombCode(unsigned int nb, const int k)
{
	if (k < 0) {
		for( ; nb > 0; nb--)
			codeSkew1(1 - k);
		codeSkew0(1 - k);
	} else {
		unsigned int l = (nb >> k) + 1;
		nb &= (1 << k) - 1;

		while ((int)l > (31 - (int)nbBits)){
			if (31 - (int)nbBits >= 0) {
				buffer <<= 31 - nbBits;
				l -= 31 - nbBits;
				nbBits = 31;
			}
			emptyBuffer();
		}

		buffer <<= l;
		buffer |= 1;
		nbBits += l;

		bitsCode(nb, k);
	}
}

unsigned int CMuxCodec::golombDecode(const int k)
{
	if (k < 0) {
		unsigned int nb = 0;
		while (decSkew(1 - k))
			nb++;
		return nb;
	} else {
		unsigned int l = 0;

		while(0 == (buffer & ((1 << nbBits) - 1))) {
			l += nbBits;
			nbBits = 0;
			fillBuffer(1);
		}

		while( (buffer & (1 << --nbBits)) == 0 )
			l++;

		unsigned int nb = (l << k) | bitsDecode(k);
		return nb;
	}
}

void CMuxCodec::golombLinCode(unsigned int nb, int k, int m)
{
	unsigned int l = 1;

	while( nb >= (1u << (k + m)) ){
		l += 1u << m;
		nb -= 1u << (k + m);
		k++;
	}

	l += nb >> k;
	nb &= (1 << k) - 1;

	while ((int)l > (31 - (int)nbBits)){
		if (31 - (int)nbBits >= 0) {
			buffer <<= 31 - nbBits;
			l -= 31 - nbBits;
			nbBits = 31;
		}
		emptyBuffer();
	}

	buffer <<= l;
	buffer |= 1;
	nbBits += l;

	bitsCode(nb, k);
}

unsigned int CMuxCodec::golombLinDecode(int k, int m)
{
	unsigned int l = 0;

	while(0 == (buffer & ((1 << nbBits) - 1))) {
		l += nbBits;
		nbBits = 0;
		fillBuffer(1);
	}

	while( (buffer & (1 << --nbBits)) == 0 )
		l++;

	unsigned int nb = ((1 << (l >> m)) - 1) << k;
	k += l >> m;
	l &= (1 << m) - 1;

	nb += (l << k) | bitsDecode(k);
	return nb;
}

void CMuxCodec::maxCode(unsigned int value, unsigned int max)
{
	unsigned int len = bitlen(max);
	unsigned int lost = (1 << len) - max - 1;
	if (value < lost)
		bitsCode(value, len - 1);
	else
		bitsCode(value + lost, len);
}

unsigned int CMuxCodec::maxDecode(unsigned int max)
{
	unsigned int value = 0;
	unsigned int len = bitlen(max);
	unsigned int lost = (1 << len) - max - 1;
	if ( len > 1) value = bitsDecode(len - 1);
	if (value >= lost) value = ((value << 1) | bitsDecode(1)) - lost;
	return value;
}

void CMuxCodec::emptyBuffer(void)
{
	do {
		nbBits -= 8;
		if (pReserved == 0) {
			*pStream = (unsigned char) (buffer >> nbBits);
			pStream++;
		} else {
			*pReserved = (unsigned char) (buffer >> nbBits);
   			pReserved = 0;
		}
	} while( nbBits >= 8 );
}

template <bool end>
void CMuxCodec::flushBuffer(void)
{
	if (nbBits >= 8)
		emptyBuffer();
	if (nbBits > 0) {
		if (end) {
			if (pReserved == 0) {
				*pStream = (unsigned char) (buffer << (8-nbBits));
				pStream++;
			} else {
				*pReserved = (unsigned char) (buffer << (8-nbBits));
				pReserved = 0;
			}
			nbBits = 0;
		} else if (pReserved == 0) {
			pReserved = pStream;
			pStream++;
		}
	}
}

void CMuxCodec::fillBuffer(const unsigned int length)
{
	do {
		nbBits += 8;
		buffer = (buffer << 8) | pStream[0];
		pStream++;
	} while( nbBits < length );
}

}
