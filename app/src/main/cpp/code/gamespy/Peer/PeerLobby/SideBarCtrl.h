/* Third-party component; the applicable licence is recorded in THIRDPARTY.md. */
#if !defined(AFX_SIDEBARCTRL_H__CC0DCA41_C330_11D5_A480_000102C2601F__INCLUDED_)
#define AFX_SIDEBARCTRL_H__CC0DCA41_C330_11D5_A480_000102C2601F__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif 


class CSideBarCtrl : public CStatic
{
public:
	CSideBarCtrl();

public:

public:


public:
	virtual ~CSideBarCtrl();

protected:
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnPaint();

	void DrawGradient(CDC* pDC, const CRect &rcGrad, COLORREF clrStart, COLORREF clrEnd, BOOL bVertical, COLORREF clrDither = 0xFFFFFFFF );

	DECLARE_MESSAGE_MAP()
};



#endif 
