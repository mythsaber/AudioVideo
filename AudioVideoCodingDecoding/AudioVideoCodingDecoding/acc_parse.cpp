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

//buf_size为buffer[]数组的长度
//ADTS的头信息都是7个字节
//检测7个字节的ADTS header的位置，获取帧长度
//将帧数据（包括ADTS header和ES）拷贝到data中
int getADTSFrame(unsigned char* buffer, int buf_size, 
	unsigned char* data, int& frame_lenth) 
{
	int lenth = 0;  //frame_length, 一个ADTS帧的长度包括ADTS头和AAC原始流
	if (!buffer || !data ) return -2;
	while (1)
	{
		if (buf_size < 7) return -1;
		if ((buffer[0] == 0xff) && ((buffer[1] & 0xf0) == 0xf0))//开始的12个bit都为1
		{
			lenth |= ((buffer[3] & 0x03) << 11);
			lenth |= (buffer[4] << 3);
			lenth |= ((buffer[5]&0xe0) >> 5);
			break;
		}
		buf_size--;
		buffer++;
	}
	if (buf_size < lenth) return 1;
	memcpy(data, buffer, lenth);
	frame_lenth = lenth;
	return 0;
}
void simplest_aac_parse(const char* url)
{
	ifstream ifs(url, ifstream::binary);
	if (!ifs) { cout << "not read" << endl; return; }

	cout << "-----+- ADTS Frame Table -+------+" << endl;
	cout << " NUM | Profile | Frequency| Size |" << endl;
	cout << "-----+---------+----------+------+" << endl;

	//aac音频码流很长，每次只读取1024 * 1024个字节，处理完后，再读取下一个
	vector<unsigned char> aacbuffer(1024 *10);
	vector<unsigned char> aacframe(1024);
	int cnt = 0;
	int rest_of_last_read = 0; //上一次从aac音频流中读取的数据处理后剩余的部分
	int sum_length = 0;
	while (ifs.peek() != EOF)
	{
		ifs.read((char*)&aacbuffer[rest_of_last_read], aacbuffer.size()- rest_of_last_read);
		cout << "rest_of_last_read + 读取的字节数：" << rest_of_last_read<<" + "
			<<ifs.gcount()<<" = "<< rest_of_last_read + ifs.gcount() << endl;
		if ((int)ifs.gcount() < (aacbuffer.size() - rest_of_last_read))  //最后一次读取时，可能读到文件末尾也读不够aacbuffer.size()- rest_of_last_read个
		{
			aacbuffer.resize(rest_of_last_read + ifs.gcount());
		}
		int offset = 0; //当前aacbuffer中已处理过的数据量
		while (1)  //逐个处理ADTS帧
		{
			//一个ADTS帧，长度为frame_length，存入aacframe
			int frame_length;
			int ret = getADTSFrame(&aacbuffer[offset], (int)aacbuffer.size()-offset, &aacframe[0], frame_length);
			if (ret == -2) { cout << "未分配内存空间" << endl; exit(EXIT_FAILURE); }
			else if (ret == 1 || ret==-1)//错误码，表示字节数不足一个ADTS
			{
				cout << endl;
				memcpy((char*)&aacbuffer[0], (char*)&aacbuffer[offset], aacbuffer.size() - offset);
				rest_of_last_read = (int)aacbuffer.size() - offset;
				break;  //ifs重新读取
			}

			sum_length += frame_length;

			char profile_str[10] = { 0 };
			char frequence_str[10] = {0};
			unsigned char profile = (aacframe[2] & 0xC0)>>6; //表示使用哪个级别的AAC
			switch (profile)
			{
			case 0:
				strcpy_s(profile_str,10, "Main"); 
				break;
			case 1:
				strcpy_s(profile_str, 10, "LC");
				break;
			case 2:
				strcpy_s(profile_str, 10, "SSR");
				break;
			case 3:
				strcpy_s(profile_str, 10, "unknown");
				break;
			}

			unsigned char sampling_frequence_index = (aacframe[2] & 0x3C)>>2;
			switch (sampling_frequence_index)
			{
			case 0: strcpy_s(frequence_str, 10, "96000Hz"); break;
			case 1: strcpy_s(frequence_str, 10, "88200Hz"); break;
			case 2: strcpy_s(frequence_str, 10, "64000Hz"); break;
			case 3: strcpy_s(frequence_str, 10, "48000Hz"); break;
			case 4: strcpy_s(frequence_str, 10, "44100Hz"); break;
			case 5: strcpy_s(frequence_str, 10, "32000Hz"); break;
			case 6: strcpy_s(frequence_str, 10, "24000Hz"); break;
			case 7: strcpy_s(frequence_str, 10, "22050Hz"); break;
			case 8: strcpy_s(frequence_str, 10, "16000Hz"); break;
			case 9: strcpy_s(frequence_str, 10, "12000Hz"); break;
			case 10: strcpy_s(frequence_str, 10, "11025Hz"); break;
			case 11: strcpy_s(frequence_str, 10, "8000Hz"); break;
			default: strcpy_s(frequence_str, 10, "unknown"); break;
			}

			printf("%5d| %8s|  %8s| %5d|\n", cnt, profile_str, frequence_str, frame_length);
			offset += frame_length;
			cnt++;
			if (offset == (int)aacbuffer.size())
			{
				rest_of_last_read = 0;
				break;
			}
		}
	}
	ifs.close();

	cout << "sum_length is: "<<sum_length << endl; //481488
}

int main()
{
	ifstream ifs("nocturne.aac",ifstream::binary);
	ifs.seekg(0,std::ios::end);
	cout <<"文件大小为："<< ifs.tellg() << endl;//480002  为什么不是481464
	ifs.close();

	simplest_aac_parse("nocturne.aac"); 
}