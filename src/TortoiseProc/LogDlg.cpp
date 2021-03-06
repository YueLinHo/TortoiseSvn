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
#include "cursor.h"
#include "MergeDlg.h"
#include "InputDlg.h"
#include "PropDlg.h"
#include "SVNProgressDlg.h"
#include "ProgressDlg.h"
#include "RepositoryBrowser.h"
#include "CopyDlg.h"
#include "StatGraphDlg.h"
#include "Logdlg.h"
#include "MessageBox.h"
#include "Registry.h"
#include "Utils.h"

// CLogDlg dialog

IMPLEMENT_DYNAMIC(CLogDlg, CResizableStandAloneDialog)
CLogDlg::CLogDlg(CWnd* pParent /*=NULL*/)
	: CResizableStandAloneDialog(CLogDlg::IDD, pParent),
	m_startrev(0),
	m_endrev(0),
	m_logcounter(0),
	m_bStrict(FALSE),
	m_nSearchIndex(0)
{
	m_pFindDialog = NULL;
	m_bCancelled = FALSE;
	m_bShowedAll = FALSE;
	m_pNotifyWindow = NULL;
	m_bThreadRunning = FALSE;


}

CLogDlg::~CLogDlg()
{
	m_tempFileList.DeleteAllFiles();
	for (INT_PTR i=0; i<m_arLogPaths.GetCount(); ++i)
	{
		LogChangedPathArray * patharray = m_arLogPaths.GetAt(i);
		for (INT_PTR j=0; j<patharray->GetCount(); ++j)
		{
			delete patharray->GetAt(j);
		}
		patharray->RemoveAll();
		delete patharray;
	}
	m_arLogPaths.RemoveAll();
}

void CLogDlg::DoDataExchange(CDataExchange* pDX)
{
	CResizableStandAloneDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_LOGLIST, m_LogList);
	DDX_Control(pDX, IDC_LOGMSG, m_LogMsgCtrl);
	DDX_Control(pDX, IDC_PROGRESS, m_LogProgress);
	DDX_Check(pDX, IDC_CHECK_STOPONCOPY, m_bStrict);
}

const UINT CLogDlg::m_FindDialogMessage = RegisterWindowMessage(FINDMSGSTRING);

BEGIN_MESSAGE_MAP(CLogDlg, CResizableStandAloneDialog)
	ON_NOTIFY(LVN_KEYDOWN, IDC_LOGLIST, OnLvnKeydownLoglist)
	ON_REGISTERED_MESSAGE(m_FindDialogMessage, OnFindDialogMessage) 
	ON_BN_CLICKED(IDC_GETALL, OnBnClickedGetall)
	ON_NOTIFY(NM_DBLCLK, IDC_LOGMSG, OnNMDblclkLogmsg)
	ON_NOTIFY(NM_DBLCLK, IDC_LOGLIST, OnNMDblclkLoglist)
	ON_WM_CONTEXTMENU()
	ON_WM_SETCURSOR()
	ON_BN_CLICKED(IDHELP, OnBnClickedHelp)
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_LOGLIST, OnLvnItemchangedLoglist)
	ON_NOTIFY(EN_LINK, IDC_MSGVIEW, OnEnLinkMsgview)
	ON_BN_CLICKED(IDC_STATBUTTON, OnBnClickedStatbutton)
	ON_NOTIFY(NM_CUSTOMDRAW, IDC_LOGLIST, OnNMCustomdrawLoglist)
END_MESSAGE_MAP()



void CLogDlg::SetParams(const CString& path, long startrev /* = 0 */, long endrev /* = -1 */, BOOL bStrict /* = FALSE */)
{
	m_path.SetFromUnknown(path);
	m_startrev = startrev;
	m_endrev = endrev;
	m_hasWC = !PathIsURL(path);
	m_bStrict = bStrict;
}

BOOL CLogDlg::OnInitDialog()
{
	CResizableStandAloneDialog::OnInitDialog();
	
	CUtils::CreateFontForLogs(m_logFont);
	GetDlgItem(IDC_MSGVIEW)->SetFont(&m_logFont);
	GetDlgItem(IDC_MSGVIEW)->SendMessage(EM_AUTOURLDETECT, TRUE, NULL);
	GetDlgItem(IDC_MSGVIEW)->SendMessage(EM_SETEVENTMASK, NULL, ENM_LINK);
	m_LogList.SetExtendedStyle ( LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER );

	m_LogList.DeleteAllItems();
	int c = ((CHeaderCtrl*)(m_LogList.GetDlgItem(0)))->GetItemCount()-1;
	while (c>=0)
		m_LogList.DeleteColumn(c--);
	CString temp;
	temp.LoadString(IDS_LOG_REVISION);
	m_LogList.InsertColumn(0, temp);
	temp.LoadString(IDS_LOG_AUTHOR);
	m_LogList.InsertColumn(1, temp);
	temp.LoadString(IDS_LOG_DATE);
	m_LogList.InsertColumn(2, temp);
	temp.LoadString(IDS_LOG_MESSAGE);
	m_LogList.InsertColumn(3, temp);
	m_arLogMessages.SetSize(0,100);
	m_arRevs.SetSize(0,100);

	m_LogList.SetRedraw(false);
	m_LogMsgCtrl.InsertColumn(0, _T("Message"));
	int mincol = 0;
	int maxcol = ((CHeaderCtrl*)(m_LogList.GetDlgItem(0)))->GetItemCount()-1;
	int col;
	for (col = mincol; col <= maxcol; col++)
	{
		m_LogList.SetColumnWidth(col,LVSCW_AUTOSIZE_USEHEADER);
	}

	if (m_hasWC)
	{
		m_ProjectProperties.ReadProps(m_path.GetSVNPathString());
	}

	GetDlgItem(IDC_LOGLIST)->UpdateData(FALSE);

	m_logcounter = 0;
	m_sMessageBuf.Preallocate(100000);

	CString sTitle;
	GetWindowText(sTitle);
	if(m_path.IsDirectory())
	{
		SetWindowText(sTitle + _T(" - ") + m_path.GetWinPathString());
	}
	else
	{
		SetWindowText(sTitle + _T(" - ") + m_path.GetFilename());
	}

	AddAnchor(IDC_LOGLIST, TOP_LEFT, TOP_RIGHT);
	AddAnchor(IDC_MSGVIEW, TOP_LEFT, MIDDLE_RIGHT);
	AddAnchor(IDC_LOGMSG, MIDDLE_LEFT, BOTTOM_RIGHT);
	AddAnchor(IDC_CHECK_STOPONCOPY, BOTTOM_LEFT, BOTTOM_RIGHT);
	AddAnchor(IDC_GETALL, BOTTOM_LEFT);
	AddAnchor(IDC_STATBUTTON, BOTTOM_RIGHT);
	AddAnchor(IDC_PROGRESS, BOTTOM_LEFT, BOTTOM_RIGHT);
	AddAnchor(IDOK, BOTTOM_RIGHT);
	AddAnchor(IDHELP, BOTTOM_RIGHT);
	SetPromptParentWindow(m_hWnd);
	if (hWndExplorer)
		CenterWindow(CWnd::FromHandle(hWndExplorer));
	EnableSaveRestore(_T("LogDlg"));
	GetDlgItem(IDC_LOGLIST)->SetFocus();
	//first start a thread to obtain the log messages without
	//blocking the dialog
	if (AfxBeginThread(LogThreadEntry, this)==NULL)
	{
		CMessageBox::Show(NULL, IDS_ERR_THREADSTARTFAILED, IDS_APPNAME, MB_OK | MB_ICONERROR);
	}
	return FALSE;
}

void CLogDlg::FillLogMessageCtrl(const CString& msg, LogChangedPathArray * paths)
{
	CWnd * pMsgView = GetDlgItem(IDC_MSGVIEW);
	pMsgView->SetWindowText(_T(" "));
	pMsgView->SetWindowText(msg);
	m_ProjectProperties.FindBugID(msg, pMsgView);

	m_LogMsgCtrl.SetExtendedStyle ( LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER );
	m_LogMsgCtrl.DeleteAllItems();
	m_LogMsgCtrl.SetRedraw(FALSE);
	int line = 0;
	CString sLine;
	if (paths)
	{
		for (INT_PTR i=0; i<paths->GetCount(); ++i)
		{
			LogChangedPath * changedpath = paths->GetAt(i);
			sLine = changedpath->sAction;
			sLine += _T(" : ") + changedpath->sPath;
			if (changedpath->lCopyFromRev)
			{
				CString temp;
				temp.Format(_T(" (from %s:%ld)"), changedpath->sCopyFromPath, changedpath->lCopyFromRev);
				sLine += temp;
			}
			m_LogMsgCtrl.InsertItem(line++, sLine);
		}
	}

	m_LogMsgCtrl.SetColumnWidth(0,LVSCW_AUTOSIZE_USEHEADER);
	m_LogMsgCtrl.SetRedraw();
}

void CLogDlg::OnBnClickedGetall()
{
	UpdateData();

	for (INT_PTR i=0; i<m_arLogPaths.GetCount(); ++i)
	{
		LogChangedPathArray * patharray = m_arLogPaths.GetAt(i);
		for (INT_PTR j=0; j<patharray->GetCount(); ++j)
		{
			delete patharray->GetAt(j);
		}
		patharray->RemoveAll();
		delete patharray;
	}
	m_arLogPaths.RemoveAll();

	m_LogList.DeleteAllItems();
	m_arLogMessages.RemoveAll();
	m_arLogPaths.RemoveAll();
	m_arRevs.RemoveAll();
	m_arAuthors.RemoveAll();
	m_arDates.RemoveAll();
	m_arCopies.RemoveAll();
	m_logcounter = 0;
	m_endrev = 1;
	m_startrev = -1;
	m_bCancelled = FALSE;
	if (AfxBeginThread(LogThreadEntry, this)==NULL)
	{
		CMessageBox::Show(NULL, IDS_ERR_THREADSTARTFAILED, IDS_APPNAME, MB_OK | MB_ICONERROR);
	}
	GetDlgItem(IDC_LOGLIST)->UpdateData(FALSE);
}

BOOL CLogDlg::Cancel()
{
	return m_bCancelled;
}

void CLogDlg::OnCancel()
{
	CString temp, temp2;
	GetDlgItem(IDOK)->GetWindowText(temp);
	temp2.LoadString(IDS_MSGBOX_CANCEL);
	if (temp.Compare(temp2)==0)
	{
		m_bCancelled = TRUE;
		return;
	}
	__super::OnCancel();
}

BOOL CLogDlg::Log(LONG rev, const CString& author, const CString& date, const CString& message, LogChangedPathArray * cpaths, apr_time_t time, int filechanges, BOOL copies)
{
	CString temp;
	int found = 0;
	m_LogList.SetRedraw(FALSE);
	m_logcounter += 1;
	if (m_startrev == -1)
		m_startrev = rev;
	m_LogProgress.SetPos(m_startrev-rev+m_endrev);
	int count = m_LogList.GetItemCount();
	int lastvisible = m_LogList.GetCountPerPage();
	temp.Format(_T("%d"),rev);
	m_LogList.InsertItem(count, temp);
	m_LogList.SetItemText(count, 1, author);
	m_LogList.SetItemText(count, 2, date);
	__time64_t ttime = time/1000000L;
	m_arDates.Add((DWORD)ttime);
	m_arAuthors.Add(author);
	m_arFileChanges.Add(filechanges);
	m_arCopies.Add(copies);
	// Add as many characters from the log message to the list control
	// so it has a fixed width. If the log message is longer than
	// this predefined fixed with, add "..." as an indication.
	CString sShortMessage = message;
	// Remove newlines 'cause those are not shown nicely in the listcontrol
	sShortMessage.Replace(_T("\r"), _T(""));
	sShortMessage.Replace('\n', ' ');
	
	found = sShortMessage.Find(_T("\n\n"));
	if (found >=0)
	{
		if (found <=80)
			sShortMessage = sShortMessage.Left(found);
		else
		{
			found = sShortMessage.Find(_T("\n"));
			if ((found >= 0)&&(found <=80))
				sShortMessage = sShortMessage.Left(found);
		}
	}
	else if (sShortMessage.GetLength() > 80)
		sShortMessage = sShortMessage.Left(77) + _T("...");
		
	m_LogList.SetItemText(count, 3, sShortMessage);

	//split multiline logentries and concatenate them
	//again but this time with \r\n as line separators
	//so that the edit control recognizes them
	try
	{
		temp = "";
		if (message.GetLength()>0)
		{
			m_sMessageBuf = message;
			m_sMessageBuf.Replace(_T("\n\r"), _T("\n"));
			m_sMessageBuf.Replace(_T("\r\n"), _T("\n"));
			if (m_sMessageBuf.Right(1).Compare(_T("\n"))==0)
				m_sMessageBuf = m_sMessageBuf.Left(m_sMessageBuf.GetLength()-1);
		} // if (message.GetLength()>0)
		else
			m_sMessageBuf.Empty();
		m_arLogMessages.Add(m_sMessageBuf);
		m_arLogPaths.Add(cpaths);
		m_arRevs.Add(rev);
	}
	catch (CException * e)
	{
		::MessageBox(NULL, _T("not enough memory!"), _T("TortoiseSVN"), MB_ICONERROR);
		e->Delete();
		m_bCancelled = TRUE;
	}
	if (count < lastvisible)
	{
		m_LogList.SetRedraw();
	}
	m_bGotRevisions = TRUE;
	return TRUE;
}

//this is the thread function which calls the subversion function
UINT CLogDlg::LogThreadEntry(LPVOID pVoid)
{
	return ((CLogDlg*)pVoid)->LogThread();
}


//this is the thread function which calls the subversion function
UINT CLogDlg::LogThread()
{
	m_bThreadRunning = TRUE;
	// to make gettext happy
	SetThreadLocale(CRegDWORD(_T("Software\\TortoiseSVN\\LanguageID"), 1033));

	CString temp;
	temp.LoadString(IDS_MSGBOX_CANCEL);
	GetDlgItem(IDOK)->SetWindowText(temp);
	long r = GetHEADRevision(m_path);
	if (m_startrev == -1)
		m_startrev = r;
	if (m_endrev < (-5) || m_bStrict)
	{
		if ((r != (-2))&&(m_endrev < (-5)))
		{
			m_endrev = m_startrev + m_endrev;
		} // if (r != (-2))
		if (m_endrev <= 0)
		{
			m_endrev = 1;
			if (!m_bStrict)
				m_bShowedAll = TRUE;
		}
	} // if (m_endrev < (-5))
	else
	{
		m_bShowedAll = TRUE;
	}
	//disable the "Get All" button while we're receiving
	//log messages.
	GetDlgItem(IDC_GETALL)->EnableWindow(FALSE);
	GetDlgItem(IDC_CHECK_STOPONCOPY)->EnableWindow(FALSE);
	m_LogProgress.SetRange32(m_endrev, m_startrev);
	m_LogProgress.SetPos(0);
	GetDlgItem(IDC_PROGRESS)->ShowWindow(TRUE);
	m_bGotRevisions = FALSE;
	while ((m_bCancelled == FALSE)&&(m_bGotRevisions == FALSE))
	{
		if (!ReceiveLog(CTSVNPathList(m_path), m_startrev, m_endrev, true, m_bStrict))
		{
			CMessageBox::Show(m_hWnd, GetLastErrorMessage(), _T("TortoiseSVN"), MB_ICONERROR);
			break;
		}
		if (m_endrev <= 1)
			break;
		m_endrev -= (LONG)(DWORD)CRegDWORD(_T("Software\\TortoiseSVN\\NumberOfLogs"), 100);
		if (m_endrev <= 0)
		{
			m_endrev = 1;
		}
	}

	m_LogList.SetRedraw(false);
	int mincol = 0;
	int maxcol = ((CHeaderCtrl*)(m_LogList.GetDlgItem(0)))->GetItemCount()-1;
	int col;
	for (col = mincol; col <= maxcol; col++)
	{
		m_LogList.SetColumnWidth(col,LVSCW_AUTOSIZE_USEHEADER);
	}
	m_LogList.SetRedraw(true);

	temp.LoadString(IDS_MSGBOX_OK);
	GetDlgItem(IDOK)->SetWindowText(temp);
	if (!m_bShowedAll)
	{
		GetDlgItem(IDC_GETALL)->EnableWindow(TRUE);
		GetDlgItem(IDC_CHECK_STOPONCOPY)->EnableWindow(TRUE);
	}
	GetDlgItem(IDC_PROGRESS)->ShowWindow(FALSE);
	m_bCancelled = TRUE;
	m_bThreadRunning = FALSE;
	POINT pt;
	GetCursorPos(&pt);
	SetCursorPos(pt.x, pt.y);
	return 0;
}

void CLogDlg::OnLvnKeydownLoglist(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMLVKEYDOWN pLVKeyDow = reinterpret_cast<LPNMLVKEYDOWN>(pNMHDR);
	int selIndex = m_LogList.GetSelectionMark();
	if (selIndex >= 0)
	{
		//check which key was pressed
		//this is necessary because we get here BEFORE
		//the selection actually changes, so we have to
		//adjust the selected index
		if ((pLVKeyDow->wVKey == VK_UP)||(pLVKeyDow->wVKey == VK_DOWN))
		{
			if (pLVKeyDow->wVKey == VK_UP)
			{
				if (selIndex > 0)
					selIndex--;
			}
			if (pLVKeyDow->wVKey == VK_DOWN)
			{
				selIndex++;		
				if (selIndex >= m_LogList.GetItemCount())
					selIndex = m_LogList.GetItemCount()-1;
			}
			//m_sLogMsgCtrl = m_arLogMessages.GetAt(selIndex);
			this->m_nSearchIndex = selIndex;
			FillLogMessageCtrl(m_arLogMessages.GetAt(selIndex), m_arLogPaths.GetAt(selIndex));
			UpdateData(FALSE);
		}
		if (pLVKeyDow->wVKey == 'C')
		{
			if (GetKeyState(VK_CONTROL)&0x8000)
			{
				//Ctrl-C -> copy to clipboard
				CStringA sClipdata;
				POSITION pos = m_LogList.GetFirstSelectedItemPosition();
				if (pos != NULL)
				{
					CString sRev;
					sRev.LoadString(IDS_LOG_REVISION);
					CString sAuthor;
					sAuthor.LoadString(IDS_LOG_AUTHOR);
					CString sDate;
					sDate.LoadString(IDS_LOG_DATE);
					CString sMessage;
					sMessage.LoadString(IDS_LOG_MESSAGE);
					while (pos)
					{
						int nItem = m_LogList.GetNextSelectedItem(pos);
						CString sLogCopyText;
						CString sPaths;
						LogChangedPathArray * cpatharray = m_arLogPaths.GetAt(nItem);
						for (INT_PTR cpPathIndex = 0; cpPathIndex<cpatharray->GetCount(); ++cpPathIndex)
						{
							LogChangedPath * cpath = cpatharray->GetAt(cpPathIndex);
							sPaths += cpath->sPath + _T("\n");
						}
						sLogCopyText.Format(_T("%s: %d\n%s: %s\n%s: %s\n%s:\n%s\n----\n%s\n\n"),
							(LPCTSTR)sRev, m_arRevs.GetAt(nItem),
							(LPCTSTR)sAuthor, (LPCTSTR)m_LogList.GetItemText(nItem, 1),
							(LPCTSTR)sDate, (LPCTSTR)m_LogList.GetItemText(nItem, 2),
							(LPCTSTR)sMessage, (LPCTSTR)m_arLogMessages.GetAt(nItem),
							(LPCTSTR)sPaths);
						sClipdata +=  CStringA(sLogCopyText);
					}
					CUtils::WriteAsciiStringToClipboard(sClipdata);
				}
			}
		}
	}
	else
	{
		if (m_arLogMessages.GetCount()>0)
			FillLogMessageCtrl(m_arLogMessages.GetAt(0), m_arLogPaths.GetAt(0));
		UpdateData(FALSE);
	}
	*pResult = 0;
}

void CLogDlg::OnContextMenu(CWnd* pWnd, CPoint point)
{
//#region m_LogList
	if (pWnd == &m_LogList)
	{
		int selIndex = m_LogList.GetSelectionMark();
		if ((point.x == -1) && (point.y == -1))
		{
			CRect rect;
			m_LogList.GetItemRect(selIndex, &rect, LVIR_LABEL);
			m_LogList.ClientToScreen(&rect);
			point = rect.CenterPoint();
		}
		m_nSearchIndex = selIndex;

		if (selIndex >= 0)
		{
			//entry is selected, now show the popup menu
			CMenu popup;
			if (popup.CreatePopupMenu())
			{
				CString temp;
				if (m_LogList.GetSelectedCount() == 1)
				{
					if (!m_path.IsDirectory())
					{
						temp.LoadString(IDS_LOG_POPUP_COMPARE);
						if (m_hasWC)
						{
							popup.AppendMenu(MF_STRING | MF_ENABLED, ID_COMPARE, temp);
							popup.SetDefaultItem(ID_COMPARE, FALSE);
						}
						temp.LoadString(IDS_LOG_POPUP_GNUDIFF);
						popup.AppendMenu(MF_STRING | MF_ENABLED, ID_GNUDIFF1, temp);
						popup.AppendMenu(MF_SEPARATOR, NULL);
						temp.LoadString(IDS_LOG_POPUP_SAVE);
						popup.AppendMenu(MF_STRING | MF_ENABLED, ID_SAVEAS, temp);
						temp.LoadString(IDS_LOG_POPUP_OPEN);
						popup.AppendMenu(MF_STRING | MF_ENABLED, ID_OPEN, temp);
					}
					else
					{
						temp.LoadString(IDS_LOG_POPUP_COMPARE);
						if (m_hasWC)
						{
							popup.AppendMenu(MF_STRING | MF_ENABLED, ID_COMPARE, temp);
							popup.SetDefaultItem(ID_COMPARE, FALSE);
						}
						temp.LoadString(IDS_LOG_POPUP_GNUDIFF);
						popup.AppendMenu(MF_STRING | MF_ENABLED, ID_GNUDIFF1, temp);
						popup.AppendMenu(MF_SEPARATOR, NULL);
					}
					temp.LoadString(IDS_LOG_BROWSEREPO);
					popup.AppendMenu(MF_STRING | MF_ENABLED, ID_REPOBROWSE, temp);
					temp.LoadString(IDS_LOG_POPUP_COPY);
					popup.AppendMenu(MF_STRING | MF_ENABLED, ID_COPY, temp);
					temp.LoadString(IDS_LOG_POPUP_UPDATE);
					if (m_hasWC)
						popup.AppendMenu(MF_STRING | MF_ENABLED, ID_UPDATE, temp);
					temp.LoadString(IDS_LOG_POPUP_REVERTREV);
					if (m_hasWC)
						popup.AppendMenu(MF_STRING | MF_ENABLED, ID_REVERTREV, temp);
				}
				else if (m_LogList.GetSelectedCount() == 2)
				{
					if (!m_path.IsDirectory())
					{
						temp.LoadString(IDS_LOG_POPUP_COMPARETWO);
						popup.AppendMenu(MF_STRING | MF_ENABLED, ID_COMPARETWO, temp);
					}
					temp.LoadString(IDS_LOG_POPUP_GNUDIFF);
					popup.AppendMenu(MF_STRING | MF_ENABLED, ID_GNUDIFF2, temp);
				}
				popup.AppendMenu(MF_SEPARATOR, NULL);
				temp.LoadString(IDS_LOG_POPUP_FIND);
				popup.AppendMenu(MF_STRING | MF_ENABLED, ID_FINDENTRY, temp);

				int cmd = popup.TrackPopupMenu(TPM_RETURNCMD | TPM_LEFTALIGN | TPM_NONOTIFY, point.x, point.y, this, 0);
				GetDlgItem(IDOK)->EnableWindow(FALSE);
				SetPromptApp(&theApp);
				theApp.DoWaitCursor(1);
				switch (cmd)
				{
				case ID_GNUDIFF1:
					{
						int selIndex = m_LogList.GetSelectionMark();
						long rev = m_arRevs.GetAt(selIndex);
						this->m_bCancelled = FALSE;
						CTSVNPath tempfile = CUtils::GetTempFilePath(CTSVNPath(_T("Test.diff")));
						m_tempFileList.AddPath(tempfile);
						if (!PegDiff(m_path, (m_hasWC ? SVNRev::REV_WC : SVNRev::REV_HEAD), rev-1, rev, TRUE, FALSE, TRUE, _T(""), tempfile))
						{
							CMessageBox::Show(this->m_hWnd, GetLastErrorMessage(), _T("TortoiseSVN"), MB_ICONERROR);
							break;		//exit
						}
						else
						{
							if (CUtils::CheckForEmptyDiff(tempfile))
							{
								CMessageBox::Show(m_hWnd, IDS_ERR_EMPTYDIFF, IDS_APPNAME, MB_ICONERROR);
								break;
							}
							CUtils::StartUnifiedDiffViewer(tempfile);
						}
					}
					break;
				case ID_GNUDIFF2:
					{
						POSITION pos = m_LogList.GetFirstSelectedItemPosition();
						long rev1 = m_arRevs.GetAt(m_LogList.GetNextSelectedItem(pos));
						long rev2 = m_arRevs.GetAt(m_LogList.GetNextSelectedItem(pos));
						this->m_bCancelled = FALSE;
						CTSVNPath tempfile = CUtils::GetTempFilePath(CTSVNPath(_T("Test.diff")));
						m_tempFileList.AddPath(tempfile);
						if (!PegDiff(m_path, (m_hasWC ? SVNRev::REV_WC : SVNRev::REV_HEAD), rev2, rev1, TRUE, FALSE, TRUE, _T(""), tempfile))
						{
							CMessageBox::Show(this->m_hWnd, GetLastErrorMessage(), _T("TortoiseSVN"), MB_ICONERROR);
							break;		//exit
						}
						else
						{
							if (CUtils::CheckForEmptyDiff(tempfile))
							{
								CMessageBox::Show(m_hWnd, IDS_ERR_EMPTYDIFF, IDS_APPNAME, MB_ICONERROR);
								break;
							}
							CUtils::StartUnifiedDiffViewer(tempfile);
						}
					}
					break;
				case ID_REVERTREV:
					{
						long rev = m_arRevs.GetAt(selIndex);
						if (CMessageBox::Show(this->m_hWnd, IDS_LOG_REVERT_CONFIRM, IDS_APPNAME, MB_YESNO | MB_ICONQUESTION) == IDYES)
						{
							CString url = this->GetURLFromPath(m_path);
							if (url.IsEmpty())
							{
								CString strMessage;
								strMessage.Format(IDS_ERR_NOURLOFFILE, (LPCTSTR)(m_path.GetUIPathString()));
								CMessageBox::Show(this->m_hWnd, strMessage, _T("TortoiseSVN"), MB_ICONERROR);
								TRACE(_T("could not retrieve the URL of the folder!\n"));
								break;		//exit
							}
							else
							{
								CSVNProgressDlg dlg;
								dlg.SetParams(CSVNProgressDlg::Enum_Merge, 0, CTSVNPathList(m_path), url, url, rev);		//use the message as the second url
								dlg.m_RevisionEnd = rev-1;
								dlg.DoModal();
							}
						}
					}
					break;
				case ID_COPY:
					{
						long rev = m_arRevs.GetAt(selIndex);
						CCopyDlg dlg;
						CString url = GetURLFromPath(m_path);
						if (url.IsEmpty())
						{
							CString temp;
							temp.Format(IDS_ERR_NOURLOFFILE, m_path);
							CMessageBox::Show(this->m_hWnd, temp, _T("TortoiseSVN"), MB_ICONERROR);
							TRACE(_T("could not retrieve the URL of the folder!\n"));
							break;		//exit
						}
						else
						{
							dlg.m_URL = url;
							dlg.m_path = m_path.GetSVNPathString();
							if (dlg.DoModal() == IDOK)
							{
								SVN svn;
								if (!svn.Copy(CTSVNPath(url), CTSVNPath(dlg.m_URL), rev, dlg.m_sLogMessage))
								{
									CMessageBox::Show(this->m_hWnd, svn.GetLastErrorMessage(), _T("TortoiseSVN"), MB_ICONERROR);
								}
								else
								{
									CMessageBox::Show(this->m_hWnd, IDS_LOG_COPY_SUCCESS, IDS_APPNAME, MB_ICONINFORMATION);
								}
							} // if (dlg.DoModal() == IDOK) 
						}
					} 
					break;
				case ID_COMPARE:
					{
						//user clicked on the menu item "compare with working copy"
						//now first get the revision which is selected
						int selIndex = m_LogList.GetSelectionMark();
						long rev = m_arRevs.GetAt(selIndex);
						if ((m_path.IsDirectory())||(!m_hasWC))
						{
							this->m_bCancelled = FALSE;
							CTSVNPath tempfile = CUtils::GetTempFilePath();
							m_tempFileList.AddPath(tempfile);
							tempfile.AppendRawString(_T(".diff"));
							m_tempFileList.AddPath(tempfile);
							if (!PegDiff(m_path, (m_hasWC ? SVNRev::REV_WC : SVNRev::REV_HEAD), SVNRev::REV_WC, rev, TRUE, FALSE, TRUE, _T(""), tempfile))
							{
								CMessageBox::Show(this->m_hWnd, GetLastErrorMessage(), _T("TortoiseSVN"), MB_ICONERROR);
								break;		//exit
							} 
							else
							{
								if (CUtils::CheckForEmptyDiff(tempfile))
								{
									CMessageBox::Show(m_hWnd, IDS_ERR_EMPTYDIFF, IDS_APPNAME, MB_ICONERROR);
									break;
								}
								CString sWC, sRev;
								sWC.LoadString(IDS_DIFF_WORKINGCOPY);
								sRev.Format(IDS_DIFF_REVISIONPATCHED, rev);
								CUtils::StartExtPatch(tempfile, m_path.GetDirectory(), sWC, sRev, TRUE);
							}
						}
						else
						{
							CTSVNPath tempfile = CUtils::GetTempFilePath(m_path);
							m_tempFileList.AddPath(tempfile);
							SVN svn;
							if (!svn.Cat(m_path, rev, tempfile))
							{
								CMessageBox::Show(NULL, svn.GetLastErrorMessage(), _T("TortoiseSVN"), MB_ICONERROR);
								GetDlgItem(IDOK)->EnableWindow(TRUE);
								break;
							} 
							else
							{
								CString revname, wcname;
								revname.Format(_T("%s Revision %ld"), (LPCTSTR)m_path.GetFilename(), rev);
								wcname.Format(IDS_DIFF_WCNAME, (LPCTSTR)m_path.GetFilename());
								CUtils::StartExtDiff(tempfile, m_path, revname, wcname);
							}
						}
					}
					break;
				case ID_COMPARETWO:
					{
						//user clicked on the menu item "compare revisions"
						//now first get the revisions which are selected
						POSITION pos = m_LogList.GetFirstSelectedItemPosition();
						long rev1 = m_arRevs.GetAt(m_LogList.GetNextSelectedItem(pos));
						long rev2 = m_arRevs.GetAt(m_LogList.GetNextSelectedItem(pos));

						StartDiff(m_path, rev1, m_path, rev2);
					}
					break;
				case ID_SAVEAS:
					{
						//now first get the revision which is selected
						long rev = m_arRevs.GetAt(selIndex);

						OPENFILENAME ofn;		// common dialog box structure
						TCHAR szFile[MAX_PATH];  // buffer for file name
						ZeroMemory(szFile, sizeof(szFile));
						if (m_hasWC)
						{
							CString revFilename;
							CString strWinPath = m_path.GetWinPathString();
							int rfind = strWinPath.ReverseFind('.');
							if (rfind > 0)
								revFilename.Format(_T("%s-%ld%s"), (LPCTSTR)strWinPath.Left(rfind), rev, (LPCTSTR)strWinPath.Mid(rfind));
							else
								revFilename.Format(_T("%s-%ld"), (LPCTSTR)strWinPath, rev);
							_tcscpy(szFile, revFilename);
						}
						// Initialize OPENFILENAME
						ZeroMemory(&ofn, sizeof(OPENFILENAME));
						//ofn.lStructSize = sizeof(OPENFILENAME);
						ofn.lStructSize = OPENFILENAME_SIZE_VERSION_400;		//to stay compatible with NT4
						ofn.hwndOwner = this->m_hWnd;
						ofn.lpstrFile = szFile;
						ofn.nMaxFile = sizeof(szFile)/sizeof(TCHAR);
						CString temp;
						temp.LoadString(IDS_LOG_POPUP_SAVE);
						//ofn.lpstrTitle = "Save revision to...\0";
						CUtils::RemoveAccelerators(temp);
						if (temp.IsEmpty())
							ofn.lpstrTitle = NULL;
						else
							ofn.lpstrTitle = temp;
						ofn.Flags = OFN_OVERWRITEPROMPT;

						CString sFilter;
						sFilter.LoadString(IDS_COMMONFILEFILTER);
						TCHAR * pszFilters = new TCHAR[sFilter.GetLength()+4];
						_tcscpy (pszFilters, sFilter);
						// Replace '|' delimeters with '\0's
						TCHAR *ptr = pszFilters + _tcslen(pszFilters);  //set ptr at the NULL
						while (ptr != pszFilters)
						{
							if (*ptr == '|')
								*ptr = '\0';
							ptr--;
						} // while (ptr != pszFilters) 
						ofn.lpstrFilter = pszFilters;
						ofn.nFilterIndex = 1;
						// Display the Open dialog box. 
						CTSVNPath tempfile;
						if (GetSaveFileName(&ofn)==TRUE)
						{
							tempfile.SetFromWin(ofn.lpstrFile);
							SVN svn;
							if (!svn.Cat(m_path, rev, tempfile))
							{
								delete [] pszFilters;
								CMessageBox::Show(this->m_hWnd, svn.GetLastErrorMessage(), _T("TortoiseSVN"), MB_ICONERROR);
								GetDlgItem(IDOK)->EnableWindow(TRUE);
								break;
							} // if (!svn.Cat(m_path, rev, tempfile)) 
						} // if (GetSaveFileName(&ofn)==TRUE)
						delete [] pszFilters;
					}
					break;
				case ID_OPEN:
					{
						//now first get the revision which is selected
						int selIndex = m_LogList.GetSelectionMark();
						long rev = m_arRevs.GetAt(selIndex);

						CTSVNPath tempfile = CUtils::GetTempFilePath(m_path);
						m_tempFileList.AddPath(tempfile);
						SVN svn;
						if (!svn.Cat(m_path, rev, tempfile))
						{
							CMessageBox::Show(this->m_hWnd, svn.GetLastErrorMessage(), _T("TortoiseSVN"), MB_ICONERROR);
							GetDlgItem(IDOK)->EnableWindow(TRUE);
							break;
						} // if (!svn.Cat(m_path, rev, tempfile))
						else
						{
							ShellExecute(this->m_hWnd, _T("open"), tempfile.GetWinPath(), NULL, NULL, SW_SHOWNORMAL);
						}
					}
					break;
				case ID_UPDATE:
					{
						//now first get the revision which is selected
						int selIndex = m_LogList.GetSelectionMark();
						long rev = m_arRevs.GetAt(selIndex);

						SVN svn;
						if (!svn.Update(m_path, rev, TRUE))
						{
							CMessageBox::Show(this->m_hWnd, svn.GetLastErrorMessage(), _T("TortoiseSVN"), MB_ICONERROR);
							GetDlgItem(IDOK)->EnableWindow(TRUE);
							break;
						}
					}
					break;
				case ID_FINDENTRY:
					{
						m_nSearchIndex = m_LogList.GetSelectionMark();
						if (m_nSearchIndex < 0)
							m_nSearchIndex = 0;
						if (m_pFindDialog)
						{
							break;
						}
						else
						{
							m_pFindDialog = new CFindReplaceDialog();
							m_pFindDialog->Create(TRUE, NULL, NULL, FR_HIDEUPDOWN | FR_HIDEWHOLEWORD, this);									
						}
					}
					break;
				case ID_REPOBROWSE:
					{
						int selIndex = m_LogList.GetSelectionMark();
						long rev = m_arRevs.GetAt(selIndex);
						CString url = m_path.GetSVNPathString();
						if (m_hasWC)
						{
							url = GetURLFromPath(m_path);
							if (url.IsEmpty())
							{
								CString temp;
								temp.Format(IDS_ERR_NOURLOFFILE, m_path);
								CMessageBox::Show(this->m_hWnd, temp, _T("TortoiseSVN"), MB_ICONERROR);
								GetDlgItem(IDOK)->EnableWindow(TRUE);
								break;
							}
						} // if (m_hasWC)

						CRepositoryBrowser dlg(SVNUrl(url, SVNRev(rev)));
						dlg.DoModal();
					}
					break;
				default:
					break;
				} // switch (cmd)
				theApp.DoWaitCursor(-1);
				GetDlgItem(IDOK)->EnableWindow(TRUE);
			} // if (popup.CreatePopupMenu())
		} // if (selIndex >= 0)
	} // if (pWnd == &m_LogList)
//#endregion
//#region m_LogMsgCtrl
	if (pWnd == &m_LogMsgCtrl)
	{
		int selIndex = m_LogMsgCtrl.GetSelectionMark();
		if ((point.x == -1) && (point.y == -1))
		{
			CRect rect;
			m_LogMsgCtrl.GetItemRect(selIndex, &rect, LVIR_LABEL);
			m_LogMsgCtrl.ClientToScreen(&rect);
			point = rect.CenterPoint();
		}
		if (selIndex < 0)
			return;
		int s = m_LogList.GetSelectionMark();
		if (s < 0)
			return;
		long rev = m_arRevs.GetAt(s);
		LogChangedPath * changedpath = m_arLogPaths.GetAt(s)->GetAt(selIndex);
		//entry is selected, now show the popup menu
		CMenu popup;
		if (popup.CreatePopupMenu())
		{
			CString temp;
			if (m_LogMsgCtrl.GetSelectedCount() == 1)
			{
				CString t, tt;
				t.LoadString(IDS_SVNACTION_ADD);
				tt.LoadString(IDS_SVNACTION_DELETE);
				if ((rev > 1)&&(changedpath->sAction.Compare(t)!=0)&&(changedpath->sAction.Compare(tt)!=0))
				{
					temp.LoadString(IDS_LOG_POPUP_DIFF);
					popup.AppendMenu(MF_STRING | MF_ENABLED, ID_DIFF, temp);
				}
				temp.LoadString(IDS_REPOBROWSE_SHOWPROP);
				popup.AppendMenu(MF_STRING | MF_ENABLED, ID_POPPROPS, temp);			// "Show Properties"
				temp.LoadString(IDS_LOG_POPUP_SAVE);
				popup.AppendMenu(MF_STRING | MF_ENABLED, ID_SAVEAS, temp);
				temp.LoadString(IDS_LOG_POPUP_OPEN);
				popup.AppendMenu(MF_STRING | MF_ENABLED, ID_OPEN, temp);
				int cmd = popup.TrackPopupMenu(TPM_RETURNCMD | TPM_LEFTALIGN | TPM_NONOTIFY, point.x, point.y, this, 0);
				switch (cmd)
				{
				case ID_DIFF:
					{
						DoDiffFromLog(selIndex, rev);
					}
					break;
				case ID_POPPROPS:
					{
						GetDlgItem(IDOK)->EnableWindow(FALSE);
						SetPromptApp(&theApp);
						theApp.DoWaitCursor(1);
						CString filepath;
						if (SVN::PathIsURL(m_path.GetSVNPathString()))
						{
							filepath = m_path.GetSVNPathString();
						}
						else
						{
							SVN svn;
							filepath = svn.GetURLFromPath(m_path);
							if (filepath.IsEmpty())
							{
								theApp.DoWaitCursor(-1);
								CString temp;
								temp.Format(IDS_ERR_NOURLOFFILE, filepath);
								CMessageBox::Show(this->m_hWnd, temp, _T("TortoiseSVN"), MB_ICONERROR);
								TRACE(_T("could not retrieve the URL of the file!\n"));
								GetDlgItem(IDOK)->EnableWindow(TRUE);
								break;
							}
						}
						filepath = GetRepositoryRoot(CTSVNPath(filepath));
						filepath += changedpath->sPath;
						CPropDlg dlg;
						dlg.m_rev = rev;
						dlg.m_sPath = filepath;
						dlg.DoModal();
						GetDlgItem(IDOK)->EnableWindow(TRUE);
						theApp.DoWaitCursor(-1);
					}
					break;
				case ID_SAVEAS:
					{
						GetDlgItem(IDOK)->EnableWindow(FALSE);
						SetPromptApp(&theApp);
						theApp.DoWaitCursor(1);
						CString filepath;
						if (SVN::PathIsURL(m_path.GetSVNPathString()))
						{
							filepath = m_path.GetSVNPathString();
						}
						else
						{
							SVN svn;
							filepath = svn.GetURLFromPath(m_path);
							if (filepath.IsEmpty())
							{
								theApp.DoWaitCursor(-1);
								CString temp;
								temp.Format(IDS_ERR_NOURLOFFILE, filepath);
								CMessageBox::Show(this->m_hWnd, temp, _T("TortoiseSVN"), MB_ICONERROR);
								TRACE(_T("could not retrieve the URL of the file!\n"));
								GetDlgItem(IDOK)->EnableWindow(TRUE);
								break;
							}
						}
						filepath = GetRepositoryRoot(CTSVNPath(filepath));
						filepath += changedpath->sPath;

						OPENFILENAME ofn;		// common dialog box structure
						TCHAR szFile[MAX_PATH];  // buffer for file name
						ZeroMemory(szFile, sizeof(szFile));
						CString revFilename;
						temp = CUtils::GetFileNameFromPath(filepath);
						int rfind = filepath.ReverseFind('.');
						if (rfind > 0)
							revFilename.Format(_T("%s-%ld%s"), temp.Left(rfind), rev, temp.Mid(rfind));
						else
							revFilename.Format(_T("%s-%ld"), temp, rev);
						_tcscpy(szFile, revFilename);
						// Initialize OPENFILENAME
						ZeroMemory(&ofn, sizeof(OPENFILENAME));
						//ofn.lStructSize = sizeof(OPENFILENAME);
						ofn.lStructSize = OPENFILENAME_SIZE_VERSION_400;		//to stay compatible with NT4
						ofn.hwndOwner = this->m_hWnd;
						ofn.lpstrFile = szFile;
						ofn.nMaxFile = sizeof(szFile)/sizeof(TCHAR);
						temp.LoadString(IDS_LOG_POPUP_SAVE);
						CUtils::RemoveAccelerators(temp);
						if (temp.IsEmpty())
							ofn.lpstrTitle = NULL;
						else
							ofn.lpstrTitle = temp;
						ofn.Flags = OFN_OVERWRITEPROMPT;

						CString sFilter;
						sFilter.LoadString(IDS_COMMONFILEFILTER);
						TCHAR * pszFilters = new TCHAR[sFilter.GetLength()+4];
						_tcscpy (pszFilters, sFilter);
						// Replace '|' delimeters with '\0's
						TCHAR *ptr = pszFilters + _tcslen(pszFilters);  //set ptr at the NULL
						while (ptr != pszFilters)
						{
							if (*ptr == '|')
								*ptr = '\0';
							ptr--;
						} // while (ptr != pszFilters) 
						ofn.lpstrFilter = pszFilters;
						ofn.nFilterIndex = 1;
						// Display the Open dialog box. 
						CTSVNPath tempfile;
						if (GetSaveFileName(&ofn)==TRUE)
						{
							tempfile.SetFromWin(ofn.lpstrFile);
							SVN svn;
							if (!svn.Cat(CTSVNPath(filepath), rev, tempfile))
							{
								delete [] pszFilters;
								CMessageBox::Show(this->m_hWnd, svn.GetLastErrorMessage(), _T("TortoiseSVN"), MB_ICONERROR);
								GetDlgItem(IDOK)->EnableWindow(TRUE);
								theApp.DoWaitCursor(-1);
								break;
							} // if (!svn.Cat(m_path, rev, tempfile)) 
						} // if (GetSaveFileName(&ofn)==TRUE)
						delete [] pszFilters;
						GetDlgItem(IDOK)->EnableWindow(TRUE);
						theApp.DoWaitCursor(-1);
					}
					break;
				case ID_OPEN:
					{
						GetDlgItem(IDOK)->EnableWindow(FALSE);
						SetPromptApp(&theApp);
						theApp.DoWaitCursor(1);
						CString filepath;
						if (SVN::PathIsURL(m_path.GetSVNPathString()))
						{
							filepath = m_path.GetSVNPathString();
						}
						else
						{
							SVN svn;
							filepath = svn.GetURLFromPath(m_path);
							if (filepath.IsEmpty())
							{
								theApp.DoWaitCursor(-1);
								CString temp;
								temp.Format(IDS_ERR_NOURLOFFILE, filepath);
								CMessageBox::Show(this->m_hWnd, temp, _T("TortoiseSVN"), MB_ICONERROR);
								TRACE(_T("could not retrieve the URL of the file!\n"));
								GetDlgItem(IDOK)->EnableWindow(TRUE);
								break;
							}
						}
						filepath = GetRepositoryRoot(CTSVNPath(filepath));
						filepath += changedpath->sPath;

						CTSVNPath tempfile = CUtils::GetTempFilePath(CTSVNPath(filepath));
						m_tempFileList.AddPath(tempfile);
						SVN svn;
						if (!svn.Cat(CTSVNPath(filepath), rev, tempfile))
						{
							CMessageBox::Show(this->m_hWnd, svn.GetLastErrorMessage(), _T("TortoiseSVN"), MB_ICONERROR);
							GetDlgItem(IDOK)->EnableWindow(TRUE);
							theApp.DoWaitCursor(-1);
							break;
						}
						ShellExecute(this->m_hWnd, _T("open"), tempfile.GetWinPath(), NULL, NULL, SW_SHOWNORMAL);
						GetDlgItem(IDOK)->EnableWindow(TRUE);
						theApp.DoWaitCursor(-1);
					}
					break;
				default:
					break;
				} // switch (cmd)
			} // if (m_LogMsgCtrl.GetSelectedCount() == 1)
		} // if (popup.CreatePopupMenu())
	} // if (pWnd == &m_LogMsgCtrl) 
//#endregion

}

void CLogDlg::OnNMDblclkLoglist(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMLISTVIEW pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);
	*pResult = 0;
	int selIndex = pNMLV->iItem;
	int selSub = pNMLV->iSubItem;
	BOOL isSpecial = (GetKeyState(VK_SHIFT)<0)&&(selSub==1 || selSub==3);
	if ((selIndex >= 0)&&(!isSpecial))
	{
		CString temp;
		GetDlgItem(IDOK)->EnableWindow(FALSE);
		SetPromptApp(&theApp);
		theApp.DoWaitCursor(1);
		if ((!m_path.IsDirectory())&&(m_hasWC))
		{
			long rev = m_arRevs.GetAt(selIndex);
			CTSVNPath tempfile = CUtils::GetTempFilePath(m_path);
			m_tempFileList.AddPath(tempfile);

			SVN svn;
			if (!svn.Cat(m_path, rev, tempfile))
			{
				CMessageBox::Show(NULL, svn.GetLastErrorMessage(), _T("TortoiseSVN"), MB_ICONERROR);
				GetDlgItem(IDOK)->EnableWindow(TRUE);
			}
			else
			{
				CString revname, wcname;
				revname.Format(_T("%s Revision %ld"), (LPCTSTR)m_path.GetFilename(), rev);
				wcname.Format(IDS_DIFF_WCNAME, (LPCTSTR)m_path.GetFilename());
				CUtils::StartExtDiff(tempfile, m_path, revname, wcname);
			}
		} 
		else
		{
			long rev = m_arRevs.GetAt(selIndex);
			this->m_bCancelled = FALSE;
			CTSVNPath tempfile = CUtils::GetTempFilePath(CTSVNPath(_T("Test.diff")));
			m_tempFileList.AddPath(tempfile);
			if (!PegDiff(m_path, (m_hasWC ? SVNRev::REV_WC : SVNRev::REV_HEAD), (m_hasWC ? SVNRev::REV_WC : SVNRev::REV_HEAD), rev, TRUE, FALSE, TRUE, _T(""), tempfile))
			{
				CMessageBox::Show(this->m_hWnd, GetLastErrorMessage(), _T("TortoiseSVN"), MB_ICONERROR);
			}
			else
			{
				if (CUtils::CheckForEmptyDiff(tempfile))
				{
					CMessageBox::Show(m_hWnd, IDS_ERR_EMPTYDIFF, IDS_APPNAME, MB_ICONERROR);
				}
				else
				{
					if (m_hasWC)
					{
						CString sWC, sRev;
						sWC.LoadString(IDS_DIFF_WORKINGCOPY);
						sRev.Format(IDS_DIFF_REVISIONPATCHED, rev);
						CUtils::StartExtPatch(tempfile, m_path.GetDirectory(), sWC, sRev, TRUE);
					}
					else
					{
						CUtils::StartUnifiedDiffViewer(tempfile);
					}
				}
			}
		}
		theApp.DoWaitCursor(-1);
		GetDlgItem(IDOK)->EnableWindow(TRUE);
	}
	if (isSpecial)
	{
		CString url;
		CString name;
		CString text;
		GetDlgItem(IDOK)->EnableWindow(FALSE);
		SetPromptApp(&theApp);
		theApp.DoWaitCursor(1);
		if (SVN::PathIsURL(m_path.GetSVNPathString()))
			url = m_path.GetSVNPathString();
		else
		{
			SVN svn;
			url = svn.GetURLFromPath(m_path);
		}
		if (selSub == 1)
		{
			text.LoadString(IDS_LOG_AUTHOR);
			name = SVN_PROP_REVISION_AUTHOR;
		}
		else if (selSub == 3)
		{
			text.LoadString(IDS_LOG_MESSAGE);
			name = SVN_PROP_REVISION_LOG;
		}

		CString value = RevPropertyGet(name, url, m_arRevs.GetAt(selIndex));
		value.Replace(_T("\n"), _T("\r\n"));
		CInputDlg dlg;
		dlg.m_sHintText = text;
		dlg.m_sInputText = value;
		if (dlg.DoModal() == IDOK)
		{
			dlg.m_sInputText.Replace(_T("\r"), _T(""));
			if (!RevPropertySet(name, dlg.m_sInputText, url, m_arRevs.GetAt(selIndex)))
			{
				CMessageBox::Show(this->m_hWnd, GetLastErrorMessage(), _T("TortoiseSVN"), MB_ICONERROR);
			}
			else
			{
				if (selSub == 1)
				{
					m_arAuthors.SetAt(selIndex, dlg.m_sInputText);
					m_LogList.SetItemText(selIndex, 1, dlg.m_sInputText);
				}
				if (selSub == 3)
				{
					// Add as many characters from the log message to the list control
					// so it has a fixed width. If the log message is longer than
					// this predefined fixed with, add "..." as an indication.
					CString sShortMessage = dlg.m_sInputText;
					// Remove newlines 'cause those are not shown nicely in the listcontrol
					sShortMessage.Replace(_T("\r"), _T(""));
					sShortMessage.Replace('\n', ' ');

					int found = sShortMessage.Find(_T("\n\n"));
					if (found >=0)
					{
						if (found <=80)
							sShortMessage = sShortMessage.Left(found);
						else
						{
							found = sShortMessage.Find(_T("\n"));
							if ((found >= 0)&&(found <=80))
								sShortMessage = sShortMessage.Left(found);
						}
					}
					else if (sShortMessage.GetLength() > 80)
						sShortMessage = sShortMessage.Left(77) + _T("...");
					m_LogList.SetItemText(selIndex, 3, sShortMessage);
					//split multiline logentries and concatenate them
					//again but this time with \r\n as line separators
					//so that the edit control recognizes them
					if (dlg.m_sInputText.GetLength()>0)
					{
						m_sMessageBuf = dlg.m_sInputText;
						dlg.m_sInputText.Replace(_T("\n\r"), _T("\n"));
						dlg.m_sInputText.Replace(_T("\r\n"), _T("\n"));
						if (dlg.m_sInputText.Right(1).Compare(_T("\n"))==0)
							dlg.m_sInputText = dlg.m_sInputText.Left(dlg.m_sInputText.GetLength()-1);
					} 
					else
						dlg.m_sInputText.Empty();
					m_arLogMessages.SetAt(selIndex, dlg.m_sInputText);
					CWnd * pMsgView = GetDlgItem(IDC_MSGVIEW);
					pMsgView->SetWindowText(_T(" "));
					pMsgView->SetWindowText(dlg.m_sInputText);
					m_ProjectProperties.FindBugID(dlg.m_sInputText, pMsgView);
				}
			}
		}
		theApp.DoWaitCursor(-1);
		GetDlgItem(IDOK)->EnableWindow(TRUE);
	}
}

LRESULT CLogDlg::OnFindDialogMessage(WPARAM /*wParam*/, LPARAM /*lParam*/)
{
    ASSERT(m_pFindDialog != NULL);

    if (m_pFindDialog->IsTerminating())
    {
	    // invalidate the handle identifying the dialog box.
        m_pFindDialog = NULL;
        return 0;
    }

    if(m_pFindDialog->FindNext())
    {
        //read data from dialog
        CString FindText = m_pFindDialog->GetFindString();
        bool bMatchCase = (m_pFindDialog->MatchCase() == TRUE);
		bool bFound = FALSE;

		int i;
		for (i = this->m_nSearchIndex; i<m_LogList.GetItemCount()&&!bFound; i++)
		{
			if (bMatchCase)
			{
				if (m_arLogMessages.GetAt(i).Find(FindText) >= 0)
				{
					bFound = TRUE;
					break;
				}
				LogChangedPathArray * cpatharray = m_arLogPaths.GetAt(i);
				for (INT_PTR cpPathIndex = 0; cpPathIndex<cpatharray->GetCount(); ++cpPathIndex)
				{
					LogChangedPath * cpath = cpatharray->GetAt(cpPathIndex);
					if (cpath->sCopyFromPath.Find(FindText)>=0)
					{
						bFound = TRUE;
						--i;
						break;
					}
					if (cpath->sPath.Find(FindText)>=0)
					{
						bFound = TRUE;
						--i;
						break;
					}
				}
			}
			else
			{
				CString msg = m_arLogMessages.GetAt(i);
				msg = msg.MakeLower();
				CString find = FindText.MakeLower();
				if (msg.Find(find) >= 0)
				{
					bFound = TRUE;
					break;
				}
				LogChangedPathArray * cpatharray = m_arLogPaths.GetAt(i);
				for (INT_PTR cpPathIndex = 0; cpPathIndex<cpatharray->GetCount(); ++cpPathIndex)
				{
					LogChangedPath * cpath = cpatharray->GetAt(cpPathIndex);
					CString lowerpath = cpath->sCopyFromPath;
					lowerpath.MakeLower();
					if (lowerpath.Find(find)>=0)
					{
						bFound = TRUE;
						--i;
						break;
					}
					lowerpath = cpath->sPath;
					lowerpath.MakeLower();
					if (lowerpath.Find(find)>=0)
					{
						bFound = TRUE;
						--i;
						break;
					}
				}
			} 
		} // for (int i=this->m_nSearchIndex; i<m_LogList.GetItemCount(); i++) 
		if (bFound)
		{
			this->m_nSearchIndex = (i+1);
			m_LogList.EnsureVisible(i, FALSE);
			m_LogList.SetItemState(m_LogList.GetSelectionMark(), 0, LVIS_SELECTED);
			m_LogList.SetItemState(i, LVIS_SELECTED, LVIS_SELECTED);
			m_LogList.SetSelectionMark(i);
			FillLogMessageCtrl(m_arLogMessages.GetAt(i), m_arLogPaths.GetAt(i));
			UpdateData(FALSE);
			m_nSearchIndex++;
			if (m_nSearchIndex >= m_arLogMessages.GetCount())
				m_nSearchIndex = (int)m_arLogMessages.GetCount()-1;
		}
    } // if(m_pFindDialog->FindNext()) 

    return 0;
}

void CLogDlg::OnOK()
{
	CString temp;
	CString buttontext;
	GetDlgItem(IDOK)->GetWindowText(buttontext);
	temp.LoadString(IDS_MSGBOX_CANCEL);
	if (temp.Compare(buttontext) != 0)
		__super::OnOK();
	m_bCancelled = TRUE;
	if (m_pNotifyWindow)
	{
		int selIndex = m_LogList.GetSelectionMark();
		if (selIndex >= 0)
		{	
			m_pNotifyWindow->SendMessage(WM_REVSELECTED, m_wParam, m_arRevs.GetAt(selIndex));
		}
	}
}

void CLogDlg::OnNMDblclkLogmsg(NMHDR * /*pNMHDR*/, LRESULT *pResult)
{
	*pResult = 0;
	int selIndex = m_LogMsgCtrl.GetSelectionMark();
	if (selIndex < 0)
		return;
	int s = m_LogList.GetSelectionMark();
	if (s < 0)
		return;
	long rev = m_arRevs.GetAt(s);
	CString temp = m_LogMsgCtrl.GetItemText(selIndex, 0);
	temp = temp.Left(temp.Find(' '));
	CString t, tt;
	t.LoadString(IDS_SVNACTION_ADD);
	tt.LoadString(IDS_SVNACTION_DELETE);
	if ((rev > 1)&&(temp.Compare(t)!=0)&&(temp.Compare(tt)!=0))
	{
		DoDiffFromLog(selIndex, rev);
	}
}

void CLogDlg::DoDiffFromLog(int selIndex, long rev)
{
	GetDlgItem(IDOK)->EnableWindow(FALSE);
	SetPromptApp(&theApp);
	theApp.DoWaitCursor(1);
	//get the filename
	CString filepath;
	if (SVN::PathIsURL(m_path.GetSVNPathString()))
	{
		filepath = m_path.GetSVNPathString();
	}
	else
	{
		SVN svn;
		filepath = svn.GetURLFromPath(m_path);
		if (filepath.IsEmpty())
		{
			theApp.DoWaitCursor(-1);
			CString temp;
			temp.Format(IDS_ERR_NOURLOFFILE, filepath);
			CMessageBox::Show(this->m_hWnd, temp, _T("TortoiseSVN"), MB_ICONERROR);
			TRACE(_T("could not retrieve the URL of the file!\n"));
			GetDlgItem(IDOK)->EnableWindow(TRUE);
			theApp.DoWaitCursor(-11);
			return;		//exit
		}
	}
	CString temp = m_LogMsgCtrl.GetItemText(selIndex, 0);
	m_bCancelled = FALSE;
	filepath = GetRepositoryRoot(CTSVNPath(filepath));
	temp = temp.Mid(temp.Find(' '));
	if (temp.Find(_T(" (from "))>=0)
	{
		temp = temp.Left(temp.Find(_T(" (from "))-1);
	}
	temp = temp.Trim(_T(": "));
	filepath += temp;
	StartDiff(CTSVNPath(filepath), rev, CTSVNPath(filepath), rev-1);
	theApp.DoWaitCursor(-1);
	GetDlgItem(IDOK)->EnableWindow(TRUE);
}

BOOL CLogDlg::StartDiff(const CTSVNPath& path1, LONG rev1, const CTSVNPath& path2, LONG rev2)
{
	CTSVNPath tempfile1 = CUtils::GetTempFilePath(path1);
	CTSVNPath tempfile2 = CUtils::GetTempFilePath(path2);
	// The mere process of asking for a temp file creates it as a zero length file
	// We need to clean these up after us
	m_tempFileList.AddPath(tempfile1);
	m_tempFileList.AddPath(tempfile2);
	CProgressDlg progDlg;
	if (progDlg.IsValid())
	{
		progDlg.SetTitle(IDS_PROGRESSWAIT);
		progDlg.ShowModeless(this);
		progDlg.FormatPathLine(1, IDS_PROGRESSGETFILE, (LPCTSTR)path1.GetUIPathString());
		progDlg.FormatNonPathLine(2, IDS_PROGRESSREVISION, rev1);
	}
	m_bCancelled = FALSE;
	SetPromptApp(&theApp);
	theApp.DoWaitCursor(1);
	if (!Cat(path1, rev1, tempfile1))
	{
		theApp.DoWaitCursor(-1);
		CMessageBox::Show(NULL, GetLastErrorMessage(), _T("TortoiseSVN"), MB_ICONERROR);
		return FALSE;
	} // if (!Cat(path1, rev1, tempfile1))
	if (progDlg.IsValid())
	{
		progDlg.SetProgress(1, 2);
		progDlg.FormatPathLine(1, IDS_PROGRESSGETFILE, (LPCTSTR)path2.GetUIPathString());
		progDlg.FormatNonPathLine(2, IDS_PROGRESSREVISION, rev2);
	}
	if (!Cat(path2, rev2, tempfile2))
	{
		theApp.DoWaitCursor(-1);
		CMessageBox::Show(NULL, GetLastErrorMessage(), _T("TortoiseSVN"), MB_ICONERROR);
		return FALSE;
	}
	if (progDlg.IsValid())
	{
		progDlg.SetProgress(2,2);
		progDlg.Stop();
	}
	theApp.DoWaitCursor(-1);
	CString revname1, revname2;
	revname1.Format(_T("%s Revision %ld"), (LPCTSTR)path1.GetFileOrDirectoryName(), rev1);
	revname2.Format(_T("%s Revision %ld"), (LPCTSTR)path2.GetFileOrDirectoryName(), rev2);
	return CUtils::StartExtDiff(tempfile2, tempfile1, revname2, revname1);
}

BOOL CLogDlg::PreTranslateMessage(MSG* pMsg)
{
	if (pMsg->message == WM_KEYDOWN)
	{
		switch (pMsg->wParam)
		{
		case VK_F5:
			{
				if (!GetDlgItem(IDC_GETALL)->IsWindowEnabled())
					return __super::PreTranslateMessage(pMsg);
				OnBnClickedGetall();
			}
			break;
		case 'F':
			{
				if (GetKeyState(VK_CONTROL)&0x8000)
				{
					if (m_pFindDialog)
					{
						break;
					}
					else
					{
						m_pFindDialog = new CFindReplaceDialog();
						m_pFindDialog->Create(TRUE, NULL, NULL, FR_HIDEUPDOWN | FR_HIDEWHOLEWORD, this);									
					}
				}
			}	
			break;
		case VK_F3:
			{
				if (m_pFindDialog)
				{
					break;
				}
				else
				{
					m_pFindDialog = new CFindReplaceDialog();
					m_pFindDialog->Create(TRUE, NULL, NULL, FR_HIDEUPDOWN | FR_HIDEWHOLEWORD, this);									
				}
			}
			break;
		default:
			break;
		}
	}

	return __super::PreTranslateMessage(pMsg);
}

BOOL CLogDlg::OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message)
{
	if (m_bThreadRunning)
	{
		// only show the wait cursor over the list control
		if ((pWnd)&&
			((pWnd == GetDlgItem(IDC_LOGLIST))||
			(pWnd == GetDlgItem(IDC_MSGVIEW))||
			(pWnd == GetDlgItem(IDC_LOGMSG))))
		{
			HCURSOR hCur = LoadCursor(NULL, MAKEINTRESOURCE(IDC_WAIT));
			SetCursor(hCur);
			return TRUE;
		}
	}
	HCURSOR hCur = LoadCursor(NULL, MAKEINTRESOURCE(IDC_ARROW));
	SetCursor(hCur);
	return CResizableStandAloneDialog::OnSetCursor(pWnd, nHitTest, message);
}

void CLogDlg::OnBnClickedHelp()
{
	OnHelp();
}

void CLogDlg::OnLvnItemchangedLoglist(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMLISTVIEW pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);
	*pResult = 0;
	if (pNMLV->iItem >= 0)
	{
		int selIndex = pNMLV->iItem;
		m_nSearchIndex = selIndex;
		if (pNMLV->iSubItem != 0)
			return;
		if (pNMLV->uNewState & LVIS_SELECTED)
		{
			if (m_LogList.GetSelectedCount() > 1)
				FillLogMessageCtrl(_T(" "), NULL);
			else
				FillLogMessageCtrl(m_arLogMessages.GetAt(selIndex), m_arLogPaths.GetAt(selIndex));
			UpdateData(FALSE);
		}
	}
}

void CLogDlg::OnEnLinkMsgview(NMHDR *pNMHDR, LRESULT *pResult)
{
	ENLINK *pEnLink = reinterpret_cast<ENLINK *>(pNMHDR);
	if (pEnLink->msg == WM_LBUTTONUP)
	{
		CString url, msg;
		GetDlgItem(IDC_MSGVIEW)->GetWindowText(msg);
		msg.Replace(_T("\r\n"), _T("\n"));
		url = msg.Mid(pEnLink->chrg.cpMin, pEnLink->chrg.cpMax-pEnLink->chrg.cpMin);
		if (!::PathIsURL(url))
		{
			url = m_ProjectProperties.GetBugIDUrl(url);
		}
		if (!url.IsEmpty())
			ShellExecute(this->m_hWnd, _T("open"), url, NULL, NULL, SW_SHOWDEFAULT);
	}
	*pResult = 0;
}

void CLogDlg::OnBnClickedStatbutton()
{
	if (m_bThreadRunning)
		return;
	CStatGraphDlg dlg;
	dlg.m_parAuthors = &m_arAuthors;
	dlg.m_parDates = &m_arDates;
	dlg.m_parFileChanges = &m_arFileChanges;
	dlg.DoModal();
		
}

void CLogDlg::OnNMCustomdrawLoglist(NMHDR *pNMHDR, LRESULT *pResult)
{
	NMLVCUSTOMDRAW* pLVCD = reinterpret_cast<NMLVCUSTOMDRAW*>( pNMHDR );
	// Take the default processing unless we set this to something else below.
	*pResult = CDRF_DODEFAULT;

	// First thing - check the draw stage. If it's the control's prepaint
	// stage, then tell Windows we want messages for every item.

	if ( CDDS_PREPAINT == pLVCD->nmcd.dwDrawStage )
	{
		*pResult = CDRF_NOTIFYITEMDRAW;
	}
	else if ( CDDS_ITEMPREPAINT == pLVCD->nmcd.dwDrawStage )
	{
		// This is the prepaint stage for an item. Here's where we set the
		// item's text color. Our return value will tell Windows to draw the
		// item itself, but it will use the new color we set here.

		// Tell Windows to paint the control itself.
		*pResult = CDRF_DODEFAULT;

		COLORREF crText = GetSysColor(COLOR_WINDOWTEXT);

		if (m_arCopies.GetCount() > (INT_PTR)pLVCD->nmcd.dwItemSpec)
		{
			if (m_arCopies.GetAt(pLVCD->nmcd.dwItemSpec))
				crText = GetSysColor(COLOR_HIGHLIGHT);

			// Store the color back in the NMLVCUSTOMDRAW struct.
			pLVCD->clrText = crText;
		}
	}
}
















