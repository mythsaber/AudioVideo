#include<iostream>
#include<string>
#include<fstream>
#include<cassert>
#include<vector>
using std::vector;
using std::string;
using std::ofstream;
using std::cout;
using std::endl;
extern "C"
{
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
}

int convert_pcm2aac(const string& filename_in, const string& filename_out)
{
	std::ifstream ifs(filename_in, std::ios::binary);
	if (!ifs) {
		cout << "Couldn't open" << filename_in << endl;
		return -1;
	}
	ofstream ofs(filename_out,std::ios::binary);
	if (!ofs)
	{
		cout << "Couldn't open" << filename_out << endl;
		return -1;
	}

	uint8_t luckybuf = 0, offset = 0;  //对一个字节，offset从最高位到最低位依次为0~7
	auto writebit = [&ofs,&luckybuf, &offset](bool val)
	{
		luckybuf = luckybuf | (((int)val) << (7 - offset));
		offset++;
		if (offset == 8)
		{
			ofs.write((char*)&luckybuf, 1);
			luckybuf = 0;
			offset = 0;
		}
	};
	auto writebits = [writebit](uint16_t val,int bitnum)
	{
		for (int i = bitnum-1;i >= 0;i--)
		{
			writebit(static_cast<bool>((1<<i)&val));
		}
	};
	
	auto writeheader =[writebit, writebits](uint16_t frame_length)
	{
		writebits(0x0FFF, 12);
		writebit(0); //MPEG-4
		writebits(0x0000, 2);
		writebit(1);//protection_absent
		writebits(0x0001, 2);
		writebits(0x0004, 4);//44.1khz
		writebit(0);//private_bit
		writebits(0x0002, 3); //channel_config
		writebit(0);//origin_copy
		writebit(0);//home

		writebit(0);//copyright_identification_bit
		writebit(0);//copyright_identification_start
		writebits(frame_length, 13);//frame length,包括ADTS头和AAC原始流
		writebits(0x07FF, 11);
		writebits(0x00, 2);//number_of_raw_data_blocks_in_frame
	};

	//输入的pcm数据的先验信息
	int input_sample_rate = 44100;
	uint64_t input_channel_layout = AV_CH_LAYOUT_STEREO;
	AVSampleFormat input_sample_fmt = AV_SAMPLE_FMT_S16;
	int input_channels = av_get_channel_layout_nb_channels(input_channel_layout);
	//期望的编码方式
	AVCodecID codec_id = AV_CODEC_ID_AAC;

	AVCodec* pcodec = avcodec_find_encoder(codec_id);
	if (!pcodec)
	{
		cout << "Couldn't find encoder" << endl;
		return -1;
	}
	AVCodecContext* pcodecctx = avcodec_alloc_context3(pcodec);
	if (!pcodecctx)
	{
		cout << "Couldn't alloc AVCodecContext" << endl;
		return -1;
	}
	pcodecctx->codec_id = codec_id;//设置用于编码的avcodec的一些参数
	pcodecctx->codec_type = AVMEDIA_TYPE_AUDIO;
	pcodecctx->sample_fmt = pcodec->sample_fmts[0];
	//AVCodec的sample_fmts成员是array of supported sample formats, 
	//or NULL if unknown, array is terminated by -1
	//实际上AAC编码器仅支持一种采样格式，即AV_SAMPLE_FMT_FLTP (8)
	pcodecctx->sample_rate = input_sample_rate;
	pcodecctx->channel_layout = input_channel_layout;
	pcodecctx->channels =input_channels;
	
	//avcodec_open2会赋值pcodecctx->frame_size
	//除最后一帧外，提交给编码器的大小必须与该值相同
	if (avcodec_open2(pcodecctx, pcodec, nullptr) < 0)
	{
		cout << "Couldn't open avcodec" << endl;
		avcodec_close(pcodecctx);
		av_free(pcodecctx);
		return -1;
	}
	
	//读原始pcm数据至vector<uint8_t> input_buf，再用swr_convert转换
	//转换后的数据拷贝到AVFrame的缓冲区，最后然后用AAC编码器编码为pakcet
	//因此设定完AVCodecContext参数后，swr_convert的转换参数也就确定了
	int swr_cvted_sample_rate = pcodecctx->sample_rate;
	uint64_t swr_cvted_channel_layout = pcodecctx->channel_layout;
	int swr_cvted_channels = av_get_channel_layout_nb_channels(swr_cvted_channel_layout);
	AVSampleFormat swr_cvted_sample_fmt = pcodecctx->sample_fmt;
	SwrContext* pswrctx = swr_alloc_set_opts(
		nullptr,//SwrContext *s,已有则只设置参数，nullptr则先alloc再设置参数
		swr_cvted_channel_layout, //int64_t out_ch_layout
		swr_cvted_sample_fmt, //enum AVSampleFormat out_sample_fmt
		swr_cvted_sample_rate, //int out_sample_rate
		input_channel_layout,  //int64_t  in_ch_layout
		input_sample_fmt, //enum AVSampleFormat  in_sample_fmt
		input_sample_rate,// int  in_sample_rate
		0,
		nullptr);
	swr_init(pswrctx);

	//编码器的输入帧
	AVFrame* pframe = av_frame_alloc();  //存储原始pcm数据
	pframe->format = pcodecctx->sample_fmt;
	pframe->nb_samples = pcodecctx->frame_size; //important
	pframe->channels = pcodecctx->channels;
	//根据AVFframe将保存的数据量计算需要的缓冲区size，size太小，
	//则后文avcodec_fill_audio_frame失败
	int buffer_size = av_samples_get_buffer_size(nullptr, pframe->channels, pframe->nb_samples,
		(AVSampleFormat)pframe->format, 0);
	uint8_t* frame_buf = (uint8_t*)av_malloc(buffer_size);
	int ret = avcodec_fill_audio_frame(pframe, pframe->channels, (AVSampleFormat)pframe->format,
		frame_buf, //要绑定给pframe的缓冲区
		buffer_size,     //缓冲区大小
		0);//8192
	if (ret < 0)
	{
		cout << "avcodec_fill_audio_frame error" << endl;
		avcodec_close(pcodecctx);
		av_free(pcodecctx);
		av_free(frame_buf);
		av_frame_free(&pframe);
		swr_free(&pswrctx);
		return -1;
	}
	assert(buffer_size >= ret);

	//swr_convert()的输入和输出
	vector<uint8_t*> convert_data(swr_cvted_channels);//保存swr_convert()的输出
	int alloc_size=av_samples_alloc(&convert_data[0], 
		nullptr, 
		swr_cvted_channels,
		pframe->nb_samples, 
		(AVSampleFormat)pframe->format, 
		0);
	//swr_cvted_sample_fmt为AV_SAMPLE_FMT_FLTP，planar方式
	//因此给convert_data[0]、convert_data[1]指针赋值，分别为左、右声道
	//申请的内存大小即返回值为=每通道样点数nb_samples*
	//每样点字节数av_get_bytes_per_sample(swr_cvted_sample_fmt)*通道数swr_cvted_channels
	if (alloc_size < 0)
	{
		cout << "av_samples_alloc failed" << endl;
		avcodec_close(pcodecctx);
		av_free(pcodecctx);
		av_free(frame_buf);
		av_frame_free(&pframe);
		swr_free(&pswrctx);
		return -1;
	}
	assert(alloc_size == av_samples_get_buffer_size(nullptr, swr_cvted_channels, pframe->nb_samples,
		(AVSampleFormat)pframe->format, 0));

	//由于swr_convert具有缓冲功能，因此输入缓冲区的大小可随意设置
	vector<uint8_t> input_buf(1024); //读pcm文件数据至此处，也即swr_convert()的输入
	int in_samples = (int)input_buf.size() / av_get_bytes_per_sample(input_sample_fmt) /
		input_channels;//input_buf缓冲区能容纳每个信道的采样点数
	int framecnt = 0;	
	int output_space_offset = 0;//用单通道样点数衡量
	int group_num = 0;
	for (int i = 0; i < convert_data.size()&& convert_data[i]!=nullptr; i++)
	{
		group_num++;
	}
	vector<uint8_t*> cur_convert_data_pos(convert_data.size(),nullptr);

	AVPacket pkt;
	while ((!ifs.fail())&&(ifs.peek()!=EOF))
	{
		ifs.read((char*)&input_buf[0], input_buf.size());
		if ((int)ifs.gcount() < input_buf.size())
		{
			in_samples = (int)input_buf.size() / av_get_bytes_per_sample(input_sample_fmt) /
				input_channels; 
		}
		//必须有const，否则无法将参数从“uint8_t **”转换为“const uint8_t **”
		const uint8_t* in = &input_buf[0];  
		//convert_data关联的缓冲区的大小为alloc_size，对应的单通道采样点数为
		//数为pframe->nb_samples，通道数为swr_cvted_channels。即使是双通道，如果
		//为pack方式，则也仅仅convert_data[0]指向的内存存储数据，convert_data[1]、
		//convert_data[2]……全为nullptr，也就是说通道数不等于swr_convert后的缓冲区数。
		//这里用group_num记录哪些convert_data[i]将指向实际数据，其余的全为nullptr
		int available_output_space = pframe->nb_samples - output_space_offset;
		for (int i = 0; i <  group_num; i++)
		{
			cur_convert_data_pos[i] = convert_data[i] + 
				(int)(output_space_offset / (double)pframe->nb_samples*
				(alloc_size / (double)group_num));
		}
		int num=swr_convert(pswrctx,
			&cur_convert_data_pos[0],// output buffers, only the first one need be set in case of packed audio
			available_output_space,//amount of space available for output in samples per channel
			&in,
			in_samples //number of input samples available in one channel
		);//返回输出的数据量，用单通道采样数衡量
		
		while (1)
		{
			if (num < 0)
			{
				cout << "swr_convert" << endl;
				avcodec_close(pcodecctx);
				av_free(pcodecctx);
				av_free(frame_buf);
				av_frame_free(&pframe);
				av_freep(&convert_data[0]);
				swr_free(&pswrctx);
				return -1;
			}
			assert(num <= available_output_space);
			//输入数据量太少，输出的数据量未填满输出缓冲区
			if (num < available_output_space)
			{
				output_space_offset += num;
				break;
			}
			//输入数据量足够多，输出的数据量填满输出缓冲区，且
			//有可能部分输入数据后数据缓存在swr中
			else if (num == available_output_space)
			{
				//输出累计够一帧
				//将转换后的数据复制给AVFrame的缓冲区
				//认为每个有效convert_data[i]指向的内存的大小一致
				for (int i = 0; i <group_num; i++)
				{
					memcpy(pframe->data[i], convert_data[i], alloc_size / group_num);
				}

				int ret_send = avcodec_send_frame(pcodecctx, pframe);
				if (ret_send == 0) //成功
				{
					;
				}
				else if (ret_send == AVERROR(EAGAIN))//编码器输入缓冲区满从而无法接收frame
				{
					//由于每次avcodec_send_frame后都avcodec_receive_packet直至编码器要求输入新的frame，
					//因此这里不存在编码器输入缓冲区满从而无法接收frame的情况
					assert(ret_send != AVERROR(EAGAIN));
				}
				else if (ret_send == AVERROR_EOF)//编码器已经被刷新，不应该再给之输入frame
				{
					//没有调用avcodec_send_frame(pcodecctx,nullptr)，因此编码器不会进入刷新模式，
					//该分支不会执行
					assert(ret_send != AVERROR_EOF);
				}
				else
				{
					cout << "avcodec_send_frame failed" << endl;
					avcodec_close(pcodecctx);
					av_free(pcodecctx);
					av_free(frame_buf);
					av_frame_free(&pframe);
					av_freep(&convert_data[0]);
					swr_free(&pswrctx);
					return -1;
				}

				//循环avcodec_receive_packet直至编码器要求输入新的frame
				while (1)
				{
					av_init_packet(&pkt);
					pkt.data = NULL;
					pkt.size = 0;
					int ret_recv = avcodec_receive_packet(pcodecctx, &pkt);
					if (ret_recv == 0)  //成功
					{
						framecnt++;
						writeheader(pkt.size + 7);
						ofs.write((char*)pkt.data, pkt.size);
						cout << "frame " << framecnt << " pkt.size=" << pkt.size << endl;
						av_packet_unref(&pkt);
						continue;
					}
					else if (ret_recv == AVERROR(EAGAIN))
					{
						//必须再给编码器输入新的frame，编码器才可以输出packet
						break;
					}
					else if (ret_recv == AVERROR_EOF)//编码器被完全刷新，没有更多输出了
					{
						//没有调用avcodec_send_frame(pcodecctx,nullptr)，因此编码器不会进入刷新模式，
						//该分支不会执行
						assert(ret_recv != AVERROR_EOF);
					}
					else
					{
						cout << "avcodec_receive_packet failed" << endl;
						avcodec_close(pcodecctx);
						av_free(pcodecctx);
						av_free(frame_buf);
						av_frame_free(&pframe);
						av_freep(&convert_data[0]);
						swr_free(&pswrctx);
						return -1;
					}
				}

				//处理缓存在swr中的输入数据
				output_space_offset = 0;
				available_output_space = pframe->nb_samples - output_space_offset;
				for (int i = 0; i < group_num; i++)
				{
					cur_convert_data_pos[i] = convert_data[i] +
						(int)(output_space_offset / (double)pframe->nb_samples*
						(alloc_size / (double)group_num));
				}
				num = swr_convert(pswrctx, &cur_convert_data_pos[0], available_output_space, nullptr, 0);
			}
		}
	}

	//刷新编码器
	//必须发送刷新avcodec_send_frame(pcodecctx, nullptr)编码器
	//不发送 avcodec_send_frame(pcodecctx, nullptr)，即使一直avcodec_receive_packet()，
	//会提示要求新的输入，但队列中仍有缓冲的packet
	int ret_send = avcodec_send_frame(pcodecctx, nullptr);
	assert(ret_send != AVERROR(EAGAIN) && ret_send != AVERROR_EOF);
	if (ret_send == 0) { cout << "刷新编码器" << endl; }
	else
	{
		cout << "avcodec_send_frame failed" << endl;
		avcodec_close(pcodecctx);
		av_free(pcodecctx);
		av_free(frame_buf);
		av_frame_free(&pframe);
		av_freep(&convert_data[0]);
		swr_free(&pswrctx);
		return -1;
	}
	//循环avcodec_receive_packet直至编码器被完全刷新，没有更多输出
	while (1)
	{
		av_init_packet(&pkt);
		pkt.data = NULL;
		pkt.size = 0;
		int ret_recv = avcodec_receive_packet(pcodecctx, &pkt);
		if (ret_recv == 0)  //成功
		{
			framecnt++;
			writeheader(pkt.size + 7);
			ofs.write((char*)pkt.data, pkt.size);
			cout << "frame " << framecnt << " pkt.size=" << pkt.size << endl;
			av_packet_unref(&pkt);
			continue;
		}
		else if (ret_recv == AVERROR(EAGAIN))
		{
			assert(ret_recv != AVERROR(EAGAIN));
		}
		else if (ret_recv == AVERROR_EOF)//编码器被完全刷新，没有更多输出了
		{
			break;
		}
		else
		{
			cout << "avcodec_receive_packet failed" << endl;
			avcodec_close(pcodecctx);
			av_free(pcodecctx);
			av_free(frame_buf);
			av_frame_free(&pframe);
			av_freep(&convert_data[0]);
			swr_free(&pswrctx);
			return -1;
		}
	}


	// * Close a given AVCodecContext and free all the data associated with it
	//*(but not the AVCodecContext itself).
	avcodec_close(pcodecctx);
	av_free(pcodecctx);
	av_free(frame_buf);
	av_frame_free(&pframe);
	av_freep(&convert_data[0]);
	swr_free(&pswrctx);

	return 0;
}


int main()
{
	const string filename_in = "NocturneNo2inEflat_44.1k_s16le.pcm";
	const string filename_out = "output_adts.aac";
	int ret=convert_pcm2aac(filename_in,filename_out);
	if (ret != 0)
	{
		cout << "convert_pcm2aac failed" << endl;
		return 0;
	}
	else
	{
		cout << "输出" << filename_out << endl;
		return 0;
	}
}
