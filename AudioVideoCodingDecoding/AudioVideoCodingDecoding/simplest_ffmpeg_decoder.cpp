#include<iostream>
#include<cstdio>
#include<SDL.h>
#include<memory>
#include<fstream>
#include<string>
#include<cassert>
extern "C"
{
#include<libavcodec/avcodec.h>
#include<libavformat/avformat.h>
#include<libswscale/swscale.h>
#include<libavutil/avutil.h>
#include "libavutil/imgutils.h"  //av_image_fill_arrays
}
using std::cout;
using std::endl;

int main(int argc, char* argv[])
{
	const char* filepath = "Titanic_10s.ts"; 

	AVFormatContext* formatctx = avformat_alloc_context();

	if (avformat_open_input(&formatctx, filepath, nullptr, nullptr) != 0)
	{
		cout << "Couldn't open input stream" << endl;
		return -1;
	} 

	if (avformat_find_stream_info(formatctx, nullptr) < 0)
	{
		cout << "Couldn't find stream information" << endl;
		return -1;
	}

	int videoidx = -1;
	for (int i = 0; i < (int)formatctx->nb_streams; i++)
	{
		if (formatctx->streams[i]->codecpar->codec_type 
			== AVMEDIA_TYPE_VIDEO)
		{
			videoidx = i;
			break;
		}
	}
	if (videoidx == -1)
	{
		cout << "Couldn't find a video stream" << endl;
		return -1;
	}  

	printf("input duration is:%lld\n", formatctx->duration);
	printf("input bit rate is:%lld bit/s\n ", formatctx->bit_rate);
	printf("%s %s\n", formatctx->iformat->name, formatctx->iformat->long_name);
	printf("-----------File Information-------------\n");

	av_dump_format(formatctx, videoidx, filepath, 0);
	printf("-----------File Information-------------\n");
	/*
	-----------File Information-------------
	Input #0, mpegts, from 'C:\Users\myth\Videos\forTest\testvideo\Titanic.ts':
	Duration: 00:00:48.03, start: 1.463400, bitrate: 589 kb/s
	Program 1
	Metadata:
	service_name    : Service01
	service_provider: FFmpeg
	Stream #0:0[0x100]: Video: h264 (High) ([27][0][0][0] / 0x001B),
	yuv420p(progressive), 640x272 [SAR 1:1 DAR 40:17], 23.98 fps,
	23.98 tbr, 90k tbn, 47.95 tbc
	Stream #0:1[0x101]: Audio: mp3 ([3][0][0][0] / 0x0003), 48000 Hz,
	stereo, fltp, 128 kb/s
	-----------File Information-------------
	*/

	AVCodecParameters* codecpar = formatctx->streams[videoidx]->codecpar;

	printf("video widthxheight=%dx%d\n", codecpar->width, codecpar->height);
	printf("video avcodec_id:%d\n", codecpar->codec_id);
	printf("format: %d\n", codecpar->format); 

	AVCodecContext* codecctx  = avcodec_alloc_context3(nullptr);
	if (avcodec_parameters_to_context(codecctx, codecpar) < 0)
	{
		printf("Fail in cvting parameters to context.\n");
		return -1;
	}

	AVCodec* codec = avcodec_find_decoder(codecpar->codec_id);
	if (codec == NULL)
	{
		cout << "Couldn't find decoder" << endl;
		return -1;
	}

	printf("AVCodec long name:%s\n", codec->long_name);
	//H.264 / AVC / MPEG-4 AVC / MPEG-4 part 10


	if (avcodec_open2(codecctx, codec, nullptr) < 0)
	{
		cout << "Couldn't open codec" << endl;
		return -1;
	}

	SwsContext* swsctx;
	swsctx = sws_getContext(codecctx->width, codecctx->height,
		codecctx->pix_fmt,codecctx->width,codecctx->height,
		AV_PIX_FMT_YUV420P,SWS_BICUBIC,nullptr,nullptr,nullptr);
	
	AVFrame* frameyuv = av_frame_alloc();
	/*
	deprecate:
	uint8_t* outbuffer = (uint8_t*)av_malloc(avpicture_get_size(
	AV_PIX_FMT_YUV420P, codecctx->width, codecctx->height));
	*/
	int buffesize = av_image_get_buffer_size(AV_PIX_FMT_YUV420P, codecctx->width,
		codecctx->height, 1);
	uint8_t* outbuffer = (uint8_t*)av_malloc(buffesize);
	/*
	deprecated:
	avpicture_fill((AVPicture*)frameyuv, outbuffer, AV_PIX_FMT_YUV420P,
	codecctx->width, codecctx->height);
	*/
	av_image_fill_arrays(frameyuv->data, frameyuv->linesize, outbuffer,
		AV_PIX_FMT_YUV420P, codecctx->width, codecctx->height, 1);


	int frame_cnt = 0;
	//av_packet_alloc() differs from (AVPacket*)av_malloc(sizeof(AVPacket))
	AVPacket* packet = av_packet_alloc();
	AVFrame* frame = av_frame_alloc();

	const std::string dstpath_yuv = "output_" +
		std::to_string(codecctx->width) +
		"x" + std::to_string(codecctx->height) + ".yuv";
	std::ofstream ofs_yuv(dstpath_yuv, std::ios::binary);
	const std::string dstpath_h264 = "output_" +
		std::to_string(codecctx->width) +
		"x" + std::to_string(codecctx->height) + ".h264";
	std::ofstream ofs_h264(dstpath_h264, std::ios::binary);
	cout << "-------------------------------------" << endl;
	cout << "output: " << endl;
	cout << "1: " << dstpath_yuv << endl;
	cout << "2: " << dstpath_h264 << endl;
	cout << "-------------------------------------" << endl;
	while (av_read_frame(formatctx, packet) >= 0)
	{
		if (packet->stream_index == videoidx)
		{
			assert(codecpar->codec_id == AV_CODEC_ID_H264);
			ofs_h264.write((char*)packet->data,packet->size);

			/*avcodec_decode_video2 deprecated
			int got_picture;
			int ret = avcodec_decode_video2(codecctx, frame, 
				&got_picture,packet);
			*/

			if (avcodec_send_packet(codecctx, packet) < 0)
			{
				printf("Fail in sending packet.\n");
				return -1;
			}

			while(avcodec_receive_frame(codecctx, frame)==0)
			{
				sws_scale(swsctx, (const uint8_t* const*)frame->data,
					frame->linesize, 0, codecctx->height,
					frameyuv->data, frameyuv->linesize);
				printf("Decoded frame index:%d\n", frame_cnt);

				ofs_yuv.write((char*)frameyuv->data[0],
					codecctx->width*codecctx->height);   //y
				ofs_yuv.write((char*)frameyuv->data[1],
					codecctx->width*codecctx->height / 4);  //u
				ofs_yuv.write((char*)frameyuv->data[2],
					codecctx->width*codecctx->height / 4);   //v

				frame_cnt++;
			}
		}

		av_packet_unref(packet);
	}

	av_packet_free(&packet); 
	av_free(outbuffer);
	sws_freeContext(swsctx);
	av_frame_free(&frameyuv);
	av_frame_free(&frame);
	avcodec_close(codecctx);
	avformat_close_input(&formatctx);

	return 0;
}