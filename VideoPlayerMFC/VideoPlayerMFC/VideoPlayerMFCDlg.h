
// VideoPlayerMFCDlg.h : ͷ�ļ�
//

#pragma once
#include "afxwin.h"
#include<atomic>

// CVideoPlayerMFCDlg �Ի���
class CVideoPlayerMFCDlg : public CDialogEx
{
// ����
public:
	CVideoPlayerMFCDlg(CWnd* pParent = NULL);	// ��׼���캯��
// �Ի�������
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_VIDEOPLAYERMFC_DIALOG };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV ֧��


// ʵ��
protected:
	HICON m_hIcon;

	// ���ɵ���Ϣӳ�亯��
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnBnClickedPlay();
	afx_msg void OnBnClickedPause();
	afx_msg void OnBnClickedStop();
	afx_msg void OnBnClickedAbout();
	afx_msg void OnBnClickedBroswer();

private:
	// ��ַ��
	CEdit m_url;
	std::atomic<bool> videoon = false;
	//����
	CFont m_Font;
public:
	afx_msg void OnStnClickedScreen();
	afx_msg void OnEnChangeUrl();
};
