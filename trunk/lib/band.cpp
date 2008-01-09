
#include <stdint.h>
#include <string.h>

#include "band.h"
#include "utils.h"

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

template <class T>
	void CBand::GetBand(T * pOut)
{
	short * pIn = pBand;
	int add = 1 << (sizeof(T) * 8 - 1);
	for( unsigned int j = 0; j < DimY; j++){
		for( unsigned int i = 0; i < DimX; i++)
			pOut[i] = (T)(pIn[i] + add);
		pOut += DimX;
		pIn += DimXAlign;
	}
}

template void CBand::GetBand(unsigned char *);
template void CBand::GetBand(unsigned short *);

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
			pBand[n] *= Quant;
		}
		n += Diff;
	}
}

// {1/sqrt(2), sqrt(2), sin(3*pi/8)/2, 1/(2*sin(3*pi/8))}
static const float dct_norm[4] = {0.7071067811865475244f, 1.414213562373095049f, 0.4619397662556433781f, 0.5411961001461969844f};

unsigned int CBand::TSUQ_DCTH(short Quant, float Thres)
{
	short Q[4], T[4];
	int iQ[4];
	for( int i = 0; i < 4; i++) {
		Q[i] = (short) (Quant / (Weight * dct_norm[i]));
		if (Q[i] == 0) Q[i] = 1;
		iQ[i] = (int) (1 << 16) / Q[i];
		T[i] = (short) (Thres * Q[i]);
	}
	int Min = 0, Max = 0;
	Count = 0;
	short * pCur = pBand;
	for ( unsigned int j = 0; j < DimY ; j++) {
		for ( unsigned int i = 0; i < DimX; i++) {
			if ( (unsigned short) (pCur[i] + T[i & 3]) <= (unsigned short) (2 * T[i & 3])) {
				pCur[i] = 0;
			} else {
				Count++;
				pCur[i] = (pCur[i] * iQ[i & 3] + (1 << 15)) >> 16;
				Max = MAX(pCur[i], Max);
				Min = MIN(pCur[i], Min);
			}
		}
		pCur += DimXAlign;
	}
	this->Min = Min;
	this->Max = Max;
	return Count;
}

unsigned int CBand::TSUQ_DCTV(short Quant, float Thres)
{
	short Q[4], T[4];
	int iQ[4];
	for( int i = 0; i < 4; i++) {
		Q[i] = (short) (Quant / (Weight * dct_norm[i]));
		if (Q[i] == 0) Q[i] = 1;
		iQ[i] = (int) (1 << 16) / Q[i];
		T[i] = (short) (Thres * Q[i]);
	}
	int Min = 0, Max = 0;
	Count = 0;
	short * pCur = pBand;
	for ( unsigned int j = 0; j < DimY ; j++) {
		for ( unsigned int i = 0; i < DimX; i++) {
			if ( (unsigned short) (pCur[i] + T[j & 3]) <= (unsigned short) (2 * T[j & 3])) {
				pCur[i] = 0;
			} else {
				Count++;
				pCur[i] = (pCur[i] * iQ[j & 3] + (1 << 15)) >> 16;
				Max = MAX(pCur[i], Max);
				Min = MIN(pCur[i], Min);
			}
		}
		pCur += DimXAlign;
	}
	this->Min = Min;
	this->Max = Max;
	return Count;
}

void CBand::TSUQ_DCTHi(short Quant)
{
	short Q[4];
	for( int i = 0; i < 4; i++)
		Q[i] = (short) (Quant / (Weight * dct_norm[i]));
	short * pCur = pBand;
	for ( unsigned int j = 0; j < DimY ; j++) {
		for ( unsigned int i = 0; i < DimX; i++)
			pCur[i] *= Q[i & 3];
		pCur += DimXAlign;
	}
}

void CBand::TSUQ_DCTVi(short Quant)
{
	short Q[4];
	for( int i = 0; i < 4; i++)
		Q[i] = (short) (Quant / (Weight * dct_norm[i]));
	short * pCur = pBand;
	for ( unsigned int j = 0; j < DimY ; j++) {
		for ( unsigned int i = 0; i < DimX; i++)
			pCur[i] *= Q[j & 3];
		pCur += DimXAlign;
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
