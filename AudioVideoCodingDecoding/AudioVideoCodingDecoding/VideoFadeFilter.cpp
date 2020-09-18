#include<iostream>
#include<fstream>
#include<cassert>
using std::cout;
using std::endl;

extern "C"
{
#include<libavformat/avformat.h>
#include<libavcodec/avcodec.h>
#include<libavutil/avutil.h>
#include<libavutil/opt.h>
#include<libswscale/swscale.h>
#include<libavutil/imgutils.h>
#include<libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
}
#pragma comment(lib,"D:/shiftmedia/msvc/lib/x64/libavdeviced.lib")
#pragma comment(lib,"D:/shiftmedia/msvc/lib/x64/libpostprocd.lib")
#pragma comment(lib,"D:/shiftmedia/msvc/lib/x64/libswscaled.lib")
#pragma comment(lib,"D:/shiftmedia/msvc/lib/x64/libswresampled.lib")
#pragma comment(lib,"D:/shiftmedia/msvc/lib/x64/libavformatd.lib")
#pragma comment(lib,"D:/shiftmedia/msvc/lib/x64/libavcodecd.lib")
#pragma comment(lib,"D:/shiftmedia/msvc/lib/x64/libavfilterd.lib")
#pragma comment(lib,"D:/shiftmedia/msvc/lib/x64/libavutild.lib")

int main(int argc, char* argv[])
{
	const char* src = "端木蓉640x360.yuv";
	const char* dst = "output_640x360_fade_filter.yuv";
	cout << "输入" << src << endl;
	cout << "输出" << dst << endl;
	std::ifstream ifs(src, std::ios::binary);
	if (!ifs)
	{
		cout << "Could not open input" << endl;
		return -1;
	}
	std::ofstream ofs(dst, std::ios::binary);
	if (!ofs)
	{
		cout << "Could not open output" << endl;
		return -1;
	}


	const int in_width = 640;
	const int in_height = 360;
	const AVPixelFormat in_pixelfmt = AV_PIX_FMT_YUV420P;
	const AVRational in_timebase = { 1,25 };
	const AVRational in_pixelaspect = { 1,1 };

	const char *filter_descr = "fade=in:0:10"; //淡入
	//const char *filter_descr = "lutyuv='u=128:v=128'"; //变为黑白
	//const char *filter_descr = "boxblur";  //模糊处理
	//const char *filter_descr = "hflip";       //水平翻转
	//const char *filter_descr = "vflip";       //竖直方向翻转
	//const char *filter_descr = "hue='h=60:s=-3'";  //变颜色
	//const char *filter_descr = "crop=2/3*in_w:2/3*in_h";  //resize为2/3
	//const char *filter_descr = "drawbox=x=100:y=100:w=100:h=100:color=pink@0.5";  //绘制粉红色矩形框
	//const char *filter_descr = "drawtext=fontfile=arial.ttf:fontcolor=green:fontsize=30:text='HELLO WORLD'"; //找不到字体

	const AVFilter* buffersrc = avfilter_get_by_name("buffer");
	const AVFilter* buffersink = avfilter_get_by_name("buffersink");
	AVFilterContext* buffersrc_ctx = nullptr;
	AVFilterContext* buffersink_ctx = nullptr;
	AVFilterInOut* output = avfilter_inout_alloc();
	AVFilterInOut* input = avfilter_inout_alloc();
	AVFilterGraph* filter_graph = avfilter_graph_alloc();

	char args[512];
	snprintf(args, sizeof(args), "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:\
pixel_aspect=%d/%d", in_width, in_height, in_pixelfmt, in_timebase.num, in_timebase.den,
in_pixelaspect.num, in_pixelaspect.den);

	int num = av_cpu_count();
	printf("av_cpu_count():%d\n", num);
	filter_graph->nb_threads = 5;
	int ret = avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in", args, nullptr, filter_graph);
	if (ret < 0)
	{
		avfilter_free(buffersrc_ctx);
		avfilter_free(buffersink_ctx);
		avfilter_inout_free(&output);
		avfilter_inout_free(&input);
		avfilter_graph_free(&filter_graph);
		cout << "Could not creat buffer source" << endl;
		return -1;
	}
	ret = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out", nullptr, nullptr, filter_graph);
	if (ret < 0)
	{
		avfilter_free(buffersrc_ctx);
		avfilter_free(buffersink_ctx);
		avfilter_inout_free(&output);
		avfilter_inout_free(&input);
		avfilter_graph_free(&filter_graph);
		cout << "Could not creat buffer sink" << endl;
		return -1;
	}

	output->name = av_strdup("in");
	output->filter_ctx = buffersrc_ctx;
	output->pad_idx = 0;
	output->next = nullptr;
	input->name = av_strdup("out");
	input->filter_ctx = buffersink_ctx;
	input->pad_idx = 0;
	input->next = nullptr;

	ret = avfilter_graph_parse_ptr(filter_graph, filter_descr, &input, &output, nullptr);
	if (ret < 0)
	{
		avfilter_free(buffersrc_ctx);
		avfilter_free(buffersink_ctx);
		avfilter_inout_free(&output);
		avfilter_inout_free(&input);
		avfilter_graph_free(&filter_graph);
		cout << "Could not parse" << endl;
		return -1;
	}

	ret = avfilter_graph_config(filter_graph, nullptr);
	if (ret < 0)
	{
		avfilter_free(buffersrc_ctx);
		avfilter_free(buffersink_ctx);
		avfilter_inout_free(&output);
		avfilter_inout_free(&input);
		avfilter_graph_free(&filter_graph);
		cout << "Could not config" << endl;
		return -1;
	}

	AVFrame* frame_in = av_frame_alloc();
	frame_in->width = in_width;
	frame_in->height = in_height;
	frame_in->format = in_pixelfmt;
	int buffersize_in = av_image_get_buffer_size(in_pixelfmt, in_width, in_height, 1);
	uint8_t* buffer_in = (uint8_t*)av_malloc(buffersize_in);
	av_image_fill_arrays(frame_in->data, frame_in->linesize, buffer_in,
		in_pixelfmt, in_width, in_height, 1);

	AVFrame* frame_out = av_frame_alloc();
	uint8_t* buffer_out = (uint8_t*)av_malloc(buffersize_in);
	av_image_fill_arrays(frame_out->data, frame_out->linesize, buffer_out,
		in_pixelfmt, in_width, in_height, 1);

	while (1)
	{
		ifs.read((char*)buffer_in, buffersize_in);
		if (!ifs)
		{
			break;
		}
		ret = av_buffersrc_add_frame(buffersrc_ctx, frame_in);
		if (ret < 0)
		{
			cout << "Couldn't add frame" << endl;
			break;
		}
		while (1)
		{
			ret = av_buffersink_get_frame(buffersink_ctx, frame_out);
			if (ret < 0)
			{
				break;
			}
			assert(frame_out->format == AV_PIX_FMT_YUV420P);

			for (int i = 0; i < frame_out->height; i++)
			{
				ofs.write((char*)frame_out->data[0] + frame_out->linesize[0] * i, frame_out->width);
			}
			for (int i = 0; i < frame_out->height / 2; i++)
			{
				ofs.write((char*)frame_out->data[1] + frame_out->linesize[1] * i, frame_out->width / 2);
			}
			for (int i = 0; i < frame_out->height / 2; i++)
			{
				ofs.write((char*)frame_out->data[2] + frame_out->linesize[2] * i, frame_out->width / 2);
			}

			av_frame_unref(frame_out);
			cout << "Process 1 frame!\n";
		}
	}

	avfilter_free(buffersrc_ctx);
	avfilter_free(buffersink_ctx);
	avfilter_inout_free(&output);
	avfilter_inout_free(&input);
	avfilter_graph_free(&filter_graph);
	av_frame_free(&frame_in);
	av_frame_free(&frame_out);
}