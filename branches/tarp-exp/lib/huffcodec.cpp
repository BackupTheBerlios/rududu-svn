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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "huffcodec.h"

namespace rududu {

template <class H>
static int comp_sym(const H * sym1, const H * sym2)
{
	return sym1->value - sym2->value;
}

template <class H>
static int comp_freq(const H * sym1, const H * sym2)
{
	return sym2->code - sym1->code;
}

template <class H>
static int _comp_freq(const H * sym1, const H * sym2)
{
	return sym1->code - sym2->code;
}

template <class H>
	static int comp_len(const H * sym1, const H * sym2)
{
	if (sym1->len == sym2->len)
		return sym2->code - sym1->code;
	return sym1->len - sym2->len;
}

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
	      (int (*)(const void *, const void *)) (int (*)(const sHuffSym *, const sHuffSym *)) comp_len<sHuffSym>);
	make_codes(TmpHuff, nbSym);

	if (pSymLUT == 0) {
		qsort(TmpHuff, nbSym, sizeof(sHuffSym),
		      (int (*)(const void *, const void *)) (int (*)(const sHuffSym *, const sHuffSym *)) comp_sym<sHuffSym>);
		memcpy(pSym, TmpHuff, sizeof(sHuffSym) * nbSym);
	} else {
		enc2dec(TmpHuff, pSym, pSymLUT, nbSym);
	}
	update_step = UPDATE_STEP_MIN;
}

#define UNKNOW_WEIGHT	((1 << 16) - 1)
#define MAX_WEIGHT		(UNKNOW_WEIGHT - 1)

template <class H>
static unsigned long calc_weight(H const * sym, int n,
                                  unsigned char const * leaf_cnt,
                                  unsigned long * pack_weight,
                                  unsigned char * pack_leaf, int max_len)
{
	if (max_len == 1) {
		pack_leaf[0] = leaf_cnt[0] + 2;
		if (n >= pack_leaf[0]) {
			return sym[leaf_cnt[0]].code + sym[leaf_cnt[0] + 1].code;
		} else if (n + 1 == pack_leaf[0]) {
			pack_leaf[0] = n;
			return sym[leaf_cnt[0]].code;
		} else
			return MAX_WEIGHT;
	}

	unsigned long weight = 0;
	memcpy(pack_leaf, leaf_cnt, max_len);

	for( int i = 0; i < 2; i++){
		if (pack_weight[0] == UNKNOW_WEIGHT)
			pack_weight[0] = calc_weight(sym, n, pack_leaf + 1, pack_weight + 1,
			                             pack_leaf + max_len, max_len - 1);
		if (pack_leaf[0] >= n || pack_weight[0] <= sym[pack_leaf[0]].code) {
			memcpy(pack_leaf + 1, pack_leaf + max_len, (max_len - 1) * sizeof(*pack_leaf));
			weight += pack_weight[0];
			pack_weight[0] = UNKNOW_WEIGHT;
		} else {
			weight += sym[pack_leaf[0]++].code;
		}
	}
	return weight;
}

/**
 * Compute lenth-limited huffman codes using the methode presented in
 * "A fast and space-economical algorithm for length-limited coding"
 * http://www.diku.dk/~jyrki/Paper/ISAAC95.pdf
 *
 * the symbol frequency is stored in sHuffSym.code
 * the symbol lenth is output in sHuffSym.len
 *
 * @param sym frequency of the symbols in increasing order
 * @param n number of symbols
 * @param max_len maximum symbol lenth
 */
template <class H>
void CHuffCodec::make_len(H * sym, int n, int max_len)
{
	unsigned long pack_weight[max_len - 1]; // weight of the lookahead packages
	unsigned char leaf_cnt[(max_len * (max_len + 1)) >> 1];

	memset(leaf_cnt, 0, sizeof leaf_cnt);

	for( int i = max_len - 2; i >= 0; i--) {
		pack_weight[i] = sym[0].code + sym[1].code;
	}
	for( int i = 0, j = max_len; j > 0; j--) {
		leaf_cnt[i] = 2;
		i += j;
	}

	for( int i = 2 * n - 4; i > 0 ; i--){
		if (pack_weight[0] == UNKNOW_WEIGHT)
			pack_weight[0] = calc_weight(sym, n, leaf_cnt + 1, pack_weight + 1,
			                             leaf_cnt + max_len, max_len - 1);
		if (leaf_cnt[0] >= n || pack_weight[0] <= sym[leaf_cnt[0]].code) {
			memcpy(leaf_cnt + 1, leaf_cnt + max_len, (max_len - 1) * sizeof(*leaf_cnt));
			pack_weight[0] = UNKNOW_WEIGHT;
		} else
			leaf_cnt[0]++;
	}

	int len = 0;
	for( int i = n - 1; i >= 0; i--){
		while(len < max_len && leaf_cnt[len] > i)
			len++;
		sym[i].len = len;
	}
}

/**
 * Calculate huffman code lenth in place from a sorted frequency array
 * as in "In-Place Calculation of Minimum-Redundancy Codes" Moffat & Katajainen
 * http://www.diku.dk/~jyrki/Paper/WADS95.pdf
 *
 * the symbol frequency is stored in sHuffSym.code
 * the symbol lenth is output in sHuffSym.len
 *
 * @param sym frequency of the symbols in decreasing order
 * @param n number of symbols
 */
template <class H>
void CHuffCodec::make_len(H * sym, int n)
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
		while (root < n && sym[root].code == (uint)depth) {
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

/**
 * Generate canonical huffman codes from symbols and bit lengths
 * @param sym
 * @param n
 */
template <class H>
void CHuffCodec::make_codes(H * sym, int n)
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

template <class H>
int CHuffCodec::len2RL(sHuffRL * pRL, const H * pHuff, int n)
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
	}
	qsort(sym, nbSym, sizeof(sHuffSym),
	      (int (*)(const void *, const void *)) (int (*)(const sHuffSym *, const sHuffSym *)) comp_freq<sHuffSym>);

	make_len(sym, nbSym);
	if (sym[nbSym - 1].len > MAX_HUFF_LEN) {
		for( unsigned int i = 0; i < nbSym; i++){
			sym[i].code = pFreq[i];
			sym[i].value = i;
		}
		qsort(sym, nbSym, sizeof(sHuffSym),
		      (int (*)(const void *, const void *)) (int (*)(const sHuffSym *, const sHuffSym *)) _comp_freq<sHuffSym>);
		make_len(sym, nbSym, MAX_HUFF_LEN);
		for( unsigned int i = 0; i < (nbSym >> 1); i++){
			sHuffSym tmp = sym[i];
			sym[i] = sym[nbSym - 1 - i];
			sym[nbSym - 1 - i] = tmp;
		}
	}

	for( unsigned int i = 0; i < nbSym; i++)
		pFreq[i] = (pFreq[i] + 1) >> 1;

	make_codes(sym, nbSym);
	if (pSymLUT == 0) {
		qsort(sym, nbSym, sizeof(sHuffSym),
		      (int (*)(const void *, const void *)) (int (*)(const sHuffSym *, const sHuffSym *)) comp_sym<sHuffSym>);
		memcpy(pSym, sym, sizeof(sHuffSym) * nbSym);
	} else {
		enc2dec(sym, pSym, pSymLUT, nbSym);
	}
	count = 0;
	update_step >>= 1;
	update_step = MAX(update_step, UPDATE_STEP_MIN);
}

template <class H>
void CHuffCodec::make_huffman(H * sym, uint n, int max_len)
{
	if (max_len <= 0) {
		qsort(sym, n, sizeof(H),
		      (int (*)(const void *, const void *)) (int (*)(const H *, const H *)) comp_freq<H>);
		make_len(sym, n);
	} else {
		qsort(sym, n, sizeof(H),
		      (int (*)(const void *, const void *)) (int (*)(const H *, const H *)) _comp_freq<H>);
		make_len(sym, n, max_len);
		for( uint i = 0; i < (n >> 1); i++) {
			H tmp = sym[i];
			sym[i] = sym[n - 1 - i];
			sym[n - 1 - i] = tmp;
		}
	}
	make_codes(sym, n);
}

template void CHuffCodec::make_huffman(sHuffSymExt * sym, uint n, int max_len);
template void CHuffCodec::make_huffman(sHuffSym * sym, uint n, int max_len);

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
template <class H>
void CHuffCodec::print(H * sym, int n, int print_type, char * name)
{
	unsigned int bits, cnt;
	int (*sort_fc)(const H *, const H *);
	switch( print_type ) {
	case 0 :
		sort_fc = comp_sym<H>;
		qsort(sym, n, sizeof(H), (int (*)(const void *, const void *)) sort_fc);
		printf("static const sHuffSym %s[%i] = { ", name, n);
		for( int i = 0; i < n; i++) {
			if (i != 0)
				printf(", ");
			printf("{%u, %u}", (uint) sym[i].code, sym[i].len);
		}
		printf(" };\n");
		break;
	case 1:
		sort_fc = comp_len<H>;
		qsort(sym, n, sizeof(H), (int (*)(const void *, const void *)) sort_fc);
		printf("{\n	");
		for( int i = 0; i < n; i++) {
			printf("{0x%.4x, %u, %u}", (uint) sym[i].code << (16 - sym[i].len), sym[i].len, sym[i].value);
			if (i != 0)
				printf(", ");
		}
		printf("\n}\n");
		break;
	case 2:
		sort_fc = comp_len<H>;
		qsort(sym, n, sizeof(H), (int (*)(const void *, const void *)) sort_fc);
		printf("static const sHuffSym %s[] = { ", name);
		bits = sym[0].len;
		cnt = 1;
		for ( int i = 1; i < n; i++ ) {
			if (sym[i].len != bits) {
				bits = sym[i].len;
				printf("{0x%x, %u, %u}", (uint) sym[i-1].code << (16 - sym[i-1].len),
				       sym[i-1].len, (unsigned char)(sym[i-1].code + i - 1));
				printf(", ");
				cnt++;
			}
		}
		printf("{0x%x, %u, %u}", (uint) sym[n-1].code << (16 - sym[n-1].len),
		       sym[n-1].len, (unsigned char)(sym[n-1].code + n - 1));
		printf(" };\nstatic const uchar lut_%s[%i] = { ", name, n);
		for ( int i = 0; i < n; i++ ) {
			printf("%u", sym[i].value);
			if (i != n - 1)
				printf(", ");
		}
		printf(" };\n");
		break;
	case 4:
		sort_fc = comp_sym<H>;
		qsort(sym, n, sizeof(H), (int (*)(const void *, const void *)) sort_fc);
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

template void CHuffCodec::print(sHuffSymExt *, int, int, char *);

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
