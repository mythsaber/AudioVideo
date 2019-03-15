
// VideoPlayerMFCDlg.cpp : 实现文件
//

#include "stdafx.h"
#include "VideoPlayerMFC.h"
#include "VideoPlayerMFCDlg.h"
#include "afxdialogex.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#include "player_FFSDLMFC_OOP.h"

// 用于应用程序“关于”菜单项的 CAboutDlg 对话框

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ABOUTBOX };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

// 实现
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(IDD_ABOUTBOX)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()


// CVideoPlayerMFCDlg 对话框



CVideoPlayerMFCDlg::CVideoPlayerMFCDlg(CWnd* pParent /*=NULL*/)
	: CDialogEx(IDD_VIDEOPLAYERMFC_DIALOG, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CVideoPlayerMFCDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_URL, m_url);
}

BEGIN_MESSAGE_MAP(CVideoPlayerMFCDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_PLAY, &CVideoPlayerMFCDlg::OnBnClickedPlay)
	ON_BN_CLICKED(IDC_PAUSE, &CVideoPlayerMFCDlg::OnBnClickedPause)
	ON_BN_CLICKED(IDC_STOP, &CVideoPlayerMFCDlg::OnBnClickedStop)
	ON_BN_CLICKED(IDC_ABOUT, &CVideoPlayerMFCDlg::OnBnClickedAbout)
	ON_BN_CLICKED(IDC_BROSWER, &CVideoPlayerMFCDlg::OnBnClickedBroswer)
	ON_STN_CLICKED(IDC_SCREEN, &CVideoPlayerMFCDlg::OnStnClickedScreen)
	ON_EN_CHANGE(IDC_URL, &CVideoPlayerMFCDlg::OnEnChangeUrl)
END_MESSAGE_MAP()


// CVideoPlayerMFCDlg 消息处理程序

BOOL CVideoPlayerMFCDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// 将“关于...”菜单项添加到系统菜单中。

	// IDM_ABOUTBOX 必须在系统命令范围内。
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != NULL)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// 设置此对话框的图标。  当应用程序主窗口不是对话框时，框架将自动
	//  执行此操作
	SetIcon(m_hIcon, TRUE);			// 设置大图标
	SetIcon(m_hIcon, FALSE);		// 设置小图标

	//存储地址栏地址至硬盘文件
	std::ifstream ifs("My_CVideoPlayerMFCDlg_Config.txt");
	if (!ifs) {
		AfxMessageBox(L"不能打开配置文件");
		return false;
	}
	string str;
	std::getline(ifs,str);
	CString cstr_path;
	cstr_path.Format(_T("%s"), CStringW(str.c_str()));
	m_url.SetWindowTextW(cstr_path);

	//修改地址栏字体大小
	CFont* src_font = m_url.GetFont();
	LOGFONT dst;
	src_font->GetLogFont(&dst);
	dst.lfHeight = 20;
	m_Font.CreateFontIndirectW(&dst);
	m_url.SetFont(&m_Font);

	return TRUE;  // 除非将焦点设置到控件，否则返回 TRUE
}

void CVideoPlayerMFCDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else if (nID == SC_CLOSE)
	{
		std::ofstream ofs("My_CVideoPlayerMFCDlg_Config.txt");
		if (!ofs) {
			AfxMessageBox(L"不能正确保存配置文件");
			return;
		}
		CString src_path;
		m_url.GetWindowTextW(src_path);

		string path = CW2A(src_path.GetString());
		ofs << path;

		CDialogEx::OnSysCommand(nID, lParam);
	}
	else
	{
		CDialogEx::OnSysCommand(nID, lParam);
	}
}

// 如果向对话框添加最小化按钮，则需要下面的代码
//  来绘制该图标。  对于使用文档/视图模型的 MFC 应用程序，
//  这将由框架自动完成。

void CVideoPlayerMFCDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // 用于绘制的设备上下文

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// 使图标在工作区矩形中居中
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// 绘制图标
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

//当用户拖动最小化窗口时系统调用此函数取得光标
//显示。
HCURSOR CVideoPlayerMFCDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}


void CVideoPlayerMFCDlg::OnBnClickedPlay()
{
	//如果已有视频在播放，则不处理
	if ((bool)videoon == true) return;

	videoon = true;
	m_url.SetReadOnly(true);

	CString src_path;
	m_url.GetWindowTextW(src_path);

	string path = CW2A(src_path.GetString());
	std::ifstream ifs(path);
	if (!ifs) 
	{ 
		AfxMessageBox(L"文件不能打开"); 
		videoon = false;
		m_url.SetReadOnly(false);
		return;
	}
	else
	{
		ifs.close();
	}

	auto lam = [this](const string& path, const CVideoPlayerMFCDlg* dlg)
	{
		//清空事件队列中的所有消息
		SDL_Event event;
		while (SDL_PollEvent(&event));

		DecoderPlayer dp(path,dlg);
		dp.decode_play();

		m_url.SetReadOnly(false);
		videoon = false;
	};

	thread t(lam,path,this); 
	//必须启动一个新线程播放，不能直接在当前线程播放 
	t.detach();
}


void CVideoPlayerMFCDlg::OnBnClickedPause()
{
	if ((bool)videoon == false) return;

	// 按下空格键
	SDL_Event event;
	event.type = SDL_KEYDOWN;
	event.key.keysym.sym = ' ';
	SDL_PushEvent(&event); //向事件队列添加新事件

	//抬起空格键
	event.type = SDL_KEYUP;
	SDL_PushEvent(&event);
}


void CVideoPlayerMFCDlg::OnBnClickedStop()
{
	if (videoon == false) return; //atom有转换为T类型的函数

	SDL_Event event;
	event.type = SDL_QUIT;
	SDL_PushEvent(&event);
}


void CVideoPlayerMFCDlg::OnBnClickedAbout()
{
	// TODO: 在此添加控件通知处理程序代码
	CAboutDlg dlg;
	dlg.DoModal(); 
	//"Modal"表示CAboutDlg是一个模式对话框，
	//即弹出后，窗口其他位置不能在接收响应，
	//除非确定、叉掉后，才可以再点击敞口其他
	//位置
}


void CVideoPlayerMFCDlg::OnBnClickedBroswer()
{
	if ((bool)videoon == true) return;

	CString path;
	//TRUE为open对话框，FALSE为save as对话框
	CFileDialog dlg(TRUE);
	if (dlg.DoModal() == IDOK)
	{
		path = dlg.GetPathName();
		m_url.SetWindowTextW(path);
	}
}


void CVideoPlayerMFCDlg::OnStnClickedScreen()
{
	// TODO: 在此添加控件通知处理程序代码
}


void CVideoPlayerMFCDlg::OnEnChangeUrl()
{
	// TODO:  如果该控件是 RICHEDIT 控件，它将不
	// 发送此通知，除非重写 CDialogEx::OnInitDialog()
	// 函数并调用 CRichEditCtrl().SetEventMask()，
	// 同时将 ENM_CHANGE 标志“或”运算到掩码中。

	// TODO:  在此添加控件通知处理程序代码
}
