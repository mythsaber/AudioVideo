#include <iostream>
#include <fstream>
#include <cassert>
#include <vector>
#include <string>
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

//函数调用后，输入pkt将被av_packet_unref()，输出outpkts由调用者负责 管理，调av_packet_free释放
static int filter_packet(AVBSFContext* bsfctx, AVPacket* pkt, std::vector<AVPacket*>& outpkts)
{
	assert(outpkts.empty());

	if (bsfctx) 
	{
		int ret = av_bsf_send_packet(bsfctx, pkt);
		if (ret < 0) 
		{
			printf("Failed to call av_bsf_send_packet");
			av_packet_unref(pkt);
			return ret;
		}

		while (1)
		{
			AVPacket* outpkt = av_packet_alloc();
			ret = av_bsf_receive_packet(bsfctx, outpkt);
			if (ret == 0)
			{
				outpkts.push_back(outpkt);
			}
			else
			{
				av_packet_free(&outpkt);
				break;
			}
		}

		if (ret < 0 && (ret != AVERROR(EAGAIN) && ret != AVERROR_EOF)) 
		{
			for (auto& i : outpkts)
			{
				av_packet_free(&i);
			}
			outpkts.clear();
			printf("h264_mp4toannexb filter failed to receive output packet\n");
			return ret;
		}
	}
	return 0;
}

int extractH264()
{
	MediaReader reader("girl_640x360.mp4");
	if (reader) { ; }
	if (!reader) {
		cout << "Failed to open file" << endl;
		return -1;
	}

	//遍历AVDictionary，AVStream::metadata
	AVDictionaryEntry* item = nullptr;
	printf("video stream's metadata is:\n");
	while (item = av_dict_get(reader.get_stream()->metadata, "", item, AV_DICT_IGNORE_SUFFIX))
	{
		printf("(%s: %s)\n", item->key, item->value); //(language: und)、(handler_name: VideoByEZMediaEditor)
	}
	printf("\n");

	assert(reader.get_stream()->codecpar->codec_id == AV_CODEC_ID_H264);
	const AVStream* inst = reader.get_stream();
	uint8_t *extradata = inst->codecpar->extradata;
	int extradata_size = inst->codecpar->extradata_size;

	std::ofstream codec_extradata_ofs("C:\\Users\\myth\\Desktop\\testH264\\output_h264_codec_extradata.bin", std::ios::binary);
	if (!codec_extradata_ofs)
	{
		printf("Failed to open codec_extradata_ofs");
		return -1;
	}
	std::ofstream avpkt_ofs("C:\\Users\\myth\\Desktop\\testH264\\output_h264_avpacket.h264", std::ios::binary);
	if (!avpkt_ofs)
	{
		printf("Failed to open avpkt_ofs");
		return -1;
	}
	std::ofstream avpkt_afterfilter_ofs("C:\\Users\\myth\\Desktop\\testH264\\output_h264_avpacket_afterfilter.h264",std::ios::binary);
	if (!avpkt_afterfilter_ofs)
	{
		printf("Failed to open avpkt_afterfilter_ofs");
		return -1;
	}

	codec_extradata_ofs.write((const char*)inst->codecpar->extradata, inst->codecpar->extradata_size);

	const AVBitStreamFilter* h264bsf = av_bsf_get_by_name("h264_mp4toannexb");
	if (!h264bsf)
	{
		printf("Failed to found AVBitStreamFilter for H.264 streams");
		return -1;
	}
	AVBSFContext* bsfctx = nullptr;
	int ret=av_bsf_alloc(h264bsf, &bsfctx);
	if (ret < 0)
	{
		printf("Failed to allocate a context for a given bitstream filter");
		return -1;
	}
	ret = avcodec_parameters_copy(bsfctx->par_in, inst->codecpar);
	if (ret < 0)
	{
		printf("Failed to set par_in for AVBSFContext");
		return -1;
	}
	ret = av_bsf_init(bsfctx);
	if (ret < 0)
	{
		printf("Failed to call av_bsf_init");
		return ret;
	}

	AVPacket pkt;
	av_init_packet(&pkt);
	pkt.data = nullptr;
	pkt.size = 0;

	int pktnum = 0;
	int pktnum_bsf = 0;
	while (av_packet_unref(&pkt), reader.read_video_packet(pkt) == 0)
	{
		//std::ofstream tmp("C:\\Users\\myth\\Desktop\\testH264\\avpkt_" + std::to_string(pktnum++) + ".h264", std::ios::binary);
		//if (!tmp)
		//{
		//	printf("Failed to open tmp");
		//	return -1;
		//}
		//tmp.write((const char*)pkt.data, pkt.size);

		avpkt_ofs.write((const char*)pkt.data, pkt.size);

		std::vector<AVPacket*> outpts;
		ret = filter_packet(bsfctx, &pkt, outpts);
		if (ret!=0)
		{
			printf("Failed to get pkt after AVBitStreamFilter");
			av_bsf_free(&bsfctx);
			return -1;
		}
		if (outpts.size() > 1)
		{
			printf("Input one AVPacket, Output more than one (%d) AVPacket", outpts.size());
		}
		for (auto& i : outpts)
		{
			//std::ofstream tmp_bsf("C:\\Users\\myth\\Desktop\\testH264\\avpkt_bsf"+std::to_string(pktnum_bsf++)+".h264", std::ios::binary);
			//if (!tmp_bsf)
			//{
			//	printf("Failed to open tmp_bsf");
			//	return -1;
			//}
			//tmp_bsf.write((const char*)i->data, i->size);

			avpkt_afterfilter_ofs.write((const char*)i->data, i->size);
			av_packet_free(&i);
		}
	}
	av_bsf_free(&bsfctx);
	printf("AVPacket before bsf, num=%d\n", pktnum);
	printf("AVPacket after bsf, num=%d\n", pktnum_bsf);
	return 0;
}

int main(int argc, char* argv[])
{
	extractH264();
}

