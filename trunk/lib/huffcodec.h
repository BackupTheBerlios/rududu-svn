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

#include "muxcodec.h"

namespace rududu {

class CHuffCodec{
public:
    CHuffCodec();

    ~CHuffCodec();

	static void make_huffman(sHuffSym * sym, int n);

private:

	static void print(sHuffSym * sym, int n, int print_type, int offset);
	static void make_codes(sHuffSym * sym, int n);
	static void make_len(sHuffSym * sym, int n);

	static int comp_freq(const sHuffSym * sym1, const sHuffSym * sym2);
	static int comp_sym(const sHuffSym * sym1, const sHuffSym * sym2);
};

}

