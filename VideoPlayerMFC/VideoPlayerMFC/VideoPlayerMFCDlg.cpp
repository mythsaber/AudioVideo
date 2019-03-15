
// VideoPlayerMFCDlg.cpp : ʵ���ļ�
//

#include "stdafx.h"
#include "VideoPlayerMFC.h"
#include "VideoPlayerMFCDlg.h"
#include "afxdialogex.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#include "player_FFSDLMFC_OOP.h"

// ����Ӧ�ó��򡰹��ڡ��˵���� CAboutDlg �Ի���

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

// �Ի�������
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ABOUTBOX };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV ֧��

// ʵ��
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


// CVideoPlayerMFCDlg �Ի���



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


// CVideoPlayerMFCDlg ��Ϣ�������

BOOL CVideoPlayerMFCDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// ��������...���˵�����ӵ�ϵͳ�˵��С�

	// IDM_ABOUTBOX ������ϵͳ���Χ�ڡ�
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

	// ���ô˶Ի����ͼ�ꡣ  ��Ӧ�ó��������ڲ��ǶԻ���ʱ����ܽ��Զ�
	//  ִ�д˲���
	SetIcon(m_hIcon, TRUE);			// ���ô�ͼ��
	SetIcon(m_hIcon, FALSE);		// ����Сͼ��

	//�洢��ַ����ַ��Ӳ���ļ�
	std::ifstream ifs("My_CVideoPlayerMFCDlg_Config.txt");
	if (!ifs) {
		AfxMessageBox(L"���ܴ������ļ�");
		return false;
	}
	string str;
	std::getline(ifs,str);
	CString cstr_path;
	cstr_path.Format(_T("%s"), CStringW(str.c_str()));
	m_url.SetWindowTextW(cstr_path);

	//�޸ĵ�ַ�������С
	CFont* src_font = m_url.GetFont();
	LOGFONT dst;
	src_font->GetLogFont(&dst);
	dst.lfHeight = 20;
	m_Font.CreateFontIndirectW(&dst);
	m_url.SetFont(&m_Font);

	return TRUE;  // ���ǽ��������õ��ؼ������򷵻� TRUE
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
			AfxMessageBox(L"������ȷ���������ļ�");
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

// �����Ի��������С����ť������Ҫ����Ĵ���
//  �����Ƹ�ͼ�ꡣ  ����ʹ���ĵ�/��ͼģ�͵� MFC Ӧ�ó���
//  �⽫�ɿ���Զ���ɡ�

void CVideoPlayerMFCDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // ���ڻ��Ƶ��豸������

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// ʹͼ���ڹ����������о���
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// ����ͼ��
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

//���û��϶���С������ʱϵͳ���ô˺���ȡ�ù��
//��ʾ��
HCURSOR CVideoPlayerMFCDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}


void CVideoPlayerMFCDlg::OnBnClickedPlay()
{
	//���������Ƶ�ڲ��ţ��򲻴���
	if ((bool)videoon == true) return;

	videoon = true;
	m_url.SetReadOnly(true);

	CString src_path;
	m_url.GetWindowTextW(src_path);

	string path = CW2A(src_path.GetString());
	std::ifstream ifs(path);
	if (!ifs) 
	{ 
		AfxMessageBox(L"�ļ����ܴ�"); 
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
		//����¼������е�������Ϣ
		SDL_Event event;
		while (SDL_PollEvent(&event));

		DecoderPlayer dp(path,dlg);
		dp.decode_play();

		m_url.SetReadOnly(false);
		videoon = false;
	};

	thread t(lam,path,this); 
	//��������һ�����̲߳��ţ�����ֱ���ڵ�ǰ�̲߳��� 
	t.detach();
}


void CVideoPlayerMFCDlg::OnBnClickedPause()
{
	if ((bool)videoon == false) return;

	// ���¿ո��
	SDL_Event event;
	event.type = SDL_KEYDOWN;
	event.key.keysym.sym = ' ';
	SDL_PushEvent(&event); //���¼�����������¼�

	//̧��ո��
	event.type = SDL_KEYUP;
	SDL_PushEvent(&event);
}


void CVideoPlayerMFCDlg::OnBnClickedStop()
{
	if (videoon == false) return; //atom��ת��ΪT���͵ĺ���

	SDL_Event event;
	event.type = SDL_QUIT;
	SDL_PushEvent(&event);
}


void CVideoPlayerMFCDlg::OnBnClickedAbout()
{
	// TODO: �ڴ���ӿؼ�֪ͨ����������
	CAboutDlg dlg;
	dlg.DoModal(); 
	//"Modal"��ʾCAboutDlg��һ��ģʽ�Ի���
	//�������󣬴�������λ�ò����ڽ�����Ӧ��
	//����ȷ��������󣬲ſ����ٵ����������
	//λ��
}


void CVideoPlayerMFCDlg::OnBnClickedBroswer()
{
	if ((bool)videoon == true) return;

	CString path;
	//TRUEΪopen�Ի���FALSEΪsave as�Ի���
	CFileDialog dlg(TRUE);
	if (dlg.DoModal() == IDOK)
	{
		path = dlg.GetPathName();
		m_url.SetWindowTextW(path);
	}
}


void CVideoPlayerMFCDlg::OnStnClickedScreen()
{
	// TODO: �ڴ���ӿؼ�֪ͨ����������
}


void CVideoPlayerMFCDlg::OnEnChangeUrl()
{
	// TODO:  ����ÿؼ��� RICHEDIT �ؼ���������
	// ���ʹ�֪ͨ��������д CDialogEx::OnInitDialog()
	// ���������� CRichEditCtrl().SetEventMask()��
	// ͬʱ�� ENM_CHANGE ��־�������㵽�����С�

	// TODO:  �ڴ���ӿؼ�֪ͨ����������
}
