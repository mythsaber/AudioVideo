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

//NALUͷ1���ֽڣ�1bit��F+2bit��NRI+5bit��TYPE
//TYPE��ʾ��NALU��������ʲô
enum NaluType
{
	NALU_TYPE_SLICE = 1,  //����������IDRͼ���slice(Ƭ)
	NALU_TYPE_DPA = 2,    //ʹ��Data Partitioning����Ϊslice A
	NALU_TYPE_DPB = 3,    //ʹ��Data Partitioning����Ϊslice B
	NALU_TYPE_DPC = 4,    //ʹ��Data Partitioning����Ϊslice C
	NALU_TYPE_IDR = 5,    //IDRͼ���е�slice
	NALU_TYPE_SEI = 6,    //������ǿ��Ϣ��Ԫ(SEI)
	NALU_TYPE_SPS = 7,    //Sequence Parameter Set��SPS�����в�����
	NALU_TYPE_PPS = 8,    //Picture Parameter Set��PPS��ͼ�������
	NALU_TYPE_AUD = 9,    //�ֽ��
	NALU_TYPE_EOSEQ = 10,    //���н���
	NALU_TYPE_EOSTREAM = 11, //��������
	NALU_TYPE_FILL = 12,     //���
};

enum NaluPriority
{
	NALU_PRIORITY_DISPOSABLE = 0,
	NALU_PRIRITY_LOW = 1,
	NALU_PRIORITY_HIGH = 2,
	NALU_PRIORITY_HIGHEST = 3
};

struct NALU_t
{
	int startcodeprefix_len;      //! 4 for parameter sets and first slice in picture, 3 for everything else (suggested)
	unsigned len;                 //! Length of the NAL unit (Excluding the start code, which does not belong to the NALU)
	unsigned max_size;            //! Nal Unit Buffer size
	int forbidden_bit;            //! should be always FALSE
	int nal_reference_idc;        //! NALU_PRIORITY_xxxx
	int nal_unit_type;            //! NALU_TYPE_xxxx    
	char *buf;                    //! contains the first byte followed by the EBSP
};

//��ʼ��ֳ����֣�0x000001��3Byte������0x00000001��4Byte��
static int findStartCode2(const unsigned char* buf){
	if (buf[0] != 0 || buf[1] != 0 || buf[2] != 1) return 0;
	else return 1;
}
static int findStartCode3(const unsigned char* buf) {
	if (buf[0] != 0 || buf[1] != 0 || buf[2] != 0 || buf[3] != 1) 
		return 0;
	else return 1;
}

//ifs������ʼ�뿪ʼ��H.264������ȡ��Ϣ��nalu��,����һ��NALU��size
int getAnnexNalu(NALU_t& nalu,ifstream& ifs)
{
	vector<unsigned char> buf(nalu.max_size);

	int pos = 0;
	int info2 = 0;
	int info3 = 0;

	ifs.read((char*)&buf[0],3);  //����ʼ��
	info2 = findStartCode2(&buf[0]);
	if (info2 != 1) {
		ifs.read((char*)&buf[3],1);
		info3= findStartCode3(&buf[0]);
		if (info3 != 1) return 0;
		else{
			pos = 4;
			nalu.startcodeprefix_len = 4;
		}
	}
	else {
		nalu.startcodeprefix_len = 3;
		pos = 3;
	}

	int startCodeFound = 0;
	info2 = 0;
	info3 = 0;
	while (!startCodeFound) {
		if (ifs.peek() == EOF) {
			nalu.len = pos - nalu.startcodeprefix_len;
			memcpy(nalu.buf,&buf[nalu.startcodeprefix_len],
				nalu.len);
			nalu.forbidden_bit = (nalu.buf[0]) & 0x80;
			nalu.nal_reference_idc = (nalu.buf[0]) & 0x60;
			nalu.nal_unit_type = (nalu.buf[0]) & 0x1f;
			return pos;
		}
		buf[pos++] = ifs.get();
		info3 = findStartCode3(&buf[pos - 4]);
		if (info3 != 1)
			info2 = findStartCode2(&buf[pos - 3]);
		startCodeFound = (info2 == 1 || info3 == 1);
	}

	//������һ����ʼ��
	ifs.seekg(info3==1?-4:-3,ifs._Seekcur); //�Ѿ��������У���˺���
	nalu.len = pos + (info3 == 1 ? -4 : -3) 
		- nalu.startcodeprefix_len;
	memcpy(nalu.buf, &buf[nalu.startcodeprefix_len],
		nalu.len);
	nalu.forbidden_bit = (nalu.buf[0]) & 0x80;
	nalu.nal_reference_idc = (nalu.buf[0]) & 0x60;
	nalu.nal_unit_type = (nalu.buf[0]) & 0x1f;

	return pos + (info3 == 1 ? -4 : -3);
}

void simplest_h264_parser(const char* url)
{
	ifstream  h264bitstream(url,ifstream::binary);
	if (!h264bitstream) { cout << "not read" << endl; return; }
	int buffersize = 100000;
	NALU_t n;
	n.max_size = buffersize;
	vector<unsigned char> vec(buffersize);
	n.buf = (char*)&vec[0];

	cout << "-----+-------- NALU Table ------+---------+" << endl;;
	cout<<" NUM |    POS  |    IDC |  TYPE |   LEN   |" << endl;;
	cout << "-----+---------+--------+-------+---------+" << endl;;
	
	int nal_num = 0;
	int data_offset = 0;
	while (h264bitstream.peek() != EOF)
	{
		int data_lenth = getAnnexNalu(n,h264bitstream);
		char type_str[20]={ 0};
		switch (n.nal_unit_type)
		{
		case NALU_TYPE_SLICE: strcpy_s(type_str,20,"SLICE"); break;
		case NALU_TYPE_DPA:strcpy_s(type_str, 20, "DPA"); break;
		case NALU_TYPE_DPB:strcpy_s(type_str, 20, "DPB"); break;
		case NALU_TYPE_DPC:strcpy_s(type_str, 20, "DPC"); break;
		case NALU_TYPE_IDR:strcpy_s(type_str, 20, "IDR"); break;
		case NALU_TYPE_SEI:strcpy_s(type_str, 20, "SEI"); break;
		case NALU_TYPE_SPS:strcpy_s(type_str, 20, "SSPS"); break;
		case NALU_TYPE_PPS:strcpy_s(type_str, 20, "PPS"); break;
		case NALU_TYPE_AUD:strcpy_s(type_str, 20, "AUD"); break;
		case NALU_TYPE_EOSEQ:strcpy_s(type_str, 20, "EOSEQ"); break;
		case NALU_TYPE_EOSTREAM:strcpy_s(type_str, 20, "EOSTREAM");; break;
		case NALU_TYPE_FILL:strcpy_s(type_str, 20, "FILL"); break;
		}
		char idc_str[20] = {0};
		switch ((n.nal_reference_idc) >> 5)
		{
		case NALU_PRIORITY_DISPOSABLE:
			strcpy_s(idc_str,20, "DISPOS"); break;
		case NALU_PRIRITY_LOW:
			strcpy_s(idc_str, 20, "LOW"); break;
		case NALU_PRIORITY_HIGH:
			strcpy_s(idc_str, 20, "HIGH"); break;
		case NALU_PRIORITY_HIGHEST:
			strcpy_s(idc_str, 20, "HIGHEST"); break;
		}
		printf("%5d| %8d| %10s| %10s| %8d|\n", nal_num, 
			data_offset, idc_str, type_str, n.len);
		data_offset += data_lenth;
		nal_num++;
	}
	h264bitstream.close();
}

int main()
{
	simplest_h264_parser("sintel.h264");
}