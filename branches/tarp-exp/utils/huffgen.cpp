/***************************************************************************
 *   Copyright (C) 2009 by Nicolas Botti                                   *
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


#include <fstream>
#include <iostream>
#include <math.h>
#include <stdlib.h>

#include "huffcodec.h"

using namespace std;
using namespace rududu;

#ifdef _WIN32
#define BOLD
#define NORM
#else
#define BOLD	"\x1B[1m"
#define NORM	"\x1B[0m"
#endif // _WIN32

void usage(void)
{
	printf(
		   BOLD "--- Huffman code generator --- " NORM " built " __DATE__
			" " __TIME__ "\n(c) 2006-2009 Nicolas BOTTI\n"	BOLD "Usage : "
			NORM "huffgen [max_code_size] < stat_file > huff_code.c\n"
		  );
}

int main( int argc, char *argv[] )
{
	string progname = argv[0];
	sHuffSym input[MAX_HUFF_SYM];
	unsigned int counts[MAX_HUFF_SYM], tab_idx = 0, gran_cnt = 0;
	double huff_size = 0, theo_size = 0;
	int huff_len = 0;

	if (argc > 1) {
		char* end;
		huff_len = strtol(argv[1], &end, 0);
		if ((end - argv[1]) == 0)
			huff_len = -1;
	}

	if (argc > 2 || huff_len < 0) {
		usage();
		return 1;
	}

	cin.peek();
	while(! cin.eof()) {
		int sym_cnt = 0;
		unsigned int sum = 0;
		char c = cin.get();
		while(c != '\n' && ! cin.eof()) {
			cin.putback(c);
			cin >> counts[sym_cnt];
			sum += counts[sym_cnt];
			sym_cnt++;
			do c = cin.get();
			while( c == ' ' or c == '\t' );
		}

		int max_len = bitlen(sum), shift = 0;
		if (max_len > 16) shift = max_len - 16;

		for( int i = 0; i < sym_cnt; i++) {
			counts[i] >>= shift;
			input[i].code = counts[i];
			input[i].value = i;
		}

		if (sym_cnt != 0) {
			CHuffCodec::make_huffman(input, sym_cnt, huff_len);
			char name[16];
			sprintf(name, "enc%02i", tab_idx);
			CHuffCodec::print(input, sym_cnt, 0, name);

			double total_cnt = 0, total_size = 0, optim_size = 0;
			for( int i = 0; i < sym_cnt; i++) {
				total_cnt += counts[i];
				total_size += counts[i] * input[i].len;
				if (counts[i] != 0)
					optim_size += counts[i] * log2(counts[i]);
			}
			optim_size = total_cnt * log2(total_cnt) - optim_size;

			sprintf(name, "dec%02i", tab_idx);
			CHuffCodec::print(input, sym_cnt, 2, name);
			cout << "// count : " << total_cnt << endl;
			if (total_cnt > 0) {
				cout << "// huff : " << total_size / total_cnt << " bps, " << total_size << " bits" << endl;
				cout << "// opt : " << optim_size / total_cnt << " bps, " << optim_size << " bits" << endl;
				cout << "// loss : " << (total_size - optim_size) / total_cnt << " bps (" << (total_size - optim_size) * 100 / optim_size << "%)\n" << endl;
				double mult = 1;
				if (shift != 0) mult = (double) sum / total_cnt;
				huff_size += total_size * mult;
				theo_size += optim_size * mult;
				gran_cnt += sum;
			}
			tab_idx++;
		} else tab_idx = 0;
	}
	cout << "// " << gran_cnt << " symbols, total size : " << huff_size << " bits (" << huff_size / gran_cnt <<" bps)" << endl;
	cout << "// loss : " << (huff_size - theo_size) * 100 / theo_size << "%" << endl;

	return 0;
}
