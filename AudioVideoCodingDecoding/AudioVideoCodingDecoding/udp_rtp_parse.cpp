#include<iostream>
#include<fstream>
#include<string>
#include<vector>
#include<algorithm>
#include<numeric>
#include<cmath>
#include<chrono>
#include<winsock2.h> //in_addr
#include <WS2tcpip.h> //inet_pton、inet_ntop
using std::ifstream;
using std::ofstream;
using std::string;
using std::vector;
using std::cout;
using std::endl;
using std::cin;

#pragma comment(lib,"ws2_32.lib")

// 从地址p开始，由低地址到高地址，显示byte_num个字节的bit布局
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
pragma pack指定对齐值为1因此
结构体自身首地址位于1字节整数倍（结构体最长数据成员为4字节，1<=4）
各个成员对齐值为1（1<=k，k为成员的长度）
总大小必须是1的整数倍（1=<4）
*/
struct RtpFixedHeader  //共12字节
{
	//如下设计结构体，则在小端系统上，直接将缓冲区的数据拷贝到一个
	//RtpFixedHeader对象的内存，则该对象的各数据成员即为正确的值。要理解为什么！
	uint8_t csrc_len : 4;  
	uint8_t extension : 1; //取值1则RTP报头后跟有一个扩展报头
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
struct MpegtsHeader //共4字节
{
	///如下设计结构体，则在小端系统上，直接将缓冲区的数据拷贝到一个
	//MpegtsHeader对象的内存，此时该对象的各数据成员的值不正确。
	//也就是说 MpegtsHeader结构体的内存布局与实际内存中某个mpeg-ts包头的内存布局不一致。
	//要给MpegtsHeader赋值必须借助下文的parse_ts_header函数
	uint32_t sync_byte : 8; //同步字节，固定为0x47，表示后面的是一个TS分组，包中后面的数据是不会出现0x47的
	uint32_t transport_error_indicator : 1; //传输错误标志位，一般传输错误的话就不会处理这个包了
	uint32_t payload_unit_start_indicator : 1;//0x01表示含有PSI（Program Specific Information，节目专用信息）或者PES头
	uint32_t transport_priority : 1;//传输优先级位，1表示高优先级
	uint32_t pid : 13; //有效负载的数据类型，PAT、CAT、NIT、SDT、EIT的PID分别为: 0x00、0x01、0x10、0x11、0x12
	uint32_t scrambling_control : 2;  //加密标志位,00表示未加密
	uint32_t adaptation_field_control : 2; //调整字段控制。01仅含有效负载，10仅含调整字段，11含有调整字段和有效负载。00的话解码器不进行处理，reserved for future use by ISO/IEC
	uint32_t continuity_counter : 4; //一个4bit的计数器，范围0-15
};
#pragma pack(pop)

//返回-1表示输入非TS包头，返回0表示成功
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

	//AF_INET为ipv4，AF_INET6为ipv6
	SOCKET sersocket = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP); //创建套接字，基于udp
	if (sersocket == INVALID_SOCKET)  //返回的描述符如果是INVALID_SOCKET，则表示创建失败
	{
		cout << "socket error" << endl;
		return 0;
	}

	sockaddr_in seraddr;
	seraddr.sin_family = AF_INET;
	seraddr.sin_port = htons(port); //多字节数值，主机字节序转为网络字节序（大端）
	seraddr.sin_addr.S_un.S_addr = INADDR_ANY;
	//bind函数，将套接字描述符和服务器套接字地址联系起来，服务器用来和客户端建立连接 
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
		//sendto和recvfrom一般用于UDP协议中
		int pktsize = recvfrom(sersocket,recvdata,size,0,
			(sockaddr*)&remoteaddr,&naddrlen);
		//remoteaddr变量保存发送方IP地址及端口号
		//fromlen常置为sizeof （remoteaddr）;当recvfrom（）返回时，fromlen包含实际存入from中的数据字节数

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

				//将TCP/IP的规定的大端整数转为主机的字节顺序

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
				ofs.write(rtp_data,rtp_data_size);  //将RTP Body写到文件

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
						//小端系统上，bit内存布局形如：1110 0010     1000 0010     0000 0000     1010 1000
						//因此，sync_byte为二进制01000111
						//transport_error_indicator为0，payload_unit_start_indicator为1，transport_priority为0
						//pid为00001 00000000
						//scrambling_control为00，adaptatio_field_control为01，continuity_counter为0101
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
	int code = inet_pton(AF_INET,addrp,&dst);//点分10进制字符串转为TCPIP的大端字节整数
	unsigned char* p = (unsigned char*)&dst;
	cout << (int)p[0] << " " << (int)p[1] << " "
		<< (int)p[2] << " " << (int)p[3] << endl; //205 188 160 121
	printf("0x%x\n", dst); //0x79a0bccd
}
int main()
{
	simplest_udp_parse(8888);
	cout << "输出output_dump.ts文件" << endl;
	//cout << std::boolalpha << bigendian() << endl; //0
	//test_bigendian_tcpip();

	//test_ntop_pton();

	cout << sizeof(in_addr) << endl; //4
	cout << sizeof(sockaddr_in) << endl; //16
}