
#if !defined(AFX_CHILDFRM_H__CECBEADE_E41E_4FB6_93DD_C3735CFC4679__INCLUDED_)
#define AFX_CHILDFRM_H__CECBEADE_E41E_4FB6_93DD_C3735CFC4679__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif 


class CChildFrame : public CMDIChildWnd
{
	DECLARE_DYNCREATE(CChildFrame)
public:
	CChildFrame();

public:

public:

	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);

public:
	virtual ~CChildFrame();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

protected:
	DECLARE_MESSAGE_MAP()
};



#endif 
