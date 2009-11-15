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

#define DEPRECATED_BIT_CONTEXT_NB	16 // FIXME : remove it
#define MAX_SPEED	9
#define SPEED	(MAX_SPEED - ctxs[ctx].shift)
#define DECAY	(FREQ_POWER - SPEED)

extern const unsigned short bitcodec_thres[11];

typedef struct  {
	unsigned short freq;
	uchar shift;
	uchar mps;
}sCtx;

template<int size>
class CBitCodec
{
public:
	CBitCodec(CMuxCodec * RangeCodec)
	{
		setRange(RangeCodec);
		InitModel();
	}

	void InitModel(void)
	{
		for (int i = 0; i < size; i++) {
			ctxs[i].freq = HALF_FREQ_COUNT;
			ctxs[i].mps = 0;
			ctxs[i].shift = 0;
		}
	}

	void inline code0(const unsigned int ctx = 0){
		code(0, ctx);
	}

	void inline code1(const unsigned int ctx = 0){
		code(1, ctx);
	}

	unsigned int inline code(unsigned int sym, unsigned int ctx = 0) {
		ASSERT(ctx < size);
		unsigned int s = sym ^ ctxs[ctx].mps;
		pRange->codeBin(ctxs[ctx].freq, s ^ 1);
		ctxs[ctx].freq += (s << SPEED) - (ctxs[ctx].freq >> DECAY);
		if (unlikely((unsigned short)(ctxs[ctx].freq - bitcodec_thres[ctxs[ctx].shift + 1]) >
		             bitcodec_thres[ctxs[ctx].shift] - bitcodec_thres[ctxs[ctx].shift + 1]))
			shift_adj(ctx);
		return sym;
	}

	unsigned int inline decode(unsigned int ctx = 0) {
		ASSERT(ctx < size);
		register unsigned int sym = pRange->getBit(ctxs[ctx].freq) ^ 1;
		ctxs[ctx].freq += (sym << SPEED) - (ctxs[ctx].freq >> DECAY);
		sym ^= ctxs[ctx].mps;
		if (unlikely((unsigned short)(ctxs[ctx].freq - bitcodec_thres[ctxs[ctx].shift + 1]) >
		             bitcodec_thres[ctxs[ctx].shift] - bitcodec_thres[ctxs[ctx].shift + 1]))
			shift_adj(ctx);
		return sym;
	}

	void setRange(CMuxCodec * RangeCodec){ pRange = RangeCodec;}
	CMuxCodec * getRange(void){ return pRange;}

private:
	CMuxCodec *pRange;
	static uint last_offset;
	sCtx ctxs[size];

	void inline shift_adj(const unsigned int ctx)
	{
		if (ctxs[ctx].freq > bitcodec_thres[ctxs[ctx].shift]) {
			if (ctxs[ctx].shift == 0) {
				ctxs[ctx].mps ^= 1;
				ctxs[ctx].freq = FREQ_COUNT - ctxs[ctx].freq;
				ctxs[ctx].shift = 1;
			} else
				ctxs[ctx].shift--;
		} else if (ctxs[ctx].shift < MAX_SPEED)
			ctxs[ctx].shift++;
	}
};

}
