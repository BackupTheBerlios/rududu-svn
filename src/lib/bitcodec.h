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

#pragma once

#include "muxcodec.h"

namespace rududu {

// TODO : faire une allocation dynamique pour toute la classe
#define BIT_CONTEXT_NB	16
#define MAX_SPEED	9
#define SPEED	(MAX_SPEED - state[ctx].shift)
#define DECAY	(FREQ_POWER - SPEED)

typedef struct  {
	unsigned char shift : 4;
	unsigned char mps : 1;
}sState;

class CBitCodec
{
public:
	CBitCodec(CMuxCodec * RangeCodec = 0);
	void InitModel(void);

	void inline code0(const unsigned int ctx = 0){
		code(0, ctx);
	}

	void inline code1(const unsigned int ctx = 0){
		code(1, ctx);
	}

	unsigned int inline code(unsigned int sym, const unsigned int ctx = 0){
		unsigned int s = sym ^ state[ctx].mps;
		pRange->codeBin(freq[ctx], s ^ 1);
		freq[ctx] += (s << SPEED) - (freq[ctx] >> DECAY);
		if ((unsigned short)(freq[ctx] - thres[state[ctx].shift + 1]) >
		    thres[state[ctx].shift] - thres[state[ctx].shift + 1])
			shift_adj(ctx);
		return sym;
	}

	unsigned int inline decode(const unsigned int ctx = 0){
		register unsigned int sym = pRange->getBit(freq[ctx]) ^ 1;
		freq[ctx] += (sym << SPEED) - (freq[ctx] >> DECAY);
		sym ^= state[ctx].mps;
		if ((unsigned short)(freq[ctx] - thres[state[ctx].shift + 1]) >
		    thres[state[ctx].shift] - thres[state[ctx].shift + 1])
			shift_adj(ctx);
		return sym;
	}

	void setRange(CMuxCodec * RangeCodec){ pRange = RangeCodec;}
	CMuxCodec * getRange(void){ return pRange;}

private:
	unsigned short freq[BIT_CONTEXT_NB];
	sState state[BIT_CONTEXT_NB];
	CMuxCodec *pRange;
	static const unsigned short thres[11];

	void inline shift_adj(const unsigned int ctx)
	{
		if (freq[ctx] > thres[state[ctx].shift]) {
			if (state[ctx].shift == 0) {
				state[ctx].mps ^= 1;
				freq[ctx] = FREQ_COUNT - freq[ctx];
				state[ctx].shift = 1;
			} else
				state[ctx].shift--;
		} else if (state[ctx].shift < MAX_SPEED)
			state[ctx].shift++;
	}
};

}
