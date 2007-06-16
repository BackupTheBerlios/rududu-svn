
#include <fstream>
#include <iostream>

using namespace std;

#define WIDTH	1280
#define HEIGHT	720
#define CMPNT	3

int main( int argc, char *argv[] )
{
	string progname = argv[0];
	char buff[WIDTH * sizeof(short)];

	for( int i = 1; i < argc; i++){
		ifstream is( argv[i] , ios::in | ios::binary );
		is.seekg(512, ios::beg);
		for( int j = 0; j < HEIGHT; j++){
			is.read(buff, WIDTH * sizeof(short));
			cout.write(buff, WIDTH);
		}
	}

	return 0;
}
