// SMSDraft.cpp : implementation file
//

#include "stdafx.h"
#include "e9101main.h"
#include "DlgAnswerList.h"
#include "../DlgConfirm.h"
#include "DlgQuestionList.h"
#include "../E9101MainDlg.h"

#include "../../GSM/diaodu_data.h"
#include "../../ExTool/ex_basics.h"
#include "../../MutiLanguage/CGDICommon.h"

#include "../../SQLite3/CppSQLite3.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

//�Ƿ����µ�������Ϣ
BOOL CDlgQuestionList::m_bSMSCenter = FALSE;

CDlgQuestionList::CDlgQuestionList(BOOL bIsModelDlg /*= TRUE*/, CWnd* pParent /*=NULL*/)
	: CDialogBase(CDlgQuestionList::IDD, pParent)
{
	m_nPageIndex	= 0;
	m_nPageCount	= 1;
	m_nRecordCount	= 0;

	m_bIsModelDlg	= bIsModelDlg;	//��ǰ�����Ƿ�Ϊģ̬����
}

void CDlgQuestionList::DoDataExchange(CDataExchange* pDX)
{
	CDialogBase::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CMediaMain)
		// NOTE: the ClassWizard will add DDX and DDV calls here
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CDlgQuestionList, CDialogBase)
	//{{AFX_MSG_MAP(CMediaMain)
	ON_WM_PAINT()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

BOOL CDlgQuestionList::OnInitDialog() 
{
	CDialogBase::OnInitDialog();

	InitGdi();
	InitControl();
	
	OnLanguageChange();
	OnSkinChange();

	ShowPage( m_nPageIndex );

	CSysConfig::Instance().SetLastDialogID(1);

	//ChangeReadStatus();
	//UpdateData();

	return TRUE;
}
//�任���Ժ͵�λ
BOOL CDlgQuestionList::OnLanguageChange()//��Ƥ��
{
	CResString str;

	m_csTitile = _T("�����б�");
	
	m_Item[0].chChar = _T("");
	m_Item[1].chChar = _T("");
	m_Item[2].chChar = _T("");
	m_Item[3].chChar = _T("");

	return TRUE;
}

BOOL CDlgQuestionList::OnSkinChange()
{
	return TRUE;
}

void CDlgQuestionList::InitGdi()
{
	CRect rc;
	HDC hDC;
	hDC = ::GetDC(m_hWnd);
	m_pDC = CDC::FromHandle(hDC);
	GetClientRect(rc);
	
	memDC.CreateCompatibleDC(m_pDC);
	bitmap.CreateCompatibleBitmap(m_pDC,rc.Width(),rc.Height());

	bk_normal_dc.CreateCompatibleDC(m_pDC);
	bk_press_dc.CreateCompatibleDC(m_pDC);
	
	PSKINBMP_ST  pSkin  = g_pResMng->RequestBmp(BG_LIST_N,true);
	m_stBtnNormalBMP   = pSkin->hBitmap;
	
	pSkin = g_pResMng->RequestBmp(BG_LIST_P, true);
	m_stBtnPressBMP = pSkin->hBitmap; 

	m_pold_bk_normal_bmp = bk_normal_dc.SelectObject(CBitmap::FromHandle(m_stBtnNormalBMP));
	m_pold_bk_press_bmp  = bk_press_dc.SelectObject(CBitmap::FromHandle(m_stBtnPressBMP));
	
	::ReleaseDC(m_hWnd, hDC);
}

void CDlgQuestionList::ReleaseGdi()
{
	bk_press_dc.SelectObject(m_pold_bk_press_bmp);
	bk_normal_dc.SelectObject(m_pold_bk_normal_bmp);
	
	DeleteObject( m_stBtnPressBMP );
	DeleteObject( m_stBtnNormalBMP );
	
	bk_press_dc.DeleteDC();		
	bk_normal_dc.DeleteDC();
	
	bitmap.DeleteObject();
	memDC.DeleteDC();
}

void CDlgQuestionList::OnExit()
{
	ReleaseGdi();
	//�˳�ʱ,�����ļ���������ڴ�����
	//CSMSCenter_data::Instance()->save_data(true);

	if(m_bIsModelDlg)
		CDialogBase::EndDialog(0);		//ģ̬�Ի���
	else
		CDialogBase::DestroyWindow();	//��ģ̬�Ի���

	CSysConfig::Instance().SetLastDialogID(0);
}

void CDlgQuestionList::InitControl()
{
	int i;
 	for ( i =0;  i < BTN_COUNT; i++)
 	{
 		m_Item[i].nState = BTN_STATE_NORMAL;
 	}

	for ( i =0;  i < elist_count; i++)
 	{
		m_list[i].nState = BTN_STATE_NORMAL;
 	}

	//���
	m_Item[0].rect.left		= 368;
	m_Item[0].rect.top		= 396;
	m_Item[0].rect.right	= 512;
	m_Item[0].rect.bottom	= 480;
	//��һҳ
	m_Item[1].rect.left		= 514;
	m_Item[1].rect.top		= 396;
	m_Item[1].rect.right	= 655;
	m_Item[1].rect.bottom	= 480;
	//��һҳ
	m_Item[2].rect.left		= 657;
	m_Item[2].rect.top		= 396;
	m_Item[2].rect.right	= 800;
	m_Item[2].rect.bottom	= 480;
	//Exit
	m_Item[3].rect.left		= 720;
	m_Item[3].rect.top		= 0;
	m_Item[3].rect.right	= 800;
	m_Item[3].rect.bottom	= 60;

	//�����б�
	for(i=0; i<elist_count; i++)
	{
		m_list[i].rect.left		= 30-5;
		m_list[i].rect.top		= 86+52*i-5;
		m_list[i].rect.right	= 769+5;
		m_list[i].rect.bottom	= 85+52*i+40+5;
	}
}

void CDlgQuestionList::DrawItems(CDC* pDC,const CRect &rt, enBtnState state)
{
	ASSERT(pDC);
	int x = 0;
	int y = 0;
	switch(state)
	{
	case BTN_STATE_NORMAL:
		pDC->BitBlt(rt.left,rt.top,rt.Width(),rt.Height(),&bk_normal_dc,rt.left,rt.top,SRCCOPY);
		break;
	case BTN_STATE_DOWN:
		pDC->BitBlt(rt.left,rt.top,rt.Width(),rt.Height(),&bk_press_dc,rt.left,rt.top,SRCCOPY);
		break;
	default:
		pDC->BitBlt(rt.left,rt.top,rt.Width(),rt.Height(),&bk_normal_dc,rt.left,rt.top,SRCCOPY);
		break;
	}
}

void CDlgQuestionList::DrawBtnText(CDC* pDC,CRect &rt,CString &str,CFont& font,COLORREF col,UINT Format ,BOOL bDisable/* =FALSE */)
{
	COLORREF  nOldRgb;
	nOldRgb=pDC->SetTextColor( col );
	int      nOldMode = pDC->SetBkMode(TRANSPARENT);
	CFont* def_font = pDC->SelectObject(&font);

	pDC->DrawText(str, &rt, Format );

	pDC->SelectObject(def_font);
	pDC->SetBkMode(nOldMode);
	pDC->SetTextColor(nOldRgb);
}

void CDlgQuestionList::OnPaint() 
{
	CPaintDC dc(this);
	CRect rc;
	GetClientRect(rc);
	CBitmap* pOldBmp = memDC.SelectObject(&bitmap);
	memDC.BitBlt(0,0, rc.Width(), rc.Height(), &bk_normal_dc, 0, 0, SRCCOPY);

	CString str;
	CRect Rect;
	enBtnState state;
	int i;

	for(i=0;i<BTN_COUNT;i++)
	{	//���Ʊ���
		str = m_Item[i].chChar;
		Rect = m_Item[i].rect;
		state = m_Item[i].nState;
		DrawItems(&memDC,Rect,state);
	}

	//�����б�
	DrawList(&memDC);

	DrawBtnText(&memDC,g_retTitile,m_csTitile,CGDICommon::Instance()->bigbigfont(),
		RGB(255,255,255),DT_VCENTER|DT_CENTER);

	BOOL b = dc.BitBlt(0, 0, rc.Width(), rc.Height(),&memDC,0,0,SRCCOPY);
	ASSERT(b);

	memDC.SelectObject(pOldBmp);
}

void CDlgQuestionList::DrawList(CDC* pmemDC)
{
	/*
	CString str;
	//�������ҳ��
	int nMaxPage;
	if ( g_pSMSCenter->GetRecordCount() > 0 )
		nMaxPage = ( g_pSMSCenter->GetRecordCount() - 1 ) / elist_count + 1;
	else
		nMaxPage = 1;

	if ( m_nCurPage > nMaxPage ) 
		return;

	int nStartIdx =  ( m_nCurPage - 1 ) * elist_count;
	
	for( int i = 0; i < elist_count; i++ )
	{
		if ( nStartIdx+i >= g_pSMSCenter->GetRecordCount() )
			break;
		st_SMSCenter* pSMS = g_pSMSCenter->GetRecord( nStartIdx + i );
		if ( pSMS )
		{
			DrawItems( pmemDC, m_list[i].rect, m_list[i].nState);
			//����
			str = pSMS->m_Text.m_infoContext;
			str = "  " + str;
			DrawBtnText( pmemDC, m_list[i].rect, str, 
				CGDICommon::Instance()->smallfont(), RGB(255,255,255),DT_VCENTER|DT_LEFT);
		}
	}*/
	int i = 0;
	CString str;
	for( i = 0; i < elist_count; i++ )
	{
		str = _T("  ") + m_list[i].chChar;
		DrawBtnText( pmemDC, m_list[i].rect, str, 
			CGDICommon::Instance()->smallfont(), RGB(255,255,255),DT_VCENTER|DT_LEFT);
	}
}

void CDlgQuestionList::OnLButtonDown(UINT nFlags, CPoint point) 
{
	int i = 0;
	for( i = 0; i < BTN_COUNT; i++ )
	{
		if(PtInRect(&m_Item[i].rect, point))
		{
			if(m_Item[i].nState == BTN_STATE_NORMAL)
			{
				m_Item[i].nState = BTN_STATE_DOWN;
				PlayKeySound();
			}
		}
	}

	for( i = 0; i < elist_count; i++ )
	{
		if(PtInRect(&m_list[i].rect,point))
		{
			if(m_list[i].nState == BTN_STATE_NORMAL)
			{
				m_list[i].nState = BTN_STATE_DOWN;
				PlayKeySound();
			}
		}
	}
	
	Invalidate();
	SetCapture();
	
	CDialogBase::OnLButtonDown(nFlags, point);
}

void CDlgQuestionList::OnLButtonUp(UINT nFlags, CPoint point) 
{
	int i=0;
	for( i=0; i<BTN_COUNT; i++)
	{
		if(m_Item[i].nState == BTN_STATE_DOWN )
		{
			m_Item[i].nState = BTN_STATE_NORMAL;
			switch( i )
			{
			case 0:	OnBtnDelAll();break;
			case 1:	OnBtnPageUp();break;
			case 2:	OnBtnPageDown();break;
			case 3: OnExit();break;
			default:
				break;
			}
		}
	}

	for(i=0;i<elist_count;i++)
	{
		if(m_list[i].nState == BTN_STATE_DOWN )
		{
			m_list[i].nState = BTN_STATE_NORMAL;
			UserClickItem( i );
		}
	}

	Invalidate();
	ReleaseCapture();
	CDialogBase::OnLButtonUp(nFlags, point);
}

void  CDlgQuestionList::OnBtnPageUp()
{
	if(m_nPageIndex <= 0)	//�ѵ����һҳ
	{
		m_nPageIndex = 0;
		return;
	}
	m_nPageIndex--;
	ShowPage( m_nPageIndex );
	Invalidate();
}

void  CDlgQuestionList::OnBtnPageDown()
{
	if(m_nPageIndex >= m_nPageCount-1)//�ѵ������һҳ
	{
		m_nPageIndex = m_nPageCount-1;
		return;
	}
	m_nPageIndex++;
	
	ShowPage( m_nPageIndex );
	Invalidate();
}

void  CDlgQuestionList::OnBtnDelAll()
{
	CResString str;
	str.LoadString( RES_BUTTON_DELETE_ALL );
	
	CDlgConfirm::m_stTitle = str;
	CDlgConfirm dlg;
	dlg.DoModal();
	
	if( CDlgConfirm::s_bOk )
	{
		CppSQLite3DB db;
		db.open(PATH_SQLITE_DB_808);
		db.execDML( "DELETE FROM question;" );
		db.close();

		m_nPageIndex	= 0;
		m_nPageCount	= 1;
		m_nRecordCount	= 0;

		ShowPage( m_nPageIndex );
	}
}

void    CDlgQuestionList::UserClickItem(int nItem )
{
// 	int  nSleIdx = nItem + m_nPageIndex * elist_count;
// 	if ( nSleIdx >= m_nRecordCount )
// 		return;

	int mID = m_nAskID[nItem];
	CDlgAnswerList dlg(mID);
	dlg.DoModal();

	ShowPage( m_nPageIndex );
	Invalidate();
}

void    CDlgQuestionList::ShowPage(int nPageIndex)
{
	CString str;
	int i = 0;
	int nStartIndex = 0;
	int nOffset = 0;

	CppSQLite3DB db;
	db.open(PATH_SQLITE_DB_808);	//�����ݿ�
	
	//��ѯ��¼������
	m_nRecordCount = db.execScalar("SELECT count(*) FROM question;");
	//������ҳ��
	if(m_nRecordCount > 0)
		m_nPageCount = (m_nRecordCount-1)/elist_count+1;
	else
		m_nPageCount = 1;

	//�����ݿ��в�ѯ��nPageIndexҳ��elist_count������
	char szSqlBuffer[512];
	sprintf(szSqlBuffer, "SELECT * FROM question ORDER BY question_datatime DESC LIMIT %d, %d;", nPageIndex*elist_count, elist_count);
	CppSQLite3Query q = db.execQuery(szSqlBuffer);

	for( i = 0; i < elist_count; i++ )
	{
		if ( !q.eof() )	//������
		{
			m_nAskID[i]			= q.getIntField("UID");
			m_ItemState[i]		= q.getIntField("flag");
			m_list[i].chChar	= q.fieldValue("question_content");
			m_list[i].nState	= BTN_STATE_NORMAL;
			q.nextRow();
		}
		else			//�հ���
		{
			m_ItemState[i]		= 0;
			m_list[i].chChar	= _T("");
			m_list[i].nState	= BTN_STATE_DISABLE;
		}
	}
	//�ͷ�statement
	q.finalize();

	db.close();	//�ر����ݿ�
	return;
}

LRESULT CDlgQuestionList::DefWindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
	if(WM_REFRESH_PAGE == message)
	{
		m_nPageIndex = 0;
		ShowPage( m_nPageIndex );
		Invalidate();
	}
	return CDialogBase::DefWindowProc(message, wParam, lParam);
}

void CDlgQuestionList::ChangeReadStatus()
{
	CppSQLite3DB db;
	db.open(PATH_SQLITE_DB_808);	//�����ݿ�
	int nUnReadSMS = 0;	//δ����������
	const char* pszSQL;
	//��ѯ������Ϣδ������������
	pszSQL = "select count(*) from question where read_status = 0;";
	nUnReadSMS = db.execScalar(pszSQL);
	if(nUnReadSMS > 0)
	{
		//����״̬:δ��->�Ѷ�
		pszSQL = "update question set read_status = 1;";
        db.execDML(pszSQL);
	}
	db.close();
	m_bSMSCenter = FALSE;//������Ϣ��ʾ�ر�
}

void CDlgQuestionList::PostNcDestroy()
{
	CDialogBase::PostNcDestroy();

	if( !m_bIsModelDlg )	//��ģ̬�Ի����²���ɾ���Ի������
		delete this;
}