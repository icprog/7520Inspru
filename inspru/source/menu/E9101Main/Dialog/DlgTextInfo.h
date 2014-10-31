#if !defined(AFX_SMSINFO_H__2E0FSDFWEFW_AFSDVXCV30__INCLUDED_)
#define AFX_SMSINFO_H__2E0FSDFWEFW_AFSDVXCV30__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "DialogBase.h"
#include "BtnOfMainMenu.h"
#include "MaskCtrl.h"
#include "../../GSM/diaodu_data.h"
//#include "../FileOperator/CRecordSMS.cpp"
#include "../../SQLite3/CppSQLite3.h"

//extern CRecordSMS<>*		g_pSMSCenter;

class CDlgTextInfo : public CDialogBase
{
public:
	//�任���Ժ͵�λ
	BOOL OnLanguageChange();
	//��Ƥ��
	BOOL OnSkinChange();
	
	void OnExit();
	void ReleaseGdi();
	
public:
	CDlgTextInfo(int nIndexCurr, CWnd* pParent = NULL);   // standard constructor
	
	// Dialog Data
	//{{AFX_DATA(CMediaMain)
	enum { IDD = IDD_SMS_INFO };
	// NOTE: the ClassWizard will add data members here
	//}}AFX_DATA
	static BOOL		m_bIsOpen;		//��ǰ�����Ƿ񱻴�
	static UINT		WM_DlgTextInfo_Refresh;
	
	// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CMediaMain)
protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL
	
	// Implementation
	int			m_nRecordIndex;				//��ǰ��¼�����ݿ��е�index��0,1,2,...m_nRecordCount-1
	int			m_nRecordCount;				//���ݿ��м�¼����
	//CppSQLite3Query m_QuerySet;				//��ǰrecord
	//char		m_szSMS_ID[64];
	int			m_nSMS_ID;		//��ǰ������ϢID��
	void		ShowPage(int nRecordIndex);
protected:
	CSMSCenter_data*  m_pData;
	int          m_nSelIdx;
	ItemInfo    m_Item[6];
	ItemInfo    m_Static[3];
	HBITMAP     m_stBtnNormalBMP;
	HBITMAP     m_stBtnPressBMP;
	HBITMAP     m_stBtnDisableBMP;	
	
	CDC*  m_pDC;
	CDC   memDC;
	CBitmap  bitmap;
	CFont  smallfont;
	CFont bigfont;
	
	CString m_csTitile;
	
	CDC bk_normal_dc;
	CDC  bk_press_dc;
	CDC  bk_disable_dc;
	
	CBitmap* m_pold_bk_normal_bmp;
	CBitmap*  m_pold_bk_press_bmp;
	CBitmap*  m_pold_bk_disable_bmp;	
	//int		m_nCurrSMSIndex;	//��ǰ������Ϣ��sms_center.sms�ļ��е�index
	
	void InitGdi();
	void InitControl();
	void DrawItems(CDC* pDC,const CRect &rt,enBtnState state);
	void DrawBtnText(CDC* pDC,CRect &rt,CString &str,CFont& font, COLORREF  col = RGB(255,255,255),UINT Format = DT_CENTER | DT_VCENTER,BOOL bDisable=FALSE);
	
protected:
	void  OnBtnPageUp();
	void  OnBtnPageDown();
	void  OnBtnDel();
	void  Reply();
	BOOL  LoadSMSContext(CString& strContext);
	
	// Generated message map functions
	//{{AFX_MSG(CMediaMain)
	virtual BOOL OnInitDialog();
	afx_msg void OnPaint();
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
	virtual LRESULT DefWindowProc(UINT message, WPARAM wParam, LPARAM lParam);
};


//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_SMSINFO_H__2E0FSDFWEFW_AFSDVXCV30__INCLUDED_)