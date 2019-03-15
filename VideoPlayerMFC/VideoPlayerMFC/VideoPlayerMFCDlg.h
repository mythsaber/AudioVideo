﻿
// VideoPlayerMFCDlg.h : 头文件
//

#pragma once
#include "afxwin.h"
#include<atomic>

// CVideoPlayerMFCDlg 对话框
class CVideoPlayerMFCDlg : public CDialogEx
{
// 构造
public:
	CVideoPlayerMFCDlg(CWnd* pParent = NULL);	// 标准构造函数
// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_VIDEOPLAYERMFC_DIALOG };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV 支持


// 实现
protected:
	HICON m_hIcon;

	// 生成的消息映射函数
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
	// 地址栏
	CEdit m_url;
	std::atomic<bool> videoon = false;
	//字体
	CFont m_Font;
public:
	afx_msg void OnStnClickedScreen();
	afx_msg void OnEnChangeUrl();
};
