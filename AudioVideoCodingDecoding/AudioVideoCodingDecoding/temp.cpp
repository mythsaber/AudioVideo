#include<iostream>
#include<chrono>
#include<ctime>
#include<iomanip>
using std::cout;
using std::endl;

extern "C"
{
#include<libavformat/avformat.h>
#include<libavcodec/avcodec.h>
#include<libavutil/avutil.h>
}

struct TimeParam {
	time_t last_timepoint;
	int64_t ms_threshold;  //超时设置，以ms为单位
};
static void show_time(const time_t& t) {
	tm temp;
	localtime_s(&temp, &t);
	cout << std::put_time(&temp, "%x %X\n");
}
static int my_callback(void* param) //超时，则中断阻塞  
{
	time_t cur_time = time(nullptr);
	cout << "when call, time is: "; show_time(cur_time);
	TimeParam* tp = (TimeParam*)(param);
	cout << "delta time is (s): " << cur_time - tp->last_timepoint << endl;
	if (cur_time - tp->last_timepoint > tp->ms_threshold) {
		return 1;
	}
	return 0;
}
int main()
{
	const char* url = "http://10.210.63.11:7777/videos/焰灵姬.mp4";
	//如果ip或port出错，则阻塞  
	//如果ip和port正确，但访问的文件不存在，则avformat_open_input直接返回错误  
	AVFormatContext* fmtctx = avformat_alloc_context();
	TimeParam tp;
	fmtctx->interrupt_callback.callback = my_callback;
	fmtctx->interrupt_callback.opaque = &tp;  //回调函数的参数
	((TimeParam*)(fmtctx->interrupt_callback.opaque))->ms_threshold = 2;//超时阈值设置为2s 
	((TimeParam*)(fmtctx->interrupt_callback.opaque))->last_timepoint = time(nullptr); //调用之前初始化时间
	cout << "before call, time is: "; show_time(tp.last_timepoint);
	int ret = avformat_open_input(&fmtctx, url, nullptr, nullptr);
	if (ret < 0) {
		cout << "Failed to call avformat_open_input" << endl;
		goto error;
	}
	else {
		cout << "succeed in calling avformat_open_input" << endl;
	}

	cout << "------avformat_find_stream_info start-------" << endl;
	ret = avformat_find_stream_info(fmtctx, nullptr);
	if (ret < 0)
		goto error;
	cout << "------avformat_find_stream_info end-------" << endl;

	AVPacket pkt;
	av_init_packet(&pkt);
	pkt.data = nullptr;
	pkt.size = 0;
	((TimeParam*)(fmtctx->interrupt_callback.opaque))->ms_threshold = 2;//超时阈值设置为2s
	((TimeParam*)(fmtctx->interrupt_callback.opaque))->last_timepoint = time(nullptr);;  //调用之前初始化时间
	cout << "before call, time is: "; show_time(tp.last_timepoint);
	if (av_read_frame(fmtctx, &pkt) < 0)
	{
		cout << "Failed to call av_read_frame" << endl;
		goto error;
	}
	else
		cout << "succeed in calling av_read_frame" << endl;
error:
	avformat_close_input(&fmtctx);
	return 0;
}
