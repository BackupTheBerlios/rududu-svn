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

#pragma once

#include "utils.h"

namespace rududu {

#define FREQ_POWER	12
#define FREQ_COUNT	(1 << FREQ_POWER)
#define FREQ_MASK	((1 << FREQ_POWER) - 1)
#define HALF_FREQ_COUNT	(1 << (FREQ_POWER - 1))	// FREQ_COUNT / 2

#define RANGE_BITS		12
#define MIN_RANGE		(1 << RANGE_BITS)

#define ROT_BUF_SIZE 	4
#define ROT_BUF_MASK	(ROT_BUF_SIZE - 1)

#define REG_SIZE (sizeof(unsigned int) * 8)

// enumerative coding parameters
#define MAX_Q 16 // max length
#define MAX_P 21 // max sum
#define MAX_VAL MAX_P // max value (in the vector)

// LUT size parameter, LUT size is 1 << LUT_DEPTH
#define LUT_DEPTH 4

/// Huffman table entry.
typedef struct {
	unsigned short code;
	unsigned char len;
	unsigned char value;
} sHuffSym;

/// Huffman LUT entry.
typedef struct {
	unsigned char len;
	unsigned char value;
} sHuffLut;

/// Type used for canonical huffman decoding
typedef struct {
	sHuffSym const * const table;
	unsigned char const * const sym;
	sHuffLut lut[1 << LUT_DEPTH];
} sHuffCan;

/**
 * This class is a carryless range coder whose output is multiplexed with the
 * bits sent to bitsCode(). The decoder do the demultiplexing when bits are
 * requested. This allow to use a range coder and to do fast bit input / output.
*/
class CMuxCodec
{
private:
	unsigned char *pStream;
	unsigned char *pInitStream;

	// variables for the range coder
	unsigned int range;
	unsigned int low;
	unsigned int code; // only for the decoder
	unsigned char *pLast[ROT_BUF_SIZE]; // only for the encoder
	unsigned int outCount; // only for the encoder

	// variables for bit output
	unsigned int nbBits;
	unsigned int buffer;
	unsigned char * pReserved;

	// Taboo coding
	unsigned int nbTaboo[REG_SIZE];
	unsigned int sumTaboo[REG_SIZE];
	unsigned int nTaboo;

	static const unsigned int nbFibo[32];
	static const unsigned short Cnk[MAX_Q >> 1][MAX_Q];
	static const unsigned char CnkLen[MAX_Q][MAX_Q >> 1];
	static const unsigned short CnkLost[MAX_Q][MAX_Q >> 1];
	static unsigned long G_p_q[MAX_P][MAX_Q]; // last column is not used (only for init)
	static unsigned char len[MAX_P]; // nb of bits to send
	static unsigned long lost[MAX_P]; // nb of lost index TODO : merge with CnkLost ?

	flatten fastcall void normalize_enc(void);
	flatten fastcall void normalize_dec(void);

	fastcall void emptyBuffer(void);
	template <bool end> void flushBuffer(void);
	fastcall void fillBuffer(const unsigned int length);

	static void initEnum(void);

public:
	CMuxCodec(unsigned char *pStream, unsigned short firstWord);
	CMuxCodec(unsigned char *pStream);
	void initCoder(unsigned short firstWord, unsigned char *pStream);
	void initDecoder(unsigned char *pStream);
	unsigned char * endCoding(void);
	unsigned int getSize(void);

	void golombCode(unsigned int nb, const int k);
	unsigned int golombDecode(const int k);
	void golombLinCode(unsigned int nb, int k, int m);
	unsigned int golombLinDecode(int k, int m);

	void fiboCode(unsigned int nb);
	unsigned int fiboDecode(void);

	void initTaboo(unsigned int k);
	void tabooCode(unsigned int nb);
	unsigned int tabooDecode(void);

	template <unsigned int n_max>
		flatten fastcall void enumCode(unsigned int bits, unsigned int k);
	void enumCode(unsigned int bits, unsigned int k, unsigned int n_max);
	template <unsigned int n_max>
		flatten fastcall unsigned int enumDecode(unsigned int k);
	unsigned int enumDecode(unsigned int k, unsigned int n_max);
	void enumCode(short * values, int sum, int n_val);
	void enumDecode(short * values, int sum, int n_val);

	fastcall void maxCode(unsigned int value, unsigned int max);
	fastcall unsigned int maxDecode(unsigned int max);

	void inline encode(const unsigned int lowFreq, const unsigned int topFreq)
	{
		ASSERT(lowFreq != topFreq);
		if (unlikely(range <= MIN_RANGE))
			normalize_enc();
		unsigned int tmp = range * lowFreq;
		low += tmp >> FREQ_POWER;
		range = (range * (topFreq - lowFreq) + (tmp & FREQ_MASK)) >> FREQ_POWER;
	}

	void inline code0(const unsigned int topFreq)
	{
		ASSERT(0 < topFreq && topFreq < FREQ_COUNT);
		if (unlikely(range <= MIN_RANGE))
			normalize_enc();
		range = (range * topFreq) >> FREQ_POWER;
	}

	void inline code1(const unsigned int lowFreq)
	{
		ASSERT(0 < lowFreq && lowFreq < FREQ_COUNT);
		if (unlikely(range <= MIN_RANGE))
			normalize_enc();
		const unsigned int tmp = (range * lowFreq) >> FREQ_POWER;
		low += tmp;
		range -= tmp;
	}

	void inline codeBin(const unsigned int freq, const int bit)
	{
		ASSERT(0 < freq && freq < FREQ_COUNT);
		if (unlikely(range <= MIN_RANGE))
			normalize_enc();
		const unsigned int tmp = (range * freq) >> FREQ_POWER;
		low += tmp & -bit;
		range = tmp + ((range - 2 * tmp) & -bit);
	}

	void inline codeSkew(const unsigned int shift, const unsigned int bit)
	{
		ASSERT(0 < shift && shift < RANGE_BITS);
		if (unlikely(range <= MIN_RANGE))
			normalize_enc();
		const unsigned int tmp = range - (range >> shift);
		low += tmp & -bit;
		range = tmp + ((range - 2 * tmp) & -bit);
	}

	void inline codeSkew0(const unsigned int shift)
	{
		ASSERT(0 < shift && shift < RANGE_BITS);
		if (unlikely(range <= MIN_RANGE))
			normalize_enc();
		range -= range >> shift;
	}

	void inline codeSkew1(const unsigned int shift)
	{
		ASSERT(0 < shift && shift < RANGE_BITS);
		if (unlikely(range <= MIN_RANGE))
			normalize_enc();
		const unsigned int tmp = range - (range >> shift);
		low += tmp;
		range -= tmp;
	}

	unsigned int inline decode(const unsigned short * pFreqs)
	{
		if (unlikely(range <= MIN_RANGE))
			normalize_dec();
		unsigned short freq = ((low << FREQ_POWER) + FREQ_MASK) / range;
		unsigned int i = 1;
		while(freq >= pFreqs[i]) i++;
		i--;

		unsigned int tmp = range * pFreqs[i];
		low -= tmp >> FREQ_POWER;
		range = (range * (pFreqs[i+1] - pFreqs[i]) + (tmp & FREQ_MASK)) >> FREQ_POWER;
		return i;
	}

	unsigned int inline getBit(const unsigned int freq)
	{
		if (unlikely(range <= MIN_RANGE)) normalize_dec();
		const unsigned int tmp = (range * freq) >> FREQ_POWER;
		const int tst = (low < tmp) - 1;
		low -= tmp & tst;
		range = tmp + ((range - 2*tmp) & tst);
		return -tst;
	}

	unsigned int inline decSkew(const unsigned int shift)
	{
		if (unlikely(range <= MIN_RANGE)) normalize_dec();
		const unsigned int tmp = range - (range >> shift);
		const int tst = (low < tmp) - 1;
		low -= tmp & tst;
		range = tmp + ((range - 2*tmp) & tst);
		return -tst;
	}

	void inline bitsCode(unsigned int bits, unsigned int length)
	{
		if (unlikely(nbBits + length >= REG_SIZE)) {
			length += nbBits - REG_SIZE;
			buffer <<= 1;
			buffer = (buffer << (REG_SIZE - 1 - nbBits)) | (bits >> length);
			nbBits = REG_SIZE;
			emptyBuffer();
		}
		buffer = (buffer << length) | bits;
		nbBits += length;
	}

	uint inline bitsDecode(unsigned int length)
	{
		if (nbBits < length) {
			uint ret = buffer & ((1 << nbBits) - 1);
			length -= nbBits;
			nbBits = 0;
			fillBuffer(length);
			nbBits -= length;
			return (ret << length) | ((buffer >> nbBits) & (-1u >> (-length & 31)));
		}
		nbBits -= length;
		return (buffer >> nbBits) & (-1u >> (-length & 31));
	}

	void inline huffCode(const sHuffSym sym)
	{
		ASSERT(sym.len > 0);
		bitsCode(sym.code, sym.len);
	}

	// canonical huffman codes decoding
	unsigned int inline huffDecode(const sHuffSym * huffTable)
	{
		unsigned short code = (unsigned short)((((buffer << 16) | (pStream[0] << 8) | pStream[1]) >> nbBits) & 0xFFFF);

		while (code < huffTable->code) huffTable++;

		pStream -= (int)(nbBits - huffTable->len) >> 3;
		if (nbBits < huffTable->len) buffer = pStream[-1];
		nbBits = (nbBits - huffTable->len) & 0x07;

		return (huffTable->value - (code >> (16 - huffTable->len))) & 0xFF;
	}

	// canonical huffman codes decoding with LUT
	unsigned int inline huffDecode(const sHuffCan * can)
	{
		unsigned short code = (unsigned short)((((buffer << 16) | (pStream[0] << 8) | pStream[1]) >> nbBits) & 0xFFFF);

		sHuffLut tmp = can->lut[code >> (16 - LUT_DEPTH)];
		if (likely(tmp.len != 0)) {
			pStream -= (int)(nbBits - tmp.len) >> 3;
			if (nbBits < tmp.len) buffer = pStream[-1];
			nbBits = (nbBits - tmp.len) & 0x07;
			return tmp.value;
		}

		const sHuffSym * huffTable = can->table + tmp.value;
		while (code < huffTable->code) huffTable++;

		pStream -= (int)(nbBits - huffTable->len) >> 3;
		if (nbBits < huffTable->len) buffer = pStream[-1];
		nbBits = (nbBits - huffTable->len) & 0x07;

		return can->sym[(huffTable->value - (code >> (16 - huffTable->len))) & 0xFF];
	}
};

}

