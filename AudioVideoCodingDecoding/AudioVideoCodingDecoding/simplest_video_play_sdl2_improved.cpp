#include<cstdio>
#include<iostream>
#include<cassert>
#include<fstream>
#include<string>
#include<vector>
#include<SDL.h>
#include<SDL_version.h>
using std::cout;
using std::endl;

int wind_w = 320*2;
int wind_h = 180*2;
const int pixel_w = 320;
const int pixel_h = 180;
unsigned char buffer[pixel_w*pixel_h * 3 / 2];

//refresh event
const uint32_t REFRESH_USEREVENT = SDL_USEREVENT + 1;
//break event
const uint32_t BREAK_USEREVENT = SDL_USEREVENT + 2;

int thread_exit = 0;  //叉掉窗口时设置为1，辅助程序退出
const int normal_speed = 40;
const int fast_forword_speed = 5;
int delaytime = normal_speed;
int refresh_video(void*)//线程的入口函数，必须是int(void*)类型
{
	while (thread_exit == 0)
	{
		SDL_Event event;
		event.type = REFRESH_USEREVENT;
		SDL_PushEvent(&event); //SDl_PushEvent不会引起阻塞吧？
		SDL_Delay(delaytime);
		//SDL_PumpEvents();
	}
	
	//break
	SDL_Event event;
	event.type = BREAK_USEREVENT;
	SDL_PushEvent(&event);
	return 0;
}

int main(int argc, char* agv[])
{
	cout << "SDL version in use: " << SDL_MAJOR_VERSION << "."
		<< SDL_MINOR_VERSION << "." << SDL_PATCHLEVEL << endl;
	cout << "---------------------------------------" << endl;

	if (SDL_Init(SDL_INIT_VIDEO) != 0) {
		cout << "Couldn' initialize SDL: " << SDL_GetError << endl;
		return -1;
	}

	//SDL_CreateWindow的第二三个参数为窗口在电脑屏幕上
	//的位置，如(0, 0)表示位于左上角
	SDL_Window* wind = SDL_CreateWindow("Simplest video play sdl2",
		SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,  
		wind_w, wind_h, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
	if (!wind) {
		cout << "SDL: couldn't create window " 
			<< SDL_GetError() << endl;
		return -1;
	}

	SDL_Renderer* render = SDL_CreateRenderer(wind,-1,0);

	//IYUV: Y + U + V  (3 planes)
	//YV12: Y + V + U  (3 planes)
	uint32_t pixfmt=SDL_PIXELFORMAT_IYUV; //YUV420P
	SDL_Texture* texture = SDL_CreateTexture(render,pixfmt,
		SDL_TEXTUREACCESS_STREAMING,pixel_w,pixel_h);

	std::ifstream ifs("焰灵姬_18s_yuv420p_320x180.yuv", std::ios::binary);
	if (!ifs) { cout << "Couldn't open yuv file" << endl; return -1; }

	SDL_CreateThread(refresh_video, nullptr,nullptr);
	//主线程先于子线程结束，会如何？

	enum VideoPlayState
	{
		VSTATE_NORMAL,
		VSTATE_PAUSE,
		VSTATE_FASTFORWARD
	};
	VideoPlayState videostate = VSTATE_NORMAL;
	bool space_down_state = false;  //空格键是否处于按下状态
	SDL_Event event;
	while (1)
	{
		SDL_WaitEvent(&event); //引起阻塞
		if (event.type == SDL_WINDOWEVENT)
		{
			SDL_GetWindowSize(wind,&wind_w,&wind_h);
		}
		else if (event.type == SDL_QUIT)  //叉掉窗口事件
		{
			thread_exit = 1;
		}
		else if (event.type == BREAK_USEREVENT) 
		{
			break;
		}
		else if (event.type == SDL_KEYDOWN) 
		{
			if (event.key.keysym.sym == ' ')
			{
				cout << SDL_GetKeyName(event.key.keysym.sym)
					<< " down" << endl;
				if (videostate == VSTATE_NORMAL)
				{
					if (space_down_state == true)
						videostate = VSTATE_FASTFORWARD;
					else
						space_down_state = true;
				}
				else if (videostate == VSTATE_PAUSE)
				{
					if (space_down_state == true)
						videostate = VSTATE_FASTFORWARD;
					else
						space_down_state = true;
				}
				else if (videostate == VSTATE_FASTFORWARD)
				{

				}
			}
		}
		else if (event.type == SDL_KEYUP)  
		{
			if (event.key.keysym.sym == ' ')
			{
				cout << SDL_GetKeyName(event.key.keysym.sym)
					<< " up" << endl;

				space_down_state = false;
				if (videostate == VSTATE_NORMAL)
				{
					videostate = VSTATE_PAUSE;
				}
				else if (videostate == VSTATE_PAUSE)
				{
					videostate = VSTATE_NORMAL;
				}
				else if (videostate == VSTATE_FASTFORWARD)
				{
					videostate = VSTATE_NORMAL;
				}
			}
		}
		else if (event.type == REFRESH_USEREVENT)
		{
			if (videostate == VSTATE_NORMAL)
			{
				delaytime = normal_speed;
			}
			else if (videostate == VSTATE_PAUSE)
			{
				continue;
			}
			else if (videostate == VSTATE_FASTFORWARD)
			{
				delaytime=fast_forword_speed;
			}

			if (!ifs.read((char*)buffer, pixel_w*pixel_h * 3 / 2))
			{
				ifs.clear();
				ifs.seekg(0, std::ios::beg);
				ifs.read((char*)buffer, pixel_w*pixel_h * 3 / 2);
				assert(ifs.gcount() == pixel_w*pixel_h * 3 / 2);
			}
			SDL_Rect rect;
			rect.x = 0;
			rect.y = 0;
			rect.w = wind_w;
			rect.h = wind_h;
			//SDL_UpdateTexture第二个参数设为0，则更新整个texture
			SDL_UpdateTexture(texture,0,buffer,pixel_w);
			SDL_RenderClear(render);
			SDL_RenderCopy(render,texture,0,&rect);
			SDL_RenderPresent(render);
		}
	}
	
	SDL_Quit();
	return 0;
}
