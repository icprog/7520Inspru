#if !defined(AFX_GSMNETSET_H__090F2CDA_5ECD_49FA_8F5B_5F1CE7809872__INCLUDED_)
#define AFX_GSMNETSET_H__090F2CDA_5ECD_49FA_8F5B_5F1CE7809872__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// GSMNetSet.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CGSMNetSet dialog

class CGSMNetSet : public CDialogBase
{
public:
	//�任���Ժ͵�λ
	virtual BOOL OnLanguageChange();
	//��Ƥ��
	virtual BOOL OnSkinChange();
	
	enLanguage m_eLan;
	//��¼��������λ��
	LONG m_ListInitPos;
	
	// Construction
	
	void OnExit();
	void ReleaseGdi();
	
	void OnLeft();
	void OnRight();
	
	void OnSkinRight();
	void OnSkinLeft();

	
protected:
	
	ItemInfo    m_Item[7];
	HBITMAP     m_stBtnNormalBMP;
	HBITMAP     m_stBtnPressBMP;
	HBITMAP		m_stBtnDisableBMP;
	
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
	
	void InitGdi();
	void InitControl();
	void DrawItems(CDC* pDC,const CRect &rt,enBtnState state);
	void DrawBtnText(CDC* pDC,CRect &rt,CString &str,CFont& font, COLORREF  col = RGB(255,255,255),UINT Format = DT_CENTER | DT_VCENTER,BOOL bDisable=FALSE);
	
	
public:
	CGSMNetSet(CWnd* pParent = NULL);   // standard constructor
	
	// Dialog Data
	//{{AFX_DATA(CGSMNetSet)
	enum { IDD = IDD_GSM_NET };
	CExButton	m_BtnCancel;
	CExButton	m_BtnF;
	CExButton	m_BtnB;
	//}}AFX_DATA
	
	
	
	// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CGSMNetSet)
protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual LRESULT DefWindowProc(UINT message, WPARAM wParam, LPARAM lParam);
	//}}AFX_VIRTUAL
	
	// Implementation
protected:
	
	// Generated message map functions
	//{{AFX_MSG(CGSMNetSet)
	virtual BOOL OnInitDialog();
	afx_msg void OnBtnCancel();
	afx_msg void OnPaint();
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_GSMNETSET_H__090F2CDA_5ECD_49FA_8F5B_5F1CE7809872__INCLUDED_)
