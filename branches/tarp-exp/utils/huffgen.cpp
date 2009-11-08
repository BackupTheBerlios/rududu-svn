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

#define NAME_MAX_SIZE 128
#define MAX_CTX_NB	64

struct huff_tab_t {
	int cnt;
	sHuffSymExt huff[MAX_HUFF_SYM];
};

huff_tab_t ctx[MAX_CTX_NB];

void usage(void)
{
	printf(
		   BOLD "--- Huffman code generator --- " NORM " built " __DATE__
			" " __TIME__ "\n(c) 2006-2009 Nicolas BOTTI\n"	BOLD "Usage : "
			NORM "huffgen [max_code_size] < stat_file > huff_code.c\n"
		  );
}

static void skip_line(istream & input, char c)
{
	while (c != '\n' && ! input.eof())
		c = input.get();
}

static void print_huff(char * tab_name, int idx, bool enc)
{
	char name[NAME_MAX_SIZE];
	sprintf(name, "%s%c%02i", tab_name, enc ? 'c' : 'd', idx);
	CHuffCodec::print(ctx[idx].huff, ctx[idx].cnt, enc ? 0 : 2, name);
}

int main( int argc, char *argv[] )
{
	string progname = argv[0];
	unsigned int counts[MAX_HUFF_SYM], tab_idx = 0, gran_cnt = 0;
	double huff_size = 0, theo_size = 0;
	int huff_len = 0;
	char t_name[NAME_MAX_SIZE] = "tab";

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

		if (c < '0' || c > '9') {
			if (c == '>') {
				cin.width(NAME_MAX_SIZE);
				cin >> t_name;
			}
			skip_line(cin, c);
			tab_idx = 0;
			continue;
		}

		while(c != '\n' && ! cin.eof()) {
			cin.putback(c);
			cin >> counts[sym_cnt];
			sum += counts[sym_cnt];
			ctx[tab_idx].huff[sym_cnt].code = counts[sym_cnt];
			ctx[tab_idx].huff[sym_cnt].value = sym_cnt;
			sym_cnt++;
			do c = cin.get();
			while( c == ' ' or c == '\t' );
		}
		ctx[tab_idx].cnt = sym_cnt;

		if (sym_cnt <= 1 || sum <= 1)
			continue;

		CHuffCodec::make_huffman(ctx[tab_idx].huff, ctx[tab_idx].cnt, huff_len);
		print_huff(t_name, tab_idx, true);

		double total_cnt = 0, total_size = 0, optim_size = 0;
		for( int i = 0; i < sym_cnt; i++) {
			total_cnt += counts[i];
			total_size += counts[i] * ctx[tab_idx].huff[i].len;
			if (counts[i] != 0)
				optim_size += counts[i] * log2(counts[i]);
		}
		optim_size = total_cnt * log2(total_cnt) - optim_size;

		print_huff(t_name, tab_idx, false);

		cout << "// count : " << total_cnt << endl;
		if (total_cnt > 0) {
			cout << "// huff : " << total_size / total_cnt << " bps, " << total_size << " bits" << endl;
			cout << "// opt : " << optim_size / total_cnt << " bps, " << optim_size << " bits" << endl;
			cout << "// loss : " << (total_size - optim_size) / total_cnt << " bps (" << (total_size - optim_size) * 100 / optim_size << "%)\n" << endl;
			huff_size += total_size;
			theo_size += optim_size;
			gran_cnt += sum;
		}
		tab_idx++;
	}
	cout << "// " << gran_cnt << " symbols, total size : " << huff_size << " bits (" << huff_size / gran_cnt <<" bps)" << endl;
	cout << "// loss : " << (huff_size - theo_size) * 100 / theo_size << "%" << endl;

	return 0;
}
