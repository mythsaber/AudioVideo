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

#pragma comment(lib,"D:/shiftmedia/msvc/lib/x64/libavcodecd.lib")
//#pragma comment(lib,"D:/shiftmedia/msvc/lib/x64/libavdeviced.lib")  //使用ffmpegd必须屏蔽该静态库，否则与SDL冲突
#pragma comment(lib,"D:/shiftmedia/msvc/lib/x64/libavfilterd.lib")
#pragma comment(lib,"D:/shiftmedia/msvc/lib/x64/libavformatd.lib")
#pragma comment(lib,"D:/shiftmedia/msvc/lib/x64/libavutild.lib")
#pragma comment(lib,"D:/shiftmedia/msvc/lib/x64/libpostprocd.lib")
#pragma comment(lib,"D:/shiftmedia/msvc/lib/x64/libswresampled.lib")
#pragma comment(lib,"D:/shiftmedia/msvc/lib/x64/libswscaled.lib")

#include <string>
#include <iostream>
#include <vector>
#include <cassert>

static const std::string arg = "time_base=1/44100:sample_rate=44100:sample_fmt=fltp:channel_layout=0x3";
static const std::string outstr = "aresample=48000,aformat=sample_fmts=s16:channel_layouts=stereo";

class AudioDecoder
{
public:
	AudioDecoder() :frameidx(0)
	{
		//do nothing
	}
	~AudioDecoder()
	{
		av_packet_free(&packet); //等价于av_packet_unref(packet)+av_freep(packet)
		avcodec_close(codecctx);
		avformat_close_input(&formatctx);
	}

	//返回解码后的下一帧视频像素数据，调用者负责释放返回的AVFrame的释放
	AVFrame* next_frame()
	{
		AVFrame* frame = av_frame_alloc();
		while (1)
		{
			av_packet_unref(packet);

			if (av_read_frame(formatctx, packet) != 0)
			{
				return NULL;
			}

			if (packet->stream_index != audioidx)
			{
				continue;
			}

			if (avcodec_send_packet(codecctx, packet) != 0)
			{
				printf("Fail in sending packet.\n");
				exit(-1);
			}

			int errorcode = avcodec_receive_frame(codecctx, frame);
			if (errorcode == 0)
			{
				break;
			}
			else if (errorcode == AVERROR(EAGAIN))
			{
				//avcodec_receive_frame可能会返回AVERROR(EAGAIN)，此时
				//output is not available in this state - user must
				//try to send new input
				continue;
			}
			else
			{
				printf("Errorcode returned by avcodec_"
					"receive_frame is %d.\n", errorcode);
				printf("Fail in receiving.\n");
				exit(-1);
			}
		}
		printf("Decoded frame index:%d\n", frameidx);
		frameidx++;

		return frame;
	}

	void init(const std::string& path)
	{
		filepath = path;
		formatctx = avformat_alloc_context();
		if (avformat_open_input(&formatctx, filepath.c_str(),
			nullptr, nullptr) != 0)
		{
			std::cout << "Couldn't open input stream" << std::endl;
			exit(-1);
		}

		if (avformat_find_stream_info(formatctx, nullptr) < 0)
		{
			std::cout << "Couldn't find stream information" << std::endl;
			exit(-1);
		}

		audioidx = -1;
		for (int i = 0; i < (int)formatctx->nb_streams; i++)
		{
			if (formatctx->streams[i]->codecpar->codec_type
				== AVMEDIA_TYPE_AUDIO)
			{
				audioidx = i;
				break;
			}
		}
		if (audioidx == -1)
		{
			std::cout << "Couldn't find a video stream" << std::endl;
			exit(-1);
		}

		AVCodecParameters* codecpar = formatctx->streams[audioidx]
			->codecpar;

		codecctx = avcodec_alloc_context3(nullptr);
		if (avcodec_parameters_to_context(codecctx, codecpar) < 0)
		{
			printf("Fail in cvting parameters to context.\n");
			exit(-1);
		}

		//返回一个匹配codec_id的解码器
		AVCodec* codec = avcodec_find_decoder(codecpar->codec_id);
		if (codec == NULL)
		{
			std::cout << "Couldn't find decoder" << std::endl;
			exit(-1);
		}

		//打开解码器AVCodec
		if (avcodec_open2(codecctx, codec, nullptr) < 0)
		{
			std::cout << "Couldn't open codec" << std::endl;
			exit(-1);
		}

		packet = av_packet_alloc();

		std::cout << "AudioDecoder init Done" << std::endl;
	}
private:
	std::string filepath;

	int frameidx;

	AVFormatContext* formatctx;
	int audioidx;
	AVCodecContext* codecctx;
	AVPacket* packet;
};

class AudioResampleFilter
{
public:
	AudioResampleFilter() {}
	~AudioResampleFilter()
	{
		if (m_pBufferSrc)
		{
			avfilter_free(m_pBufferSrc);
			m_pBufferSrc = NULL;
		}
		if (m_pBufferSink)
		{
			avfilter_free(m_pBufferSink);
			m_pBufferSink = NULL;
		}
		if (m_pGraph)
		{
			avfilter_graph_free(&m_pGraph);
		}
	}
	bool init()
	{
		const AVFilter* abuffersrc = avfilter_get_by_name("abuffer");
		const AVFilter* abuffersink = avfilter_get_by_name("abuffersink");
		AVFilterInOut* outputs = avfilter_inout_alloc();
		AVFilterInOut* inputs = avfilter_inout_alloc();
		m_pGraph = avfilter_graph_alloc();
		int num = av_cpu_count();
		printf("av_cpu_count():%d\n", num);
		m_pGraph->nb_threads = 5;
		int ret = avfilter_graph_create_filter(&m_pBufferSrc, abuffersrc, "in", arg.c_str(), NULL, m_pGraph);
		if (ret < 0)
		{
			std::cout << "Failed to call avfilter_graph_create_filter" << std::endl;
			avfilter_graph_free(&m_pGraph);
			avfilter_inout_free(&inputs);
			avfilter_inout_free(&outputs);
			return false;
		}

		ret = avfilter_graph_create_filter(&m_pBufferSink, abuffersink, "out", NULL, NULL, m_pGraph);
		if (ret < 0)
		{
			std::cout << "Failed to call avfilter_graph_create_filter" << std::endl;
			avfilter_free(m_pBufferSrc);
			m_pBufferSrc = NULL;
			avfilter_graph_free(&m_pGraph);
			avfilter_inout_free(&inputs);
			avfilter_inout_free(&outputs);
			return false;
		}

		//这些设置似乎没有必要
		//enum AVSampleFormat out_sample_fmts[] = { AV_SAMPLE_FMT_S16, AV_SAMPLE_FMT_NONE };
		//int64_t out_channel_layouts[] = { 3, -1 };
		//int out_sample_rates[] = { 44100, -1 };
		//ret = av_opt_set_int_list(m_pBufferSink, "sample_fmts", out_sample_fmts, -1, AV_OPT_SEARCH_CHILDREN);
		//if (ret < 0)
		//{
		//	std::cout << "Failed to set  sample_fmts" << std::endl;
		//	avfilter_free(m_pBufferSrc);
		//	m_pBufferSrc = NULL;
		//	avfilter_free(m_pBufferSink);
		//	m_pBufferSink = NULL;
		//	avfilter_graph_free(&m_pGraph);
		//	avfilter_inout_free(&inputs);
		//	avfilter_inout_free(&outputs);
		//	return false;
		//}
		//ret = av_opt_set_int_list(m_pBufferSink, "channel_layouts", out_channel_layouts, -1, AV_OPT_SEARCH_CHILDREN);
		//if (ret < 0)
		//{
		//	std::cout << "Failed to set out_channel_layouts" << std::endl;
		//	avfilter_free(m_pBufferSrc);
		//	m_pBufferSrc = NULL;
		//	avfilter_free(m_pBufferSink);
		//	m_pBufferSink = NULL;
		//	avfilter_graph_free(&m_pGraph);
		//	avfilter_inout_free(&inputs);
		//	avfilter_inout_free(&outputs);
		//	return false;
		//}
		//ret = av_opt_set_int_list(m_pBufferSink, "sample_rates", out_sample_rates, -1, AV_OPT_SEARCH_CHILDREN);
		//if (ret < 0)
		//{
		//	std::cout << "Failed to set out_sample_rates" << std::endl;
		//	avfilter_free(m_pBufferSrc);
		//	m_pBufferSrc = NULL;
		//	avfilter_free(m_pBufferSink);
		//	m_pBufferSink = NULL;
		//	avfilter_graph_free(&m_pGraph);
		//	avfilter_inout_free(&inputs);
		//	avfilter_inout_free(&outputs);
		//	return false;
		//}

		outputs->name = av_strdup("in");
		outputs->filter_ctx = m_pBufferSrc;
		outputs->pad_idx = 0;
		outputs->next = NULL;

		inputs->name = av_strdup("out");
		inputs->filter_ctx = m_pBufferSink;
		inputs->pad_idx = 0;
		inputs->next = NULL;

		if ((ret = avfilter_graph_parse_ptr(m_pGraph, outstr.c_str(), &inputs, &outputs, NULL)) < 0)
		{
			std::cout << "Failed to call avfilter_graph_parse_ptr" << std::endl;
			avfilter_free(m_pBufferSrc);
			m_pBufferSrc = NULL;
			avfilter_free(m_pBufferSink);
			m_pBufferSink = NULL;
			avfilter_graph_free(&m_pGraph);
			avfilter_inout_free(&inputs);
			avfilter_inout_free(&outputs);
			return false;
		}

		//打印graph中的AVFilterContext
		for (int i = 0; i < m_pGraph->nb_filters; i++)
		{
			AVFilterContext* cur = m_pGraph->filters[i];
			printf("AVFilterContext [%d], name: %s\n", i, cur->name);
		}
		//这里只考虑单链连接
		printf("connection: ");
		AVFilterContext* first = NULL;
		for (int i = 0; i < m_pGraph->nb_filters; i++)
		{
			AVFilterContext* cur = m_pGraph->filters[i];
			if (cur->nb_inputs == 0)
			{
				assert(!first);
				first = cur;
			}
		}
		assert(first);
		printf("%s", first->name);
		AVFilterContext* next = first;
		while (1)
		{
			if (next->outputs != 0)
			{
				next = next->outputs[0]->dst;
				printf(" -> %s", next->name);
			}
			else
			{
				break;
			}
		}


		if ((ret = avfilter_graph_config(m_pGraph, NULL)) < 0)
		{
			std::cout << "Failed to call avfilter_graph_config" << std::endl;
			avfilter_free(m_pBufferSrc);
			m_pBufferSrc = NULL;
			avfilter_free(m_pBufferSink);
			m_pBufferSink = NULL;
			avfilter_graph_free(&m_pGraph);
			avfilter_inout_free(&inputs);
			avfilter_inout_free(&outputs);
			return false;
		}
		return true;
	}
	std::vector<AVFrame*> FilterFrame(AVFrame* ff_in_frame)
	{
		if (av_buffersrc_add_frame_flags(m_pBufferSrc, ff_in_frame, AV_BUFFERSRC_FLAG_PUSH | AV_BUFFERSRC_FLAG_KEEP_REF) < 0)
		{
			printf("Failed to feeding the filtergraph. \n");
			return std::vector<AVFrame*>();
		}

		std::vector<AVFrame*> ret;
		AVFrame* ff_out_frame = av_frame_alloc();
		do
		{
			if (av_buffersink_get_frame_flags(m_pBufferSink, ff_out_frame, AV_BUFFERSINK_FLAG_NO_REQUEST) < 0)
			{
				break;
			}
			ret.push_back(ff_out_frame);
		} while (true);
		return ret;
	}
private:
	AVFilterContext*	m_pBufferSrc;
	AVFilterContext*	m_pBufferSink;
	AVFilterGraph*		m_pGraph;
};

void listAllFilter()
{
	const AVFilter *f = NULL;
	void *opaque = 0;
	int cnt = 0;
	printf("filter number: name    flag\n");
	while ((f = av_filter_iterate(&opaque)))
	{
		printf("%d:               %s      %d\n", cnt, f->name, f->flags);
		cnt++;
	}
}

int main2()
{
	//listAllFilter(); return 0;

	//av_log_set_level(AV_LOG_DEBUG);
	std::string path = "C:\\Users\\myth\\Videos\\forTest\\h264_cbr_420p_stride_768_384.ts";
	AudioDecoder decoder;
	decoder.init(path);

	AudioResampleFilter filter;
	filter.init();
	AVFrame* frame;
	while ((frame = decoder.next_frame()) != NULL)
	{
		std::vector<AVFrame*>ret = filter.FilterFrame(frame);
		av_frame_free(&frame);
		for (auto& i : ret)
		{
			av_frame_free(&i);
		}
	}
	return 0;
}