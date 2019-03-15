#include<cstdio>
#include<tchar.h>
#include<cstdint> //uint8t
#include<SDL.h>
#include<fstream>
#include<string>
#include<vector>
#include<cassert>
using std::vector;
using std::string;

class AudioPlayer
{
public:
	static int audio_len;
	static uint8_t* audio_pos;
public:
	AudioPlayer() {}

	/*��Ƶ�ص�����
	* userdata��SDL_AudioSpec�ṹ�е��û��Զ������ݣ�һ������¿��Բ��á�
	* stream����ָ��ָ����Ҫ������Ƶ��������
	* len����Ƶ�������Ĵ�С�����ֽ�Ϊ��λ��

	* This function usually runs in a separate thread, and so you 
	* should protect data structures that it accesses by calling
	* SDL_LockAudio() and SDL_UnlockAudio() in your code
	*/
	friend void fill_audio(void* udata, uint8_t* stream, int len)
	{
		SDL_memset(stream, 0, len);
		if (audio_len == 0) return;
		len = (len > audio_len ? audio_len : len);

		SDL_MixAudio(stream, audio_pos, len, SDL_MIX_MAXVOLUME);
		audio_pos += len;
		audio_len -= len;
	}

	int play()
	{
		printf("---------------SDL:%d.%d.%d----------------\n",
			SDL_MAJOR_VERSION, SDL_MINOR_VERSION, SDL_PATCHLEVEL);
		
		//��ʼ��SDL
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
		wanted.silence = 0; //����ʱ����������ֵ
		//SDL_AudioSpec::samples��Ƶ�������Ĵ�С���Բ�������Ϊ��λ
		//SDL_AudioSpec::samples��������Ƶ�������ܻ�����ٺ������Ƶ��
		//������=(samples*1000)/freq
		//SDL_AudioSpec::size�����ֽ�Ϊ��λ����Ƶ��������С��
		//ֵ��SDL_OpenAudio()����
		//�ڶ�AUDIO_S16SYS��ʽ��˫�����£�SDL_AudioSpec::samples*2����*2�ֽ�
		//����SDL_AudioSpec::size
		wanted.samples = 1024;
		//��Ƶ�豸��Ҫ��������ʱ�����øûص�����wanted.callback
		wanted.callback = fill_audio;

		//���ݲ�������Ƶ�豸���򿪺�ص�����δ���У����ž�����ֵ
		SDL_AudioSpec obtained;
		//�ڶ�������������ڴ洢ʵ����Ƶ�豸�Ĳ���
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
		printf("wanted.format:%x\n", wanted.format); //0x8010,��֮ǰ���õ�ֵ
		printf("wanted.channels:%u\n", wanted.channels);//2
		printf("wanted.silence:%u\n", wanted.silence);//0
		printf("wanted.samples:%u\n", wanted.samples);//1024
		printf("wanted.size:%u\n", wanted.size);//��Чֵ��4096���ڶ�������Ϊnullptr��
		printf("wanted.callback:%p\n", wanted.callback);//00007FF73C931569
		printf("------------------------------------------------\n");
		//printf("obtained.freq:%d\n", obtained.freq); //44100
		//printf("obtained.format:%x\n", obtained.format); //0x8120,����AUDIO_F32LSB
		//printf("obtained.channels:%u\n", obtained.channels); //2
		//printf("obtained.silence:%u\n", obtained.silence); //0
		//printf("obtained.samples:%u\n", obtained.samples);//981
		//printf("obtained.size:%u\n", obtained.size); //7848=981*2*4
		//printf("obtained.callback:%p\n", obtained.callback);//00007FF73C931569

		const string src_path = "out_temp.pcm";
		std::ifstream ifs(src_path, std::ios::binary);
		if (!ifs) {
			printf("Couldn't open %s\n", src_path.c_str());
			return -1;
		}

		vector<unsigned char> pcm_buffer(4096);
		int data_cnt = 0;

		//��ͣ/�����ص�����
		//����Ƶ�豸SDL_OpenAudio���Բ���0���ã���ʼ�ص���������Ƶ
		//����Ϊ1������ͣ�ص������ž�����ֵ
		SDL_PauseAudio(0);
		
		while (1)
		{
			if (!ifs.read((char*)&pcm_buffer[0], pcm_buffer.size()))
			{
				ifs.clear();
				ifs.seekg(0, std::ios::beg);
				ifs.read((char*)&pcm_buffer[0], pcm_buffer.size());

				data_cnt = 0;
				assert(ifs.gcount() == pcm_buffer.size());
			}
			printf("Now playing %10d Bytes data\n", data_cnt);
			data_cnt += (int)pcm_buffer.size();

			audio_len = (int)pcm_buffer.size();
			audio_pos = (uint8_t*)&pcm_buffer[0];

			while (audio_len > 0)
			{
				SDL_Delay(1);
			}
		}
		SDL_Quit();
		return 0;
	}
};
int AudioPlayer::audio_len = 0;
uint8_t* AudioPlayer::audio_pos = nullptr;

int main(int argc, char* argv[])
{
	AudioPlayer ap;
	ap.play();

	return 0;
}
