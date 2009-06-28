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

#include <iostream>
#include "bitcodec.h"

using namespace std;

namespace rududu {

CBitCodec::CBitCodec(uint nb_ctx, CMuxCodec * RangeCodec):
		ctx_offset(last_offset)
{
	last_offset += nb_ctx;
	if (last_offset > BIT_CONTEXT_NB) {
		cerr << "missing context memory : at least " << last_offset;
		cerr << "contexts are needed" << endl;
	}
	InitModel(); // FIXME : do not delete all ctx
	setRange(RangeCodec);
}

void CBitCodec::InitModel(void)
{
	for (int i = 0; i < BIT_CONTEXT_NB; i++){
		freq[i] = HALF_FREQ_COUNT;
		state[i].mps = 0;
		state[i].shift = 0;
	}
}

const unsigned short CBitCodec::thres[11] = {
	2584, 1512, 745, 371, 185, 92, 46, 23, 12, 6, 3
};

uint CBitCodec::last_offset = 0;
unsigned short CBitCodec::freq[BIT_CONTEXT_NB]; /// global array to store context frequencies
sState CBitCodec::state[BIT_CONTEXT_NB];

}
