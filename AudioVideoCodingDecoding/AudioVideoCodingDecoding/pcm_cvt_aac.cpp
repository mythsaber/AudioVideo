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

	uint8_t luckybuf = 0, offset = 0;  //��һ���ֽڣ�offset�����λ�����λ����Ϊ0~7
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
		writebits(frame_length, 13);//frame length,����ADTSͷ��AACԭʼ��
		writebits(0x07FF, 11);
		writebits(0x00, 2);//number_of_raw_data_blocks_in_frame
	};

	//�����pcm���ݵ�������Ϣ
	int input_sample_rate = 44100;
	uint64_t input_channel_layout = AV_CH_LAYOUT_STEREO;
	AVSampleFormat input_sample_fmt = AV_SAMPLE_FMT_S16;
	int input_channels = av_get_channel_layout_nb_channels(input_channel_layout);
	//�����ı��뷽ʽ
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
	pcodecctx->codec_id = codec_id;//�������ڱ����avcodec��һЩ����
	pcodecctx->codec_type = AVMEDIA_TYPE_AUDIO;
	pcodecctx->sample_fmt = pcodec->sample_fmts[0];
	//AVCodec��sample_fmts��Ա��array of supported sample formats, 
	//or NULL if unknown, array is terminated by -1
	//ʵ����AAC��������֧��һ�ֲ�����ʽ����AV_SAMPLE_FMT_FLTP (8)
	pcodecctx->sample_rate = input_sample_rate;
	pcodecctx->channel_layout = input_channel_layout;
	pcodecctx->channels =input_channels;
	
	//avcodec_open2�ḳֵpcodecctx->frame_size
	//�����һ֡�⣬�ύ���������Ĵ�С�������ֵ��ͬ
	if (avcodec_open2(pcodecctx, pcodec, nullptr) < 0)
	{
		cout << "Couldn't open avcodec" << endl;
		avcodec_close(pcodecctx);
		av_free(pcodecctx);
		return -1;
	}
	
	//��ԭʼpcm������vector<uint8_t> input_buf������swr_convertת��
	//ת��������ݿ�����AVFrame�Ļ����������Ȼ����AAC����������Ϊpakcet
	//����趨��AVCodecContext������swr_convert��ת������Ҳ��ȷ����
	int swr_cvted_sample_rate = pcodecctx->sample_rate;
	uint64_t swr_cvted_channel_layout = pcodecctx->channel_layout;
	int swr_cvted_channels = av_get_channel_layout_nb_channels(swr_cvted_channel_layout);
	AVSampleFormat swr_cvted_sample_fmt = pcodecctx->sample_fmt;
	SwrContext* pswrctx = swr_alloc_set_opts(
		nullptr,//SwrContext *s,������ֻ���ò�����nullptr����alloc�����ò���
		swr_cvted_channel_layout, //int64_t out_ch_layout
		swr_cvted_sample_fmt, //enum AVSampleFormat out_sample_fmt
		swr_cvted_sample_rate, //int out_sample_rate
		input_channel_layout,  //int64_t  in_ch_layout
		input_sample_fmt, //enum AVSampleFormat  in_sample_fmt
		input_sample_rate,// int  in_sample_rate
		0,
		nullptr);
	swr_init(pswrctx);

	//������������֡
	AVFrame* pframe = av_frame_alloc();  //�洢ԭʼpcm����
	pframe->format = pcodecctx->sample_fmt;
	pframe->nb_samples = pcodecctx->frame_size; //important
	pframe->channels = pcodecctx->channels;
	//����AVFframe�������������������Ҫ�Ļ�����size��size̫С��
	//�����avcodec_fill_audio_frameʧ��
	int buffer_size = av_samples_get_buffer_size(nullptr, pframe->channels, pframe->nb_samples,
		(AVSampleFormat)pframe->format, 0);
	uint8_t* frame_buf = (uint8_t*)av_malloc(buffer_size);
	int ret = avcodec_fill_audio_frame(pframe, pframe->channels, (AVSampleFormat)pframe->format,
		frame_buf, //Ҫ�󶨸�pframe�Ļ�����
		buffer_size,     //��������С
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

	//swr_convert()����������
	vector<uint8_t*> convert_data(swr_cvted_channels);//����swr_convert()�����
	int alloc_size=av_samples_alloc(&convert_data[0], 
		nullptr, 
		swr_cvted_channels,
		pframe->nb_samples, 
		(AVSampleFormat)pframe->format, 
		0);
	//swr_cvted_sample_fmtΪAV_SAMPLE_FMT_FLTP��planar��ʽ
	//��˸�convert_data[0]��convert_data[1]ָ�븳ֵ���ֱ�Ϊ��������
	//������ڴ��С������ֵΪ=ÿͨ��������nb_samples*
	//ÿ�����ֽ���av_get_bytes_per_sample(swr_cvted_sample_fmt)*ͨ����swr_cvted_channels
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

	//����swr_convert���л��幦�ܣ�������뻺�����Ĵ�С����������
	vector<uint8_t> input_buf(1024); //��pcm�ļ��������˴���Ҳ��swr_convert()������
	int in_samples = (int)input_buf.size() / av_get_bytes_per_sample(input_sample_fmt) /
		input_channels;//input_buf������������ÿ���ŵ��Ĳ�������
	int framecnt = 0;	
	int output_space_offset = 0;//�õ�ͨ������������
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
		//������const�������޷��������ӡ�uint8_t **��ת��Ϊ��const uint8_t **��
		const uint8_t* in = &input_buf[0];  
		//convert_data�����Ļ������Ĵ�СΪalloc_size����Ӧ�ĵ�ͨ����������Ϊ
		//��Ϊpframe->nb_samples��ͨ����Ϊswr_cvted_channels����ʹ��˫ͨ�������
		//Ϊpack��ʽ����Ҳ����convert_data[0]ָ����ڴ�洢���ݣ�convert_data[1]��
		//convert_data[2]����ȫΪnullptr��Ҳ����˵ͨ����������swr_convert��Ļ���������
		//������group_num��¼��Щconvert_data[i]��ָ��ʵ�����ݣ������ȫΪnullptr
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
		);//������������������õ�ͨ������������
		
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
			//����������̫�٣������������δ�������������
			if (num < available_output_space)
			{
				output_space_offset += num;
				break;
			}
			//�����������㹻�࣬����������������������������
			//�п��ܲ����������ݺ����ݻ�����swr��
			else if (num == available_output_space)
			{
				//����ۼƹ�һ֡
				//��ת��������ݸ��Ƹ�AVFrame�Ļ�����
				//��Ϊÿ����Чconvert_data[i]ָ����ڴ�Ĵ�Сһ��
				for (int i = 0; i <group_num; i++)
				{
					memcpy(pframe->data[i], convert_data[i], alloc_size / group_num);
				}

				int ret_send = avcodec_send_frame(pcodecctx, pframe);
				if (ret_send == 0) //�ɹ�
				{
					;
				}
				else if (ret_send == AVERROR(EAGAIN))//���������뻺�������Ӷ��޷�����frame
				{
					//����ÿ��avcodec_send_frame��avcodec_receive_packetֱ��������Ҫ�������µ�frame��
					//������ﲻ���ڱ��������뻺�������Ӷ��޷�����frame�����
					assert(ret_send != AVERROR(EAGAIN));
				}
				else if (ret_send == AVERROR_EOF)//�������Ѿ���ˢ�£���Ӧ���ٸ�֮����frame
				{
					//û�е���avcodec_send_frame(pcodecctx,nullptr)����˱������������ˢ��ģʽ��
					//�÷�֧����ִ��
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

				//ѭ��avcodec_receive_packetֱ��������Ҫ�������µ�frame
				while (1)
				{
					av_init_packet(&pkt);
					pkt.data = NULL;
					pkt.size = 0;
					int ret_recv = avcodec_receive_packet(pcodecctx, &pkt);
					if (ret_recv == 0)  //�ɹ�
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
						//�����ٸ������������µ�frame���������ſ������packet
						break;
					}
					else if (ret_recv == AVERROR_EOF)//����������ȫˢ�£�û�и��������
					{
						//û�е���avcodec_send_frame(pcodecctx,nullptr)����˱������������ˢ��ģʽ��
						//�÷�֧����ִ��
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

				//��������swr�е���������
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

	//ˢ�±�����
	//���뷢��ˢ��avcodec_send_frame(pcodecctx, nullptr)������
	//������ avcodec_send_frame(pcodecctx, nullptr)����ʹһֱavcodec_receive_packet()��
	//����ʾҪ���µ����룬�����������л����packet
	int ret_send = avcodec_send_frame(pcodecctx, nullptr);
	assert(ret_send != AVERROR(EAGAIN) && ret_send != AVERROR_EOF);
	if (ret_send == 0) { cout << "ˢ�±�����" << endl; }
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
	//ѭ��avcodec_receive_packetֱ������������ȫˢ�£�û�и������
	while (1)
	{
		av_init_packet(&pkt);
		pkt.data = NULL;
		pkt.size = 0;
		int ret_recv = avcodec_receive_packet(pcodecctx, &pkt);
		if (ret_recv == 0)  //�ɹ�
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
		else if (ret_recv == AVERROR_EOF)//����������ȫˢ�£�û�и��������
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
		cout << "���" << filename_out << endl;
		return 0;
	}
}
