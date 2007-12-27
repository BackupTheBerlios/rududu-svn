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

// TODO : faire une allocation dynamique pour toute la classe
#define BIT_CONTEXT_NB	16
#define SPEED	8
#define DECAY	(FREQ_POWER - SPEED)
#define ROUND	(1 << (DECAY - 1))

class CBitCodec
{
public:
	CBitCodec(CMuxCodec * RangeCodec = 0);
	void InitModel(void);

	void inline code0(const unsigned int Context = 0){
		pRange->code0(freq[Context]);
		freq[Context] += (1 << SPEED) - ((freq[Context] + ROUND) >> DECAY);
	}

	void inline code1(const unsigned int Context = 0){
		pRange->code1(freq[Context]);
		freq[Context] -= ((freq[Context] + ROUND) >> DECAY);
	}

	void inline code(const unsigned int Symbol, const unsigned int Context = 0){
		pRange->codeBin(freq[Context], Symbol);
		freq[Context] += ((Symbol ^ 1) << SPEED) - ((freq[Context] + ROUND) >> DECAY);
	}

	unsigned int inline decode(const unsigned int Context = 0){
		const register unsigned int ret = pRange->getBit(freq[Context]);
		freq[Context] += ((ret ^ 1) << SPEED) - ((freq[Context] + ROUND) >> DECAY);
		return ret;
	}

	void setRange(CMuxCodec * RangeCodec){ pRange = RangeCodec;}
	CMuxCodec * getRange(void){ return pRange;}

private:
	unsigned short freq[BIT_CONTEXT_NB];
	CMuxCodec *pRange;
};

}
