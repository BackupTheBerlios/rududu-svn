/***************************************************************************
 *   Copyright (C) 2008 by Nicolas Botti                                   *
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

#include "huffcodec.h"

using namespace std;
using namespace rududu;

void usage(string & progname)
{
}

int main( int argc, char *argv[] )
{
	string progname = argv[0];
	sHuffSym input[MAX_HUFF_SYM];
	unsigned short counts[MAX_HUFF_SYM];
	int sym_cnt = 0;

	cin >> input[sym_cnt].code;
	while(! cin.eof()) {
		counts[sym_cnt] = input[sym_cnt].code;
		input[sym_cnt].value = sym_cnt;
		sym_cnt++;
		cin >> input[sym_cnt].code;
	}

	CHuffCodec::make_huffman(input, sym_cnt);

	double total_cnt = 0, total_size = 0, optim_size = 0;
	for( int i = 0; i < sym_cnt; i++) {
		total_cnt += counts[i];
		total_size += counts[i]* input[i].len;
		optim_size += counts[i] * __builtin_log2(counts[i]);
	}
	optim_size = total_cnt * __builtin_log2(total_cnt) - optim_size;
	cout << endl << "count : " << total_cnt << endl;
	cout << "huff : " << total_size / total_cnt << " bps" << endl;
	cout << "opt : " << optim_size / total_cnt << " bps" << endl;
	cout << "loss : " << (total_size - optim_size) / total_cnt << " bps (" << (total_size - optim_size) * 100 / optim_size << "%)" << endl;

	return 0;
}
