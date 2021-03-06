// TortoiseSVN - a Windows shell extension for easy version control

// Copyright (C) 2003-2005 - Stefan Kueng

// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
#pragma once

#include "Balloon.h"

// CToolAssocDlg dialog

class CToolAssocDlg : public CDialog
{
	DECLARE_DYNAMIC(CToolAssocDlg)

public:
	CToolAssocDlg(const CString& type, bool add, CWnd* pParent = NULL);
	virtual ~CToolAssocDlg();

// Dialog Data
	enum { IDD = IDD_TOOLASSOC };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual BOOL OnInitDialog();
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	afx_msg void OnBnClickedToolbrowse();

	DECLARE_MESSAGE_MAP()

public:
	CString     m_sType;
	bool        m_bAdd;
	CString		m_sExtension;
	CString		m_sTool;
	CBalloon	m_tooltips;
};

