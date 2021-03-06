// OKJDlg.cpp : implementation file
//

#include "stdafx.h"
#include "OKJ.h"
#include "OKJDlg.h"
#include "ceddk.h"
#include "Lib/midware.h"

#pragma comment(lib,"./Lib/midware210.lib")

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

//主窗口名称
#define WNDTITLE_CAMERA_VIDEO				_T("CAMERA_VIDEO_20110411")
#define MSG_COLSE_VIDEO				(WM_USER+0x376)//摄像头退出


COKJDlg::COKJDlg(CWnd* pParent /*=NULL*/)
	: CDialog(COKJDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(COKJDlg)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
	// Note that LoadIcon does not require a subsequent DestroyIcon in Win32
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void COKJDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(COKJDlg)
		// NOTE: the ClassWizard will add DDX and DDV calls here
	DDX_Control(pDX, IDC_STATIC_VIDEO, m_stcVideo);
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(COKJDlg, CDialog)
	//{{AFX_MSG_MAP(COKJDlg)
	ON_BN_CLICKED(IDC_BUTTON1, OnCam1)
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_BN_CLICKED(IDC_STATIC_VIDEO, OnStaticVideo)
	//}}AFX_MSG_MAP
	ON_WM_PAINT()
	ON_WM_TIMER()
END_MESSAGE_MAP()

BOOL COKJDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon
	SetWindowText(WNDTITLE_CAMERA_VIDEO);
	MoveWindow(0,0,800,480);

	
	OnCam1();

	return TRUE;  // return TRUE  unless you set the focus to a control
}


void COKJDlg::OnCam1()
{
	CamPreview(TRUE,CRect(80,0,720,480));
}

void COKJDlg::OnClose() 
{
	CamPreview(FALSE,NULL);
	CDialog::OnClose();
}


void COKJDlg::OnLButtonDown(UINT nFlags, CPoint point) 
{
	CDialog::OnLButtonDown(nFlags, point);
}

void COKJDlg::OnLButtonUp(UINT nFlags, CPoint point) 
{
	OnExit();
	CDialog::OnLButtonUp(nFlags, point);
}


void COKJDlg::OnStaticVideo() 
{
}

void COKJDlg::OnExit()
{
	OnClose();
	Sleep(100);
	EndDialog(0);
}


void COKJDlg::OnPaint()
{
	CPaintDC dc(this);
	CRect   rect;
	GetClientRect(rect);
	dc.FillSolidRect(rect,RGB(0,0,0));   //设置为绿色背景
}

LRESULT COKJDlg::DefWindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
	if (MSG_COLSE_VIDEO == message)
	{
		KillTimer(1005);
		SetTimer(1005,1000,NULL);
	}
//	else if (WM_COPYDATA == message)
//	{
//		//TRACE(szdata);
//		OnCmdCOPYDATA(wParam,lParam);
//	}
	
	return CDialog::DefWindowProc(message, wParam, lParam);
}

void COKJDlg::OnCmdCOPYDATA(WPARAM wParam, LPARAM lParam )
{
//	if (wParam == 11)
//	{
//		COPYDATASTRUCT* pcds = (COPYDATASTRUCT*)(lParam);
//
//		char szdata[1024];
//		memset(szdata, 0, sizeof(szdata));
//		memcpy(szdata, pcds->lpData,pcds->cbData);
//
//		for (int i = 0; i < 1024; i++)
//		{
//			TRACE1("%0x", (UCHAR)szdata[i]);
//		}
//	}
}

void COKJDlg::OnTimer(UINT_PTR nIDEvent)
{
	// TODO: Add your message handler code here and/or call default
	if (1005 == nIDEvent)
	{
		KillTimer(1005);
		OnExit();
	}

	CDialog::OnTimer(nIDEvent);
}
