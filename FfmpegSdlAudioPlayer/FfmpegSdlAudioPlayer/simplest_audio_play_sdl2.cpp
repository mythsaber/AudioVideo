#define SDL_MAIN_HANDLED
#include<cstdio>
#include<cstdint> //uint8t
#if defined(_WIN32)
#include<SDL.h>
#else
#include<SDL2/SDL.h>
#endif
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
static unsigned long long data_cnt = 0;
static std::atomic<bool> eof(false);


static void fill_audio(void* udata, uint8_t* stream, int len)
{
    SDL_memset(stream, 0, len);  //len为1764
    cout << "len to read: " << len << endl;
    unsigned char* buf;
    int actual_size = pcm_buffer.read(buf, len);
    if (actual_size == 0)
    {
        eof = true;
        return;
    }
    SDL_MixAudioFormat(stream, buf, AUDIO_S16SYS, actual_size, SDL_MIX_MAXVOLUME);
    data_cnt += actual_size;
    cout << "len  actual read: " << actual_size << endl;
    printf("Now playing %10llu Bytes data\n", data_cnt);
}

int play_pcm()
{
    printf("---------------SDL:%d.%d.%d----------------\n",
        SDL_MAJOR_VERSION, SDL_MINOR_VERSION, SDL_PATCHLEVEL);

    const char* src = "NocturneNo2inEflat_44.1k_s16le.pcm";
    //const char* src = "夏日水韵_44.1k_s16le.pcm";

    if (SDL_InitSubSystem(SDL_INIT_AUDIO))
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
    //静音时，缓冲区的值
    wanted.silence = 0;
    //SDL_AudioSpec::samples音频缓冲区的大小，以采样点数为单位
    //SDL_AudioSpec::samples决定了音频缓冲区能缓存多少毫秒的视频，
    //毫秒数=(samples*1000)/freq
    //SDL_AudioSpec::size是以字节为单位的音频缓冲区大小，
    //值由SDL_OpenAudio()计算
    //在对AUDIO_S16SYS格式、双声道下，SDL_AudioSpec::samples*2声道*2字节
    //等于SDL_AudioSpec::size
    wanted.samples = 441;
    wanted.size = 441 * 2 * 2;
    //音频设备需要更多数据时，调用该回调函数wanted.callback
    wanted.callback = fill_audio;

    //根据参数打开音频设备，打开后回调处理未进行，播放静音的值
    SDL_AudioSpec obtained;
    //SDL_OpenAudio的第二个参数用于存储实际音频设备的参数
    //如果是SDL_OpenAudio(&wanted, &obtained)，则不能
    //正常播放，且调用后发现 obtained.samples和wanted.samples不一致
    //第二个参数是nullptr，则传递给回调函数的数据自动转为硬件要求的格式
    //如果第二个参数为&obtained，则SDL_OpenAudio不会改变第一个参数的成员
    //如果第二个参数为nullptr，则SDL_OpenAudio会计算size的值，并填入第一个参数中
    int capture = 0;
    int device_index = 0;
#if defined(_WIN32)
    const char* device_name = SDL_GetAudioDeviceName(device_index, capture);
    const int audio_dev_id = SDL_OpenAudioDevice(device_name, capture, &wanted, &obtained, 0);
    if (audio_dev_id == 0)
    {
        printf("Couldn't open audio dev: %s\n", device_name);
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return -1;
    }
    printf("success open audio device: %s\n", device_name);
#else
    const int audio_dev_id = SDL_OpenAudioDevice(nullptr, capture, &wanted, &obtained, 0);
    if (audio_dev_id == 0)
    {
        printf("Couldn't open audio dev: %s\n", "default");
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return -1;
    }
    printf("success open audio device: %s\n", "default");
#endif
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
    //printf("obtained.format:%x\n", obtained.format); //0x8120，即AUDIO_F32LSB
    //printf("obtained.channels:%u\n", obtained.channels); //2
    //printf("obtained.silence:%u\n", obtained.silence); //0
    //printf("obtained.samples:%u\n", obtained.samples);//981
    //printf("obtained.size:%u\n", obtained.size); //7848=981*2*4
    //printf("obtained.callback:%p\n", obtained.callback);//00007FF73C931569

    int ret = pcm_buffer.open(src);
    if (ret < 0)
    {
        cout << "Failed to open " << src << endl;
        SDL_CloseAudioDevice(audio_dev_id);
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return -1;
    }

    SDL_PauseAudioDevice(audio_dev_id, 0);

    while (1)
    {
        if (eof == true)
            break;
        else
            SDL_Delay(1);
    }
    SDL_CloseAudioDevice(audio_dev_id);
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    return 0;
}

void list_all_audio_out_devices()
{
    if (SDL_InitSubSystem(SDL_INIT_AUDIO))
    {
        printf("Couldn't init SDL - %s\n", SDL_GetError());
        return;
    }

    int capture = 0;
    int count = SDL_GetNumAudioDevices(capture);
    for (int i = 0; i < count; i++)
    {
        printf("%dth audio dev: %s\n", i, SDL_GetAudioDeviceName(i, capture));
    }

    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    return;
}

int main(int argc, char* argv[])
{
#if defined(_WIN32)
    system("chcp 65001");
#endif
    list_all_audio_out_devices();
    play_pcm();
    return 0;
}
