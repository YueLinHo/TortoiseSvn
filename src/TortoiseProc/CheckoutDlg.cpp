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
#include "CheckoutDlg.h"
#include "RepositoryBrowser.h"
#include "Messagebox.h"
#include "PathUtils.h"
#include "BrowseFolder.h"


// CCheckoutDlg dialog

IMPLEMENT_DYNAMIC(CCheckoutDlg, CStandAloneDialog)
CCheckoutDlg::CCheckoutDlg(CWnd* pParent /*=NULL*/)
	: CStandAloneDialog(CCheckoutDlg::IDD, pParent)
	, Revision(_T("HEAD"))
	, m_strCheckoutDirectory(_T(""))
	, IsExport(FALSE)
	, m_bNonRecursive(FALSE)
{
}

CCheckoutDlg::~CCheckoutDlg()
{
}

void CCheckoutDlg::DoDataExchange(CDataExchange* pDX)
{
	CStandAloneDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_URLCOMBO, m_URLCombo);
	DDX_Control(pDX, IDC_REVISION_NUM, m_editRevision);
	DDX_Control(pDX, IDC_BROWSE, m_butBrowse);
	DDX_Text(pDX, IDC_REVISION_NUM, m_sRevision);
	DDX_Text(pDX, IDC_CHECKOUTDIRECTORY, m_strCheckoutDirectory);
	DDX_Check(pDX, IDC_NON_RECURSIVE, m_bNonRecursive);
}


BEGIN_MESSAGE_MAP(CCheckoutDlg, CStandAloneDialog)
	ON_BN_CLICKED(IDC_REVISION_N, OnBnClickedRevisionN)
	ON_BN_CLICKED(IDC_REVISION_HEAD, OnBnClickedRevisionHead)
	ON_BN_CLICKED(IDC_BROWSE, OnBnClickedBrowse)
	ON_BN_CLICKED(IDC_CHECKOUTDIRECTORY_BROWSE, OnBnClickedCheckoutdirectoryBrowse)
	ON_EN_CHANGE(IDC_CHECKOUTDIRECTORY, OnEnChangeCheckoutdirectory)
	ON_BN_CLICKED(IDHELP, OnBnClickedHelp)
END_MESSAGE_MAP()

BOOL CCheckoutDlg::OnInitDialog()
{
	CStandAloneDialog::OnInitDialog();

	m_URLCombo.SetURLHistory(TRUE);
	m_URLCombo.LoadHistory(_T("Software\\TortoiseSVN\\History\\repoURLS"), _T("url"));

	// set head revision as default revision
	CheckRadioButton(IDC_REVISION_HEAD, IDC_REVISION_N, IDC_REVISION_HEAD);

	m_editRevision.SetWindowText(_T(""));

	m_tooltips.Create(this);
	m_tooltips.AddTool(IDC_CHECKOUTDIRECTORY, IDS_CHECKOUT_TT_DIR);
	//m_tooltips.SetEffectBk(CBalloon::BALLOON_EFFECT_HGRADIENT);
	//m_tooltips.SetGradientColors(0x80ffff, 0x000000, 0xffff80);

	SHAutoComplete(GetDlgItem(IDC_CHECKOUTDIRECTORY)->m_hWnd, SHACF_FILESYSTEM);
	if (IsExport)
	{
		CString temp;
		temp.LoadString(IDS_PROGRS_TITLE_EXPORT);
		this->SetWindowText(temp);
		temp.LoadString(IDS_CHECKOUT_EXPORTDIR);
		GetDlgItem(IDC_EXPORT_CHECKOUTDIR)->SetWindowText(temp);
		GetDlgItem(IDC_NON_RECURSIVE)->ShowWindow(SW_HIDE);
	} // if (IsExport)

	if (hWndExplorer)
		CenterWindow(CWnd::FromHandle(hWndExplorer));
	EnableSaveRestore(_T("CheckoutDlg"));
	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

void CCheckoutDlg::OnOK()
{
	if (!UpdateData(TRUE))
		return; // don't dismiss dialog (error message already shown by MFC framework)

	// if head revision, set revision as -1
	if (GetCheckedRadioButton(IDC_REVISION_HEAD, IDC_REVISION_N) == IDC_REVISION_HEAD)
	{
		Revision = SVNRev(_T("HEAD"));
	}
	else
		Revision = SVNRev(m_sRevision);
	if (!Revision.IsValid())
	{
		CBalloon::ShowBalloon(this, CBalloon::GetCtrlCentre(this,IDC_REVISION_NUM), IDS_ERR_INVALIDREV, TRUE, IDI_EXCLAMATION);
		return;
	}

	m_URLCombo.SaveHistory();
	m_URL = m_URLCombo.GetString();

	if (m_URL.Left(7).CompareNoCase(_T("file://"))==0)
	{
		//check if the url is on a network share
		CString temp = m_URL.Mid(7);
		temp = temp.TrimLeft('/');
		temp.Replace('/', '\\');
		temp = temp.Left(3);
		if (GetDriveType(temp)==DRIVE_REMOTE)
		{
			if (SVN::IsBDBRepository(m_URL))
				if (CMessageBox::Show(this->m_hWnd, IDS_WARN_SHAREFILEACCESS, IDS_APPNAME, MB_ICONWARNING | MB_YESNO)==IDNO)
					return;
		}
	}

	if (m_strCheckoutDirectory.IsEmpty())
	{
		return;			//don't dismiss the dialog
	}
	if (!PathFileExists(m_strCheckoutDirectory))
	{
		CString temp;
		temp.Format(IDS_WARN_FOLDERNOTEXIST, (LPCTSTR)m_strCheckoutDirectory);
		if (CMessageBox::Show(this->m_hWnd, temp, _T("TortoiseSVN"), MB_YESNO | MB_ICONQUESTION) == IDYES)
		{
			CPathUtils::MakeSureDirectoryPathExists(m_strCheckoutDirectory);
		}
		else
			return;		//don't dismiss the dialog
	}
	if (!PathIsDirectoryEmpty(m_strCheckoutDirectory))
	{
		CString message(MAKEINTRESOURCE(IDS_WARN_FOLDERNOTEMPTY));
		if (CMessageBox::Show(this->m_hWnd, message, _T("TortoiseSVN"), MB_YESNO | MB_ICONQUESTION) == IDNO)
			return;		//don't dismiss the dialog
	}
	UpdateData(FALSE);
	CStandAloneDialog::OnOK();
}

void CCheckoutDlg::OnBnClickedRevisionN()
{
	m_editRevision.EnableWindow();
}

void CCheckoutDlg::OnBnClickedRevisionHead()
{
	m_editRevision.EnableWindow(FALSE);
}

void CCheckoutDlg::OnBnClickedBrowse()
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
	} // if (strUrl.Left(7) == _T("file://")) 
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

void CCheckoutDlg::OnBnClickedCheckoutdirectoryBrowse()
{
	//
	// Create a folder browser dialog. If the user selects OK, we should update
	// the local data members with values from the controls, copy the checkout
	// directory from the browse folder, then restore the local values into the
	// dialog controls.
	//
	CBrowseFolder browseFolder;
	browseFolder.m_style = BIF_EDITBOX | BIF_NEWDIALOGSTYLE | BIF_RETURNFSANCESTORS | BIF_RETURNONLYFSDIRS;
	CString strCheckoutDirectory;
	if (browseFolder.Show(GetSafeHwnd(), strCheckoutDirectory) == CBrowseFolder::OK) 
	{
		UpdateData(TRUE);
		m_strCheckoutDirectory = strCheckoutDirectory;
		UpdateData(FALSE);
	}
}

BOOL CCheckoutDlg::PreTranslateMessage(MSG* pMsg)
{
	m_tooltips.RelayEvent(pMsg);
	return CStandAloneDialog::PreTranslateMessage(pMsg);
}

void CCheckoutDlg::OnEnChangeCheckoutdirectory()
{
	UpdateData(TRUE);
	if (m_strCheckoutDirectory.IsEmpty())
	{
		GetDlgItem(IDOK)->EnableWindow(FALSE);
	}
	else
	{
		GetDlgItem(IDOK)->EnableWindow(TRUE);
	}
}

void CCheckoutDlg::OnBnClickedHelp()
{
	OnHelp();
}
