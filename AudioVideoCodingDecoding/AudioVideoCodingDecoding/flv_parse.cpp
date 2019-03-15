#include<iostream>
#include<fstream>
#include<string>
#include<vector>
#include<algorithm>
#include<numeric>
#include<cmath>
#include<memory>
using std::ifstream;
using std::ofstream;
using std::string;
using std::vector;
using std::cout;
using std::endl;
using std::cin;

#include<cstring>
#include<iostream>
using std::cout;
using std::endl;

typedef unsigned char byte;
const byte TAG_TYPE_AUDIO = 8;
const byte TAG_TYPE_VIDEO = 9;
const byte TAG_TYPE_SCRIPT = 18;

#pragma pack(1)
struct FlvHeader
{
	byte signature[3]; //�ļ���ʶ����Ϊ"FLV"
	byte version;    //�汾���汾1λ0x01
	byte flags;    //ǰ��λ����7λ��Ϊ0����6λ��ʾ�Ƿ������Ƶtag
	//��8λ��ʾ�Ƿ������Ƶtag
	unsigned data_offset; //flvheader�ļ�ͷ�Ĵ�С���汾1Ϊ9
};
#pragma pack()

#pragma pack(1)
struct TagHeader
{
	byte tag_type;  //tag���ͣ�
	byte data_size[3]; //TagData���ֵĴ�С
	byte time_stamp[3];  //��tag��ʱ���
	unsigned reserved;  //1�ֽ�ʱ�������չ+3�ֽڵ�stream id������0��
};
#pragma pack()

//����ֽ����鰴С��ģʽ�洢���ļ��У���ֱ��ָ������ת�����ɵõ�intֵ
//flv�ļ��д��ģʽ���ֽ����飬תΪint
unsigned reverse_bytes(byte* p, char c)
{
	int r = 0;
	for (int i = 0; i < c; i++)
	{
		r |= (*(p + i) << ((c - 1) * 8 - 8 * i));
	}
	return r;
}

void simplest_flv_parse(const char* url)
{
	ifstream ifs(url, std::ios::binary);
	if (!ifs) { cout << "not read" << endl; return; }
	ofstream ofs_audio;
	ofstream ofs_video;
	string ofs_audio_path = "output_from_flv.mp3";
	string ofs_video_path = "output_from_flv.flv";

	bool output_a = true;  //�Ƿ������������Ƶ
	bool output_v = true;   //�Ƿ������������Ƶ
	FlvHeader flvheader;
	TagHeader tagheader;
	unsigned previoustagsize;
	unsigned ts = 0;
	unsigned ts_new = 0;

	ifs.read((char*)&flvheader, sizeof(FlvHeader));

	printf("============== FLV Header ==============\n");
	printf("Signature:  0x %c %c %c\n", flvheader.signature[0], flvheader.signature[1], flvheader.signature[2]);
	printf("Version:    0x %X\n", flvheader.version);
	printf("Flags  :    0x %X\n", flvheader.flags);
	printf("HeaderSize: 0x %X\n", reverse_bytes((byte *)&flvheader.data_offset, sizeof(flvheader.data_offset)));
	printf("========================================\n");

	while(!ifs.fail())
	{
		ifs.read((char*)&previoustagsize,4);//�ȼ��ڰ���˶�
		ifs.read((char*)&tagheader,sizeof(TagHeader));

		char tagtype_str[10];
		switch (tagheader.tag_type)
		{
		case TAG_TYPE_AUDIO:strcpy_s(tagtype_str, 10, "AUDIO");
			break;
		case TAG_TYPE_VIDEO:strcpy_s(tagtype_str, 10, "VIDEO");
			break;
		case TAG_TYPE_SCRIPT:strcpy_s(tagtype_str, 10, "SCRIPT");
			break;
		default:strcpy_s(tagtype_str, 10, "unkonwn"); break;
		}
		
		int tagheader_datasize = reverse_bytes((byte *)&tagheader.data_size, sizeof(tagheader.data_size));
		int tagheader_timestamp = reverse_bytes((byte *)&tagheader.time_stamp, sizeof(tagheader.time_stamp));

		printf("[%6s] %6d %6d |", tagtype_str, tagheader_datasize, tagheader_timestamp);
		
		if (ifs.fail()) break;

		switch (tagheader.tag_type)
		{
			case TAG_TYPE_AUDIO:
			{
				char audiotag_str[100] = { 0 };
				strcat_s(audiotag_str, 100, "| ");
				unsigned char tagdata_first_byte;
				ifs.read((char*)&tagdata_first_byte, 1);
				int x = (tagdata_first_byte & 0xF0) >> 4;
				switch (x)
				{
					case 0:strcat_s(audiotag_str, 100, "Linear PCM, platform endian"); break;
					case 1:strcat_s(audiotag_str, 100, "ADPCM"); break;
					case 2:strcat_s(audiotag_str, 100, "MP3"); break;
					case 3:strcat_s(audiotag_str, 100, "Linear PCM, little endian"); break;
					case 4:strcat_s(audiotag_str, 100, "Nellymoser 16-kHz mono"); break;
					case 5:strcat_s(audiotag_str, 100, "Nellymoser 8-kHz mono"); break;
					case 6:strcat_s(audiotag_str, 100, "Nellymoser"); break;
					case 7:strcat_s(audiotag_str, 100, "G.711 A-law logarithmic PCM"); break;
					case 8:strcat_s(audiotag_str, 100, "G.711 mu-law logarithmic PCM"); break;
					case 9:strcat_s(audiotag_str, 100, "reserved"); break;
					case 10:strcat_s(audiotag_str, 100, "AAC"); break;
					case 11:strcat_s(audiotag_str, 100, "Speex"); break;
					case 14:strcat_s(audiotag_str, 100, "MP3 8-Khz"); break;
					case 15:strcat_s(audiotag_str, 100, "Device-specific sound"); break;
					default:strcat_s(audiotag_str, 100, "UNKNOWN"); break;
				}
				strcat_s(audiotag_str, 100, " | ");
				
				int y= (tagdata_first_byte & 0x0C) >> 2;
				switch (y)
				{
					case 0:strcat_s(audiotag_str, 100, "5.5-kHz"); break;
					case 1:strcat_s(audiotag_str, 100, "1-kHz"); break;
					case 2:strcat_s(audiotag_str, 100, "22-kHz"); break;
					case 3:strcat_s(audiotag_str, 100, "44-kHz"); break;
					default:strcat_s(audiotag_str, 100, "UNKNOWN"); break;
				}
				strcat_s(audiotag_str, 100, " | ");
				
				int z = (tagdata_first_byte & 0x02)>>1;
				switch (z)
				{
					case 0:strcat_s(audiotag_str,100, "8Bit"); break;
					case 1:strcat_s(audiotag_str, 100, "16Bit"); break;
					default:strcat_s(audiotag_str, 100, "UNKNOWN"); break;
				}
				strcat_s(audiotag_str, 100, " | ");
				int w = tagdata_first_byte & 0x01;
				switch (w)
				{
					case 0:strcat_s(audiotag_str, 100, "Mono"); break;
					case 1:strcat_s(audiotag_str, 100, "Stereo"); break;
					default:strcat_s(audiotag_str, 100, "UNKNOWN"); break;
				}
				strcat_s(audiotag_str, 100, " |");
				cout << audiotag_str << endl;

				int data_size= reverse_bytes((byte *)&tagheader.data_size, sizeof(tagheader.data_size))-1;
				if (output_a && !ofs_audio)
				{
					ofs_audio.open(ofs_audio_path, std::ios::binary);
				}
				if (output_a)
				{
					vector<byte> temp(data_size);
					ifs.read((char*)&temp[0],data_size);
					ofs_audio.write((char*)&temp[0], data_size);
				}
				else
				{
					ifs.seekg(data_size, std::ios::cur);
				}
				break;
			}
			case TAG_TYPE_VIDEO:
			{
				char videotag_str[100] = { 0 };
				strcat_s(videotag_str,100, "| ");
				unsigned char tagdata_first_byte;
				ifs.read((char*)&tagdata_first_byte, 1);
				int x = tagdata_first_byte & 0xF0;
				x = x >> 4;
				switch (x)
				{
					case 1:strcat_s(videotag_str,100, "key frame  "); break;
					case 2:strcat_s(videotag_str, 100, "inter frame"); break;
					case 3:strcat_s(videotag_str, 100, "disposable inter frame"); break;
					case 4:strcat_s(videotag_str, 100, "generated keyframe"); break;
					case 5:strcat_s(videotag_str, 100, "video info/command frame"); break;
					default:strcat_s(videotag_str, 100, "UNKNOWN"); break;
				}
				strcat_s(videotag_str,100, " | ");

				int y;
				y = tagdata_first_byte & 0x0F;
				switch (y)
				{
					case 1:strcat_s(videotag_str, 100, "JPEG (currently unused)"); break;
					case 2:strcat_s(videotag_str, 100, "Sorenson H.263"); break;
					case 3:strcat_s(videotag_str, 100, "Screen video"); break;
					case 4:strcat_s(videotag_str, 100, "On2 VP6"); break;
					case 5:strcat_s(videotag_str, 100, "On2 VP6 with alpha channel"); break;
					case 6:strcat_s(videotag_str, 100, "Screen video version 2"); break;
					case 7:strcat_s(videotag_str, 100, "AVC"); break;
					default:strcat_s(videotag_str, 100, "UNKNOWN"); break;
				}

				cout << videotag_str << endl;
				
				if (output_v && !ofs_video)
				{
					ofs_video.open(ofs_video_path,std::ios::binary);
					flvheader.flags = 1;
					ofs_video.write((char*)&flvheader,sizeof(flvheader));
					unsigned previoustagsize_z = 0;
					ofs_video.write((char *)&previoustagsize_z,sizeof(previoustagsize_z));
				}

				ifs.seekg(-1, std::ios::cur);  //���˵�Tag Data����ʼλ��

				//����Tag Data��Tag Data�����ŵ�previoustagsize���ܴ�С
				int data_size = reverse_bytes((byte *)&tagheader.data_size, sizeof(tagheader.data_size)) + 4;
				if (output_v)
				{
					ofs_video.write((char *)&tagheader, sizeof(tagheader));
					vector<byte> temp(data_size);
					ifs.read((char*)&temp[0], data_size);
					ofs_video.write((char*)&temp[0], data_size);
				}
				else
				{
					ifs.seekg(data_size, std::ios::cur);
				}
				//rewind 4 bytes, because we need to read the previoustagsize again for the loop's sake
				ifs.seekg(-4, std::ios::cur);
				break;
			}
			case TAG_TYPE_SCRIPT:
			{
				cout << endl;

				ofs_video.write((char *)&tagheader, sizeof(tagheader));
				int data_size = reverse_bytes((byte *)&tagheader.data_size, sizeof(tagheader.data_size))+4;
				vector<byte> temp(data_size);
				ifs.read((char*)&temp[0], data_size);
				ofs_video.write((char*)&temp[0], data_size);
				ifs.seekg(-4,std::ios::cur);
				break;
			}
			default:
			{
				cout << "TAG_TYPE error!" << endl;
				return;
			}
		}
	}
}
int main()
{
	simplest_flv_parse("cuc_ieschool.flv");
	//Flv Header�е�Headersizeռ4�ֽڣ����ģʽ�洢��Tag Header��Datasizeռ4�ֽڣ�Ҳ�Ǵ��ģʽ�洢
}