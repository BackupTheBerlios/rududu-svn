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

#include "huffcodec.h"

namespace rududu {

CHuffCodec::CHuffCodec()
{
}


CHuffCodec::~CHuffCodec()
{
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

void CHuffCodec::make_huffman(sHuffSym * sym, int n)
{
	qsort(sym, n, sizeof(sHuffSym),
	      (int (*)(const void *, const void *)) comp_freq);

	make_len(sym, n);
	make_codes(sym, n);
	print(sym, n, 2, 0);
	print(sym, n, 0, 0);
}

/**
 * Print the huffman tables
 * print_type = 0 => print the coding table
 * print_type = 1 => print the decoding table
 * print_type = 2 => print the canonical decoding table
 * @param sym
 * @param n
 * @param print_type
 * @param offset
 */
void CHuffCodec::print(sHuffSym * sym, int n, int print_type, int offset)
{
	switch( print_type ) {
	case 0 :
		qsort(sym, n, sizeof(sHuffSym),
		      (int (*)(const void *, const void *)) comp_sym);
		printf("{\n	");
		for( int i = 0; i < n; i++) {
			if (i != 0)
				printf(", ");
			printf("{%u, %u}", sym[i].code, sym[i].len);
		}
		printf("\n}\n");
		break;
	case 1:
		printf("{\n	");
		for( int i = 0; i < n; i++) {
			printf("{0x%.4x, %u, %i}", sym[i].code << (16 - sym[i].len), sym[i].len, sym[i].value - offset);
			if (i != 0)
				printf(", ");
		}
		printf("\n}\n");
		break;
	case 2:
		printf("{\n	");
		unsigned int bits = sym[0].len;
		unsigned int cnt = 1;
		for ( int i = 1; i < n; i++ ) {
			if (sym[i].len != bits) {
				bits = sym[i].len;
				printf("{0x%x, %i, %i}", sym[i-1].code << (16 - sym[i-1].len),
				       sym[i-1].len, (char)(sym[i-1].code + i - 1));
				printf(", ");
				cnt++;
			}
		}
		printf("{0x%x, %i, %i}", sym[n-1].code << (16 - sym[n-1].len),
		       sym[n-1].len, (char)(sym[n-1].code + n - 1));
		printf("\n}; // %i\n", cnt);
		printf("{ ");
		for ( int i = 0; i < n; i++ ) {
			printf("%i", sym[i].value - offset);
			if (i != n - 1)
				printf(", ");
		}
		printf(" };\n");
	}
}

}
