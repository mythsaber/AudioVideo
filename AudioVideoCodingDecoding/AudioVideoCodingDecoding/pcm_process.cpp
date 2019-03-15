#include<iostream>
#include<fstream>
#include<string>
#include<vector>
#include<algorithm>
#include<numeric>
#include<cmath>
#include<chrono>
using std::ifstream;
using std::ofstream;
using std::string;
using std::vector;
using std::cout;
using std::endl;
using std::cin;

//分离PCM16LE双声道音频采样数据的左声道和右声道
void simplest_pcm16le_split(const char* url)
{
	ifstream ifs(url, ifstream::binary);
	if (!ifs) { cout << "not read" << endl; return; }
	ofstream ofs1("output_left.pcm", ofstream::binary);
	ofstream ofs2("output_right.pcm",ofstream::binary);
	unsigned char sample[4];

	/*
	//peek()函数读取一个字节，但该字符依然是将要提取到输入流的下一个字符
	//这种写法造成不必要的开销，不如后文方法效率高
	auto begin = std::chrono::steady_clock::now();
	while (ifs.peek()!=EOF)  
	{
		ifs.read((char*)sample,4);
		ofs1.write((char*)sample,2);
		ofs2.write((char*)(sample+2), 2);
	}
	auto end = std::chrono::steady_clock::now();
	cout << "method 1 time:" << std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count() << "微秒" << endl;
	*/
	
	auto begin = std::chrono::steady_clock::now();
	while (ifs.read((char*)sample,4))
	{
		ofs1.write((char*)sample, 2);
		ofs2.write((char*)(sample + 2), 2);
	}
	auto end = std::chrono::steady_clock::now();
	cout << "method 2 time:" << std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count() << "微秒" << endl;
	

	ifs.close();
	ofs1.close();
	ofs2.close();
}

//将PCM16LE双声道音频采样数据中左声道的音量降一半
void simplest_pcm16le_halfvolumeleft(const char* url)
{
	ifstream ifs(url, ifstream::binary);
	if (!ifs) { cout << "not read" << endl; return; }
	ofstream ofs("output_halfleft.pcm", ofstream::binary);
	unsigned char sample[4];
	while (ifs.read((char*)sample, 4))
	{
		*((short*)sample) = (*((short*)sample)) >> 1;  
		//2个单字节转为short，再除以2
		ofs.write((char*)sample, 2);
		ofs.write((char*)(sample + 2), 2);
	}
	ifs.close();
	ofs.close();
}

void test_seekg_eof()
{
	ifstream ifs("nums.bin", ifstream::binary);
	unsigned char x;
	while (ifs.peek() != EOF)
	{
		ifs >> x;
		cout << "x read : "<<(int)x << endl;
		ifs.seekg(2,std::ios::cur);//cin._Seekcur
		cout << "failed ?:"<<cin.fail() << endl;
	}
}

//将PCM16LE双声道音频采样数据的声音速度提高一倍
void simplest_pcm16le_doublespeed(const char *url)
{
	ifstream ifs(url,ifstream::binary);
	if (!ifs) { cout << "not read" << endl; return; }
	ofstream ofs("output_doublespeed.pcm",ofstream::binary);
	unsigned char sample[4];
	while (ifs.read((char*)sample, 4))
	{
		ofs.write((char*)sample,4);
		ifs.seekg(4,ifs._Seekcur); //如果之前读的已经是最后4个字节，则该句相当于没有，并不是导致ifs fail
	}
	ifs.close();
	ofs.close();
}

//将PCM16LE双声道音频采样数据转换为PCM8音频采样数据
void simplest_pcm16le_to_pcm8(const char *url)
{
	ifstream ifs(url, ifstream::binary);
	if (!ifs) { cout << "not read" << endl; return; }
	ofstream ofs("output_8.pcm", ofstream::binary);
	unsigned char sample[4];
	while (ifs.read((char*)sample, 4))
	{
		short samplenum16 = *((short*)sample);
		unsigned char samplenum8_u = (unsigned char)((samplenum16 >> 8) + 128);
		ofs.write((char*)&samplenum8_u,1);

		samplenum16 = *((short*)(sample+2));
		samplenum8_u = (unsigned char)((samplenum16 >> 8) + 128);
		ofs.write((char*)&samplenum8_u, 1);
	}
	ifs.close();
	ofs.close();
}

void test_read_to_end()
{
	ifstream ifs("nums.bin", ifstream::binary); //存储的是unsigned char型的1 2 3 4 5
	unsigned char buf[16];
	cout << ifs.read((char*)buf, 8).fail() << endl;
	for (auto i : buf)
		cout << (int)i << " ";
	cout << endl;
	/*
	运行结果：
	1
	1 2 3 4 5 204 204 204 204 204 204 204 204 204 204 204
	请按任意键继续. . .
	*/
}

//从PCM16LE单声道音频采样数据中截取一部分数据
void simplest_pcm16le_cut_singlechannel(const char *url, int start_num,
	int dur_num)
{
	ifstream ifs(url, ifstream::binary);
	if (!ifs) { cout << "not read" << endl; return; }
	ofstream ofs("output_cut_of_pcm16_single_channel.pcm", ofstream::binary);
	ofstream ofs_stat("output_cut_of_pcm16_single_channel.txt",ofstream::binary);
	
	unsigned char sample[2];
	int cnt = 0;
	while (ifs.read((char*)sample, 2))
	{
		if (cnt >= start_num&&cnt < (start_num + dur_num))
		{
			ofs.write((char*)sample,2);
			short samplenum = *((short*)sample);
			ofs_stat << samplenum <<' ';
			if ((cnt % 10) == 0)
			{
				ofs_stat << "\r\n";
			}
		}
		cnt++;
	}
	ifs.close();
	ofs.close();
	ofs_stat.close();
}

//将PCM16LE音频(单通道、双通道)采样数据转换为WAVE格式音频数据
void simplest_pcm16le_to_wave(const char *pcmpath, int channels,
	int sample_rate, const char *wavepath)
{
	struct WAVE_HEADER {
		char         fccID[4];  //"RIFF"
		unsigned   long    dwSize; //文件大小-8，整个文件的长度减去fccID和dwSize的长度
		char         fccType[4]; //"WAVE"
	};

	struct WAVE_FMT {
		char         fccID[4]; //“fmt ”
		unsigned   long       dwSize;  //16,表示该区块数据的长度（不包含ID和Size的长度)
		unsigned   short     wFormatTag; //表示Data区块存储的音频数据的格式，PCM音频数据的值为1
		unsigned   short     wChannels;  //1：单声道，2：双声道
		unsigned   long       dwSamplesPerSec;
		unsigned   long       dwAvgBytesPerSec; //SampleRate * NumChannels * BitsPerSample / 8
		unsigned   short     wBlockAlign; //数据块对齐
		unsigned   short     uiBitsPerSample; //采样位数
	};

	struct WAVE_DATA {
		char       fccID[4];  //"data"
		unsigned long dwSize;  //音频数据的长度
	};

	ifstream ifs(pcmpath, ifstream::binary);
	if (!ifs) { cout << "not read" << endl; return; }
	ofstream ofs(wavepath, ofstream::binary);

	WAVE_HEADER pcmHeader;
	WAVE_FMT pcmFmt;
	WAVE_DATA pcmData;
	
	//获取音频数据的长度
	ifs.seekg(0,ifs._Seekend);// //基地址为文件结束处，偏移地址为0，于是指针定位在文件结束处
	int size=(int)ifs.tellg(); //定位指针，因为它在文件结束处，所以也就是文件的大小
	ifs.seekg(0,ifs._Seekbeg);

	memcpy(pcmHeader.fccID, "RIFF",4);
	memcpy(pcmHeader.fccType, "WAVE", 4);
	pcmHeader.dwSize = sizeof(WAVE_HEADER) + sizeof(WAVE_FMT) + sizeof(WAVE_DATA) + size - 8;
	
	memcpy(pcmFmt.fccID, "fmt ", 4);
	pcmFmt.dwSize = 16;
	pcmFmt.wFormatTag = 1;
	pcmFmt.wChannels = channels;
	pcmFmt.dwSamplesPerSec = sample_rate;
	pcmFmt.dwAvgBytesPerSec = sample_rate * channels * 2;
	pcmFmt.wBlockAlign = 2;
	pcmFmt.uiBitsPerSample = 16;

	memcpy(pcmData.fccID, "data", 4);
	pcmData.dwSize = size;

	ofs.write((char*)&pcmHeader,sizeof(WAVE_HEADER));
	ofs.write((char*)&pcmFmt, sizeof(WAVE_FMT));
	ofs.write((char*)&pcmData, sizeof(WAVE_DATA));
	
	vector<unsigned char> vec(size);
	ifs.read((char*)&vec[0], size);
	ofs.write((char*)&vec[0], size);

	ifs.close();
	ofs.close();
}
int main()
{
	//simplest_pcm16le_split("NocturneNo2inEflat_44.1k_s16le.pcm");
	//simplest_pcm16le_halfvolumeleft("NocturneNo2inEflat_44.1k_s16le.pcm");
	
	//test_seekg_eof();

	//simplest_pcm16le_doublespeed("NocturneNo2inEflat_44.1k_s16le.pcm");
	//simplest_pcm16le_to_pcm8("NocturneNo2inEflat_44.1k_s16le.pcm");
	//simplest_pcm16le_cut_singlechannel("drum.pcm", 2361, 120);
	
	//test_read_to_end();
	
	simplest_pcm16le_to_wave("NocturneNo2inEflat_44.1k_s16le.pcm", 2, 44100, "output_nocturne.wav");
	//simplest_pcm16le_to_wave("output_left.pcm", 1, 44100, "output_left.wav");
}