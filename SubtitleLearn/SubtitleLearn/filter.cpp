#include<iostream>
#include<string>
#include<cassert>
#include<vector>
#include<fstream>
using std::vector;
using std::cout;
using std::endl;
using std::string;
using std::ofstream;
using std::string;

extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavutil/dict.h>
#include <libavutil/opt.h>
#include<libavfilter/avfilter.h>
#include<libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
}

//Source Filter：没有输入端的滤镜
//Sink Filter：没有输出端的滤镜
//Filter：既有输入端，又有输出端
int main(int argc, char* argv[])
{
	const char* url = "doctor.mp4";
	int ret;
	AVFormatContext* fmtctx = avformat_alloc_context();
	ret =avformat_open_input(&fmtctx,url,nullptr,nullptr);
	if (ret < 0)
	{
		cout << "Failed to call avformat_open_input" << endl;
		avformat_close_input(&fmtctx);
		return -1;
	}
	ret=avformat_find_stream_info(fmtctx,nullptr);
	if (ret < 0)
	{
		avformat_close_input(&fmtctx);
		cout << "Failed to call avformat_find_stream_info" << endl;
		return -1;
	}
	int idx=av_find_best_stream(fmtctx,AVMEDIA_TYPE_VIDEO,-1,
		-1,nullptr,0);
	if (idx < 0)
	{
		avformat_close_input(&fmtctx);
		cout << "Failed to call av_find_best_stream" << endl;
		return -1;
	}
	AVCodecParameters* codecpar = fmtctx->streams[idx]->codecpar;
	AVCodec* codec = avcodec_find_decoder(codecpar->codec_id);
	if (codec == nullptr)
	{
		avformat_close_input(&fmtctx);
		cout << "Failed to call avcodec_find_decoder" << endl;
		return -1;
	}
	AVCodecContext* codecctx = avcodec_alloc_context3(codec);
	ret=avcodec_parameters_to_context(codecctx,codecpar);
	if (ret < 0)
	{
		avformat_close_input(&fmtctx);
		avcodec_close(codecctx);
	    avcodec_free_context(&codecctx);
		cout << "Failed to call avcodec_parameters_to_context" << endl;
		return -1;
	}
	//如果采用旧版的fmtctx->streams[idx]->codec，则以下两个成员无须手动设置
	codecctx->pkt_timebase = fmtctx->streams[idx]->time_base;
	codecctx->framerate = fmtctx->streams[idx]->avg_frame_rate;
	if (avcodec_open2(codecctx, codec, nullptr) < 0)
	{
		avformat_close_input(&fmtctx);
		avcodec_close(codecctx);
		avcodec_free_context(&codecctx);
		cout << "Failed to call avcodec_open2" << endl;
		return -1;
	}

    //创建AVFilterContext结构体，存储input与output的Filter信息
	const AVFilter* buffer = avfilter_get_by_name("buffer");
	const AVFilter* buffersink = avfilter_get_by_name("buffersink");
	AVFilterContext* buffersink_ctx;
	AVFilterContext* buffer_ctx;
	constexpr int size = 1024;
	char args[size];
	snprintf(args,size,"video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:\
pixel_aspect=%d/%d",codecctx->width,codecctx->height,
codecctx->pix_fmt,
fmtctx->streams[idx]->time_base.num, fmtctx->streams[idx]->time_base.den,
codecctx->sample_aspect_ratio.num,codecctx->sample_aspect_ratio.den);
	AVFilterGraph* filter_graph = avfilter_graph_alloc();
	ret = avfilter_graph_create_filter(&buffer_ctx,buffer,"in",
		args,nullptr,filter_graph);
	//"in"表示buffer在整个Graph中叫做'in'。 名称可以随便叫，只要保证唯一不重复就好。
	if (ret < 0)
	{
		avformat_close_input(&fmtctx);
		avcodec_close(codecctx);
		avcodec_free_context(&codecctx);
		avfilter_free(buffer_ctx);
		avfilter_graph_free(&filter_graph);
		cout << "Failed to call vfilter_graph_create_filter in" << endl;
		return -1;
	}

	ret = avfilter_graph_create_filter(&buffersink_ctx,buffersink,
		"out",nullptr,nullptr,filter_graph);
	if (ret < 0)
	{
		avformat_close_input(&fmtctx);
		avcodec_close(codecctx);
		avcodec_free_context(&codecctx);
		avfilter_free(buffer_ctx);
		avfilter_free(buffersink_ctx);
		avfilter_graph_free(&filter_graph);
		cout << "Failed to call vfilter_graph_create_filter out" << endl;
		return -1;
	}

	//设置一些其他与Filter相关的参数
	//如设置 AVFilterContext 的输出的pix_fmt参数
	enum AVPixelFormat pix_fmts[] = { AV_PIX_FMT_YUV420P, AV_PIX_FMT_NONE };
	ret = av_opt_set_int_list(buffersink_ctx,"pix_fmts",
		pix_fmts,AV_PIX_FMT_NONE,
		AV_OPT_SEARCH_CHILDREN);
	if (ret < 0)
	{
		avformat_close_input(&fmtctx);
		avcodec_close(codecctx);
		avcodec_free_context(&codecctx);
		avfilter_free(buffer_ctx);
		avfilter_free(buffersink_ctx);
		avfilter_graph_free(&filter_graph);
		cout << "Failed to call av_opt_set_int_list" << endl;
		return -1;
	}

	/*
	* The buffer source output must be connected to the input pad of
	* the first filter described by filters_descr; since the first
	* filter input label is not specified, it is set to "in" by
	* default.
	*/
	AVFilterInOut* outputs = avfilter_inout_alloc();
	outputs->name = av_strdup("in");
	outputs->filter_ctx = buffer_ctx;
	outputs->pad_idx = 0;
	outputs->next = nullptr;

	/*
	* The buffer sink input must be connected to the output pad of
	* the last filter described by filters_descr; since the last
	* filter output label is not specified, it is set to "out" by
	* default.
	*/
	AVFilterInOut* inputs = avfilter_inout_alloc();
	inputs->name = av_strdup("out");
	inputs->filter_ctx = buffersink_ctx;
	inputs->pad_idx = 0;
	inputs->next = nullptr;

	//建立滤镜解析器
	const char* filter_descr = "movie=logo.png[logo];\
[logo]colorkey=White:0.5:0.1[alphawm];\
[in][alphawm]overlay=0:0[out]";
	ret=avfilter_graph_parse_ptr(filter_graph,filter_descr,&inputs,&outputs,nullptr);
	if (ret < 0)
	{
		avformat_close_input(&fmtctx);
		avcodec_close(codecctx);
		avcodec_free_context(&codecctx);
		avfilter_free(buffer_ctx);
		avfilter_free(buffersink_ctx);
		avfilter_graph_free(&filter_graph);
		avfilter_inout_free(&inputs);
		avfilter_inout_free(&outputs);
		cout << "Failed to call  avfilter_graph_parse_ptr" << endl;
		return -1;
	}

	//检查有效性以及 graph中所有链路和格式的配置
	ret=avfilter_graph_config(filter_graph,nullptr);
	if (ret < 0)
	{
		avformat_close_input(&fmtctx);
		avcodec_close(codecctx);
		avcodec_free_context(&codecctx);
		avfilter_free(buffer_ctx);
		avfilter_free(buffersink_ctx);
		avfilter_graph_free(&filter_graph);
		avfilter_inout_free(&inputs);
		avfilter_inout_free(&outputs);
		cout << "Failed to call  avfilter_graph_config" << endl;
		return -1;
	}

	//数据解码，解码完成后需要对解码后的每一帧进行滤镜操作
	AVPacket packet;
	av_init_packet(&packet);
	packet.data = nullptr;
	packet.size = 0;
	AVFrame* frame = av_frame_alloc();
	AVFrame* filt_frame = av_frame_alloc();

	const std::string dstpath_yuv = "output_" +
		std::to_string(codecctx->width) +
		"x" + std::to_string(codecctx->height) + ".yuv";
	cout << "输出" << dstpath_yuv << endl;
	std::ofstream ofs_yuv(dstpath_yuv, std::ios::binary);

	while (av_read_frame(fmtctx, &packet) == 0)
	{
		if (packet.stream_index != idx)
		{
			av_packet_unref(&packet);
			continue;
		}
		if (packet.data == nullptr || packet.size == 0)
			assert(packet.data != nullptr && packet.size != 0);
		ret = avcodec_send_packet(codecctx, &packet);
		if (ret == 0)
			;
		else if (ret == AVERROR(EAGAIN))
			assert(ret != AVERROR(EAGAIN));
		else if (ret == AVERROR_EOF)
			assert(ret != AVERROR_EOF);
		else
		{
			avformat_close_input(&fmtctx);
			avcodec_close(codecctx);
			avcodec_free_context(&codecctx);
			avfilter_free(buffer_ctx);
			avfilter_free(buffersink_ctx);
			avfilter_graph_free(&filter_graph);
			av_frame_free(&frame);
			av_frame_free(&filt_frame);
			avfilter_inout_free(&inputs);
			avfilter_inout_free(&outputs);
			cout << "Failed to call  avcodec_send_packet" << endl;
			return -1;
		}
		while (1)
		{
			ret = avcodec_receive_frame(codecctx, frame);
			if (ret == 0)
				;
			else if (ret == AVERROR(EAGAIN))
				break;
			else if (ret == AVERROR_EOF)
				assert(ret != AVERROR_EOF);
			else
			{
				avformat_close_input(&fmtctx);
				avcodec_close(codecctx);
				avcodec_free_context(&codecctx);
				avfilter_free(buffer_ctx);
				avfilter_free(buffersink_ctx);
				avfilter_graph_free(&filter_graph);
				av_frame_free(&frame);
				av_frame_free(&filt_frame);
				avfilter_inout_free(&inputs);
				avfilter_inout_free(&outputs);
				cout << "Failed to call  avcodec_receive_frame" << endl;
				return -1;
			}

			//-----------------------------------滤镜处理-----------------------------------
			/* push the decoded frame into the filtergraph 向Filter Graph加入一帧数据*/
			ret = av_buffersrc_add_frame_flags(buffer_ctx, frame, AV_BUFFERSRC_FLAG_KEEP_REF) < 0;
			if (ret < 0)
			{
				avformat_close_input(&fmtctx);
				avcodec_close(codecctx);
				avcodec_free_context(&codecctx);
				avfilter_free(buffer_ctx);
				avfilter_free(buffersink_ctx);
				avfilter_graph_free(&filter_graph);
				av_frame_free(&frame);
				av_frame_free(&filt_frame);
				avfilter_inout_free(&inputs);
				avfilter_inout_free(&outputs);
				cout << "av_buffersrc_add_frame_flags" << endl;
			}
			while (1)
			{
				/* pull filtered frames from the filtergraph 从Filter Graph取出一帧数据*/
				ret = av_buffersink_get_frame(buffersink_ctx, filt_frame);
				if (ret >= 0)
					;
				else if (ret == AVERROR(EAGAIN))
					break;
				else if (ret == AVERROR_EOF)
					break;
				else
				{
					avformat_close_input(&fmtctx);
					avcodec_close(codecctx);
					avcodec_free_context(&codecctx);
					avfilter_free(buffer_ctx);
					avfilter_free(buffersink_ctx);
					avfilter_graph_free(&filter_graph);
					av_frame_free(&frame);
					av_frame_free(&filt_frame);
					avfilter_inout_free(&inputs);
					avfilter_inout_free(&outputs);
					cout << "Failed to call  av_buffersink_get_frame" << endl;
				}

				//前文设置了buffersink_ctx的pix_fmt参数
				assert(filt_frame->format == (int)AV_PIX_FMT_YUV420P);
				ofs_yuv.write((char*)filt_frame->data[0],
					codecctx->width*codecctx->height);   //y
				ofs_yuv.write((char*)filt_frame->data[1],
					codecctx->width*codecctx->height / 4);  //u
				ofs_yuv.write((char*)filt_frame->data[2],
					codecctx->width*codecctx->height / 4);   //v

				av_frame_unref(filt_frame);
			}

			av_frame_unref(frame);
		}
	}

	if (packet.size == 0 || packet.data == 0)
	{
		//是否需要刷新
	}

	avformat_close_input(&fmtctx);
	avcodec_close(codecctx);//Close a given AVCodecContext and free all the data associated with it*(but not the AVCodecContext itself).
	avcodec_free_context(&codecctx);
	avfilter_free(buffer_ctx);
	avfilter_free(buffersink_ctx);
	avfilter_graph_free(&filter_graph);
	av_frame_free(&frame);
	av_frame_free(&filt_frame);
	avfilter_inout_free(&inputs);
	avfilter_inout_free(&outputs);
}