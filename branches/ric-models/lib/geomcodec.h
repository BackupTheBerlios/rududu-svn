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

#include "utils.h"
#include "muxcodec.h"

namespace rududu {

#define GEO_CONTEXT_NB	16
#define GEO_MAX_SHIFT	10
#define GEO_MAX_SPEED	(GEO_MAX_SHIFT - 1)
#define GEO_DECAY		(FREQ_POWER - GEO_MAX_SPEED + s)

class CGeomCodec
{
public:
	CGeomCodec(CMuxCodec * RangeCodec = 0, const unsigned char * k_init = 0);
	void setCtx(const unsigned char * k_init);

	void inline code(unsigned int sym, const unsigned int ctx = 0){
		unsigned int sum, k, f = freq[ctx];
		unsigned char s = shift[idx[ctx]];
		sum = k = K[idx[ctx]];

		while( sym >= sum ){
			pRange->codeBin(f, 1);
			freq[ctx] -= freq[ctx] >> GEO_DECAY;
			sum += k;
		}
		pRange->codeBin(f, 0);
		if (k > 1) pRange->maxCode(sym - sum + k, k - 1);

		freq[ctx] += (FREQ_COUNT - freq[ctx]) >> GEO_DECAY;

		if ((unsigned short)(freq[ctx] - thres[s - 1]) > thres[s] - thres[s - 1])
			shift_adj(ctx);
	}

	unsigned int inline decode(const unsigned int ctx = 0){
		unsigned int sum = 0, k = K[idx[ctx]], f = freq[ctx];
		unsigned char s = shift[idx[ctx]];

		while( pRange->getBit(f) ){
			freq[ctx] -= freq[ctx] >> GEO_DECAY;
			sum += k;
		}
		// TODO : use a LUT for log(k) = log[idx[ctx]]
		if (k > 1) sum += pRange->maxDecode(k - 1);

		freq[ctx] += (FREQ_COUNT - freq[ctx]) >> GEO_DECAY;

		if ((unsigned short)(freq[ctx] - thres[s - 1]) > thres[s] - thres[s - 1])
			shift_adj(ctx);
		return sum;
	}

	void setRange(CMuxCodec * RangeCodec){ pRange = RangeCodec;}
	CMuxCodec * getRange(void){ return pRange;}

private:
	unsigned short freq[GEO_CONTEXT_NB];
	unsigned char idx[GEO_CONTEXT_NB];
	CMuxCodec *pRange;
	static const unsigned short thres[11];
	static const unsigned short K[39];
	static const unsigned char shift[39];

	void inline shift_adj(const unsigned int ctx)
	{
		unsigned char s = shift[idx[ctx]];
		if (freq[ctx] < thres[s - 1])
			idx[ctx]++;
		else if (idx[ctx] > 0)
			idx[ctx]--;
		if (idx[ctx] >= GEO_MAX_SHIFT - 1)
			freq[ctx] = HALF_FREQ_COUNT;
	}

};

}
