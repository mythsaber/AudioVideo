#include<cstdio>
#include<iostream>
#include<cassert>
#include<fstream>
#include<string>
#include<vector>
#include<SDL.h>
#include<SDL_version.h>
#include<atomic>
#include<thread>
using std::cout;
using std::endl;

int wind_w = 640;
int wind_h = 360;

int main(int argc, char* agv[])
{
	cout << "SDL version in use: " << SDL_MAJOR_VERSION << "."
		<< SDL_MINOR_VERSION << "." << SDL_PATCHLEVEL << endl;
	cout << "---------------------------------------" << endl;

	SDL_Window* wind = nullptr;
	SDL_Renderer* render = nullptr;
	std::thread td;
	int ret = 0;

	ret = SDL_Init(SDL_INIT_VIDEO);
	if (ret != 0) {
		cout << "Couldn' initialize SDL: " << SDL_GetError << endl;
		goto error;
	}

	//SDL_CreateWindow�ĵڶ���������Ϊ�����ڵ�����Ļ��
	//��λ�ã���(0, 0)��ʾλ�����Ͻ�
	wind = SDL_CreateWindow("Simplest video play sdl2",
		SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		wind_w, wind_h, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
	if (!wind) {
		cout << "SDL: couldn't create window " << SDL_GetError() << endl;
		ret = -1;
		goto error;
	}

	render = SDL_CreateRenderer(wind, -1, 0);
	if (!render) {
		cout << "SDL: couldn't create windowrender " << SDL_GetError() << endl;
		ret = -1;
		goto error;
	}

	SDL_RenderClear(render);

	SDL_SetRenderDrawColor(render,0,0,255,255);//���û�����ɫ
	SDL_RenderDrawLine(render,20,30,620,330); //����ֱ��

	SDL_SetRenderDrawColor(render, 0, 255, 0, 255);
	SDL_Rect rect;
	rect.x = 20;
	rect.y =30;
	rect.w = 600;
	rect.h = 300;
	SDL_RenderDrawRect(render, &rect); //���ƾ���

	SDL_SetRenderDrawColor(render, 0, 0, 255, 255);
	SDL_RenderDrawRect(render, &SDL_Rect{70,340,500,10}); 
	SDL_SetRenderDrawColor(render, 255, 0, 0, 255);
	SDL_RenderFillRect(render, &SDL_Rect{ 70,340,500,10 });

	SDL_RenderPresent(render);

	SDL_Event event;
	while (1)
	{
		//whileѭ�������ֻ��delay()��䣬�򴰿ڲ��ܽ�������¼�
		//whileѭ������SDL_WaitEvent��SDL_PollEvent�򶼿��Խ�������¼�
		SDL_WaitEvent(&event); //��������
		if (event.type == SDL_WINDOWEVENT)
		{
			SDL_GetWindowSize(wind, &wind_w, &wind_h);
		}
		else if (event.type == SDL_QUIT)  //��������¼�
		{
			break;
		}
	}

error:
	SDL_DestroyRenderer(render);
	SDL_DestroyWindow(wind);
	SDL_Quit();
	return 0;
}
