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

	//�������
	SDL_Texture* sdltexture = SDL_CreateTexture(sdlrenderer,pixelfmt,
		SDL_TEXTUREACCESS_STREAMING,texture_w,texture_h);

	ifstream ifs("��ľ��640x360.yuv", std::ios::binary);
	if (!ifs) { cout << "Couldn't open" << endl; return -1; }

	while (1)
	{
		if (!ifs.read((char*)buffer, texture_w*texture_h*bpp / 8))
		{
			cout << "set failbit" << endl;
			//readδ�����ֽ�����������failbit
			ifs.clear();
			ifs.seekg(0,std::ios::beg);
			ifs.read((char*)buffer, texture_w*texture_h*bpp / 8);
			assert((int)ifs.gcount()== texture_w*texture_h*bpp / 8);
		}

		//Ϊʲô��Ҫ����texture_w��������w��h����Ҫ�򶼲���Ҫ
		SDL_UpdateTexture(sdltexture, nullptr, buffer, texture_w);

		//��Ƶ��Χ����Χ10���صĺڿ�
		//��Ƶ���Զ�resize��(sdlrect1.w,sdlrect1.h)��С
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
		//һ��������Զ�Ӧ���SDL_Rect


		SDL_RenderClear(sdlrenderer);
		//��texture��һ�������򿽱���render��һ��������
		//������������С��һ����������ź���ʾ������
		SDL_RenderCopy(sdlrenderer,sdltexture,nullptr,&sdlrect1);
		SDL_RenderCopy(sdlrenderer, sdltexture, nullptr, &sdlrect2);

		SDL_RenderPresent(sdlrenderer);
			
		//Delay 40ms
		for (int i = 0; i < 40; i++) {
			SDL_PumpEvents();
			SDL_Delay(1);
		}
		//Ϊʲô����ʹ��SDL_Delay(1000)��
		/*
		SDL_PumpEvents�������ƣ���Ҫ�Ĺ������ƶ�event�����Խ��ж���״̬��
		���£�����������һ�������ǽ�����Ƶ��ϵͳ���豸״̬���£����������
		�������������ʾ����Ƶ���ڴ�Լ10���ס
		*/
	}

	SDL_Quit();
	return 0;
}
