#include<iostream>
#include<fstream>
#include<string>
#include<vector>
#include<algorithm>
#include<numeric>
#include<cmath>
using std::ifstream;
using std::ofstream;
using std::string;
using std::vector;
using std::cout;
using std::endl;

void simplest_yuv420p_split(const char* url, int w, int h, int num)
{
	ifstream fp(url,std::ifstream::binary);
	if (!fp)
	{
		cout << "not read" << endl;
		return;
	}
	ofstream fp1("output_420_y.y",std::ofstream::binary);
	ofstream fp2("output_420_u.y",std::ofstream::binary);
	ofstream fp3("output_420_v.y", std::ofstream::binary);
	vector<unsigned char> pic(w*h * 3 / 2);
	for (int i = 0; i < num; i++)
	{
		fp.read((char*)&pic[0], w*h * 3 / 2); //��Ҫʹ��pic.begin()
		fp1.write((char*)&pic[0], w*h);
		fp2.write((char*)&pic[0] + w*h, w*h / 4);
		fp3.write((char*)&pic[0] + w*h * 5 / 4, w*h / 4);
	}
	fp.close();
	fp1.close();
	fp2.close();
	fp3.close();
}

/**
* Split Y, U, V planes in YUV444P file.
* @param url  Location of YUV file.
* @param w    Width of Input YUV file.
* @param h    Height of Input YUV file.
* @param num  Number of frames to process.
*/
void simplest_yuv444p_split(const char* url, int w, int h, int num)
{
	ifstream ifs(url, ifstream::binary);
	if (!ifs)
	{
		cout << "not read" << endl;
		return;
	}
	ofstream ofs1("output_444_y.y",ofstream::binary);
	ofstream ofs2("output_444_u.y", ofstream::binary);
	ofstream ofs3("output_444_v.y", ofstream::binary);
	vector<unsigned char> pic(w*h * 3);
	for (int i = 0; i < num; i++)
	{
		ifs.read((char*)&pic[0], w*h * 3);
		ofs1.write((char*)&pic[0], w*h);
		ofs2.write((char*)&pic[0]+w*h, w*h);
		ofs3.write((char*)&pic[0]+w*h*2, w*h);	
	}
	ifs.close();
	ofs1.close();
	ofs2.close();
	ofs3.close();
}

// ��YUV420P��������ȥ����ɫ����ɻҶ�ͼ��
void simplest_yuv420p_gray(const char* url, int w, int h, int num)
{
	ifstream ifs(url, ifstream::binary);
	if (!ifs)
	{
		cout << "not read" << endl;
		return;
	}
	ofstream ofs1("output_gray.yuv", ofstream::binary);
	vector<unsigned char> pic(w*h * 3 / 2);
	for (int i = 0; i < num; i++)
	{
		ifs.read((char*)&pic[0], w * h * 3 / 2);
		std::memset((char*)&pic[w*h], 128, w*h / 2);
		ofs1.write((char*)&pic[0], w*h*3/2);
	}
	ifs.close();
	ofs1.close();
}

//��YUV420P�������ݵ����ȼ���
void simplest_yuv420p_half(const char* url, int w, int h, int num)
{
	ifstream ifs(url, ifstream::binary);
	if (!ifs)
	{
		cout << "not read" << endl;
		return;
	}
	ofstream ofs1("output_half_brightness.yuv", ofstream::binary);
	vector<unsigned char> pic(w*h * 3 / 2);
	for (int i = 0; i < num; i++)
	{
		ifs.read((char*)&pic[0], w * h * 3 / 2);
		std::for_each(pic.begin(), pic.begin() + w*h,
			[](unsigned char& b) {b = b / 2; });
		ofs1.write((char*)&pic[0], w*h * 3 / 2);
	}
	ifs.close();
	ofs1.close();
}

//��YUV420P�������ݵ���Χ���ϱ߿�
void simplest_yuv420p_border(const char* url, int w, int h, int border, int num)
{
	ifstream ifs(url, ifstream::binary);
	if (!ifs)
	{
		cout << "not read" << endl;
		return;
	}
	ofstream ofs1("output_border.yuv", ofstream::binary);
	vector<unsigned char> pic(w*h * 3 / 2);
	for (int i = 0; i < num; i++)
	{
		ifs.read((char*)&pic[0], w * h * 3 / 2);
		for (int i = 0; i < h; i++)
		{
			for (int j = 0; j < w; j++)
			{
				if (i < border || i >= (h - border) || j < border || j >= w - border)
					pic[i*w + j] = 255;
			}
		}
		ofs1.write((char*)&pic[0], w*h * 3 / 2);
	}
	ifs.close();
	ofs1.close();
}

//����YUV420P��ʽ�Ļҽײ���ͼ
void simplest_yuv420_graybar(int width, int height, int ymin, int ymax, int barnum, const char *url_out)
{
	int barwidth=width/barnum;
	double lum_inc=(ymax-ymin)/(double)(barnum); 
	int uv_width = width / 2;
	int uv_height = height / 2;
	vector<unsigned char> data_y(width*height);
	vector<unsigned char> data_u(uv_width*uv_height);
	vector<unsigned char> data_v(uv_width*uv_height);
	ofstream ofs(url_out, ofstream::binary);
	if (!ofs)
	{
		cout << "not open" << endl;
		return;
	}

	for (int j = 0; j < height; j++)
	{
		for (int i = 0; i < width; i++)
		{
			int t = i / barwidth;
			unsigned char temp = (unsigned char)(ymin + t*lum_inc);
			data_y[j*width + i] = temp;
		}
	}
	for (int j = 0; j < uv_height; j++)
	{
		for (int i = 0; i < uv_width; i++)
		{
			data_u[j * uv_width + i] = 128;
			data_v[j * uv_width + i] = 128;
		}
	}
	ofs.write((char*)&data_y[0],width*height);
	ofs.write((char*)&data_u[0], uv_width*uv_height);
	ofs.write((char*)&data_v[0], uv_width*uv_height);
	ofs.close();
}

void test_fstream_seekg()
{
	ofstream ofs("nums.bin", ofstream::binary);
	unsigned char a[] = {1,2,3,4,5};
	ofs.write((char*)a, 5);
	ofs.close();

	ifstream ifs("nums.bin", ifstream::binary);
	unsigned char b[5]={ 0 };
	ifs.read((char*)b, 1);
	ifs.seekg(2, std::ios::cur); //ifstream::_Seekbeg

	unsigned char c[5] = { 0 };
	ifs.read((char*)c, 2);
	ifs.close();
	for (auto i : b) cout << (int)i << " "; cout << endl; //1 0 0 0 0
	for (auto i : c) cout << (int)i << " "; cout << endl; //4 5 0 0 0
}

void simplest_yuv420_psnr(const char* url1, const char* url2, int w, int h, int num)
{
	ifstream ifs1(url1,ifstream::binary);
	ifstream ifs2(url2, ifstream::binary);
	vector<unsigned char> pic1(w*h);
	vector<unsigned char> pic2(w*h);
	vector<double> difsquare(w*h);
	for (int i = 0; i < num; i++)
	{
		ifs1.read((char*)&pic1[0], w*h);
		ifs2.read((char*)&pic2[0], w*h);

		std::transform(pic1.begin(),pic1.end(),pic2.begin(), difsquare.begin(),
			[](int a, int b) {return (a - b)*(a - b); });
		double mse_sum = std::accumulate(difsquare.begin(), difsquare.end(), 0.0);
		double mse = mse_sum / (w*h);
		double psnr = 10 * std::log10(255.0*255.0 / mse);
		cout << "psnr: " << psnr << endl;

		ifs1.seekg(w*h / 2, ifstream::_Seekcur);//std::ios::cur
		ifs2.seekg(w*h / 2, ifstream::_Seekcur);
	}
	ifs1.close();
	ifs2.close();
}

//����RGB24���������е�R��G��B����
void simplest_rgb24_split(const char* url, int w, int h, int num)
{
	ifstream ifs(url, ifstream::binary);
	if (!ifs) { cout << "not read" << endl; return; }
	ofstream ofs1("output_rgb_r.y", ofstream::binary);
	ofstream ofs2("output_rgb_g.y", ofstream::binary);
	ofstream ofs3("output_rgb_b.y", ofstream::binary);
	vector<unsigned char> pic(w*h * 3);
	for (int i = 0; i < num; i++)
	{
		ifs.read((char*)&pic[0],w*h*3);
		for (int j = 0; j < w*h * 3; j += 3)
		{
			ofs1 << pic[j];
			ofs2 << pic[j + 1];
			ofs3 << pic[j + 2];
		}
	}
	ifs.close();
	ofs1.close();
	ofs2.close();
	ofs3.close();
}

//��RGB24��ʽ�������ݷ�װΪBMPͼ��
void simplest_rgb24_to_bmp(const char *rgb24path, int width, int height, const char *bmppath)
{
	ifstream ifs(rgb24path, ifstream::binary);
	if (!ifs) {
		cout << "not read" << endl; return;
	}
	ofstream ofs(bmppath, ofstream::binary);
	if (!ofs) {
		cout << "not read" << endl; return;
	}
	vector<unsigned char> rgb24(width*height*3);
	ifs.read((char*)&rgb24[0], width*height * 3);
	
	char bfType[2] = { 'B','M' }; //λͼ�ļ������ͣ�����ΪBM�����ܽ�bfType�ϲ���struct BITMAPFILEHEADER�У����������ݶ�������
	struct BITMAPFILEHEADER
	{
		unsigned long       bfSize;       //�ļ���С�����ֽ�Ϊ��λ
		unsigned short int  bfReserverd1; //λͼ�ļ������֣�����Ϊ0 
		unsigned short int  bfReserverd2; //λͼ�ļ������֣�����Ϊ0 
		unsigned long       bfbfOffBits;  //λͼ�ļ�ͷ�����ݵ�ƫ���������ֽ�Ϊ��λ
	};
	struct BITMAPINFOHEADER
	{
		long  biSize;                        //�ýṹ��С���ֽ�Ϊ��λ
		long  biWidth;                     //ͼ�ο��������Ϊ��λ
		long  biHeight;                     //ͼ�θ߶�������Ϊ��λ,BMP storage pixel data in opposite direction of Y-axis (from bottom to top)
		unsigned short int  biPlanes;               //Ŀ���豸�ļ��𣬱���Ϊ1 
		unsigned short int  biBitcount;             //��ɫ��ȣ�ÿ����������Ҫ��λ��
		long  biCompression;        //λͼ��ѹ�����ͣ���ȡ0
		long  biSizeImage;              //λͼ�Ĵ�С�����ֽ�Ϊ��λ��ָ���ݲ��ֵ��ֽ���
		long  biXPelsPermeter;       //λͼˮƽ�ֱ��ʣ�ÿ������������ȡ0
		long  biYPelsPermeter;       //λͼ��ֱ�ֱ��ʣ�ÿ������������ȡ0
		long  biClrUsed;            //λͼʵ��ʹ�õ���ɫ���е���ɫ������ȡ0
		long  biClrImportant;       //λͼ��ʾ��������Ҫ����ɫ������ȡ0
	};

	unsigned long bfSize = 2 + sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + width*height * 3;
	unsigned long bfbfOffBits = 2 + sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
	BITMAPFILEHEADER filehead{bfSize, 0, 0, bfbfOffBits};
	BITMAPINFOHEADER infohead{sizeof(BITMAPINFOHEADER),width,-height,1,24,0,width*height*3,0,0,0,0};
	ofs.write(bfType, 2);
	ofs.write((char*)&filehead, sizeof(BITMAPFILEHEADER));
	ofs.write((char*)&infohead, sizeof(BITMAPINFOHEADER));
	
	for (int i = 0; i < width*height * 3; i+=3) //����r��bλ��
	{
		unsigned char temp = rgb24[i];
		rgb24[i] = rgb24[i + 2];
		rgb24[i + 2] = temp;
	}
	ofs.write((char*)&rgb24[0], width*height * 3);

	ifs.close();
	ofs.close();
}

//��RGB24��ʽ��������ת��ΪYUV420P��ʽ��������
void simplest_rgb24_to_yuv420(const char *url_in, int w, int h, int num, const char *url_out)
{
	auto rgb24_to_yuv420 = [](unsigned char *rgbbuf,int w,int h,unsigned char* yuvbuf) 
	{
		unsigned char* ptrY = yuvbuf;
		int iy=0;
		unsigned char* ptrU = yuvbuf + w*h;
		int iu=0;
		unsigned char* ptrV = yuvbuf + w*h * 5 / 4;
		int iv=0;
		for (int i = 0; i < h; i++)
		{
			for (int j = 0; j < w; j++)
			{
				unsigned char r = rgbbuf[3 * (i*w+j)];
				unsigned char g = rgbbuf[3 * (i*w + j) + 1];
				unsigned char b = rgbbuf[3 * (i*w + j) + 2];
				unsigned char y = (unsigned char)((66 * r + 129 * g + 25 * b + 128) >> 8) + 16;
				unsigned char u = (unsigned char)((-38 * r - 74 * g + 112 * b + 128) >> 8) + 128;
				unsigned char v = (unsigned char)((112 * r - 94 * g - 18 * b + 128) >> 8) + 128;
				//ת����Y�ķ�Χ��[16,235]��UV �ķ�Χ��[16,239]����[0,255]�ܽӽ�
				
				ptrY[iy++] = y;
				if ((i & 1) == 0 && (j & 1) == 0)  //�����()��ע�����ȼ�
				{
					ptrU[iu++] = u;
				}
				else if((i&1)!=0 && (j&1)==0)
				{
					ptrV[iv++] = v;
				}
			}
		}
	};
	ifstream ifs(url_in, ifstream::binary);
	if (!ifs) { cout << "not read" << endl; return; }
	ofstream ofs(url_out, ofstream::binary);
	vector<unsigned char> vin(w*h * 3);
	vector<unsigned char> vout(w*h * 3 / 2);
	for (int i = 0; i < num; i++)
	{
		ifs.read((char*)&vin[0], w*h * 3);
		rgb24_to_yuv420(&vin[0], w, h, &vout[0]);
		ofs.write((char*)&vout[0], w*h * 3 / 2);
	}
	ifs.close();
	ofs.close();
}

void simplest_rgb24_colorbar(int width, int height, const char *url_out)
{
	ofstream ofs(url_out,ofstream::binary);
	const int barnum = 8;
	int barwidth = width / barnum;
	vector<unsigned char> pic(width*height * 3);
	for (int i = 0; i < height; i++)
	{
		for (int j = 0; j < width; j++)
		{
			int baridx = j / barwidth;
			switch (baridx)
			{
			case 0:
				pic[3 * (i*width + j)] = 255;
				pic[3 * (i*width + j) + 1] = 255;
				pic[3 * (i*width + j) + 2] = 255;
				break;
			case 1:
				pic[3 * (i*width + j)] = 255;
				pic[3 * (i*width + j) + 1] = 255;
				pic[3 * (i*width + j) + 2] = 0;
				break;
			case 2:
				pic[3 * (i*width + j)] = 0;
				pic[3 * (i*width + j) + 1] = 255;
				pic[3 * (i*width + j) + 2] = 255;
				break;
			case 3:
				pic[3 * (i*width + j)] = 0;
				pic[3 * (i*width + j) + 1] = 255;
				pic[3 * (i*width + j) + 2] = 0;
				break;
			case 4:
				pic[3 * (i*width + j)] = 255;
				pic[3 * (i*width + j) + 1] = 0;
				pic[3 * (i*width + j) + 2] = 255;
				break;
			case 5:
				pic[3 * (i*width + j)] = 255;
				pic[3 * (i*width + j) + 1] = 0;
				pic[3 * (i*width + j) + 2] = 0;
				break;
			case 6:
				pic[3 * (i*width + j)] = 0;
				pic[3 * (i*width + j) + 1] = 0;
				pic[3 * (i*width + j) + 2] = 255;
				break;
			case 7:
				pic[3 * (i*width + j)] = 0;
				pic[3 * (i*width + j) + 1] = 0;
				pic[3 * (i*width + j) + 2] = 0;
				break;
			}	
		}
	}
	ofs.write((char*)&pic[0], width*height * 3);
	ofs.close();
}
int main()
{
	//simplest_yuv420p_split("girl_350540_yuv420p.yuv", 350, 540, 1);
	//simplest_yuv444p_split("girl_350540_yuv444p.yuv", 350, 540, 1);
	//simplest_yuv420p_gray("girl_350540_yuv420p.yuv", 350, 540, 1);
	//simplest_yuv420p_half("girl_350540_yuv420p.yuv", 350, 540, 1);
	//simplest_yuv420p_border("girl_350540_yuv420p.yuv", 350, 540, 10, 1);
	//simplest_yuv420_graybar(350, 540, 0, 255,10,"graybar_350x540.yuv");
	//simplest_yuv420_psnr("output_half_brightness.yuv", "girl_350540_yuv420p.yuv", 350, 540, 1);

	//test_fstream_seekg();

	//simplest_rgb24_split("��׼��CIE1931ɫ��ͼ_400382_rgb24.rgb", 400, 382, 1);
	//simplest_rgb24_to_bmp("��׼��CIE1931ɫ��ͼ_400382_rgb24.rgb", 400, 382, "output_��׼��CIE1931ɫ��ͼ_400382.bmp");
	//simplest_rgb24_to_yuv420("��׼��CIE1931ɫ��ͼ_400382_rgb24.rgb", 400, 382, 1,"output_��׼��CIE1931ɫ��ͼ_400382.yuv");
	simplest_rgb24_colorbar(640, 360, "colorbar_640x360.rgb");
	simplest_rgb24_to_yuv420("colorbar_640x360.rgb", 640, 360, 1, "colorbar_640x360.yuv");
}