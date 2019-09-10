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
		// isalnum�ж��ַ��Ƿ���������Ӣ��
		// ispunct�ж�һ���ַ��Ƿ�Ϊ�����Ż������ַ�
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
		//�����Ӣ��ֱ�Ӹ��ƾͿ���
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

//������Gb2312���루2�ֽڣ�תΪUnicode��㣨2�ֽڣ�
void Gb2312ToUnicode(WCHAR* pOut, char *gbBuffer)
{
	//gb2312ÿ������ռ2�ֽ�
	::MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, gbBuffer, 2, pOut, 1);
	return;
}
//������UTF-8���루���ֽڣ�תΪunicode��㣨2�ֽڣ�
void UTF_8ToUnicode(WCHAR* out, char *input)
{
	//input-8����ռ�����ֽڣ���չB���Ժ�ĺ���ռ�ĸ��ֽ�
	//���ֽ�input-8���ŵı����ʽΪ1110xxxx 10xxxxxx 10xxxxxx
	//ע�� WCHAR�ߵ��ֵ�˳��,���ֽ���ǰ�����ֽ��ں�
	char* uchar = (char *)out;
	uchar[1] = ((input[0] & 0x0F) << 4) + ((input[1] >> 2) & 0x0F);
	uchar[0] = ((input[1] & 0x03) << 6) + (input[2] & 0x3F);
	return;
}
//������unicode��㣨2�ֽڣ�תΪUTF-8���루���ֽڣ�
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
	char str[] = "http://10.210.63.11:7776/videos/���鼧.mp4";
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
