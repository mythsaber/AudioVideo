#include<iostream>
#include<fstream>
#include<string>
#include<vector>
#include<algorithm>
#include<numeric>
#include<cmath>
#include<chrono>
#include<winsock2.h> //in_addr
#include <WS2tcpip.h> //inet_pton��inet_ntop
using std::ifstream;
using std::ofstream;
using std::string;
using std::vector;
using std::cout;
using std::endl;
using std::cin;

#pragma comment(lib,"ws2_32.lib")

// �ӵ�ַp��ʼ���ɵ͵�ַ���ߵ�ַ����ʾbyte_num���ֽڵ�bit����
void dump_native_bits_storage_layout(unsigned char *p, int bytes_num)
{
	union flag_t
	{
		unsigned char c;
		struct base_flag_t
		{
			unsigned int p7 : 1, p6 : 1, p5 : 1, p4 : 1, p3 : 1, p2 : 1, p1 : 1, p0 : 1;
		} base;
	} f;
	for (int i = 0; i < bytes_num; i++) {
		f.c = *(p + i);
		printf("%d%d%d%d %d%d%d%d ",
			f.base.p7, f.base.p6, f.base.p5, f.base.p4, f.base.p3,
			f.base.p2, f.base.p1, f.base.p0);
	}
	printf("\n");
}

#pragma pack(push,1)
/*
pragma packָ������ֵΪ1���
�ṹ�������׵�ַλ��1�ֽ����������ṹ������ݳ�ԱΪ4�ֽڣ�1<=4��
������Ա����ֵΪ1��1<=k��kΪ��Ա�ĳ��ȣ�
�ܴ�С������1����������1=<4��
*/
struct RtpFixedHeader  //��12�ֽ�
{
	//������ƽṹ�壬����С��ϵͳ�ϣ�ֱ�ӽ������������ݿ�����һ��
	//RtpFixedHeader������ڴ棬��ö���ĸ����ݳ�Ա��Ϊ��ȷ��ֵ��Ҫ���Ϊʲô��
	uint8_t csrc_len : 4;  
	uint8_t extension : 1; //ȡֵ1��RTP��ͷ�����һ����չ��ͷ
	uint8_t padding : 1;  
	uint8_t version : 2;   

	uint8_t payload_type : 7;
	uint8_t marker : 1;

	uint16_t seq_no;
	uint32_t timestamp;
	uint32_t ssrc;//stream number is used here.
};
#pragma pack(pop)

#pragma pack(push,1)
struct MpegtsHeader //��4�ֽ�
{
	///������ƽṹ�壬����С��ϵͳ�ϣ�ֱ�ӽ������������ݿ�����һ��
	//MpegtsHeader������ڴ棬��ʱ�ö���ĸ����ݳ�Ա��ֵ����ȷ��
	//Ҳ����˵ MpegtsHeader�ṹ����ڴ沼����ʵ���ڴ���ĳ��mpeg-ts��ͷ���ڴ沼�ֲ�һ�¡�
	//Ҫ��MpegtsHeader��ֵ����������ĵ�parse_ts_header����
	uint32_t sync_byte : 8; //ͬ���ֽڣ��̶�Ϊ0x47����ʾ�������һ��TS���飬���к���������ǲ������0x47��
	uint32_t transport_error_indicator : 1; //��������־λ��һ�㴫�����Ļ��Ͳ��ᴦ���������
	uint32_t payload_unit_start_indicator : 1;//0x01��ʾ����PSI��Program Specific Information����Ŀר����Ϣ������PESͷ
	uint32_t transport_priority : 1;//�������ȼ�λ��1��ʾ�����ȼ�
	uint32_t pid : 13; //��Ч���ص��������ͣ�PAT��CAT��NIT��SDT��EIT��PID�ֱ�Ϊ: 0x00��0x01��0x10��0x11��0x12
	uint32_t scrambling_control : 2;  //���ܱ�־λ,00��ʾδ����
	uint32_t adaptation_field_control : 2; //�����ֶο��ơ�01������Ч���أ�10���������ֶΣ�11���е����ֶκ���Ч���ء�00�Ļ������������д���reserved for future use by ISO/IEC
	uint32_t continuity_counter : 4; //һ��4bit�ļ���������Χ0-15
};
#pragma pack(pop)

//����-1��ʾ�����TS��ͷ������0��ʾ�ɹ�
int parse_ts_header(unsigned char *buf, MpegtsHeader *pheader)
{
	pheader->sync_byte= buf[0];
	if (pheader->sync_byte!= 0x47)
		return -1;
	pheader->transport_error_indicator= buf[1] >> 7;
	pheader->payload_unit_start_indicator= buf[1] >> 6 & 0x01;
	pheader->transport_priority= buf[1] >> 5 & 0x01;
	pheader->pid= (buf[1] & 0x1F) << 8 | buf[2];
	pheader->scrambling_control= buf[3] >> 6;
	pheader->adaptation_field_control= buf[3] >> 4 & 0x03;
	pheader->continuity_counter= buf[3] & 0x0F;
	return 0;
}

int simplest_udp_parse(int port)
{
	WSADATA wsadata;
	WORD sockversion = MAKEWORD(2, 2);
	int cnt = 0;

	ofstream ofs("output_dump.ts", std::ios::binary);

	if (WSAStartup(sockversion, &wsadata) != 0) return 0;

	//AF_INETΪipv4��AF_INET6Ϊipv6
	SOCKET sersocket = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP); //�����׽��֣�����udp
	if (sersocket == INVALID_SOCKET)  //���ص������������INVALID_SOCKET�����ʾ����ʧ��
	{
		cout << "socket error" << endl;
		return 0;
	}

	sockaddr_in seraddr;
	seraddr.sin_family = AF_INET;
	seraddr.sin_port = htons(port); //���ֽ���ֵ�������ֽ���תΪ�����ֽ��򣨴�ˣ�
	seraddr.sin_addr.S_un.S_addr = INADDR_ANY;
	//bind���������׽����������ͷ������׽��ֵ�ַ��ϵ�����������������Ϳͻ��˽������� 
	if (bind(sersocket,(sockaddr*)&seraddr,sizeof(seraddr)) == SOCKET_ERROR)
	{
		cout << "bind error!" << endl;
		closesocket(sersocket);
		return 0;
	}

	sockaddr_in remoteaddr;
	int naddrlen = sizeof(remoteaddr);

	//how to parse?
	int parse_rtp = 1;
	int parse_mpegts = 1;
	constexpr int size = 10240;
	char recvdata[size];
	MpegtsHeader tsheader;
	while (1)
	{
		//sendto��recvfromһ������UDPЭ����
		int pktsize = recvfrom(sersocket,recvdata,size,0,
			(sockaddr*)&remoteaddr,&naddrlen);
		//remoteaddr�������淢�ͷ�IP��ַ���˿ں�
		//fromlen����Ϊsizeof ��remoteaddr��;��recvfrom��������ʱ��fromlen����ʵ�ʴ���from�е������ֽ���

		if (pktsize > 0)
		{
			if (parse_rtp != 0)
			{
				char payload_str[10];
				RtpFixedHeader* rtp_header=(RtpFixedHeader*)recvdata;

				//RFC3551
				char payload = rtp_header->payload_type;
				switch (payload)
				{
				case 0:case 1:case 2:case 3:case 4:case 5:case 6:
				case 7:case 8:case 9:case 10:case 11:case 12:case 13:
				case 14:case 15:case 16:case 17:
				case 18: strcpy_s(payload_str, 10, "Audio"); break;
				case 31: strcpy_s(payload_str, 10,"H.261"); break;
				case 32: strcpy_s(payload_str, 10, "MPV"); break;
				case 33: strcpy_s(payload_str, 10, "MP2T"); break;
				case 34: strcpy_s(payload_str, 10, "H.263"); break;
				case 96: strcpy_s(payload_str, 10, "H.264"); break;
				default:strcpy_s(payload_str, 10, "others"); break;
				}

				//��TCP/IP�Ĺ涨�Ĵ������תΪ�������ֽ�˳��

				unsigned timestamp = ntohl(rtp_header->timestamp); //net to host long int
				unsigned seq_no = ntohs(rtp_header->seq_no); //net to host short int
				cout << "rtp_header->timestamp=" << rtp_header->timestamp << endl;
				cout << "timestamp=" << timestamp << endl;
				printf("[RTP Pkt] %5d | %5s | %10u | %5d %5d |\n",
					cnt, payload_str, timestamp, seq_no, pktsize);
				cout << (int)rtp_header->csrc_len << " " << (int)rtp_header->extension << " "
					<< (int)rtp_header->padding << " " << (int)rtp_header->version << endl; //0 0 0 2

				//RTP Data
				char *rtp_data = recvdata + sizeof(RtpFixedHeader);
				int rtp_data_size = pktsize - sizeof(RtpFixedHeader);
				ofs.write(rtp_data,rtp_data_size);  //��RTP Bodyд���ļ�

				//Parse MPEGTS
				if (parse_mpegts == 1 && payload == 33)
				{
					//MpegtsHeader mpegts_header;
					for (int i = 0; i < rtp_data_size; i += 188)
					{
						memset(&tsheader,0,sizeof(tsheader));
						int ret = parse_ts_header((unsigned char*)(rtp_data + i), &tsheader);
						dump_native_bits_storage_layout((unsigned char*)(rtp_data + i), sizeof(MpegtsHeader));
						if (ret != 0)
						{
							cout << "not ts header" << endl;
							break;
						}
						//С��ϵͳ�ϣ�bit�ڴ沼�����磺1110 0010     1000 0010     0000 0000     1010 1000
						//��ˣ�sync_byteΪ������01000111
						//transport_error_indicatorΪ0��payload_unit_start_indicatorΪ1��transport_priorityΪ0
						//pidΪ00001 00000000
						//scrambling_controlΪ00��adaptatio_field_controlΪ01��continuity_counterΪ0101
						cout << "sync_byte: " << tsheader.sync_byte << endl; //71=0x47
						cout << "transport_error_indicator: " << tsheader.transport_error_indicator << endl; //1
						cout << "payload_unit_start_indicator: " << tsheader.payload_unit_start_indicator << endl; //0
						cout << "transport_priority: " << tsheader.transport_priority << endl; //0
						cout << "pid: " << tsheader.pid << endl; //8
						cout << "scrambling_control: " << tsheader.scrambling_control << endl;  //1
						cout << "adaptatio_field_control: " << tsheader.adaptation_field_control << endl;  //1
						cout << "continuity_counter: " << tsheader.continuity_counter << endl; //1
						cout << endl;
					}
				}
			}
			else
			{
				printf("[UDP Pkt] %5d | %5d |\n",
					cnt,pktsize);
				ofs.write(recvdata, pktsize);
			}
			cnt++;
		}
	}
	closesocket(sersocket);
	WSACleanup();

	return 0;
}

bool bigendian()
{
	unsigned short x = 0x1234;
	if (*((unsigned char*)&x) == 0x12) return true;
	else return false;
}

void test_bigendian_tcpip()
{
	in_addr addr;
	addr.S_un.S_addr = 0x12345678u;
	unsigned char* p = (unsigned char*)&addr.S_un.S_addr;
	cout << (int)p[0] << " " << (int)p[1] << " "
		<< (int)p[2] << " " << (int)p[3] << endl;//120 86 52 18

	uint32_t res = htonl(addr.S_un.S_addr);
	printf("%x\n", res);//78563412
	p = (unsigned char*)&res;
	cout << (int)p[0] << " " << (int)p[1] << " " 
		<< (int)p[2] << " " << (int)p[3] << endl; //18 52 86 120
}

void test_ntop_pton()
{
	const char* addrp = "205.188.160.121"; //cd.bc.a0.79
	uint32_t dst;
	int code = inet_pton(AF_INET,addrp,&dst);//���10�����ַ���תΪTCPIP�Ĵ���ֽ�����
	unsigned char* p = (unsigned char*)&dst;
	cout << (int)p[0] << " " << (int)p[1] << " "
		<< (int)p[2] << " " << (int)p[3] << endl; //205 188 160 121
	printf("0x%x\n", dst); //0x79a0bccd
}
int main()
{
	simplest_udp_parse(8888);
	cout << "���output_dump.ts�ļ�" << endl;
	//cout << std::boolalpha << bigendian() << endl; //0
	//test_bigendian_tcpip();

	//test_ntop_pton();

	cout << sizeof(in_addr) << endl; //4
	cout << sizeof(sockaddr_in) << endl; //16
}