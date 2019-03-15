#include<cstdio>
#include<cstdlib>
#include<cstring>
#include<fstream>
#include<string>
#include<vector>
using std::string;
using std::vector;

extern "C"
{
#include<libavformat/avformat.h>
#include<libavcodec/avcodec.h>
#include<libswresample/swresample.h>
}

class AudioDecoder
{
private:
	const int max_audio_frame_size = 192000;// 1 second of 48khz 32bit audio
	string src_path;
	AVFormatContext* fmtctx = nullptr;
	AVCodecContext* codecctx = nullptr;
	AVPacket* packet = nullptr;
	AVFrame* frame = nullptr;
	SwrContext* aucvtctx = nullptr;
	int audio_idx;
	int index = 0;
	int out_buffer_size;
public:
	AudioDecoder(const string& path):src_path(path)
	{
		init();
	}
	~AudioDecoder()
	{
		//avformat_close_input()关闭后会调用avformat_free_context
		avformat_close_input(&fmtctx);
		//调用avformat_close_input前无须if(fmtctx != nullptr)判断
		//如果fmtctx为nullptr则函数什么都不做，avcodec_close、
		//av_packet_free、av_frame_free、swr_free同理
		avcodec_close(codecctx);
		av_packet_free(&packet);
		av_frame_free(&frame);
		swr_free(&aucvtctx);
	}
	int init()
	{
		fmtctx = avformat_alloc_context();
		if (avformat_open_input(&fmtctx, src_path.c_str(),
			nullptr, nullptr) != 0)
		{
			printf("Couldn't open input\n");
			return -1;
		}

		if (avformat_find_stream_info(fmtctx, nullptr) < 0)
		{
			printf("Couldn't find stream info\n");
			return -1;
		}

		av_dump_format(fmtctx, 0, src_path.c_str(), 0);

		audio_idx = -1;
		for (int i = 0; i < (int)fmtctx->nb_streams; i++)
		{
			if (fmtctx->streams[i]->codecpar->codec_type
				== AVMEDIA_TYPE_AUDIO)
			{
				audio_idx = i;
				break;
			}
		}
		if (audio_idx == -1) {
			printf("Couldn't find audio stream\n");
			return -1;
		}

		codecctx = avcodec_alloc_context3(nullptr);
		if (avcodec_parameters_to_context(codecctx,
			fmtctx->streams[audio_idx]->codecpar) < 0)
		{
			printf("Couldn't convert parameters to AVCodecContext\n");
			return -1;
		}

		AVCodec* codec = avcodec_find_decoder(codecctx->codec_id);
		if (codec == nullptr)
		{
			printf("Couldn't find decoder\n");
			return -1;
		}

		if (avcodec_open2(codecctx, codec, nullptr) != 0)
		{
			printf("Couldn't open AVCodec\n");
			return -1;
		}

		packet = av_packet_alloc();
		printf("--------------------av_packet_alloc()-------------------\n");
		printf("packet->data:%p\n", packet->data);
		printf("packet->size:%d\n",packet->size);
		printf("packet->buf:%p\n", packet->buf);

		frame = av_frame_alloc();

		//FIX:Some Codec's Context Information is missing
		int64_t in_channel_layout = 
			av_get_default_channel_layout(codecctx->channels);

		//Out Audio Param
		int64_t out_channel_layout = AV_CH_LAYOUT_STEREO;
		int out_nb_samples = codecctx->frame_size;
		AVSampleFormat out_sample_fmt = AV_SAMPLE_FMT_S16;
		int out_sample_rate = 44100;
		int out_channels = av_get_channel_layout_nb_channels(out_channel_layout);
		//根据给定的音频参数求必要的缓存大小
		out_buffer_size = av_samples_get_buffer_size(nullptr, out_channels,
			out_nb_samples, out_sample_fmt, 1);

		aucvtctx = swr_alloc();
		aucvtctx = swr_alloc_set_opts(aucvtctx,out_channel_layout,out_sample_fmt,
			out_sample_rate,in_channel_layout,codecctx->sample_fmt,
			codecctx->sample_rate,0,nullptr);
		swr_init(aucvtctx);

		return 0;
	}
public:
	//解码整个文件，并保存结果。
	static void decode(const string& src_url, const string& dst_url)
	{
		std::ofstream ofs(dst_url, std::ios::binary);
		if (!ofs) {
			printf("Couldn't open %s\n", src_url.c_str());
			return;
		}
		AudioDecoder decoder(src_url);
		vector<uint8_t> data;
		while (1)
		{
			data = decoder.next_frame();
			if (data.empty()) break;
			ofs.write((char*)&data[0], data.size());
		}
	}

	vector<uint8_t> next_frame()
	{
		while (1)
		{
			av_packet_unref(packet);
			//将管理的内存释放掉，所有数据成员恢复av_packet_alloc时的初始状态
			//不能省略，因为每个AVPacket的大小不同

			if (av_read_frame(fmtctx, packet) != 0)
			{
				return vector<uint8_t>();
			}

			if (packet->stream_index != audio_idx)
			{
				continue;
			}

			printf("-----av_read_frame(fmtctx, packet)--------\n");
			printf("packet->data:%p\n", packet->data);
			printf("packet->size:%d\n", packet->size);
			printf("packet->buf:%p\n", packet->buf);
			printf("packet->buf->data:%p\n", packet->buf->data);
			printf("packet->buf->size:%d\n", packet->buf->size);
			printf("packet->buf->buffer:%p\n", packet->buf->buffer);
			printf("\n");

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
				exit (-1);
			}
		}

		std::vector<uint8_t> out_buffer(out_buffer_size);
		uint8_t* out = &out_buffer[0];
		swr_convert(aucvtctx, &out, max_audio_frame_size,
			(const uint8_t **)frame->data, frame->nb_samples);

		printf("index:%5d    pts:%lld    packet's size:%d\n",
			index, packet->pts, packet->size);

		index++;

		return out_buffer;
	}
};

int main()
{
	const string& dst = "output_skycity1.pcm";
	AudioDecoder::decode("skycity1.mp3",dst);
	printf("Output %s\n",dst.c_str());
}



