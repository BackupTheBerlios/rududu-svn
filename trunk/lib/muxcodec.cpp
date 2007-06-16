

#include "muxcodec.h"

namespace rududu {

CMuxCodec::CMuxCodec(unsigned char *pStream, unsigned short firstWord){
	initCoder(firstWord, pStream);
	initTaboo(2);
}

CMuxCodec::CMuxCodec(unsigned char *pStream){
	initDecoder(pStream);
	initTaboo(2);
}

void CMuxCodec::initCoder(unsigned short firstWord = 0,
						  unsigned char *pOutStream = 0){
	low = firstWord << 16;
	range = MIN_RANGE << 4;
	outCount = 0;
	nbBits = 0;
	pReserved = 0;
	if (pOutStream != 0){
		pStream = pOutStream + 4;
		pInitStream = pOutStream + 2;
		for( int i = 0; i < 4; i++)
			pLast[i] = pOutStream + i;
	}
}

void CMuxCodec::initDecoder(unsigned char *pInStream)
{
	range = MIN_RANGE << 4;
	nbBits = 0;
	if (pInStream){
		pInitStream = pInStream + 2;
		pStream = pInStream + 2;
		code = low = (pStream[0] << 8) | pStream[1];
		pStream += 2;
	}
}

void CMuxCodec::normalize_enc(void)
{
	flushBuffer<false>();
	do{
		*pLast[outCount++ & ROT_BUF_MASK] = low >> 24;
		if (((low + range - 1) ^ low) >= 0x01000000)
			range = -low & (MIN_RANGE - 1);
		pLast[(outCount + 3) & ROT_BUF_MASK] = pStream++;
		range <<= 8;
		low <<= 8;
	} while (range <= MIN_RANGE);
}

void CMuxCodec::normalize_dec(void){
	do{
		if (((code - low + range - 1) ^ (code - low)) >= 0x01000000)
			range = (low - code) & (MIN_RANGE - 1);
		low = (low << 8) | (*pStream);
		code = (code << 8) | (*pStream);
		pStream++;
		range <<= 8;
	} while (range <= MIN_RANGE);
}

unsigned char * CMuxCodec::endCoding(void){
	flushBuffer<true>();

	if (range <= MIN_RANGE)
		normalize_enc();

	int last_out = 0x200 | 'W';
	if ((low & (MIN_RANGE - 1)) > (last_out & (MIN_RANGE - 1)))
		low += MIN_RANGE;

	low = (low & ~(MIN_RANGE - 1)) | (last_out & (MIN_RANGE - 1));

	*pLast[outCount & ROT_BUF_MASK] = low >> 24;
	*pLast[(outCount + 1) & ROT_BUF_MASK] = low >> 16;
	*pLast[(outCount + 2) & ROT_BUF_MASK] = low >> 8;
	*pLast[(outCount + 3) & ROT_BUF_MASK] = low;

	return pStream;
}

void CMuxCodec::initTaboo(unsigned int k)
{
	unsigned int i;
	nbTaboo[0] = 1;
	nTaboo = k;
	for( i = 1; i < k; i++)
		nbTaboo[i] = 1 << (i - 1);
	for( i = k; i < REG_SIZE; i++) {
		unsigned int j, accu = nbTaboo[i - k];
		for( j = i - k + 1; j < i; j++)
			accu += nbTaboo[j];
		nbTaboo[i] = accu;
	}
	sumTaboo[0] = nbTaboo[0];
	for( i = 1; i < REG_SIZE; i++)
		sumTaboo[i] = sumTaboo[i - 1] + nbTaboo[i];
}

const unsigned int CMuxCodec::nbFibo[32] =
{
	1, 2, 3, 5, 8, 13, 21, 34, 55, 89, 144, 233, 377, 610, 987, 1597, 2584,
	4181, 6765, 10946, 17711, 28657, 46368, 75025, 121393, 196418, 317811,
	514229, 832040, 1346269, 2178309, 3524578
};

void CMuxCodec::fiboCode(unsigned int nb)
{
	if ( nbBits >= 8 )
		emptyBuffer();

	int i = 1, t;
	for( ; nbFibo[i] <= nb; i++){
	}
	int l = i + 1;
	i--;
	nb -= nbFibo[i];

	register unsigned int r = 0xC0000000;
	t = i;
	i--;
	while( nb > 0 ){
		i--;
		if (nbFibo[i] <= nb){
			nb -= nbFibo[i];
			r >>= t-i;
			r |= 0x80000000;
			t = i;
			i--;
		}
	}
	buffer = (buffer << l) | (r >> (33 - l + i));
	nbBits += l;
}

unsigned int CMuxCodec::fiboDecode(void)
{
	if ( nbBits < 2 )
		fillBuffer(2);

	int l = 2;
	unsigned int t = 3 << (nbBits - l);
	while( (buffer & t) != t ){
		l++;
		if (l > nbBits){
			fillBuffer(l);
			t <<= 8;
		}
		t >>= 1;
	}
	nbBits -= l;
	l -= 2;
	unsigned int nb = nbFibo[l];
	t = 1 << (nbBits + 2);
	l--;
	while( l > 0 ){
		l--;
		t <<= 1;
		if (buffer & t){
			nb += nbFibo[l];
			t <<= 1;
			l--;
		}
	}
	return nb;
}

/**
 * This is an implementation of taboo codes as described by Steven Pigeon in :
 * http://www.iro.umontreal.ca/~brassard/SEMINAIRES/taboo.ps
 * or for french speaking people in his phd thesis :
 * http://www.stevenpigeon.com/Publications/publications/phd.pdf
 *
 * the taboo length is set in initTaboo() which have to be called once before
 * using tabooCode() or tabooDecode()
 *
 * @param nb number >= 0 to encode
 */
void CMuxCodec::tabooCode(unsigned int nb)
{
	int i = 0, l;
	unsigned int r = 0;

	while (sumTaboo[i] <= nb) i++;

	if (i == 0) {
		bitsCode(0, nTaboo);
		return;
	}

	l = i;
	i--;
	nb -= sumTaboo[i];

	while (i > nTaboo) {
		unsigned int k = i - nTaboo + 1, cnt = nbTaboo[k], j = 0;
		while (nb >= cnt)
			cnt += nbTaboo[k + ++j];
		nb -= cnt - nbTaboo[k + j];
		j = nTaboo - j;
		r = (r << j) | 1;
		i -= j;
	}

	if (i == nTaboo) nb++;

	r = ((((r << i) | (nb & ((1 << i) - 1))) << 1) | 1) << nTaboo;
	bitsCode(r, l + nTaboo);
}

unsigned int CMuxCodec::tabooDecode(void)
{
	int i = 0, l = nTaboo;
	unsigned int nb = 0;

	if ( nbBits < nTaboo ) fillBuffer(nTaboo);

	unsigned int t = ((1 << nTaboo) - 1) << (nbBits - nTaboo);
	while( (~buffer & t) != t ){
		l++;
		if (l > nbBits){
			fillBuffer(l);
			t <<= 8;
		}
		t >>= 1;
	}
	nbBits -= l;

	unsigned int cd = buffer >> (nbBits + nTaboo + 1);

	i = l - nTaboo;

	if (i > 0) {
		i--;
		nb += sumTaboo[i];
	}

	while (i > nTaboo) {
		unsigned int j = 1;
		while (((cd >> (i - j)) & 1) == 0) j++;
		nb += sumTaboo[i - j] - sumTaboo[i - nTaboo];
		i -= j;
	}

	if (i == nTaboo) nb -= 1;
	nb += cd & ((1 << i) - 1);

	return nb;
}

const unsigned int CMuxCodec::Cnk[16][16] =
{
	{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
	{0, 0, 1, 3, 6, 10, 15, 21, 28, 36, 45, 55, 66, 78, 91, 105},
	{0, 0, 0, 1, 4, 10, 20, 35, 56, 84, 120, 165, 220, 286, 364, 455},
	{0, 0, 0, 0, 1, 5, 15, 35, 70, 126, 210, 330, 495, 715, 1001, 1365},
	{0, 0, 0, 0, 0, 1, 6, 21, 56, 126, 252, 462, 792, 1287, 2002, 3003},
	{0, 0, 0, 0, 0, 0, 1, 7, 28, 84, 210, 462, 924, 1716, 3003, 5005},
	{0, 0, 0, 0, 0, 0, 0, 1, 8, 36, 120, 330, 792, 1716, 3432, 6435},
	{0, 0, 0, 0, 0, 0, 0, 0, 1, 9, 45, 165, 495, 1287, 3003, 6435},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 10, 55, 220, 715, 2002, 5005},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 11, 66, 286, 1001, 3003},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 12, 78, 364, 1365},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 13, 91, 455},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 14, 105},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 15},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
};

const unsigned int CMuxCodec::enumLenth[] =
{0, 4, 7, 10, 11, 13, 13, 14, 14, 14, 13, 13, 11, 10, 7, 4, 0};

const unsigned int CMuxCodec::enumLost[] =
{0, 0, 8, 464, 228, 3824, 184, 4944, 3514, 4944, 184, 3824, 228, 464, 8, 0, 0};

/**
 * Attention : il n'est pas possible de coder 0
 * @param bits
 */
void CMuxCodec::enum16Code(unsigned int bits, const unsigned int k)
{
	unsigned int code = 0;
	const unsigned int * C = Cnk[0];
	unsigned int n = 0;
	do {
		if (bits & 1) {
			code += C[n];
			C += 16;
		}
		n++;
		bits >>= 1;
	} while(bits != 0);

	if (code < enumLost[k])
		bitsCode(code, enumLenth[k]-1);
	else
		bitsCode(code + enumLost[k], enumLenth[k]);
}

/**
 * Attention : il n'est pas possible de décoder k = 0 ou k = 16
 * @param k
 * @return
 */
unsigned int CMuxCodec::enum16Decode(unsigned int k)
{
	unsigned int n = 15;
	unsigned int code = bitsDecode(enumLenth[k] - 1);
	const unsigned int * C = Cnk[k-1];
	unsigned int bits = 0;

	if (code >= enumLost[k])
		code = ((code << 1) | bitsDecode(1)) - enumLost[k];

	do {
		if (code >= C[n]) {
			bits |= 1 << n;
			code -= C[n];
			C -= 16;
			k--;
		}
		n--;
	} while(k > 0);

	return bits;
}

void CMuxCodec::golombCode(unsigned int nb, const int k)
{
	if (k < 0) {
		for( ; nb > 0; nb--)
			codeSkew1(1 - k);
		codeSkew0(1 - k);
	} else {
		unsigned int l = (nb >> k) + 1;
		nb &= (1 << k) - 1;

		while ((int)l > (31 - (int)nbBits)){
			if (31 - (int)nbBits >= 0) {
				buffer <<= 31 - nbBits;
				l -= 31 - nbBits;
				nbBits = 31;
			}
			emptyBuffer();
		}

		buffer <<= l;
		buffer |= 1;
		nbBits += l;

		bitsCode(nb, k);
	}
}

unsigned int CMuxCodec::golombDecode(const int k)
{
	if (k < 0) {
		unsigned int nb = 0;
		while (decSkew(1 - k))
			nb++;
		return nb;
	} else {
		unsigned int l = 0;

		while(0 == (buffer & ((1 << nbBits) - 1))) {
			l += nbBits;
			nbBits = 0;
			fillBuffer(1);
		}

		while( (buffer & (1 << --nbBits)) == 0 )
			l++;

		unsigned int nb = (l << k) | bitsDecode(k);
		return nb;
	}
}

void CMuxCodec::emptyBuffer(void)
{
	do {
		nbBits -= 8;
		if (pReserved == 0) {
			*pStream = (unsigned char) (buffer >> nbBits);
			pStream++;
		} else {
			*pReserved = (unsigned char) (buffer >> nbBits);
   			pReserved = 0;
		}
	} while( nbBits >= 8 );
}

template <bool end>
void CMuxCodec::flushBuffer(void)
{
	if (nbBits >= 8)
		emptyBuffer();
	if (nbBits > 0) {
		if (end) {
			if (pReserved == 0) {
				*pStream = (unsigned char) (buffer << (8-nbBits));
				pStream++;
			} else {
				*pReserved = (unsigned char) (buffer << (8-nbBits));
				pReserved = 0;
			}
			nbBits = 0;
		} else if (pReserved == 0) {
			pReserved = pStream;
			pStream++;
		}
	}
}

void CMuxCodec::fillBuffer(const unsigned int length)
{
	do {
		nbBits += 8;
		buffer = (buffer << 8) | pStream[0];
		pStream++;
	} while( nbBits < length );
}

}
