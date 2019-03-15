//�������汾
//�������ı�̣�OOP��Object Oriented Programming��

extern "C"
{
#include<libavformat/avformat.h>
#include<libavcodec/avcodec.h>
#include<libavutil/avutil.h>
#include<libswscale/swscale.h>
#include<libavutil/imgutils.h>
}
#include<SDL.h>

#include<cstdio>
#include<iostream>
#include<fstream>
#include<string>
#include<cassert>
#include<thread>
#include<condition_variable>
#include<mutex>
#include<deque>
#include<vector>
#include<atomic>
using std::vector;
using std::deque;
using std::string;
using std::cout;
using std::endl;
using std::thread;
using std::condition_variable;
using std::mutex;

template<class T>
class threadsafe_deque
{
private:
	mutable mutex mut;
	deque<T> data_deque;
	int maxsize;
	condition_variable condvar_not_empty;
	condition_variable condvar_not_full;
public:
	threadsafe_deque(int maxsize_) :maxsize(maxsize_) {}
	threadsafe_deque(const threadsafe_deque<T>& rhs)
	{
		std::unique_lock<mutex> unilk(rhs.mut);
		data_deque = rhs.data_deque;
	}
	threadsafe_deque<T>& operator =(const threadsafe_deque<T>& rhs) = delete;
	threadsafe_deque<T>& operator =(threadsafe_deque<T>&& rhs) = delete;
	threadsafe_deque(threadsafe_deque<T>&& rhs) = delete;

	void push(T x)
	{
		std::unique_lock<mutex> unilk(mut);
		condvar_not_full.wait(unilk, [this]() {return data_deque.size() < maxsize; });
		data_deque.push_back(x);
		condvar_not_empty.notify_one();
	}
	std::shared_ptr<T> pop()
	{
		std::unique_lock<mutex> unilk(mut);
		condvar_not_empty.wait(unilk, [this]() {return !data_deque.empty(); });
		std::shared_ptr<T> sptr;
		try
		{
			sptr = std::make_shared<T>(data_deque.front());
		}
		catch (...)
		{
			condvar_not_empty.notify_one();
			throw "exception";
			return std::shared_ptr<T>(nullptr);
		}
		data_deque.pop_front();
		condvar_not_full.notify_one();
		return sptr;
	}
	std::shared_ptr<T> try_to_pop()
	{
		std::unique_lock<mutex> unilk(mut);
		if (data_deque.empty()) return nullptr;

		std::shared_ptr<T> sptr;
		sptr = std::make_shared<T>(data_deque.front());
		data_deque.pop_front();
		condvar_not_full.notify_one();
		return sptr;
	}
};

class FrameDataYuv420p
{
private:
	int w;
	int h;
	vector<unsigned char> data[3];
public:
	FrameDataYuv420p() :w(0), h(0) {}
	FrameDataYuv420p(int w_, int h_) :w(w_), h(h_)
	{
		data[0].resize(w*h);
		data[1].resize(w*h / 4);
		data[2].resize(w*h / 4);
	}
	bool empty() const { return w == 0 || h == 0; }
	void store(unsigned char* ydata, unsigned char* udata,
		unsigned char* vdata)
	{
		memcpy((char*)&data[0][0], ydata, w*h);
		memcpy((char*)&data[1][0], udata, w*h / 4);
		memcpy((char*)&data[2][0], vdata, w*h / 4);
	}
	void output(unsigned char* buffer, int buffersize) const
	{
		assert(buffersize >= w*h * 3 / 2);
		memcpy(buffer, (char*)&data[0][0], w*h);
		memcpy(buffer + w*h, (char*)&data[1][0], w*h / 4);
		memcpy(buffer + w*h * 5 / 4, (char*)&data[2][0], w*h / 4);
	}
};

class VideoDecoder
{
public:
	VideoDecoder(const string& path) :
		filepath(path), dst_pixfmt(AV_PIX_FMT_YUV420P),frameidx(0)
	{
		init();
	}
	~VideoDecoder()
	{
		av_free(outbuffer);
		sws_freeContext(swsctx);
		av_packet_free(&packet);
		av_frame_free(&frameyuv);
		av_frame_free(&frame);
		avcodec_close(codecctx);
		avformat_close_input(&formatctx);
	}
	
	//���ؽ�������һ֡��Ƶ�������ݣ�
	//���δ������Ƶ���ݣ��򷵻ؿ�FrameDataYuv420p
	FrameDataYuv420p next_frame()
	{
		while (1)
		{
			av_packet_unref(packet);

			if (av_read_frame(formatctx, packet) != 0)
			{
				return FrameDataYuv420p();
			}

			if (packet->stream_index != videoidx)
			{
				continue;
			}
			assert(packet->stream_index == videoidx);

			if (avcodec_send_packet(codecctx, packet) != 0)
			{
				printf("Fail in sending packet.\n");
				exit(-1);
			}

			int errorcode= avcodec_receive_frame(codecctx, frame);
			if (errorcode== 0)
			{
				//avcodec_receive_frame���ܻ᷵��AVERROR(EAGAIN)����ʱ
				//output is not available in this state - user must
				//try to send new input
				break;
			}
			else if (errorcode == AVERROR(EAGAIN))
			{
				continue;
			}
			else
			{
				printf("Errorcode returned by avcodec_"
					"receive_frame is %d.\n", errorcode);
				printf("Fail in receiving.\n");
				exit(-1);
			}
		}

		sws_scale(swsctx, (const uint8_t* const*)frame->data,
			frame->linesize, 0, codecctx->height,
			frameyuv->data, frameyuv->linesize);

		printf("Decoded frame index:%d\n", frameidx);

		FrameDataYuv420p fd(frame_width, frame_height);
		fd.store(frameyuv->data[0], frameyuv->data[1],
			frameyuv->data[2]);

		frameidx++;

		return fd;
	}
	
	//���������ļ�������������
	static void decode(const string& src_url, const string& dst_url)
	{
		std::ofstream ofs(dst_url,std::ios::binary);
		VideoDecoder decoder(src_url);
		FrameDataYuv420p temp;
		vector<unsigned char> data(decoder.get_width()*
			decoder.get_height() * 3 / 2);
		while (!(temp=decoder.next_frame()).empty())
		{
			temp.output(&data[0],(int)data.size());
			ofs.write((char*)&data[0],data.size());
		}
	}

	int get_width()const { return frame_width; }
	int get_height()const { return frame_height; }
	int get_fps()const { return fps; }
private:
	void init()
	{
		formatctx = avformat_alloc_context();
		if (avformat_open_input(&formatctx, filepath.c_str(),
			nullptr, nullptr) != 0)
		{
			cout << "Couldn't open input stream" << endl;
			exit(-1);
		}

		if (avformat_find_stream_info(formatctx, nullptr) < 0)
		{
			cout << "Couldn't find stream information" << endl;
			exit(-1);
		}

		videoidx = -1;
		for (int i = 0; i < (int)formatctx->nb_streams; i++)
		{
			if (formatctx->streams[i]->codecpar->codec_type
				== AVMEDIA_TYPE_VIDEO)
			{
				videoidx = i;
				break;
			}
		}
		if (videoidx == -1)
		{
			cout << "Couldn't find a video stream" << endl;
			exit(-1);
		}

		AVCodecParameters* codecpar = formatctx->streams[videoidx]
			->codecpar;

		codecctx = avcodec_alloc_context3(nullptr);
		if (avcodec_parameters_to_context(codecctx, codecpar) < 0)
		{
			printf("Fail in cvting parameters to context.\n");
			exit(-1);
		}

		frame_width = codecctx->width;
		frame_height = codecctx->height;
		AVRational rt = formatctx->streams[videoidx]->avg_frame_rate;
		fps = rt.num / rt.den;

		//����һ��ƥ��codec_id�Ľ�����
		AVCodec* codec = avcodec_find_decoder(codecpar->codec_id);
		if (codec == NULL)
		{
			cout << "Couldn't find decoder" << endl;
			exit(-1);
		}

		//�򿪽�����AVCodec
		if (avcodec_open2(codecctx, codec, nullptr) < 0)
		{
			cout << "Couldn't open codec" << endl;
			exit(-1);
		}

		assert(dst_pixfmt == AV_PIX_FMT_YUV420P);
		swsctx = sws_getContext(codecctx->width,
			codecctx->height,
			codecctx->pix_fmt, codecctx->width, codecctx->height,
			dst_pixfmt, SWS_BICUBIC, nullptr, nullptr, nullptr);

		frameyuv = av_frame_alloc();
		int buffesize = av_image_get_buffer_size(dst_pixfmt,
			codecctx->width, codecctx->height, 1);
		outbuffer = (uint8_t*)av_malloc(buffesize);

		av_image_fill_arrays(frameyuv->data, frameyuv->linesize, outbuffer,
			dst_pixfmt, codecctx->width, codecctx->height, 1);

<<<<<<< HEAD
		packet = av_packet_alloc();	
=======
		packet = av_packet_alloc();
>>>>>>> av_packet_alloc() differ from (AVPacket*)av_malloc(sizeof(AVPacket))
		frame = av_frame_alloc();

		cout << "VideoDecoder init Done" << endl;
	}
private:
	string filepath;
	const AVPixelFormat dst_pixfmt;
	int frame_width;
	int frame_height;
	int fps;

	int frameidx;

	AVFormatContext* formatctx;
	int videoidx;
	AVCodecContext* codecctx;
	SwsContext* swsctx;
	AVFrame* frameyuv;
	uint8_t* outbuffer;
	AVPacket* packet;
	AVFrame* frame;
};

class SdlVideoPlayer
{
private:
	enum VideoPlayState
	{
		VSTATE_NORMAL,
		VSTATE_PAUSE,
		VSTATE_FASTFORWARD
	};
public:
	SdlVideoPlayer(int frame_width_, int frame_height_, int fps_,
		const CVideoPlayerMFCDlg* dlg_) :dlg(dlg_),
		frame_width(frame_width_), frame_height(frame_height_),
		fps(fps_), close_wind(false),videostate(VSTATE_NORMAL),
		space_down_state(false),normal_delay(1000/fps_),
		fast_forword_delay(1000/fps_/10),
		REFRESH_USEREVENT(SDL_USEREVENT + 1)
	{
		delaytime = normal_delay;
		init();
	}
	~SdlVideoPlayer()
	{
		SDL_DestroyWindow(wind);
		SDL_Quit();

		//���SDL_Quit���ٴ�����SDL��Ƶ��Ⱦ�޻�������
		dlg->GetDlgItem(IDC_SCREEN)->ShowWindow(SW_SHOWNORMAL);

		//ȷ��refresh_video()�߳��Ƚ���
		refresh_thd.join();
	}
	void imshow_frame(const FrameDataYuv420p& fd)
	{
		SDL_Event event;
		SDL_WaitEvent(&event); //δ�ȵ�������������

		while (event.type != REFRESH_USEREVENT
			|| videostate == VSTATE_PAUSE)
		{
			if (event.type == SDL_WINDOWEVENT)
			{
				SDL_GetWindowSize(wind, &wind_w, &wind_h);
			}
			else if (event.type == SDL_QUIT)  //��������¼�
			{
				close_wind = true;
				return;
			}
			else if (event.type == SDL_KEYDOWN)
			{
				if (event.key.keysym.sym == ' ')
				{
					cout << SDL_GetKeyName(event.key.keysym.sym)
						<< " down" << endl;
					if (videostate == VSTATE_NORMAL)
					{
						if (space_down_state == true)
							videostate = VSTATE_FASTFORWARD;
						else
							space_down_state = true;
					}
					else if (videostate == VSTATE_PAUSE)
					{
						if (space_down_state == true)
							videostate = VSTATE_FASTFORWARD;
						else
							space_down_state = true;
					}
					else if (videostate == VSTATE_FASTFORWARD)
					{

					}
				}
			}
			else if (event.type == SDL_KEYUP)
			{
				if (event.key.keysym.sym == ' ')
				{
					cout << SDL_GetKeyName(event.key.keysym.sym)
						<< " up" << endl;

					space_down_state = false;
					if (videostate == VSTATE_NORMAL)
					{
						videostate = VSTATE_PAUSE;
					}
					else if (videostate == VSTATE_PAUSE)
					{
						videostate = VSTATE_NORMAL;
					}
					else if (videostate == VSTATE_FASTFORWARD)
					{
						videostate = VSTATE_NORMAL;
					}
				}
			}

			SDL_WaitEvent(&event);
		}

		//��ʱ��ΪREFRESH_USEREVENT�¼����ҷ�VSTATE_PAUSE״̬
		if (videostate == VSTATE_NORMAL)
		{
			delaytime = normal_delay;
		}
		else if (videostate == VSTATE_FASTFORWARD)
		{
			delaytime = fast_forword_delay;
		}

		vector<unsigned char> buffer(frame_width*frame_height * 3 / 2);
		fd.output(&buffer[0], frame_width*frame_height * 3 / 2);

		SDL_Rect rect;
		rect.x = 0;
		rect.y = 0;
		rect.w = wind_w;
		rect.h = wind_h;
		//SDL_UpdateTexture�ڶ���������Ϊ0�����������texture
		SDL_UpdateTexture(texture, 0, &buffer[0], frame_width);
		SDL_RenderClear(render);
		SDL_RenderCopy(render, texture, 0, &rect);
		SDL_RenderPresent(render);
	}

	bool get_close_wind() const { return (bool)close_wind; }
	void set_close_wind(bool bl) { close_wind = bl; }
	
	static void play_yuv420p(const string& src_path,int width,
		int height,int fps=25)
	{
		std::ifstream ifs(src_path, std::ios::binary);
		SdlVideoPlayer player(width,height,fps,nullptr);
		vector<unsigned char> buffer(width*height * 3 / 2);
		while (1)
		{
			if (player.get_close_wind())
			{
				break;
			}
			//ѭ������
			if (!ifs.read((char*)&buffer[0], buffer.size()))
			{
				ifs.clear();
				ifs.seekg(0, std::ios::beg);
				ifs.read((char*)&buffer[0], buffer.size());
				assert(ifs.gcount() == buffer.size());
			}
			FrameDataYuv420p fdata(width,height);
			fdata.store(&buffer[0], &buffer[width*height],
				&buffer[width*height * 5 / 4]);
			player.imshow_frame(fdata);
		}
	}
private:
	void init()
	{
		cout << "---------------------------------------" << endl;
		cout << "SDL version in use: " << SDL_MAJOR_VERSION << "."
			<< SDL_MINOR_VERSION << "." << SDL_PATCHLEVEL << endl;
		cout << "---------------------------------------" << endl;

		if (SDL_Init(SDL_INIT_VIDEO) != 0) {
			cout << "Couldn' initialize SDL: " << SDL_GetError << endl;
			exit(-1);
		}

		//���Ŵ���
		//wind_w = frame_width;
		//wind_h = frame_height;
		//wind = SDL_CreateWindow("Simplest video play sdl2",
		//	SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		//	wind_w, wind_h, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);

		CWnd *pWnd = dlg->GetDlgItem(IDC_SCREEN);//IDC_SCREENΪ�ؼ�ID
		wind = SDL_CreateWindowFrom(pWnd->GetSafeHwnd());
		if (!wind) {
			cout << "SDL: couldn't create window "
				<< SDL_GetError() << endl;
			exit(-1);
		}

		CRect rect;
		pWnd->GetClientRect(&rect);
		wind_w = rect.Width();
		wind_h = rect.Height();
		assert(rect.TopLeft().x == 0);
		assert(rect.TopLeft().y == 0);

		render = SDL_CreateRenderer(wind, -1, 0);

		uint32_t pixfmt = SDL_PIXELFORMAT_IYUV; //YUV420P
		texture = SDL_CreateTexture(render, pixfmt,
			SDL_TEXTUREACCESS_STREAMING, frame_width, frame_height);

		refresh_thd=thread(&SdlVideoPlayer::refresh_video, this);

		cout << "SdlVideoPlayer init Done" << endl;
	}
	void refresh_video()//�̵߳���ں�����������int(void*)����
	{
		while (close_wind == false)
		{
			SDL_Event event;
			event.type = REFRESH_USEREVENT;
			SDL_PushEvent(&event); //���¼������������¼�
			SDL_Delay(delaytime);
		}
	}
private:
	const int frame_width;
	const int frame_height;
	const int fps;
	const CVideoPlayerMFCDlg* dlg;
	
	const uint32_t REFRESH_USEREVENT;
	const int normal_delay;
	const int fast_forword_delay;
	int delaytime;

	int wind_w;
	int wind_h;
	SDL_Window* wind;
	SDL_Renderer* render;
	SDL_Texture* texture;
	std::atomic<bool> close_wind;//�����Ƿ����
	VideoPlayState videostate;
	bool space_down_state;//�ո���Ƿ��ڰ���״̬
	thread refresh_thd;
};

class DecoderPlayer
{
public:
	DecoderPlayer(const string& path, 
		const CVideoPlayerMFCDlg* dlg_,
		int deque_maxsize=30) :
		decoder(path),
		dlg(dlg_),
		mydeque(deque_maxsize)
	{
		frame_width = decoder.get_width();
		frame_height = decoder.get_height();
		fps = decoder.get_fps();
	}
	void decode_play()
	{
		SdlVideoPlayer player(frame_width, frame_height, fps,
			dlg);

		//�����߳�
		auto lam = [this, &player]()
		{
			FrameDataYuv420p data;
			while (1)
			{
				//����������
				if (player.get_close_wind()) break;

				data = decoder.next_frame();
				mydeque.push(data);

				//�Ѿ��������������ݣ����Ϳ�FrameDataYuv420p��Ϊ����֡��־
				if (data.empty()) break;
			}
		};
		thread t(lam);

		//���ų����ܷ������߳���
		FrameDataYuv420p dataget;
		while (1)
		{
			//����������
			if (player.get_close_wind())
			{
				mydeque.try_to_pop();
				break;
				//���˳�ѭ��֮ǰ����try_to_pop()���ý����̼߳���һ��ѭ��
				//�������bug����ͣʱ���ڣ�һֱ���룬��Ž������ݵĶ�������
				//��ʱ������ڣ����½����߳��޷�����ѭ��
				//�����if (player.get_close_wind())
			}

			std::shared_ptr<FrameDataYuv420p> dataget;
			dataget= mydeque.pop();

<<<<<<< HEAD
			//���δ�������������ݣ�˵����Ƶ�Բ������
=======
			//���δ�������������ݣ�˵����Ƶ�Ѳ������
>>>>>>> av_packet_alloc() differ from (AVPacket*)av_malloc(sizeof(AVPacket))
			if (dataget->empty())
			{
				player.set_close_wind(true);
				break;
			}

			player.imshow_frame(*dataget);
		}	  

		t.join();
	}
private:
	VideoDecoder decoder;
	threadsafe_deque<FrameDataYuv420p>  mydeque;
	int frame_width;
	int frame_height;
	int fps;
	const CVideoPlayerMFCDlg* dlg;
};

//int main(int argc,char* argv[])
//{
//	DecoderPlayer dp("���鼧_178s_640x360.mp4");
//	dp.decode_play();
//
//	return 0;
//}