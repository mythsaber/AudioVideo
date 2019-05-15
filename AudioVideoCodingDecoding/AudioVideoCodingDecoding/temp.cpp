#include<iostream>
using namespace std;

extern "C"
{
#include<libavformat/avformat.h>
#include<libavcodec/avcodec.h>
#include<libavutil/avutil.h>
}

int main()
{
	auto show = [](const AVRational& x) {printf("(%d / %d)\n",x.num,x.den); };
	AVRational a = {2,3};
	AVRational b = { 1,2 };
	cout << av_q2d(a) << endl; //0.666667
	show(av_add_q(a,b));  // (7 / 6)
	show(av_sub_q(a, b)); // (1 / 6)
	show(av_mul_q(a, b)); // (1 / 3)
	show(av_div_q(a, b)); // (4 / 3)


	const char* url = "¶ËÄ¾ÈØ.mp4";
	//const char* url = "Titanic_10s.ts";
	AVFormatContext* pfmtctx = nullptr;
	pfmtctx=avformat_alloc_context();
	int ret=avformat_open_input(&pfmtctx,url,nullptr,nullptr);
	if (ret < 0)
		goto error;
	ret = avformat_find_stream_info(pfmtctx,nullptr);
	if (ret < 0)
		goto error;

error:
	avformat_close_input(&pfmtctx);
	return 0;
}