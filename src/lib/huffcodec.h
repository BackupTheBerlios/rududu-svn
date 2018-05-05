/***************************************************************************
 *   Copyright (C) 2007 by Nicolas Botti   *
 *   rududu@laposte.net   *
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
#include "muxcodec.h"

namespace rududu {

// maximum size of a huffman code in bits
#define MAX_HUFF_LEN	16
// maximum huffman table size
#define MAX_HUFF_SYM	256
#define UPDATE_STEP_MIN	128u
#define UPDATE_STEP_MAX	2048u
#define UPDATE_THRES	(1u << 14)

typedef struct {
	signed char diff;
	unsigned char len;
} sHuffRL;

class CHuffCodec{
public:
	CHuffCodec(cmode mode, const sHuffRL * pInitTable, unsigned int n);

    ~CHuffCodec();

	static void make_huffman(sHuffSym * sym, int n);
	static void init_lut(sHuffCan * data, const int bits);

	static void print(sHuffSym * sym, int n, int print_type, char * name);
	void print(int print_type, char * name);

	unsigned int nbSym;

private:

	char * pData;
	sHuffSym * pSym;
	unsigned char * pSymLUT;
	unsigned short * pFreq;
	unsigned int count;
	unsigned int update_step;

	void init(const sHuffRL * pInitTable);
	void update_code(void);

	static void make_codes(sHuffSym * sym, int n);
	static void make_len(sHuffSym * sym, int n);

	static int comp_freq(const sHuffSym * sym1, const sHuffSym * sym2);
	static int comp_sym(const sHuffSym * sym1, const sHuffSym * sym2);
	static int comp_len(const sHuffSym * sym1, const sHuffSym * sym2);

	static void RL2len(const sHuffRL * pRL, sHuffSym * pHuff, int n);
	static int len2RL(sHuffRL * pRL, const sHuffSym * pHuff, int n);
	static int enc2dec(sHuffSym * sym, sHuffSym * outSym, unsigned char * pSymLUT, int n);

public:

	inline void code(unsigned int sym, CMuxCodec * codec)
	{
		if (count >= UPDATE_THRES)
			update_code();
		codec->bitsCode(pSym[sym].code, pSym[sym].len);
		pFreq[sym] += update_step;
		count += update_step;
	}

	inline unsigned int decode(CMuxCodec * codec)
	{
		if (count >= UPDATE_THRES)
			update_code();
		unsigned int sym = pSymLUT[codec->huffDecode(pSym)];
		pFreq[sym] += update_step;
		count += update_step;
		return sym;
	}
};

}

