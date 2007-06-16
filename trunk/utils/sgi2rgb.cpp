
#include <fstream>
#include <iostream>

using namespace std;

void usage(string & progname)
{
	cerr << "Usage: " << progname << " <infile.sgi> [<infile2.sgi> ... <infilen.sgi>]" << endl;
	cerr << "output 8 bits rgb frames from a 1280x720 rgb 16 bits file list to stdout" << endl;
}

#define WIDTH	(1280 * 3)
#define HEIGHT	720


void convert(char * buff)
{
	for( int i = 0; i < WIDTH; i++){
		short tmp = (buff[2*i] << 8) | buff[2*i+1];
		buff[i] = (tmp + 128) >> 8;
	}
}

int main( int argc, char *argv[] )
{
	string progname = argv[0];
	char buff[WIDTH * sizeof(short)];

	if (argc < 2) {
		usage(progname);
		return 1;
	}

	for( int i = 1; i < argc; i++){
		ifstream is( argv[i] , ios::in | ios::binary );
		is.seekg(512, ios::beg);
		for( int j = 0; j < HEIGHT; j++){
			is.read(buff, WIDTH * sizeof(short));
			convert(buff);
			cout.write(buff, WIDTH);
		}
	}

	return 0;
}
