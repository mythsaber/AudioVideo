#include<iostream>  
#include<stdio.h>  
#include<windows.h>
using namespace std;

string  UrlUTF8(char * str);
void GB2312ToUTF_8(string& pOut, char *pText, int pLen);
void Gb2312ToUnicode(WCHAR* pOut, char *gbBuffer);
void  UnicodeToUTF_8(char* pOut, WCHAR* pText);

string  UrlUTF8(char * str)
{
	string tt;
	string dd;
	GB2312ToUTF_8(tt, str, strlen(str));
	int len = tt.length();
	for (int i = 0; i<len; i++)
	{
		// isalnum判断字符是否是数或者英文
		// ispunct判断一个字符是否为标点符号或特殊字符
		if (isalnum((BYTE)tt.at(i)) || ispunct((BYTE)tt.at(i)))
		{
			char tempbuff[2] = { 0 };
			sprintf_s(tempbuff, "%c", (BYTE)tt.at(i));
			dd.append(tempbuff);
		}
		else if (isspace((BYTE)tt.at(i)))
		{
			dd.append("+");
		}
		else
		{
			char tempbuff[4];
			sprintf_s(tempbuff, "%%%X%X", ((BYTE)tt.at(i)) >> 4, ((BYTE)tt.at(i)) % 16);
			dd.append(tempbuff);
		}

	}
	return dd;
}

void GB2312ToUTF_8(string& out, char *input, int len)
{
	char buf[4];
	memset(buf, 0, 4);
	out.clear();
	int i = 0;
	while (i < len)
	{
		//如果是英文直接复制就可以
		if (input[i] >= 0)
		{
			char asciistr[2] = { 0 };
			asciistr[0] = (input[i++]);
			out.append(asciistr);
		}
		else
		{
			WCHAR pbuffer;
			Gb2312ToUnicode(&pbuffer, input + i);
			UnicodeToUTF_8(buf, &pbuffer);
			out.append(buf);
			i += 2;
		}
	}

	return;
}

//汉字由Gb2312编码（2字节）转为Unicode码点（2字节）
void Gb2312ToUnicode(WCHAR* pOut, char *gbBuffer)
{
	//gb2312每个汉字占2字节
	::MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, gbBuffer, 2, pOut, 1);
	return;
}
//汉字由UTF-8编码（三字节）转为unicode码点（2字节）
void UTF_8ToUnicode(WCHAR* out, char *input)
{
	//input-8汉字占三个字节，扩展B区以后的汉字占四个字节
	//三字节input-8符号的编码格式为1110xxxx 10xxxxxx 10xxxxxx
	//注意 WCHAR高低字的顺序,低字节在前，高字节在后
	char* uchar = (char *)out;
	uchar[1] = ((input[0] & 0x0F) << 4) + ((input[1] >> 2) & 0x0F);
	uchar[0] = ((input[1] & 0x03) << 6) + (input[2] & 0x3F);
	return;
}
//汉字由unicode码点（2字节）转为UTF-8编码（三字节）
void  UnicodeToUTF_8(char* out, WCHAR* input)
{
	char* pchar = (char *)input;
	out[0] = (0xE0 | ((pchar[1] & 0xF0) >> 4));
	out[1] = (0x80 | ((pchar[1] & 0x0F) << 2)) + ((pchar[0] & 0xC0) >> 6);
	out[2] = (0x80 | (pchar[0] & 0x3F));
	return;
}

int main()
{
	char str[] = "http://10.210.63.11:7776/videos/焰灵姬.mp4";
	string utf8Code = "";
	utf8Code = UrlUTF8(str);
	char code[1024];
	strcpy_s(code, utf8Code.c_str());
	for (int i = 0; i<utf8Code.length(); i++)
	{
		printf("%c", code[i]);
	}
	return 0;
}
