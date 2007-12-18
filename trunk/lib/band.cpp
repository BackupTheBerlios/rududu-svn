
#include <stdint.h>
#include <string.h>

#include "band.h"

// #include <math.h>
// #include <string.h>
// #include <iostream>

// using namespace std;

namespace rududu {

CBand::CBand( void ):
pData(0)
{
	pParent = 0;
	pChild = 0;
	Init();
}

CBand::~CBand()
{
	delete[] pData;
}

void CBand::Init( unsigned int x, unsigned int y, int Align)
{
	DimX = x;
	DimY = y;
	DimXAlign = (( DimX * sizeof(short) + Align - 1 ) & ( -Align )) / sizeof(short);
	BandSize = DimXAlign * DimY;
	Weight = 1;
	Count = 0;
	if (BandSize != 0){
		pData = new char[BandSize * sizeof(short) + Align];
		pBand = (short*)(((intptr_t)pData + Align - 1) & (-Align));
	}
}

void CBand::GetBand(short * pOut)
{
	memcpy(pOut, pBand, BandSize * sizeof(short));
}

void CBand::Mean( float & Mean, float & Var )
{
	int64_t Sum = 0;
	int64_t SSum = 0;
	for ( unsigned int j = 0; j < DimY; j++ ) {
		unsigned int J = j * DimXAlign;
		for ( unsigned int i = 0; i < DimX; i++ ) {
			Sum += pBand[i + J];
			SSum += pBand[i + J] * pBand[i + J];
		}
	}
	Mean = (float) Sum * Weight / ( DimX * DimY );
	Var = ((float) (SSum - Sum * Sum)) * Weight * Weight /
			(( DimX * DimY ) * ( DimX * DimY ));
}

unsigned int CBand::TSUQ(short Quant, float Thres)
{
	int Diff = DimXAlign - DimX;
	Quant = (short) (Quant / Weight);
	if (Quant == 0) Quant = 1;
	int iQuant = (int) (1 << 16) / Quant;
	short T = (short) (Thres * Quant);
	int Min = 0, Max = 0;
	Count = 0;
	for ( unsigned int j = 0, n = 0; j < DimY ; j++ ) {
		for ( unsigned int nEnd = n + DimX; n < nEnd ; n++ ) {
			if ( (unsigned short) (pBand[n] + T) <= (unsigned short) (2 * T)) {
				pBand[n] = 0;
			} else {
				Count++;
				pBand[n] = (pBand[n] * iQuant + (1 << 15)) >> 16;
				if (pBand[n] > Max) Max = pBand[n];
				if (pBand[n] < Min) Min = pBand[n];
			}
		}
		n += Diff;
	}
	this->Min = Min;
	this->Max = Max;
	return Count;
}

void CBand::TSUQi(short Quant)
{
	int Diff = DimXAlign - DimX;
	Quant = (short) (Quant / Weight);
	if (Quant == 0) Quant = 1;
	for ( unsigned int j = 0, n = 0; j < DimY ; j ++ ) {
		for ( unsigned int nEnd = n + DimX; n < nEnd ; n++ ) {
			pBand[n] = pBand[n] * Quant;
		}
		n += Diff;
	}
}

void CBand::Add( short val )
{
	for ( unsigned int i = 0; i < BandSize; i++ )
		pBand[i] += val;
}

void CBand::Clear(bool recurse)
{
	memset(pBand, 0, BandSize * sizeof(short));
	if (recurse && pParent != 0)
		pParent->Clear(true);
}

}
