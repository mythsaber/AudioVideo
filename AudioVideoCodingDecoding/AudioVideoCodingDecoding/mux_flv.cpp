#include <iostream>
#include <fstream>
#include <cassert>
#pragma comment(lib,"D:/shiftmedia/msvc/lib/x64/libavcodecd.lib")
#pragma comment(lib,"D:/shiftmedia/msvc/lib/x64/libavdeviced.lib")
#pragma comment(lib,"D:/shiftmedia/msvc/lib/x64/libavfilterd.lib")
#pragma comment(lib,"D:/shiftmedia/msvc/lib/x64/libavformatd.lib")
#pragma comment(lib,"D:/shiftmedia/msvc/lib/x64/libavutild.lib")
#pragma comment(lib,"D:/shiftmedia/msvc/lib/x64/libpostprocd.lib")
#pragma comment(lib,"D:/shiftmedia/msvc/lib/x64/libswresampled.lib")
#pragma comment(lib,"D:/shiftmedia/msvc/lib/x64/libswscaled.lib")

extern "C"
{
#include<libavformat/avformat.h>
#include<libavcodec/avcodec.h>
#include<libswscale/swscale.h>
#include<libswresample/swresample.h>
#include<libavutil/avutil.h>
#include<libavutil/opt.h> //AVOption
}

using std::cout;
using std::endl;

class MediaReader
{
private:
	AVFormatContext* m_fmtctx;
	bool opened;
	int m_videost_idx;
public:
	MediaReader(const char* path) {
		m_fmtctx = avformat_alloc_context();
		int ret = avformat_open_input(&m_fmtctx, path, nullptr, nullptr);
		opened = ret < 0 ? false : true;
		if (opened)
		{
			ret = avformat_find_stream_info(m_fmtctx, nullptr);
			opened = ret < 0 ? false : true;
		}
		if (opened)
		{
			m_videost_idx = av_find_best_stream(m_fmtctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
		}
	}
	operator bool() const//是否成功打开文件
	{
		return opened;
	}
	//读一个AVPacket，return 0 if OK, < 0 on error or end of file
	int read_video_packet(AVPacket& pkt)
	{
		if (!opened) return -1;
		while (1)
		{
			int ret = av_read_frame(m_fmtctx, &pkt);
			if (ret < 0)
				return ret;
			if (pkt.stream_index == m_videost_idx)
				return 0;
		}
	}

	//读取的视频流的time_base
	AVRational get_timebase()
	{
		if (!opened) return{ 0,0 };
		return m_fmtctx->streams[m_videost_idx]->time_base;
	}

	//读取的视频流
	const AVStream* get_stream()
	{
		if (!opened) return nullptr;
		return m_fmtctx->streams[m_videost_idx];
	}

	~MediaReader()
	{
		avformat_close_input(&m_fmtctx);
	}
};

int mux_flv_h264()
{
	const char* file_path = "output_encode.flv";

	MediaReader reader("girl_640x360.mp4");
	if (reader) { ; }
	if (!reader) {
		cout << "Failed to open file" << endl;
		return -1;
	}

	AVFormatContext* output_fmtctx = nullptr;
	//分配AVFormatContext，根据filename的扩展名填写成员AVOutputFormat
	//此时，成员AVIOContext* pb为nullptr
	int ret = avformat_alloc_output_context2(&output_fmtctx, nullptr, nullptr, file_path);
	if (ret < 0) {
		cout << "Failed to call avformat_alloc_output_context2" << endl;
		avformat_free_context(output_fmtctx);
		return -1;
	}

	//创建并初始化用于访问文件file_path的AVIOContext，并打开文件
	ret = avio_open2(&output_fmtctx->pb, file_path, AVIO_FLAG_WRITE, &output_fmtctx->interrupt_callback, nullptr);
	if (ret < 0) {
		cout << "Failed to call avio_open2" << endl;
		avio_close(output_fmtctx->pb);
		avformat_free_context(output_fmtctx);
		return -1;
	}

	//向AVFormatContext增加一个AVStream.申请AVStream，并分配codec、codecpar等成员的内存
	AVStream* st = avformat_new_stream(output_fmtctx, nullptr);
	if (st == nullptr) {
		cout << "Failed to call avformat_new_stream" << endl;
		avio_close(output_fmtctx->pb);
		avformat_free_context(output_fmtctx);
		return -1;
	}
	av_dict_copy(&st->metadata, reader.get_stream()->metadata, AV_DICT_DONT_OVERWRITE);

	//Method 1，手动设置输出流的codecpar
	auto set_codecpar_method1 = [&]()
	{
		//不需要手动设置st->time-base，即使设置，也会被avformat_write_header里设置的值覆盖
		st->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
		/*
		此时，output_fmtctx->video_codec_id为AV_CODEC_ID_NONE
		因此不能st->codecpar->codec_id = output_fmtctx->video_codec_id;
		如果，st->codecpar->codec_id = av_guess_codec(output_fmtctx->oformat,nullptr,file_path,nullptr,AVMEDIA_TYPE_VIDEO);
		则对.flv扩展名，av_guess_codec返回AV_CODEC_ID_FLV1；
		对.h264扩展名，av_guess_codec返回AV_CODEC_ID_H264
		这里手动设置，如下
		*/
		assert(reader.get_stream()->codecpar->codec_id == AV_CODEC_ID_H264);
		st->codecpar->codec_id = AV_CODEC_ID_H264; //27
		st->codecpar->width = 640;
		st->codecpar->height = 360;
		const AVStream* inst = reader.get_stream();
		st->codecpar->format = inst->codecpar->format;
		av_free(st->codecpar->extradata);
		/*必须设置st->codecpar->extradata，否则播放生成的flv时报错：
		[NULL @ 00000146674BC400] missing picture in access unit with size 23100
		[AVBSFContext @ 000001465F1CDD80] No start code is found.
		output_encode.flv: could not find codec parameters
		*/
		st->codecpar->extradata_size = inst->codecpar->extradata_size;
		st->codecpar->extradata = (unsigned char*)av_malloc(inst->codecpar->extradata_size + AV_INPUT_BUFFER_PADDING_SIZE);
		memcpy_s(st->codecpar->extradata, st->codecpar->extradata_size,
			inst->codecpar->extradata, inst->codecpar->extradata_size);
	};

	//Method 2，拷贝输入流的codecpar至输出流，并单独设置codecpar->codec_tag 
	//与Method 1相比，Method 2还额外设置了bit_rate、bit_per_coded_sample、video_delay这些值
	auto set_codecpar_method2 = [&]()
	{
		//st->codecpar->codec_tag是0
		uint32_t tag_backup = st->codecpar->codec_tag;
		//reader.get_stream()->codecpar->codec_tag是828601953，即0x31637661，即"avc1"
		avcodec_parameters_copy(st->codecpar, reader.get_stream()->codecpar);
		//输出流的codecpar->codec_tag不能拷贝输入流的，
		//否则avformat_write_header失败，报错Tag avc1 incompatible with output codec id '27' ([7][0][0][0])
		//因为codec_tag为"avc1"和codec_id为AV_CODEC_ID_H264（27）这个组合并不在
		//FVL的AVFormatContext->oformat->codec_tag数组内，即对flv封装格式来说二者相矛盾
		st->codecpar->codec_tag = tag_backup; //st->codecpar->codec_tag是0，则只看 st->codecpar->codec_id
											  //avc1是无起始码的，AV_CODEC_ID_H264是带起始码的
	};

	set_codecpar_method2(); //set_codecpar_method1()亦可

	ret = avformat_write_header(output_fmtctx, nullptr);
	if (ret < 0)
	{
		cout << "Failed to call avformat_write_header" << endl;
		avio_close(output_fmtctx->pb);
		avformat_free_context(output_fmtctx);
		return -1;
	}


	AVPacket pkt;
	av_init_packet(&pkt);
	pkt.data = nullptr;
	pkt.size = 0;
	AVRational origin_timebase = reader.get_timebase();
	while (av_packet_unref(&pkt), reader.read_video_packet(pkt) == 0)
	{
		pkt.stream_index = 0;
		pkt.pts = av_rescale_q(pkt.pts, origin_timebase, output_fmtctx->streams[0]->time_base);
		pkt.dts = av_rescale_q(pkt.dts, origin_timebase, output_fmtctx->streams[0]->time_base);
		pkt.duration = av_rescale_q(pkt.duration, origin_timebase, output_fmtctx->streams[0]->time_base);
		int ret = av_write_frame(output_fmtctx, &pkt);
		if (ret < 0)
		{
			avio_close(output_fmtctx->pb);
			avformat_free_context(output_fmtctx);
			cout << "Failed to call av_write_frame" << endl;
			return -1;
		}
	}
	av_write_trailer(output_fmtctx);

	avio_close(output_fmtctx->pb);
	avformat_free_context(output_fmtctx);
	return 0;
}



int main(int argc, char* argv[])
{
	mux_flv_h264();
}