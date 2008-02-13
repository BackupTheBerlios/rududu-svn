

#pragma once

namespace rududu {

#define LL_BAND		0
#define V_BAND		1
#define H_BAND		2
#define D1_BAND		3	// diag direction = "\"
#define D2_BAND		4	// diag direction = "/"

#define ALIGN	32

class CBand
{
public:
	CBand(void);
	virtual ~CBand();

	unsigned int DimX;		// Width of the band (Datas)
	unsigned int DimY;		// Height of the band
	unsigned int DimXAlign;	// Alignement Width (Buffer)
	unsigned int BandSize;	// (DimXAlign * DimY), the band size in SAMPLES
	int Max;				// Max of the band
	int Min;				// Min of the band
	unsigned int Dist;		// Distortion (Square Error)
	unsigned int Count;		// Count of non-zero coeffs
	float Weight;			// Weighting of the band distortion
							// (used for Quant calc and PSNR calc)

	CBand *pParent;			// Parent Band
	CBand *pChild;			// Child Band
	CBand *pNeighbor[3];	// Band neighbors (other component bands
							// that can be used for context modeling)
	short *pBand;			// Band datas

	void Init(unsigned int x = 0, unsigned int y = 0, int Align = ALIGN);

	// Quantification
	unsigned int TSUQ(short Quant, float Thres);
	void TSUQi(short Quant);
	unsigned int TSUQ_DCTH(short Quant, float Thres);
	unsigned int TSUQ_DCTV(short Quant, float Thres);
	void TSUQ_DCTHi(short Quant);
	void TSUQ_DCTVi(short Quant);

	// Statistiques
	void Mean(float & Mean, float & Var);

	// Utilitaires
	void Add(short val);
	void Clear(bool recurse = false);
	template <class T> void GetBand(T * pOut);

private:
	char * pData;
};

}
