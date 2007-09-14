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

#pragma once

#include "utils.h"
#include "muxcodec.h"

namespace rududu {

#define MAX_HUFF_SYM	256 // maximum huffman table size

typedef struct {
	signed char diff;
	unsigned char len;
} sHuffRL;

class CHuffCodec{
public:
    CHuffCodec(cmode mode, unsigned int n);

    ~CHuffCodec();

	static void make_huffman(sHuffSym * sym, int n);

	void init(sHuffSym * pInitTable);

private:

	char * pData;
	unsigned int nbSym;
	sHuffSym * pSym;
	unsigned char * pSymLUT;
	unsigned short * pFreq;

	void update_code(sHuffSym * sym);

	static void print(sHuffSym * sym, int n, int print_type, int offset);
	static void make_codes(sHuffSym * sym, int n);
	static void make_len(sHuffSym * sym, int n);

	static int comp_freq(const sHuffSym * sym1, const sHuffSym * sym2);
	static int comp_sym(const sHuffSym * sym1, const sHuffSym * sym2);

	static void RL2len(const sHuffRL * pRL, sHuffSym * pHuff, int n);
	static int len2RL(sHuffRL * pRL, const sHuffSym * pHuff, int n);
	static int enc2dec(sHuffSym * sym, sHuffSym * outSym, unsigned char * pSymLUT, int n);
};

}

