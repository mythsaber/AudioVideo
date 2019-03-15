#include<iostream>
#include<fstream>
#include<string>
#include<vector>
#include<algorithm>
#include<numeric>
#include<cmath>
#include<chrono>
#include<winsock2.h> //in_addr
#include <WS2tcpip.h> //inet_pton
using std::ifstream;
using std::ofstream;
using std::string;
using std::vector;
using std::cout;
using std::endl;
using std::cin;

#pragma comment(lib,"ws2_32.lib")

typedef unsigned char byte;

#pragma pack(1)
struct RtpFixedHeader  //共12字节
{
	byte csrc_len : 4;  //expect 0 
	byte extension : 1; //expect 1 
	byte padding : 1; //expect 0 
	byte version : 2;  //expect 2

	byte payload : 7;
	byte marker : 1;

	unsigned short seq_no;

	unsigned long timestamp;

	unsigned long ssrc;//stream number is used here.
};
#pragma pack()

#pragma pack(1)
struct MpegtsFixedHeader
{
	unsigned sync_byte : 8;
	unsigned transport_error_indicator : 1;
	unsigned payload_unit_start_indicator : 1;
	unsigned transport_priority : 1;
	unsigned pid : 13;
	unsigned scrambling_control : 2;
	unsigned adaptation_field_exist : 2;
	unsigned continuity_counter : 4;
};
#pragma pack()

int simplest_udp_parse(int port)
{
	WSADATA wsadata;
	WORD sockversion = MAKEWORD(2, 2);
	int cnt = 0;

	ofstream ofs("output_dump.ts", std::ios::binary);

	if (WSAStartup(sockversion, &wsadata) != 0) return 0;

	SOCKET sersocket = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP); //创建套接字描述符
	if (sersocket == INVALID_SOCKET)  //返回的描述符如果是INVALID_SOCKET，则表示创建失败
	{
		cout << "socket error" << endl;
		return 0;
	}

	sockaddr_in seraddr;
	seraddr.sin_family = AF_INET;
	seraddr.sin_port = htons(port);
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
	char recvdata[10000];
	while (1)
	{
		//sendto和recvfrom一般用于UDP协议中，但是如果在TCP中connect函数调用后也可以用
		int pktsize = recvfrom(sersocket,recvdata,10000,0,
			(sockaddr*)&remoteaddr,&naddrlen);
		//remoteaddr变量保存源机的IP地址及端口号
		//fromlen常置为sizeof （remoteaddr）;当recvfrom（）返回时，fromlen包含实际存入from中的数据字节数

		if (pktsize > 0)
		{
			if (parse_rtp != 0)
			{
				char payload_str[10];
				RtpFixedHeader rtp_header;
				int rtp_header_size = sizeof(RtpFixedHeader);
				memcpy((void*)&rtp_header, recvdata, rtp_header_size);

				//RFC3551
				char payload = rtp_header.payload;
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
				unsigned timestamp = ntohl(rtp_header.timestamp);
				unsigned seq_no = ntohs(rtp_header.seq_no);

				printf("[RTP Pkt] %5d | %5s | %10u | %5d %5d |\n",
					cnt, payload_str, timestamp, seq_no, pktsize);

				//RTP Data
				char *rtp_data = recvdata + SETABORTPROC;
				int rtp_data_size = pktsize - rtp_header_size;
				ofs.write(rtp_data,rtp_data_size);

				//Parse MPEGTS
				if (parse_mpegts != 0 && payload == 33)
				{
					//MpegtsFixedHeader mpegts_header;
					for (int i = 0; i < rtp_data_size; i += 188)
					{
						if (rtp_data[i] != 0x47) break;
						cout << " [MPEGTS Pkt]" << endl;
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
	//simplest_udp_parse(8880);

	//cout << std::boolalpha << bigendian() << endl; //0
	//test_bigendian_tcpip();

	//test_ntop_pton();

	cout << sizeof(in_addr) << endl; //4
	cout << sizeof(sockaddr_in) << endl; //16
}