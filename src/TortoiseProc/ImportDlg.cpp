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
#include "stdafx.h"
#include "TortoiseProc.h"
#include "ImportDlg.h"
#include "RepositoryBrowser.h"
#include ".\importdlg.h"
#include "DirFileEnum.h"
#include "MessageBox.h"
#include "BrowseFolder.h"
#include "Registry.h"


// CImportDlg dialog

IMPLEMENT_DYNAMIC(CImportDlg, CResizableStandAloneDialog)
CImportDlg::CImportDlg(CWnd* pParent /*=NULL*/)
	: CResizableStandAloneDialog(CImportDlg::IDD, pParent)
{
	m_url = _T("");
}

CImportDlg::~CImportDlg()
{
}

void CImportDlg::DoDataExchange(CDataExchange* pDX)
{
	CResizableStandAloneDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_URLCOMBO, m_URLCombo);
	DDX_Control(pDX, IDC_BROWSE, m_butBrowse);
	DDX_Control(pDX, IDC_MESSAGE, m_cMessage);
	DDX_Control(pDX, IDC_OLDLOGS, m_OldLogs);
}


BEGIN_MESSAGE_MAP(CImportDlg, CResizableStandAloneDialog)
	ON_BN_CLICKED(IDC_BROWSE, OnBnClickedBrowse)
	ON_BN_CLICKED(IDHELP, OnBnClickedHelp)
	ON_CBN_SELCHANGE(IDC_OLDLOGS, OnCbnSelchangeOldlogs)
	ON_CBN_CLOSEUP(IDC_OLDLOGS, OnCbnCloseupOldlogs)
END_MESSAGE_MAP()

BOOL CImportDlg::OnInitDialog()
{
	CResizableStandAloneDialog::OnInitDialog();

	if (m_url.IsEmpty())
	{
		m_URLCombo.SetURLHistory(TRUE);
		m_URLCombo.LoadHistory(_T("Software\\TortoiseSVN\\History\\repoURLS"), _T("url"));
	}
	else
	{
		m_URLCombo.SetWindowText(m_url);
		m_URLCombo.EnableWindow(FALSE);
	}

	m_tooltips.Create(this);
	m_OldLogs.LoadHistory(_T("Software\\TortoiseSVN\\History\\commit"), _T("logmsgs"));
	m_ProjectProperties.ReadProps(m_path);
	m_cMessage.Init(m_ProjectProperties.lProjectLanguage);
	m_cMessage.SetFont((CString)CRegString(_T("Software\\TortoiseSVN\\LogFontName"), _T("Courier New")), (DWORD)CRegDWORD(_T("Software\\TortoiseSVN\\LogFontSize"), 8));

	if (m_ProjectProperties.nLogWidthMarker)
	{
		m_cMessage.Call(SCI_SETWRAPMODE, SC_WRAP_NONE);
		m_cMessage.Call(SCI_SETEDGEMODE, EDGE_LINE);
		m_cMessage.Call(SCI_SETEDGECOLUMN, m_ProjectProperties.nLogWidthMarker);
	}
	else
	{
		m_cMessage.Call(SCI_SETEDGEMODE, EDGE_NONE);
		m_cMessage.Call(SCI_SETWRAPMODE, SC_WRAP_WORD);
	}
	m_cMessage.SetText(m_ProjectProperties.sLogTemplate);

	AddAnchor(IDC_STATIC1, TOP_LEFT, TOP_RIGHT);
	AddAnchor(IDC_STATIC4, TOP_LEFT);
	AddAnchor(IDC_URLCOMBO, TOP_LEFT, TOP_RIGHT);
	AddAnchor(IDC_BROWSE, TOP_RIGHT);
	AddAnchor(IDC_STATIC2, TOP_LEFT, BOTTOM_RIGHT);
	AddAnchor(IDC_MESSAGE, TOP_LEFT, BOTTOM_RIGHT);
	AddAnchor(IDC_CHIST, BOTTOM_LEFT);
	AddAnchor(IDC_OLDLOGS, BOTTOM_LEFT, BOTTOM_RIGHT);
	AddAnchor(IDOK, BOTTOM_RIGHT);
	AddAnchor(IDCANCEL, BOTTOM_RIGHT);
	AddAnchor(IDHELP, BOTTOM_RIGHT);

	if (hWndExplorer)
		CenterWindow(CWnd::FromHandle(hWndExplorer));
	EnableSaveRestore(_T("ImportDlg"));
	return TRUE;  // return TRUE  unless you set the focus to a control
}

void CImportDlg::OnOK()
{
	if (m_URLCombo.IsWindowEnabled())
	{
		m_URLCombo.SaveHistory();
		m_url = m_URLCombo.GetString();
		UpdateData();
	}

	if (m_url.Left(7).CompareNoCase(_T("file://"))==0)
	{
		//check if the url is on a network share
		CString temp = m_url.Mid(7);
		temp = temp.TrimLeft('/');
		temp.Replace('/', '\\');
		temp = temp.Left(3);
		if (GetDriveType(temp)==DRIVE_REMOTE)
		{
			if (SVN::IsBDBRepository(m_url))
				if (CMessageBox::Show(this->m_hWnd, IDS_WARN_SHAREFILEACCESS, IDS_APPNAME, MB_ICONWARNING | MB_YESNO)==IDNO)
					return;
		}
	}
	UpdateData();
	m_sMessage = m_cMessage.GetText();
	m_OldLogs.AddString(m_sMessage, 0);
	m_OldLogs.SaveHistory();

	CResizableStandAloneDialog::OnOK();
}

void CImportDlg::OnBnClickedBrowse()
{
	CString strUrl;
	m_URLCombo.GetWindowText(strUrl);
	if (strUrl.Left(7) == _T("file://"))
	{
		CString strFile(strUrl);
		SVN::UrlToPath(strFile);

		SVN svn;
		if (svn.IsRepository(strFile))
		{
			// browse repository - show repository browser
			CRepositoryBrowser browser(strUrl, this);
			if (browser.DoModal() == IDOK)
			{
				m_URLCombo.SetWindowText(browser.GetPath());
			}
		}
		else
		{
			// browse local directories
			CBrowseFolder folderBrowser;
			folderBrowser.m_style = BIF_EDITBOX | BIF_NEWDIALOGSTYLE | BIF_RETURNFSANCESTORS | BIF_RETURNONLYFSDIRS;
			if (folderBrowser.Show(GetSafeHwnd(), strUrl) == CBrowseFolder::OK)
			{
				SVN::PathToUrl(strUrl);

				m_URLCombo.SetWindowText(strUrl);
			}
		}
	}
	else if ((strUrl.Left(7) == _T("http://")
		||(strUrl.Left(8) == _T("https://"))
		||(strUrl.Left(6) == _T("svn://"))
		||(strUrl.Left(10) == _T("svn+ssh://"))) && strUrl.GetLength() > 6)
	{
		// browse repository - show repository browser
		CRepositoryBrowser browser(strUrl, this);
		if (browser.DoModal() == IDOK)
		{
			m_URLCombo.SetWindowText(browser.GetPath());
		}
	}
	else
	{
		// browse local directories
		CBrowseFolder folderBrowser;
		folderBrowser.m_style = BIF_EDITBOX | BIF_NEWDIALOGSTYLE | BIF_RETURNFSANCESTORS | BIF_RETURNONLYFSDIRS;
		if (folderBrowser.Show(GetSafeHwnd(), strUrl) == CBrowseFolder::OK)
		{
			SVN::PathToUrl(strUrl);

			m_URLCombo.SetWindowText(strUrl);
		}
	}
}

BOOL CImportDlg::PreTranslateMessage(MSG* pMsg)
{
	m_tooltips.RelayEvent(pMsg);
	if (pMsg->message == WM_KEYDOWN)
	{
		switch (pMsg->wParam)
		{
		case VK_RETURN:
			{
				if (GetAsyncKeyState(VK_CONTROL)&0x8000)
				{
					PostMessage(WM_COMMAND, IDOK);
				}
			}
			break;
				}
			}
	return CResizableStandAloneDialog::PreTranslateMessage(pMsg);
}

void CImportDlg::OnBnClickedHelp()
{
	OnHelp();
}

void CImportDlg::OnCbnSelchangeOldlogs()
{
	m_cMessage.SetText(m_OldLogs.GetString());
}

void CImportDlg::OnCbnCloseupOldlogs()
{
	m_cMessage.SetText(m_OldLogs.GetString());
}

void CImportDlg::OnCancel()
{
	UpdateData();
	m_OldLogs.AddString(m_cMessage.GetText(), 0);
	m_OldLogs.SaveHistory();
	CResizableStandAloneDialog::OnCancel();
}
