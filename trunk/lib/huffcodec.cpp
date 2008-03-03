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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "huffcodec.h"

namespace rududu {

CHuffCodec::CHuffCodec(cmode mode, const sHuffRL * pInitTable, unsigned int n) :
	nbSym(n),
	count(0),
	update_step(UPDATE_STEP_MAX)
{
	if (mode == encode) {
		pData = new char[(sizeof(sHuffSym) + sizeof(unsigned short)) * n];
		pSym = (sHuffSym *) pData;
		pSymLUT = 0;
		pFreq = (unsigned short *) (pSym + n);
	} else {
		pData = new char[sizeof(sHuffSym) * MAX_HUFF_LEN + n * (1 + sizeof(unsigned short))];
		pSym = (sHuffSym *) pData;
		pSymLUT = (unsigned char *) (pSym + MAX_HUFF_LEN);
		pFreq = (unsigned short *) (pSymLUT + n);
	}
	for (unsigned int i = 0; i < n; i++) pFreq[i] = 8;
	if (pInitTable != 0)
		init(pInitTable);
	else
		update_code();
}


CHuffCodec::~CHuffCodec()
{
	delete[] pData;
}

void CHuffCodec::init(const sHuffRL * pInitTable)
{
	sHuffSym TmpHuff[MAX_HUFF_SYM];

	RL2len(pInitTable, TmpHuff, nbSym);
	qsort(TmpHuff, nbSym, sizeof(sHuffSym),
	      (int (*)(const void *, const void *)) comp_len);
	make_codes(TmpHuff, nbSym);

	if (pSymLUT == 0) {
		qsort(TmpHuff, nbSym, sizeof(sHuffSym),
		      (int (*)(const void *, const void *)) comp_sym);
		memcpy(pSym, TmpHuff, sizeof(sHuffSym) * nbSym);
	} else {
		enc2dec(TmpHuff, pSym, pSymLUT, nbSym);
	}
	update_step = UPDATE_STEP_MIN;
}

/**
 * Calculate huffman code lenth in place from a sorted frequency array
 * as in "In-Place Calculation of Minimum-Redundancy Codes" Moffat & Katajainen
 * @param sym
 * @param n
 */
void CHuffCodec::make_len(sHuffSym * sym, int n)
{
	int root = n - 1, leaf = n - 3, next, nodes_left, nb_nodes, depth;

	sym[n - 1].code += sym[n - 2].code;
	for (int i = n - 2; i > 0; i--) {
        /* select first item for a pairing */
		if (leaf < 0 || sym[root].code < sym[leaf].code) {
			sym[i].code = sym[root].code;
			sym[root--].code = i;
		} else
			sym[i].code = sym[leaf--].code;

        /* add on the second item */
		if (leaf < 0 || (root > i && sym[root].code < sym[leaf].code)) {
			sym[i].code += sym[root].code;
			sym[root--].code = i;
		} else
			sym[i].code += sym[leaf--].code;
	}

	sym[1].code = 0;
	for (int i = 2; i < n; i++)
		sym[i].code = sym[sym[i].code].code + 1;

	nodes_left = 1;
	nb_nodes = depth = 0;
	root = 1;
	next = 0;
	while (nodes_left > 0) {
		while (root < n && sym[root].code == depth) {
			nb_nodes++;
			root++;
		}
		while (nodes_left > nb_nodes) {
			sym[next++].len = depth;
			nodes_left--;
		}
		nodes_left = 2 * nb_nodes;
		depth++;
		nb_nodes = 0;
	}
}

int CHuffCodec::comp_sym(const sHuffSym * sym1, const sHuffSym * sym2)
{
	return sym1->value - sym2->value;
}

int CHuffCodec::comp_freq(const sHuffSym * sym1, const sHuffSym * sym2)
{
	return sym2->code - sym1->code;
}

int CHuffCodec::comp_len(const sHuffSym * sym1, const sHuffSym * sym2)
{
	if (sym1->len == sym2->len)
		return sym2->code - sym1->code;
	return sym1->len - sym2->len;
}

/**
 * Generate canonical huffman codes from symbols and bit lengths
 * @param sym
 * @param n
 */
void CHuffCodec::make_codes(sHuffSym * sym, int n)
{
	unsigned int bits = sym[n - 1].len, code = 0;
	sym[n - 1].code = 0;

	for( int i = n - 2; i >= 0; i--){
		code >>= bits - sym[i].len;
		bits = sym[i].len;
		code++;
		sym[i].code = code;
	}
}

void CHuffCodec::RL2len(const sHuffRL * pRL, sHuffSym * pHuff, int n)
{
	int i = 0, j = 0, len = 0;
	while( i < n ){
		len += pRL[j].diff;
		for( int iend = i + pRL[j].len; i < iend; i++){
			pHuff[i].value = i;
			pHuff[i].len = len;
		}
		j++;
	}
}

int CHuffCodec::len2RL(sHuffRL * pRL, const sHuffSym * pHuff, int n)
{
	int i = 0, j = 0, len = 0;
	while( i < n ){
		pRL[j].diff = pHuff[i].len - len;
		len = pHuff[i].len;
		int k = i;
		do {
			i++;
		} while( i < n && pHuff[k].len == pHuff[i].len );
		pRL[j].len = i - k;
		j++;
	}
	return j;
}

int CHuffCodec::enc2dec(sHuffSym * sym, sHuffSym * outSym,
                        unsigned char * pSymLUT, int n)
{
	unsigned int bits = sym[0].len;
	unsigned int cnt = 0;
	for ( int i = 1; i < n; i++ ) {
		if (sym[i].len != bits) {
			bits = sym[i].len;
			outSym[cnt].code = sym[i-1].code << (16 - sym[i-1].len);
			outSym[cnt].len = sym[i-1].len;
			outSym[cnt++].value = (char)(sym[i-1].code + i - 1);
		}
	}
	outSym[cnt].code = sym[n-1].code << (16 - sym[n-1].len);
	outSym[cnt].len = sym[n-1].len;
	outSym[cnt++].value = (char)(sym[n-1].code + n - 1);

	for ( int i = 0; i < n; i++ )
		pSymLUT[i] = sym[i].value;
	return cnt;
}

void CHuffCodec::update_code(void)
{
	sHuffSym sym[MAX_HUFF_SYM];
	for( unsigned int i = 0; i < nbSym; i++){
		sym[i].code = pFreq[i];
		sym[i].value = i;
		pFreq[i] = (pFreq[i] + 1) >> 1;
	}
	qsort(sym, nbSym, sizeof(sHuffSym),
	      (int (*)(const void *, const void *)) comp_freq);

	make_len(sym, nbSym);
	make_codes(sym, nbSym);
	if (pSymLUT == 0) {
		qsort(sym, nbSym, sizeof(sHuffSym),
		      (int (*)(const void *, const void *)) comp_sym);
		memcpy(pSym, sym, sizeof(sHuffSym) * nbSym);
	} else {
		enc2dec(sym, pSym, pSymLUT, nbSym);
	}
	count = 0;
	update_step >>= 1;
	update_step = MAX(update_step, UPDATE_STEP_MIN);
}

void CHuffCodec::make_huffman(sHuffSym * sym, int n)
{
	qsort(sym, n, sizeof(sHuffSym),
	      (int (*)(const void *, const void *)) comp_freq);

	make_len(sym, n);
	make_codes(sym, n);
}

/**
 * Print the huffman tables
 * print_type = 0 => print the coding table
 * print_type = 1 => print the decoding table
 * print_type = 2 => print the canonical decoding table
 * print_type = 3 => print the RL-coded table
 * print_type = 4 => print the RL-coded table, sorting the table
 * @param sym
 * @param n
 * @param print_type
 */
void CHuffCodec::print(sHuffSym * sym, int n, int print_type, char * name)
{
	unsigned int bits, cnt;
	switch( print_type ) {
	case 0 :
		qsort(sym, n, sizeof(sHuffSym),
		      (int (*)(const void *, const void *)) comp_sym);
		printf("%s[%i] = { ", name, n);
		for( int i = 0; i < n; i++) {
			if (i != 0)
				printf(", ");
			printf("{%u, %u}", sym[i].code, sym[i].len);
		}
		printf(" };\n");
		break;
	case 1:
		qsort(sym, n, sizeof(sHuffSym),
		      (int (*)(const void *, const void *)) comp_len);
		printf("{\n	");
		for( int i = 0; i < n; i++) {
			printf("{0x%.4x, %u, %u}", sym[i].code << (16 - sym[i].len), sym[i].len, sym[i].value);
			if (i != 0)
				printf(", ");
		}
		printf("\n}\n");
		break;
	case 2:
		qsort(sym, n, sizeof(sHuffSym),
		      (int (*)(const void *, const void *)) comp_len);
		printf("%s[] = { ", name);
		bits = sym[0].len;
		cnt = 1;
		for ( int i = 1; i < n; i++ ) {
			if (sym[i].len != bits) {
				bits = sym[i].len;
				printf("{0x%x, %u, %u}", sym[i-1].code << (16 - sym[i-1].len),
				       sym[i-1].len, (unsigned char)(sym[i-1].code + i - 1));
				printf(", ");
				cnt++;
			}
		}
		printf("{0x%x, %u, %u}", sym[n-1].code << (16 - sym[n-1].len),
		       sym[n-1].len, (unsigned char)(sym[n-1].code + n - 1));
		printf(" };\nlut_%s[%i] = { ", name, n);
		for ( int i = 0; i < n; i++ ) {
			printf("%u", sym[i].value);
			if (i != n - 1)
				printf(", ");
		}
		printf(" };\n");
		break;
	case 4:
		qsort(sym, n, sizeof(sHuffSym),
		      (int (*)(const void *, const void *)) comp_sym);
	case 3:
		sHuffRL rl_table[MAX_HUFF_SYM];
		int k = len2RL(rl_table, sym, n);
		printf("{\n	");
		for( int i = 0; i < k; i++) {
			if (i != 0)
				printf(", ");
			printf("{%i, %i}", rl_table[i].diff, rl_table[i].len);
		}
		printf("\n}\n");
	}
	fflush(0);
}

void CHuffCodec::print(int print_type, char * name)
{
	if (pSymLUT == 0)
		print(pSym, nbSym, print_type, name);
}

void CHuffCodec::init_lut(sHuffCan * data, const int bits)
{
	int i, idx = 0;
	const int shift = 16 - bits;
	const sHuffSym * table = data->table;
	const unsigned char * sym = data->sym;
	sHuffLut * lut = data->lut;
	for (i = (1 << bits) - 1; i >= 0 ; i--) {
		if ((table[idx].code >> shift) < i) {
			if (table[idx].len <= bits) {
				lut[i].len = table[idx].len;
				lut[i].value = sym[(table[idx].value - (i >> (bits - table[idx].len))) & 0xFF];
			} else {
				lut[i].len = 0;
				lut[i].value = idx;
			}
		} else {
			if (table[idx].len <= bits) {
				lut[i].len = table[idx].len;
				lut[i].value = sym[(table[idx].value - (i >> (bits - table[idx].len))) & 0xFF];
			} else {
				lut[i].len = 0;
				lut[i].value = idx;
			}
			if (i != 0)
				do {
					idx++;
				} while ((table[idx].code >> shift) == i);
		}
	}
}

}
