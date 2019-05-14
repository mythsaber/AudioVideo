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
}

string format_time(int64_t ms)//将毫秒数1137661转为时:分:秒.毫秒0:18:57,661
{
	int64_t ss = 1000;
	int64_t mi = ss * 60;
	int64_t hh = mi * 60;
	int64_t dd = hh * 24;

	int64_t day = ms / dd;
	int64_t hour = (ms - day * dd) / hh;
	int64_t minute = (ms - day * dd - hour * hh) / mi;
	int64_t second = (ms - day * dd - hour * hh - minute * mi) / ss;
	int64_t millisecond = ms - day * dd - hour * hh - minute * mi - second * ss;

	string hou = std::to_string(hour);
	string min = std::to_string(minute);
	string sec = std::to_string(second);
	string msec = std::to_string(millisecond);
	return hou + ":" + min + ":" + sec + "," + msec;
}

class Subtitle
{
public:
	Subtitle(const string filepath)
	{
		formatctx = avformat_alloc_context();
		if (avformat_open_input(&formatctx, filepath.c_str(),
			nullptr, nullptr) != 0)
		{
			cout << "Couldn't open input stream" << endl;
			success = false;
			return;
		}
		if (avformat_find_stream_info(formatctx, nullptr) < 0)
		{
			cout << "Couldn't find stream information" << endl;
			success = false;
			return;
		}

		subidx = -1;
		for (int i = 0; i < (int)formatctx->nb_streams; i++)
		{
			if (formatctx->streams[i]->codecpar->codec_type== AVMEDIA_TYPE_SUBTITLE)
			{
				subidx = i;
				break;
			}
		}
		if (subidx == -1)
		{
			cout << "Couldn't find a subtitle stream" << endl;
			success = false;
			return;
		}

		AVStream* avstream = formatctx->streams[subidx];
		codec = avcodec_find_decoder(avstream->codecpar->codec_id);
		//codec->id为AV_CODEC_ID_SUBRIP (96264)
		if (codec == NULL)
		{
			cout << "Couldn't find decoder" << endl;
			success = false;
			return;
		}

		codecctx = avcodec_alloc_context3(codec);//或avcodec_alloc_context3(nullptr);
		if (avcodec_parameters_to_context(codecctx, avstream->codecpar) < 0)
		{
			printf("Fail in cvting parameters to context.\n");
			success = false;
			return;
		}

		//codecctx->pkt_timebase为{name=0,den=1}、avstream->time_base为{name=1,den=1000}
		//如果省略该句，则后文 avcodec_decode_subtitle2不能正确解码得到AVSubtitle
		codecctx->pkt_timebase = avstream->time_base;

		if (avcodec_open2(codecctx, codec, nullptr) < 0)
		{
			cout << "Couldn't open codec" << endl;
			success = false;
			return;
		}
	}
	~Subtitle()
	{
		avformat_close_input(&formatctx);
		avcodec_close(codecctx);
	}
	int save(const string& srt_out_path,const string& packet_out_path)
	{
		std::ofstream ofs_sub(srt_out_path, std::ios::binary);
		if (!ofs_sub)
		{
			cout << "Couldn't open " << srt_out_path << endl;
			return -1;
		}

		std::ofstream ofs_pkt(packet_out_path, std::ios::binary);
		if (!ofs_pkt)
		{
			cout << "Couldn't open " << packet_out_path << endl;
			return -1;
		}

		AVRational rational = formatctx->streams[subidx]->time_base; 


		AVPacket pkt;
		av_init_packet(&pkt);
		pkt.data = NULL;
		pkt.size = 0;

		while (av_read_frame(formatctx, &pkt) == 0)
		{
			if (pkt.stream_index != subidx)
			{
				av_packet_unref(&pkt);
				continue;
			}
			if (pkt.size > 0)
			{
				ofs_pkt.write((char*)pkt.data, pkt.size);

				AVSubtitle subtitle;
				memset(&subtitle, 0, sizeof(subtitle));
				int gotSubtitle = 0;
				int len = avcodec_decode_subtitle2(codecctx, &subtitle, &gotSubtitle, &pkt);
				if (len < 0)
				{
					cout << "Failed to call avcodec_decode_subtitle2" << endl;
					return -1;
				}
				if (gotSubtitle > 0)
				{
					for (int i = 0; i < subtitle.num_rects; i++)
					{
						cout << subtitle.rects[0]->ass << endl;
						ofs_sub.write(subtitle.rects[0]->ass, strlen(subtitle.rects[0]->ass));
					}
				}
				avsubtitle_free(&subtitle);
			}
			else if (pkt.data == nullptr && pkt.size==0) //EOF,需要刷新decoder
			{
				int gotSubtitle = 1;
				while (gotSubtitle != 0)
				{
					AVSubtitle subtitle;
					memset(&subtitle, 0, sizeof(subtitle));
					//送入avcode_decode_subtitle2的pkt.data为nullptr、size为0，则表示刷新decoder
					int len = avcodec_decode_subtitle2(codecctx, &subtitle, &gotSubtitle, &pkt);
					if (len < 0) //avcodec_decode_subtitle2返回负值表示出错
					{
						cout << "Failed to call avcodec_decode_subtitle2" << endl;
						return -1;
					}
					if (gotSubtitle > 0) //等于0表示没有可以解压缩的数据了
					{
						for (int i = 0; i < subtitle.num_rects; i++)
						{
							cout << subtitle.rects[0]->ass << endl;
							ofs_sub.write(subtitle.rects[0]->ass, strlen(subtitle.rects[0]->ass));
						}
					}
					avsubtitle_free(&subtitle);
				}
			}
			av_packet_unref(&pkt);
		}
		return 0;
	}
private:
	AVFormatContext* formatctx=nullptr;
	int subidx=-1;
	AVCodec *codec=nullptr;
	AVCodecContext* codecctx=nullptr;
	bool success=true;
public:
	bool is_success() { return success; }
};

int main()
{
	cout << "LIBAVCODEC_VERSION:"<<LIBAVCODEC_VERSION_MAJOR 
		<< "." << LIBAVCODEC_VERSION_MINOR
		<< "." << LIBAVCODEC_VERSION_MICRO << endl;//58.42.104

	const string filepath = "multi_audio_track_subtitle_track.mkv";
	const string srt_out_path = "output_srt.srt";
	const string packet_out_path = "output_pkt.srt";

	cout << "输出文件" << srt_out_path << "为解码所得AVPacket::data的内容" << endl;
	cout << "输出文件" << srt_out_path << "为解码所得AVSubtitle的内容" << endl;


	Subtitle sub(filepath);
	if (sub.is_success())
	{
		sub.save(srt_out_path, packet_out_path);
		cout << "output file" << srt_out_path << endl;
	}
	return 0;
}