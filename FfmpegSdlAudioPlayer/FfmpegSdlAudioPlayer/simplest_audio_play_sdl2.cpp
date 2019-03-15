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

	/*音频回调函数
	* userdata：SDL_AudioSpec结构中的用户自定义数据，一般情况下可以不用。
	* stream：该指针指向需要填充的音频缓冲区。
	* len：音频缓冲区的大小（以字节为单位）

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
		
		//初始化SDL
		if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_TIMER))
		{
			printf("Couldn't init SDL - %s\n", SDL_GetError());
			return -1;
		}

		SDL_AudioSpec wanted;
		wanted.freq = 44100;
		//S16SYS中的第一个S表示“signed"(有符号），16位采样，
		//SYS表示大小端顺序依赖系统
		//AUDIO_U16SYS：Unsigned 16-bit samples
		//AUDIO_S16SYS：Signed 16 - bit samples
		//AUDIO_S32SYS：32 - bit integer samples
		//AUDIO_F32SYS：32 - bit floating point samples
		wanted.format = AUDIO_S16SYS;
		wanted.channels = 2;
		wanted.silence = 0; //静音时，缓冲区的值
		//SDL_AudioSpec::samples音频缓冲区的大小，以采样点数为单位
		//SDL_AudioSpec::samples决定了音频缓冲区能缓存多少毫秒的视频，
		//毫秒数=(samples*1000)/freq
		//SDL_AudioSpec::size是以字节为单位的音频缓冲区大小，
		//值由SDL_OpenAudio()计算
		//在对AUDIO_S16SYS格式、双声道下，SDL_AudioSpec::samples*2声道*2字节
		//等于SDL_AudioSpec::size
		wanted.samples = 1024;
		//音频设备需要更多数据时，调用该回调函数wanted.callback
		wanted.callback = fill_audio;

		//根据参数打开音频设备，打开后回调处理未进行，播放静音的值
		SDL_AudioSpec obtained;
		//第二个参数如果用于存储实际音频设备的参数
		//如果是SDL_OpenAudio(&wanted, &obtained)，则不能
		//正常播放，且调用后发现 obtained.samples和wanted.samples不一致
		//第二个参数是nullptr，则传递给回调函数的数据自动转为硬件要求的格式
		//如果第二个参数为&obtained，则SDL_OpenAudio不会改变第一个参数的成员
		//如果第二个参数为nullptr，则SDL_OpenAudio会计算size的值，并填入第一个参数中
		if (SDL_OpenAudio(&wanted, nullptr) < 0)
		{
			printf("Couldn't open audio\n");
			return -1;
		}

		//printf("wanted.callback:%p\n", wanted.callback);
		printf("wanted.freq:%d\n", wanted.freq);//44100
		printf("wanted.format:%x\n", wanted.format); //0x8010,即之前设置的值
		printf("wanted.channels:%u\n", wanted.channels);//2
		printf("wanted.silence:%u\n", wanted.silence);//0
		printf("wanted.samples:%u\n", wanted.samples);//1024
		printf("wanted.size:%u\n", wanted.size);//无效值或4096（第二个参数为nullptr）
		printf("wanted.callback:%p\n", wanted.callback);//00007FF73C931569
		printf("------------------------------------------------\n");
		//printf("obtained.freq:%d\n", obtained.freq); //44100
		//printf("obtained.format:%x\n", obtained.format); //0x8120,，即AUDIO_F32LSB
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

		//暂停/继续回调处理
		//打开音频设备SDL_OpenAudio后，以参数0调用，则开始回调，播放音频
		//设置为1，则暂停回调，播放静音的值
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
