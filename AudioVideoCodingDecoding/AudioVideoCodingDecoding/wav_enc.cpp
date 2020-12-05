#include<fstream>
#include<vector>
#include<cassert>
// Common data formats.
#define DR_WAVE_FORMAT_PCM 0x1
#define DR_WAVE_FORMAT_ADPCM 0x2
#define DR_WAVE_FORMAT_IEEE_FLOAT 0x3
#define DR_WAVE_FORMAT_ALAW 0x6
#define DR_WAVE_FORMAT_MULAW 0x7
#define DR_WAVE_FORMAT_DVI_ADPCM 0x11
#define DR_WAVE_FORMAT_EXTENSIBLE 0xFFFE

enum class WavContainer
{
	container_riff,
	container_w64
};

struct WavFormat
{
	WavContainer container; // RIFF, W64.
	uint16_t format;         // DR_WAVE_FORMAT_*
	uint32_t sampleRate;
	uint16_t channels;
	uint16_t bitsPerSample; //8 16 32
};

void write_riff(std::ofstream&ofs, const WavFormat& format, int32_t totalSampleCount)
{
	const uint32_t file_size = 12 + 24 + (8 + totalSampleCount * format.bitsPerSample / 8);
	const uint32_t size = file_size - 8;

	ofs.write("RIFF", 4);
	ofs.write((const char*)&size, 4);
	ofs.write("WAVE", 4);
}

void write_format(std::ofstream&ofs, const WavFormat& format)
{
	const uint32_t byterate = format.sampleRate*format.channels*format.bitsPerSample / 8;
	const uint16_t block_align = format.channels*format.bitsPerSample / 8;

	const uint32_t size = 16;
	ofs.write("fmt ", 4);
	ofs.write((const char*)&size, 4);
	ofs.write((const char*)&format.format, 2);
	ofs.write((const char*)&format.channels, 2);
	ofs.write((const char*)&format.sampleRate, 4);
	ofs.write((const char*)&byterate, 4);
	ofs.write((const char*)&block_align, 2);
	ofs.write((const char*)&format.bitsPerSample, 2);
}

void write_data(std::ofstream&ofs, const uint8_t* buffer, int32_t bufsize)
{
	ofs.write("data", 4);
	ofs.write((const char*)&bufsize, 4);
	ofs.write((const char*)buffer,bufsize);
}

//写wav文件,保存pcm数据
bool muxer_pcm_to_wav(char *filename, uint8_t* buffer, int32_t totalSampleCount, 
	uint32_t sampleRate, uint16_t channels, uint16_t bitsPerSample)
{
	WavFormat format = {};
	format.container = WavContainer::container_riff; // <-- drwav_container_riff = normal WAV files, drwav_container_w64 = Sony Wave64.
	format.format = DR_WAVE_FORMAT_PCM;      // <-- Any of the DR_WAVE_FORMAT_* codes.
	format.channels = channels;
	format.sampleRate = sampleRate;
	format.bitsPerSample = bitsPerSample;

	std::ofstream ofs(filename, std::ios::binary);
	if (!ofs)
	{
		printf("Failed to open %s\n",filename);
		return false;
	}

	write_riff(ofs, format, totalSampleCount);
	write_format(ofs, format);
	write_data(ofs, buffer, totalSampleCount * format.bitsPerSample / 8);
	ofs.close();
	printf("generate %s success\n", filename);
	return true;
}

int main()
{
	const std::string src = "dark_s32le_44100_2ch.pcm";
	std::ifstream ifs(src, std::ios::binary);
	if (!ifs)
	{
		printf("Failed to open %s\n",src.c_str());
		return -1;
	}
	ifs.seekg(0, std::ios::end);
	const std::streamoff file_size = ifs.tellg();
	ifs.seekg(0,std::ios::beg);
	
	if (file_size > 500 * 1000000)
	{
		printf("file %s is too large to deal with", src.c_str());
		return -1;
	}
	std::vector<uint8_t> buf(file_size);
	ifs.read((char*)&buf[0],buf.size());
	assert(ifs.gcount() == buf.size());

	muxer_pcm_to_wav("out_dark_s32le_44100_2ch.wav", &buf[0],
		static_cast<int32_t>(buf.size() / 4), 44100, 2, 32);
	return 0;
}