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


class Subtitle
{
public:
	Subtitle() 
	{

	}
private:
	int open_input_file(const string& src_path,int subidx_)
	{
		subidx = subidx_;

		input_formatctx = avformat_alloc_context();
		if (avformat_open_input(&input_formatctx, src_path.c_str(),
			nullptr, nullptr) != 0)
		{
			cout << "Couldn't open input stream" << endl;
			return -1;
		}
		if (avformat_find_stream_info(input_formatctx, nullptr) < 0)
		{
			cout << "Couldn't find stream information" << endl;
			return -1;
		}
		
		bool legal = true;
		if (subidx == -1)
		{
			subidx = av_find_best_stream(input_formatctx,
				AVMEDIA_TYPE_SUBTITLE, -1, -1, nullptr, 0);
			if (subidx < 0)
				legal = false;
		}
		else if (subidx < 0 || subidx >= input_formatctx->nb_streams)
			legal = false;
		else
		{
			legal = (input_formatctx->streams[subidx]->codecpar->codec_type
				== AVMEDIA_TYPE_SUBTITLE);
		}
		if (!legal)
		{
			cout << "illlegal param subidx" << endl;
			return -1;
		}

		AVStream* input_stream = input_formatctx->streams[subidx];
		input_decodec = avcodec_find_decoder(input_stream->codecpar->codec_id);
		//input_decodec->idΪAV_CODEC_ID_SUBRIP (96264)
		if (input_decodec == NULL)
		{
			cout << "Couldn't find decoder" << endl;
			return -1;
		}

		input_decodecctx = avcodec_alloc_context3(input_decodec);//��avcodec_alloc_context3(nullptr);
		if (avcodec_parameters_to_context(input_decodecctx, input_stream->codecpar) < 0)
		{
			printf("Fail in cvting parameters to context.\n");
			return -1;
		}

		//input_decodecctx->pkt_timebaseΪ{name=0,den=1}��input_stream->time_baseΪ{name=1,den=1000}
		//���ʡ�Ըþ䣬����� avcodec_decode_subtitle2������ȷ����õ�AVSubtitle
		input_decodecctx->pkt_timebase = input_stream->time_base;

		return 0;
	}
	int open_output_file(const string& dst_path)
	{
		int ret = 0;
		ret=avformat_alloc_output_context2(&output_formatctx,nullptr,nullptr,dst_path.c_str());
		if (ret < 0)
		{
			cout << "'Failed to call avformat_alloc_output_context2" << endl;
			return -1;
		}

		ret=avio_open2(&output_formatctx->pb,dst_path.c_str(), AVIO_FLAG_WRITE, 
			&output_formatctx->interrupt_callback,nullptr);
		if (ret < 0)
		{
			cout << "'Failed to call avio_open2" << endl;
			return -1;
		}

		//��AVFormatContext oc������һ��AVStream
		//����AVStream��������codec��codecpar�ȳ�Ա���ڴ�
		AVStream* st = avformat_new_stream(output_formatctx,nullptr);
		if (st == nullptr) {
			cout << "" << endl;
			return -1;
		}

		assert(output_formatctx->nb_streams == 1);
		st->codecpar->codec_type = AVMEDIA_TYPE_SUBTITLE;
		st->codecpar->codec_id = av_guess_codec(output_formatctx->oformat,
			nullptr, output_formatctx->url,nullptr,st->codecpar->codec_type);
		output_encodec = avcodec_find_encoder(st->codecpar->codec_id);
		output_encodecctx = avcodec_alloc_context3(output_encodec);
		if (output_encodecctx == nullptr)
		{
			cout << "Failed to call avcodec_alloc_context3" << endl;
			return -1;
		}
		output_encodecctx->codec_id = st->codecpar->codec_id;
		output_encodecctx->codec_type = st->codecpar->codec_type;

		//���ñ����ʱ���timebase������Ϊ����ֵ����Ӱ�����ɵ�srt�ļ�����
		output_encodecctx->time_base = AVRational{ 1, 100000 };
		output_formatctx->streams[0]->time_base = output_encodecctx->time_base;

		return 0;
	}
	int do_subtitle_out(AVSubtitle* sub, AVCodecContext* encctx, AVFormatContext* fmtctx, AVRational pkt_timebase)
	{
		const int subtitle_out_max_size = 1024 * 1024;
		
		//AVSubtitle::pts��{1,AV_TIME_BASE}Ϊʱ�����������AV_TIME_BASE�õ�Ϊ����
		//AVSubtitle::start_display_time��end_display_time�Խ������AVCodecContext��pkt_timebaseΪ
		//ʱ�����������pkt_timebase��Ϊ����
		//start_display_timeҪ�������0
		sub->pts += av_rescale_q(sub->start_display_time, pkt_timebase, AVRational{ 1, AV_TIME_BASE });
		sub->end_display_time -= sub->start_display_time;
		sub->start_display_time = 0;
		int64_t sub_duration = sub->end_display_time;

		uint8_t * subtitle_out = (uint8_t*)av_mallocz(subtitle_out_max_size);
		//subtitle.rects[0]->ass���Ѿ�������Ļʱ�����Ϣ
		//subtitle.rects[0]->assΪ��Dialogue: 0,0:00:29.75,0:00:31.78,Default,,0,0,0,,�ڹ��ϵ�ʱ��
		//�����subtitle_outΪ��"�ڹ��ϵ�ʱ��"
		int subtitle_out_size = avcodec_encode_subtitle(encctx , subtitle_out,
			subtitle_out_max_size, sub);

		//���۱��뻹�ǽ��룬AVPacket::data�ж���ʱ�����Ϣ
		//��Ļ��ʱ�����Ϣ������AVPacket::pts��AVPacket::duration��AVPacket::dts
		AVPacket pkt;
		av_init_packet(&pkt);
		pkt.data = subtitle_out;
		pkt.size = subtitle_out_size;
		//ҪдAVPacket������������AVPacket��pts���������time_baseΪʱ���
		pkt.pts = av_rescale_q(sub->pts, AVRational { 1, AV_TIME_BASE }, 
			fmtctx->streams[0]->time_base);
		pkt.duration = av_rescale_q(sub_duration,
			pkt_timebase, fmtctx->streams[0]->time_base);
		pkt.dts = pkt.pts;

		int ret = 0;
		ret=av_write_frame(fmtctx, &pkt);
		if (ret < 0)
		{
			cout << "Failed to call av_write_frame" << endl;
			return -1;
		}
		return 0;
	}
public:
	~Subtitle()
	{
		avformat_close_input(&input_formatctx);
		avcodec_close(input_decodecctx);
	}
	int save(const string& src_path, const string& dst_path, int subidx_)
	{
		int ret = 0;
		ret=open_input_file(src_path,subidx_);
		if (ret < 0) 
			return -1;
		ret = open_output_file(dst_path);
		if (ret < 0)
			return -1;
		
		ret = avcodec_open2(input_decodecctx,input_decodec,nullptr);
		if (ret < 0)
		{
			cout << "Failed to call avcodec_open2 when decoding" << endl;
			return -1;
		}

		if (input_decodecctx->subtitle_header)
		{
			output_encodecctx->subtitle_header =
				(uint8_t*)av_mallocz(input_decodecctx->subtitle_header_size + 1);
			memcpy(output_encodecctx->subtitle_header,
				input_decodecctx->subtitle_header, input_decodecctx->subtitle_header_size);
			output_encodecctx->subtitle_header_size = input_decodecctx->subtitle_header_size;
		}

		ret = avcodec_open2(output_encodecctx,output_encodec,nullptr);
		if (ret < 0)
		{
			cout << "Failed to call avcodec_open2 when encoding" << endl;
			return -1;
		}
		
		avcodec_parameters_from_context(output_formatctx->streams[0]->codecpar,
			output_encodecctx);
		avcodec_copy_context(output_formatctx->streams[0]->codec, output_encodecctx);//��ʡ��

		//AVStream::duration���װʱ����ffmpeg�����ã���װʱ�����û����ã���
		//Ϊmuxer�ṩһЩ��ʾ
		//output_formatctx->streams[0]->durationΪ-9223372036854775808
		//ist->durationΪ -9223372036854775808
		AVStream* ist = input_formatctx->streams[subidx];
		if (output_formatctx->streams[0]->duration <= 0 &&ist->duration > 0)
			output_formatctx->streams[0]->duration =
			av_rescale_q(ist->duration, ist->time_base, output_formatctx->streams[0]->time_base);//ʱ���ת��

		output_formatctx->streams[0]->codec->codec = output_encodecctx->codec;

		avformat_write_header(output_formatctx,nullptr);
		//------------------------------�������ļ�����ת�롪д������ļ�-----------------------------
		AVPacket pkt_from_input;
		av_init_packet(&pkt_from_input);
		pkt_from_input.data = NULL;
		pkt_from_input.size = 0;
		AVPacket pkt_for_output;
		av_init_packet(&pkt_for_output);
		pkt_for_output.data = NULL;
		pkt_for_output.size = 0;

		while (av_read_frame(input_formatctx, &pkt_from_input) == 0)
		{
			if (pkt_from_input.stream_index != subidx)
			{
				av_packet_unref(&pkt_from_input);
				continue;
			}

			if (pkt_from_input.size > 0)
			{
				AVSubtitle subtitle;
				memset(&subtitle, 0, sizeof(subtitle));
				int gotSubtitle = 0;
				ret = avcodec_decode_subtitle2(input_decodecctx, &subtitle, 
					&gotSubtitle, &pkt_from_input); //pkt_timebase={1,1000}
				if (ret < 0){
					cout << "Failed to call avcodec_decode_subtitle2" << endl;
					return -1;
				}
				if (gotSubtitle > 0){
					ret=do_subtitle_out(&subtitle, output_encodecctx, output_formatctx,input_decodecctx->pkt_timebase);
				}
				avsubtitle_free(&subtitle);
			}
			else if (pkt_from_input.data == nullptr && pkt_from_input.size == 0) //EOF,��Ҫˢ��decoder
			{
				int gotSubtitle = 1;
				while (gotSubtitle != 0)
				{
					AVSubtitle subtitle;
					memset(&subtitle, 0, sizeof(subtitle));
					//����avcode_decode_subtitle2��pkt_from_input.dataΪnullptr��sizeΪ0�����ʾˢ��decoder
					ret= avcodec_decode_subtitle2(input_decodecctx, &subtitle, &gotSubtitle, &pkt_from_input);
					if (ret < 0) //avcodec_decode_subtitle2���ظ�ֵ��ʾ����
					{
						cout << "Failed to call avcodec_decode_subtitle2" << endl;
						return -1;
					}
					if (gotSubtitle > 0) //����0��ʾû�п��Խ�ѹ����������
					{
						ret = do_subtitle_out(&subtitle, output_encodecctx, output_formatctx, input_decodecctx->pkt_timebase);
					}
					avsubtitle_free(&subtitle);
				}
			}
			av_packet_unref(&pkt_from_input);
		}
		
		av_write_trailer(output_formatctx);

		return 0;
	}
private:
	AVFormatContext* input_formatctx = nullptr;
	AVFormatContext* output_formatctx = nullptr;
	AVCodec *input_decodec = nullptr;
	AVCodecContext* input_decodecctx = nullptr;
	AVCodec *output_encodec = nullptr;
	AVCodecContext* output_encodecctx = nullptr;
	int subidx = -1;
};

int main()
{
	cout << "LIBAVCODEC_VERSION:" << LIBAVCODEC_VERSION_MAJOR
		<< "." << LIBAVCODEC_VERSION_MINOR
		<< "." << LIBAVCODEC_VERSION_MICRO << endl;

	const string src_path = "multi_audio_track_subtitle_track.mkv";
	const string dst_path = "output_srt_by_decode_encode.srt";

	cout << "������Ƶ�ļ�Ϊ��" << src_path <<endl;
	cout << "�����Ļ�ļ�Ϊ��" << dst_path << endl;

	Subtitle sub;
	int ret=sub.save(src_path, dst_path,-1);
	if (ret < 0)
	{
		cout << "ʧ��" << endl;
	}
	else
	{
		cout << "�ɹ�" << endl;
	}
	return 0;
}