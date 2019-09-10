#include<cstdio>
#include<tchar.h>
#include<cstdint> //uint8t
#include<SDL.h>
#include<fstream>
#include<string>
#include<vector>
#include<cassert>
#include<iostream>
#include<atomic>
using std::vector;
using std::string;
using std::cout;
using std::endl;

class PCMBuffer
{
public:
	PCMBuffer(unsigned buf_size)
	{
		buffer.resize(buf_size);
		m_buf_ptr = 0;
		m_buf_end = 0;
	}
	int open(const char* file) 
	{
		ifs.open(file, std::ios::binary);
		if (!ifs) return -1;
		else return 0;
	}
	int read(unsigned char*& buf, unsigned size) 
	{
		if (empty())
		{
			fill();
			if (empty())
			{
				buf = nullptr;
				return 0;
			}
		}
		unsigned available = available_size();
		int ret = size < available ? size : available;
		buf = &buffer[m_buf_ptr];
		m_buf_ptr += ret;
		return ret;
	}
	void close()
	{
		ifs.close();
	}
private:
	void fill() 
	{
		ifs.read((char*)&buffer[0], buffer.size());
		m_buf_end = (int)ifs.gcount();
		m_buf_ptr = 0;
	}
	unsigned available_size()
	{
		return m_buf_end - m_buf_ptr;
	}
	bool empty()
	{
		return m_buf_end - m_buf_ptr == 0;
	}
private:
	vector<unsigned char> buffer;
	int m_buf_ptr;  
	int m_buf_end; 
	std::ifstream ifs;
};

static PCMBuffer pcm_buffer(1764);
static uint64_t data_cnt = 0;
static std::atomic<bool> eof(false);


static void fill_audio(void* udata, uint8_t* stream, int len)  
{
	SDL_memset(stream, 0, len);  //lenΪ1764
	cout << "len to read: " << len << endl;
	unsigned char* buf;
	int actual_size = pcm_buffer.read(buf, len);
	if (actual_size == 0)
	{
		eof = true;
		return;
	}
	SDL_MixAudio(stream, buf, actual_size, SDL_MIX_MAXVOLUME);
	data_cnt += actual_size;
	cout << "len  actual read: " << actual_size << endl;
	printf("Now playing %10lld Bytes data\n", data_cnt);
}

int play_pcm()
{
	printf("---------------SDL:%d.%d.%d----------------\n",
		SDL_MAJOR_VERSION, SDL_MINOR_VERSION, SDL_PATCHLEVEL);
	
	const char* src = "NocturneNo2inEflat_44.1k_s16le.pcm";
	//const char* src = "output.pcm";

	if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_TIMER))
	{
		printf("Couldn't init SDL - %s\n", SDL_GetError());
		return -1;
	}

	SDL_AudioSpec wanted;
	wanted.freq = 44100;
	//S16SYS�еĵ�һ��S��ʾ��signed"(�з��ţ���16λ������
	//SYS��ʾ��С��˳������ϵͳ
	//AUDIO_U16SYS��Unsigned 16-bit samples
	//AUDIO_S16SYS��Signed 16 - bit samples
	//AUDIO_S32SYS��32 - bit integer samples
	//AUDIO_F32SYS��32 - bit floating point samples
	wanted.format = AUDIO_S16SYS;
	wanted.channels = 2;
	//����ʱ����������ֵ
	wanted.silence = 0;
	//SDL_AudioSpec::samples��Ƶ�������Ĵ�С���Բ�������Ϊ��λ
	//SDL_AudioSpec::samples��������Ƶ�������ܻ�����ٺ������Ƶ��
	//������=(samples*1000)/freq
	//SDL_AudioSpec::size�����ֽ�Ϊ��λ����Ƶ��������С��
	//ֵ��SDL_OpenAudio()����
	//�ڶ�AUDIO_S16SYS��ʽ��˫�����£�SDL_AudioSpec::samples*2����*2�ֽ�
	//����SDL_AudioSpec::size
	wanted.samples = 441;
	//��Ƶ�豸��Ҫ��������ʱ�����øûص�����wanted.callback
	wanted.callback = fill_audio;

	//���ݲ�������Ƶ�豸���򿪺�ص�����δ���У����ž�����ֵ
	SDL_AudioSpec obtained;
	//SDL_OpenAudio�ĵڶ����������ڴ洢ʵ����Ƶ�豸�Ĳ���
	//�����SDL_OpenAudio(&wanted, &obtained)������
	//�������ţ��ҵ��ú��� obtained.samples��wanted.samples��һ��
	//�ڶ���������nullptr���򴫵ݸ��ص������������Զ�תΪӲ��Ҫ��ĸ�ʽ
	//����ڶ�������Ϊ&obtained����SDL_OpenAudio����ı��һ�������ĳ�Ա
	//����ڶ�������Ϊnullptr����SDL_OpenAudio�����size��ֵ���������һ��������
	if (SDL_OpenAudio(&wanted, nullptr) < 0)
	{
		printf("Couldn't open audio\n");
		return -1;
	}

	//printf("wanted.callback:%p\n", wanted.callback);
	printf("wanted.freq:%d\n", wanted.freq);//44100
	printf("wanted.format:%x\n", wanted.format);  //8010
	printf("wanted.channels:%u\n", wanted.channels);//2
	printf("wanted.silence:%u\n", wanted.silence);//0
	printf("wanted.samples:%u\n", wanted.samples);//441
	printf("wanted.size:%u\n", wanted.size);  //1764
	printf("wanted.callback:%p\n", wanted.callback);//00007FF73C931569
	printf("------------------------------------------------\n");
	//printf("obtained.freq:%d\n", obtained.freq); //44100
	//printf("obtained.format:%x\n", obtained.format); //0x8120����AUDIO_F32LSB
	//printf("obtained.channels:%u\n", obtained.channels); //2
	//printf("obtained.silence:%u\n", obtained.silence); //0
	//printf("obtained.samples:%u\n", obtained.samples);//981
	//printf("obtained.size:%u\n", obtained.size); //7848=981*2*4
	//printf("obtained.callback:%p\n", obtained.callback);//00007FF73C931569

	int ret = pcm_buffer.open(src);
	if (ret < 0)
	{
		cout << "Failed to open " << src << endl;
		SDL_Quit();
		return -1;
	}

	SDL_PauseAudio(0);

	while (1)
	{
		if (eof == true)
			break;
		else
			SDL_Delay(1);
	}
	SDL_Quit();
	return 0;
}

int main(int argc, char* argv[])
{
	play_pcm();
	return 0;
}