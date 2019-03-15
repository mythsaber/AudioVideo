#include<cstdio>
#include<iostream>
#include<cassert>
#include<fstream>
#include<string>
#include<vector>

extern "C"
{
#include<libavformat/avformat.h>
#include<libavcodec/avcodec.h>
#include<libswscale/swscale.h>
#include<libavutil/avutil.h>
}
#include<SDL.h>
#include<SDL_version.h>  //SDL_MAJOR_VERSION

using std::endl;
using std::cout;
using std::vector;
using std::ifstream;

const int screen_w = 640;
const int screen_h = 360*2;
const int texture_w = 640;
const int texture_h = 360;

const int bpp = 12;
unsigned char buffer[texture_w*texture_h*bpp / 8];

int main(int argc, char* argv[])
{
	cout << "SDL version in use:" << SDL_MAJOR_VERSION << "."
		<< SDL_MINOR_VERSION << "." << SDL_PATCHLEVEL << endl;

	if (SDL_Init(SDL_INIT_VIDEO)!=0)
	{
		printf("Couldn't initialize SDL -%s\n",SDL_GetError());
		return -1;
	}
	SDL_Window* screen = SDL_CreateWindow("Simplest Video Play SDL2",
		SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		screen_w, screen_h, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
	if (!screen)
	{
		cout << "SDL: couldn't create window: " 
			<< SDL_GetError() << endl;
		return -1;
	}
	
	SDL_Renderer* sdlrenderer = SDL_CreateRenderer(screen, -1, 0);
	
	//IYUV: Y + U + V  (3 planes)
	//YV12: Y + V + U  (3 planes)
	uint32_t pixelfmt = SDL_PIXELFORMAT_IYUV;

	//纹理材质
	SDL_Texture* sdltexture = SDL_CreateTexture(sdlrenderer,pixelfmt,
		SDL_TEXTUREACCESS_STREAMING,texture_w,texture_h);

	ifstream ifs("端木蓉640x360.yuv", std::ios::binary);
	if (!ifs) { cout << "Couldn't open" << endl; return -1; }

	while (1)
	{
		if (!ifs.read((char*)buffer, texture_w*texture_h*bpp / 8))
		{
			cout << "set failbit" << endl;
			//read未读够字节数，则设置failbit
			ifs.clear();
			ifs.seekg(0,std::ios::beg);
			ifs.read((char*)buffer, texture_w*texture_h*bpp / 8);
			assert((int)ifs.gcount()== texture_w*texture_h*bpp / 8);
		}

		//为什么需要参数texture_w？而不是w、h都需要或都不需要
		SDL_UpdateTexture(sdltexture, nullptr, buffer, texture_w);

		//视频周围将包围10像素的黑框
		//视频将自动resize到(sdlrect1.w,sdlrect1.h)大小
		SDL_Rect sdlrect1;
		sdlrect1.x = 10;
		sdlrect1.y = 10;
		sdlrect1.w = (screen_w-20);
		sdlrect1.h = (screen_h-40)/2;

		SDL_Rect sdlrect2;
		sdlrect2.x = 10;
		sdlrect2.y = screen_h/2+10;
		sdlrect2.w = (screen_w - 20);
		sdlrect2.h = (screen_h - 40) / 2;
		//一个纹理可以对应多个SDL_Rect


		SDL_RenderClear(sdlrenderer);
		//将texture的一部分区域拷贝到render的一部分区域
		//如果两个区域大小不一致则进行缩放后显示到窗口
		SDL_RenderCopy(sdlrenderer,sdltexture,nullptr,&sdlrect1);
		SDL_RenderCopy(sdlrenderer, sdltexture, nullptr, &sdlrect2);

		SDL_RenderPresent(sdlrenderer);
			
		//Delay 40ms
		for (int i = 0; i < 40; i++) {
			SDL_PumpEvents();
			SDL_Delay(1);
		}
		//为什么不能使用SDL_Delay(1000)？
		/*
		SDL_PumpEvents如其名称，主要的功能是推动event队列以进行队列状态的
		更新，不过它还有一个作用是进行视频子系统的设备状态更新，如果不调用
		这个函数，所显示的视频会在大约10秒后卡住
		*/
	}

	SDL_Quit();
	return 0;
}
