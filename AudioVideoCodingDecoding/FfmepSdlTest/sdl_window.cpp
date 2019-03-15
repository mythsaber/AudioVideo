#include<SDL.h>
#include<iostream>
#include<opencv.hpp>
using std::cout;
using std::endl;
int main(int argc,char* argv[])
{
	int windw = 640;
	int windh = 480;
	SDL_Window* wind = SDL_CreateWindow("2019-01-17",
		SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,
		windw,windh,
		SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE);

	//SDL_CreateWindow�����Ĵ���һ�����ţ�
	//��SDL_Delay���������ʱ�䣬����OpenCV��cv::waitKey()
	//��SDL_Delay(0)�����0ms��������һֱ����
	SDL_Delay(200);

	SDL_Renderer* render = SDL_CreateRenderer(wind,
		-1,0);

	cv::Mat src = cv::imread("bv.jpg");
	if (src.empty()) {
		cout << "Couldn't not read img" << endl;return -1;
	}
	//imshow��ʾ��ʵͼƬ������תΪcv::COLOR_BGR2RGB
	cv::imshow("bgr", src); cv::waitKey(0); 
	
	uint32_t format = SDL_PIXELFORMAT_BGR24;
	SDL_Texture* texture = SDL_CreateTexture(render, format, 
		SDL_TEXTUREACCESS_STATIC,
		src.cols, src.rows);

	//����pitchΪThe number of bytes in a row of pixel data, 
	//including padding between lines����SDL_PIXELFORMAT_BGR24��
	//Ϊͼ������3����SDL_PIXELFORMAT_IYUVΪͼ���
	SDL_UpdateTexture(texture, nullptr, src.data, src.cols*3);//***
	SDL_Rect rect;
	rect.x = windw / 10;
	rect.y = windh / 10;
	rect.w = windw * 8 / 10;
	rect.h = windh * 8 / 10;

	SDL_RenderClear(render);
	SDL_RenderCopy(render, texture, nullptr, &rect);
	SDL_RenderPresent(render);  //
	
	cout << "After prenset" << endl;
	while (1)
	{
		SDL_Delay(40);
	}

	cout << "over" << endl;
	return 0;
}