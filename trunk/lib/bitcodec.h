

#pragma once

#include "muxcodec.h"

namespace rududu {

#define BIT_CONTEXT_NB	16

#if FREQ_COUNT != 4096
#error "mod values calculated for FREQ_COUNT = 4096"
#endif

class CBitCodec
{
private:
	static const int mod[][2];

public:
	CBitCodec(CMuxCodec * RangeCodec = 0);
	virtual ~CBitCodec();
	void InitModel(void);

	void inline code0(const unsigned int Context = 0){
		pRange->code0(Freq[Context] >> FREQ_POWER);
		Freq[Context] += mod[Freq[Context] >> (2*FREQ_POWER-6)][0];
	}

	void inline code1(const unsigned int Context = 0){
		pRange->code1(Freq[Context] >> FREQ_POWER);
		Freq[Context] += mod[Freq[Context] >> (2*FREQ_POWER-6)][1];
	}

	void inline code(const unsigned short Symbol, const unsigned int Context = 0){
		pRange->codeBin(Freq[Context] >> FREQ_POWER, Symbol);
		Freq[Context] += mod[Freq[Context] >> (2*FREQ_POWER-6)][Symbol];
	}

	unsigned int inline decode(const unsigned int Context = 0){
		const register unsigned int ret = pRange->getBit(Freq[Context] >> FREQ_POWER);
		Freq[Context] += mod[Freq[Context] >> (2*FREQ_POWER-6)][ret];
		return ret;
	}

	void setRange(CMuxCodec * RangeCodec){ pRange = RangeCodec;}
	CMuxCodec * getRange(void){ return pRange;}

private:
	unsigned int Freq[BIT_CONTEXT_NB];
	CMuxCodec *pRange;
};

}
