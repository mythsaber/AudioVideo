#include<cstdio>
#include<iostream>
#include <SDL.h>
using std::cout;
using std::endl;

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
};


extern "C"  int main(int argc, char* argv[])
{
	if (SDL_Init(SDL_INIT_VIDEO))
	{
		cout << "not initializaton SDL"
			<< SDL_GetError() << endl;
	}
	else
		cout << "success" << endl;
	
	cout << avcodec_configuration() << endl;
	return 0;
}

