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
#include "messagebox.h"
#include "SVNProgressDlg.h"
#include "LogDlg.h"
#include "TSVNPath.h"
#include "Registry.h"
#include "SVNStatus.h"
#include "Utils.h"
#include "UnicodeUtils.h"


// CSVNProgressDlg dialog

BOOL	CSVNProgressDlg::m_bAscending = FALSE;
int		CSVNProgressDlg::m_nSortedColumn = -1;

IMPLEMENT_DYNAMIC(CSVNProgressDlg, CResizableStandAloneDialog)
CSVNProgressDlg::CSVNProgressDlg(CWnd* pParent /*=NULL*/)
	: CResizableStandAloneDialog(CSVNProgressDlg::IDD, pParent)
	, m_Revision(_T("HEAD"))
	, m_RevisionEnd(0)
{
	m_bCloseOnEnd = FALSE;
	m_bCancelled = FALSE;
	m_bThreadRunning = FALSE;
	m_bConflictsOccurred = FALSE;
	m_nUpdateStartRev = -1;
	m_pThread = NULL;
	m_options = ProgOptNone;

	m_pSvn = this;
}

CSVNProgressDlg::~CSVNProgressDlg()
{
	m_templist.DeleteAllFiles();
	for (size_t i=0; i<m_arData.size(); i++)
	{
		delete m_arData[i];
	} // for (int i=0; i<m_arData.GetCount(); i++)
	if(m_pThread != NULL)
	{
		delete m_pThread;
	}
}

void CSVNProgressDlg::DoDataExchange(CDataExchange* pDX)
{
	CResizableStandAloneDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_SVNPROGRESS, m_ProgList);
}


BEGIN_MESSAGE_MAP(CSVNProgressDlg, CResizableStandAloneDialog)
	ON_BN_CLICKED(IDC_LOGBUTTON, OnBnClickedLogbutton)
	ON_NOTIFY(NM_CUSTOMDRAW, IDC_SVNPROGRESS, OnNMCustomdrawSvnprogress)
	ON_WM_CLOSE()
	ON_NOTIFY(NM_DBLCLK, IDC_SVNPROGRESS, OnNMDblclkSvnprogress)
	ON_NOTIFY(HDN_ITEMCLICK, 0, OnHdnItemclickSvnprogress)
	ON_WM_SETCURSOR()
	ON_WM_CONTEXTMENU()
END_MESSAGE_MAP()


BOOL CSVNProgressDlg::Cancel()
{
	return m_bCancelled;
}

void CSVNProgressDlg::AddItemToList(const NotificationData* pData)
{
	int count = m_ProgList.GetItemCount();

	int iInsertedAt = m_ProgList.InsertItem(count, pData->sActionColumnText);
	if (iInsertedAt != -1)
	{
		m_ProgList.SetItemText(iInsertedAt, 1, pData->sPathColumnText);
		m_ProgList.SetItemText(iInsertedAt, 2, pData->mime_type);

		//make columns width fit
		if (iFirstResized < 30)
		{
			//only resize the columns for the first 30 or so entries.
			//after that, don't resize them anymore because that's an
			//expensive function call and the columns will be sized
			//close enough already.
			ResizeColumns();
			iFirstResized++;
		}
	
		// Make sure the item is *entirely* visible even if the horizontal
		// scroll bar is visible.
		m_ProgList.EnsureVisible(iInsertedAt, false);
	}
}

BOOL CSVNProgressDlg::Notify(const CTSVNPath& path, svn_wc_notify_action_t action, svn_node_kind_t kind, const CString& mime_type, svn_wc_notify_state_t content_state, svn_wc_notify_state_t prop_state, LONG rev)
{
	NotificationData * data = new NotificationData();
	data->path = path;
	data->action = action;
	data->kind = kind;
	data->mime_type = mime_type;
	data->content_state = content_state;
	data->prop_state = prop_state;
	data->rev = rev;

	switch (data->action)
	{
	case svn_wc_notify_add:
	case svn_wc_notify_update_add:
	case svn_wc_notify_commit_added:
		data->color = GetSysColor(COLOR_HIGHLIGHT);
		break;
	case svn_wc_notify_delete:
	case svn_wc_notify_update_delete:
	case svn_wc_notify_commit_deleted:
	case svn_wc_notify_commit_replaced:
		data->color = RGB(100,0,0);
		break;
	case svn_wc_notify_update_update:
		if ((data->content_state == svn_wc_notify_state_conflicted) || (data->prop_state == svn_wc_notify_state_conflicted))
		{
			data->color = RGB(255, 0, 0);
			data->bConflictedActionItem = true;
			m_bConflictsOccurred = true;
		}
		else if ((data->content_state == svn_wc_notify_state_merged) || (data->prop_state == svn_wc_notify_state_merged))
		{
			data->color = RGB(0, 100, 0);
		}
		break;
	case svn_wc_notify_commit_modified:
		data->color = GetSysColor(COLOR_HIGHLIGHT);
		break;

	case svn_wc_notify_update_external:
		// For some reason we build a list of externals...
		m_ExtStack.AddHead(path.GetUIPathString());
		break;

	case svn_wc_notify_update_completed:
		{
			data->bAuxItem = true;

			if (!m_ExtStack.IsEmpty())
				data->sPathColumnText.Format(IDS_PROGRS_PATHATREV, (LPCTSTR)m_ExtStack.RemoveHead(), rev);
			else
				data->sPathColumnText.Format(IDS_PROGRS_ATREV, rev);

			if ((m_bConflictsOccurred)&&(m_ExtStack.IsEmpty()))
			{
				// We're going to add another aux item - let's shove this current onto the list first
				// I don't really like this, but it will do for the moment.
				m_arData.push_back(data);
				AddItemToList(data);

				data = new NotificationData();
				data->bAuxItem = true;
				data->sActionColumnText.LoadString(IDS_PROGRS_CONFLICTSOCCURED_WARNING);
				data->sPathColumnText.LoadString(IDS_PROGRS_CONFLICTSOCCURED);
				data->color = RGB(255,0,0);

				// This item will now be added after the switch statement
			}
			m_RevisionEnd = rev;

		}
		break;

	default:
		// Lots of actions fall through here
		break;
	} // switch (action)

	if (data->sActionColumnText.IsEmpty())
	{
		data->sActionColumnText = SVN::GetActionText(action, content_state, prop_state);
	}
	if(!data->bAuxItem)
	{
		data->sPathColumnText = path.GetUIPathString();
	}

	m_arData.push_back(data);
	AddItemToList(data);

	if ((action == svn_wc_notify_commit_postfix_txdelta)&&(bSecondResized == FALSE))
	{
		ResizeColumns();
		bSecondResized = TRUE;
	}

	return TRUE;
}

CString CSVNProgressDlg::BuildInfoString()
{
	CString infotext;
	CString temp;
	int added = 0;
	int copied = 0;
	int deleted = 0;
	int restored = 0;
	int reverted = 0;
	int resolved = 0;
	int conflicted = 0;
	int updated = 0;
	int merged = 0;
	int modified = 0;

	for (size_t i=0; i<m_arData.size(); ++i)
	{
		const NotificationData * dat = m_arData[i];
		switch (dat->action)
		{
		case svn_wc_notify_add:
		case svn_wc_notify_update_add:
		case svn_wc_notify_commit_added:
			added++;
			break;
		case svn_wc_notify_copy:
			copied++;
			break;
		case svn_wc_notify_delete:
		case svn_wc_notify_update_delete:
			deleted++;
			break;
		case svn_wc_notify_restore:
			restored++;
			break;
		case svn_wc_notify_revert:
			reverted++;
			break;
		case svn_wc_notify_resolved:
			resolved++;
			break;
		case svn_wc_notify_update_update:
			if (dat->bConflictedActionItem)
				conflicted++;
			else if ((dat->content_state == svn_wc_notify_state_merged) || (dat->prop_state == svn_wc_notify_state_merged))
				merged++;
			else
				updated++;
			break;
		case svn_wc_notify_commit_modified:
			modified++;
			break;
		}
	}
	if (conflicted)
	{
		temp.LoadString(IDS_SVNACTION_CONFLICTED);
		infotext += temp;
		temp.Format(_T(":%d "), conflicted);
		infotext += temp;
	}
	if (merged)
	{
		temp.LoadString(IDS_SVNACTION_MERGED);
		infotext += temp;
		infotext.AppendFormat(_T(":%d "), merged);
	}
	if (added)
	{
		temp.LoadString(IDS_SVNACTION_ADD);
		infotext += temp;
		infotext.AppendFormat(_T(":%d "), added);
	}
	if (deleted)
	{
		temp.LoadString(IDS_SVNACTION_DELETE);
		infotext += temp;
		infotext.AppendFormat(_T(":%d "), deleted);
	}
	if (modified)
	{
		temp.LoadString(IDS_SVNACTION_MODIFIED);
		infotext += temp;
		infotext.AppendFormat(_T(":%d "), modified);
	}
	if (copied)
	{
		temp.LoadString(IDS_SVNACTION_COPY);
		infotext += temp;
		infotext.AppendFormat(_T(":%d "), copied);
	}
	if (updated)
	{
		temp.LoadString(IDS_SVNACTION_UPDATE);
		infotext += temp;
		infotext.AppendFormat(_T(":%d "), updated);
	}
	if (restored)
	{
		temp.LoadString(IDS_SVNACTION_RESTORE);
		infotext += temp;
		infotext.AppendFormat(_T(":%d "), restored);
	}
	if (reverted)
	{
		temp.LoadString(IDS_SVNACTION_REVERT);
		infotext += temp;
		infotext.AppendFormat(_T(":%d "), reverted);
	}
	if (resolved)
	{
		temp.LoadString(IDS_SVNACTION_RESOLVE);
		infotext += temp;
		infotext.AppendFormat(_T(":%d "), resolved);
	}
	return infotext;
}

void CSVNProgressDlg::SetParams(Command cmd, int options, const CTSVNPathList& pathList, const CString& url /* = "" */, const CString& message /* = "" */, SVNRev revision /* = -1 */)
{
	m_Command = cmd;
	m_options = options;

	m_targetPathList = pathList;

	//WGD - I'm removing this for the moment, because it can actually happen.
	// For example, do an Add, then select all the items in the list box, right-click and choose 'Add'.
	// Then click OK (on the now empty list)
	// Ultimately, I think it might be better to stop the progress dialog being opened when there's no work to do
	// but not just yet.
//	ASSERT(m_targetPathList.GetCount() > 0);

	m_url.SetFromUnknown(url);
	m_sMessage = message;
	m_Revision = revision;
}

void CSVNProgressDlg::ResizeColumns()
{
	m_ProgList.SetRedraw(FALSE);

	int mincol = 0;
	int maxcol = ((CHeaderCtrl*)(m_ProgList.GetDlgItem(0)))->GetItemCount()-1;

	int col;
	for (col = mincol; col <= maxcol; col++)
	{
		m_ProgList.SetColumnWidth(col,LVSCW_AUTOSIZE_USEHEADER);
	}
	
	m_ProgList.SetRedraw(TRUE);
	
}


// CSVNProgressDlg message handlers

BOOL CSVNProgressDlg::OnInitDialog()
{
	__super::OnInitDialog();

	m_ProgList.SetExtendedStyle (LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_DOUBLEBUFFER);

	m_ProgList.DeleteAllItems();
	int c = ((CHeaderCtrl*)(m_ProgList.GetDlgItem(0)))->GetItemCount()-1;
	while (c>=0)
		m_ProgList.DeleteColumn(c--);
	CString temp;
	temp.LoadString(IDS_PROGRS_ACTION);
	m_ProgList.InsertColumn(0, temp);
	temp.LoadString(IDS_PROGRS_PATH);
	m_ProgList.InsertColumn(1, temp);
	temp.LoadString(IDS_PROGRS_MIMETYPE);
	m_ProgList.InsertColumn(2, temp);

	//first start a thread to obtain the log messages without
	//blocking the dialog
	m_pThread = AfxBeginThread(ProgressThreadEntry, this, THREAD_PRIORITY_NORMAL,0,CREATE_SUSPENDED);
	if (m_pThread==NULL)
	{
		CMessageBox::Show(this->m_hWnd, IDS_ERR_THREADSTARTFAILED, IDS_APPNAME, MB_ICONERROR);
	}
	else
	{
		m_pThread->m_bAutoDelete = FALSE;
		m_pThread->ResumeThread();
	}

	UpdateData(FALSE);

	// Call this early so that the column headings aren't hidden before any
	// text gets added.
	ResizeColumns();

	AddAnchor(IDC_SVNPROGRESS, TOP_LEFT, BOTTOM_RIGHT);
	AddAnchor(IDC_INFOTEXT, BOTTOM_LEFT, BOTTOM_RIGHT);
	AddAnchor(IDCANCEL, BOTTOM_RIGHT);
	AddAnchor(IDOK, BOTTOM_RIGHT);
	AddAnchor(IDC_LOGBUTTON, BOTTOM_RIGHT);
	this->SetPromptParentWindow(this->m_hWnd);
	CenterWindow(CWnd::FromHandle(hWndExplorer));
	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

void CSVNProgressDlg::ReportSVNError() const
{
	TRACE(_T("%s"), (LPCTSTR)m_pSvn->GetLastErrorMessage());
	CMessageBox::Show(m_hWnd, m_pSvn->GetLastErrorMessage(), _T("TortoiseSVN"), MB_ICONERROR);
}

UINT CSVNProgressDlg::ProgressThreadEntry(LPVOID pVoid)
{
	// to make gettext happy
	SetThreadLocale(CRegDWORD(_T("Software\\TortoiseSVN\\LanguageID"), 1033));

	return ((CSVNProgressDlg*)pVoid)->ProgressThread();
}

UINT CSVNProgressDlg::ProgressThread()
{
	int updateFileCounter = 0;

	// The SetParams function should have loaded something for us
// See comments in SetParams about this ASSERT
//	ASSERT(m_targetPathList.GetCount() > 0);

	CString temp;
	CString sWindowTitle;
	CString sTempWindowTitle;

	GetDlgItem(IDOK)->EnableWindow(FALSE);
	GetDlgItem(IDCANCEL)->EnableWindow(TRUE);

	m_bThreadRunning = TRUE;
	iFirstResized = 0;
	bSecondResized = FALSE;
	switch (m_Command)
	{
		case Checkout:			//no tempfile!
			ASSERT(m_targetPathList.GetCount() == 1);
			sWindowTitle.LoadString(IDS_PROGRS_TITLE_CHECKOUT);
			sTempWindowTitle = m_url.GetFileOrDirectoryName()+_T(" - ")+sWindowTitle;
			SetWindowText(sTempWindowTitle);
			if (!m_pSvn->Checkout(m_url, m_targetPathList[0], m_Revision, m_options & ProgOptRecursive))
			{
				ReportSVNError();
			}
			break;
		case Import:			//no tempfile!
			ASSERT(m_targetPathList.GetCount() == 1);
			sWindowTitle.LoadString(IDS_PROGRS_TITLE_IMPORT);
			sTempWindowTitle = m_targetPathList[0].GetFileOrDirectoryName()+_T(" - ")+sWindowTitle;
			SetWindowText(sTempWindowTitle);
			if (!m_pSvn->Import(m_targetPathList[0], m_url, m_sMessage, true))
			{
				ReportSVNError();
			}
			break;
		case Update:
			sWindowTitle.LoadString(IDS_PROGRS_TITLE_UPDATE);
			SetWindowText(sWindowTitle);
			updateFileCounter = 0;
			{
				int targetcount = m_targetPathList.GetCount();
				CString sfile;
				CStringA uuid;
				typedef std::map<CStringA, LONG> UuidMap;
				UuidMap uuidmap;
				bool bRecursive = !!(m_options & ProgOptRecursive);
				SVNRev revstore = m_Revision;
				for(int nItem = 0; nItem < m_targetPathList.GetCount(); nItem++)
				{
					const CTSVNPath& targetPath = m_targetPathList[nItem];
					SVNStatus st;
					LONG headrev = -1;
					m_Revision = revstore;
					if (m_Revision.IsHead())
					{
						if ((targetcount > 1)&&((headrev = st.GetStatus(targetPath.GetSVNPathString(), true)) != (-2)))
						{
							if (st.status->entry != NULL)
							{
								m_nUpdateStartRev = st.status->entry->cmt_rev;
								if (st.status->entry->uuid)
								{
									uuid = st.status->entry->uuid;
									UuidMap::const_iterator iter;
									if ((iter = uuidmap.find(uuid)) == uuidmap.end())
										uuidmap[uuid] = headrev;
									else
										headrev = iter->second;
									m_Revision = headrev;
								} // if (st.status->entry->uuid)
								else
									m_Revision = headrev;
							} // if (st.status->entry != NULL) 
						} // if ((headrev = st.GetStatus(strLine)) != (-2)) 
						else
						{
							if ((headrev = st.GetStatus(targetPath.GetSVNPathString(), FALSE)) != (-2))
							{
								if (st.status->entry != NULL)
									m_nUpdateStartRev = st.status->entry->cmt_rev;
							}
						}
					} // if (m_Revision.IsHead()) 
					TRACE(_T("update file %s\n"), targetPath.GetWinPath());
					SetWindowText(targetPath.GetFileOrDirectoryName()+_T(" - ")+sWindowTitle);
					if (!m_pSvn->Update(targetPath, m_Revision, bRecursive))
					{
						ReportSVNError();
						break;
					}
					updateFileCounter++;
					m_updatedPath = targetPath;
				} // while (file.ReadString(strLine)) 

				// after an update, show the user the log button, but only if only one single item was updated
				// (either a file or a directory)
				if (updateFileCounter == 1)
					GetDlgItem(IDC_LOGBUTTON)->ShowWindow(SW_SHOW);
			} 
			break;
		case Commit:
			{
				sWindowTitle.LoadString(IDS_PROGRS_TITLE_COMMIT);
				SetWindowText(sWindowTitle);
				if (m_targetPathList.GetCount()==0)
				{
					SetWindowText(sWindowTitle);
					temp.LoadString(IDS_MSGBOX_OK);

					GetDlgItem(IDCANCEL)->EnableWindow(FALSE);
					GetDlgItem(IDOK)->EnableWindow(TRUE);

					m_bThreadRunning = FALSE;
					GetDlgItem(IDOK)->EnableWindow(true);
					break;
				}
				if (m_targetPathList.GetCount()==1)
				{
					sTempWindowTitle = m_targetPathList[0].GetFileOrDirectoryName()+_T(" - ")+sWindowTitle;
					SetWindowText(sTempWindowTitle);
				}
				BOOL isTag = FALSE;
				BOOL bURLFetched = FALSE;
				CString url;
				for (int i=0; i<m_targetPathList.GetCount(); ++i)
				{
					if (bURLFetched == FALSE)
					{
						url = m_pSvn->GetURLFromPath(m_targetPathList[i]);
						if (!url.IsEmpty())
							bURLFetched = TRUE;
						CString urllower = url;
						urllower.MakeLower();
						// test if the commit goes to a tag.
						// now since Subversion doesn't force users to
						// create tags in the recommended /tags/ folder
						// only a warning is shown. This won't work if the tags
						// are stored in a non-recommended place, but the check
						// still helps those who do.
						if (urllower.Find(_T("/tags/"))>=0)
							isTag = TRUE;
						break;
					}
				}
				if (isTag)
				{
					if (CMessageBox::Show(m_hWnd, IDS_PROGRS_COMMITT_TRUNK, IDS_APPNAME, MB_OKCANCEL | MB_DEFBUTTON2 | MB_ICONEXCLAMATION)==IDCANCEL)
						break;
				}
				if (!m_pSvn->Commit(m_targetPathList, m_sMessage, (m_Revision == 0)))
				{
					ReportSVNError();
				}
			}
			break;
		case Add:
			sWindowTitle.LoadString(IDS_PROGRS_TITLE_ADD);
			SetWindowText(sWindowTitle);
			if (!m_pSvn->Add(m_targetPathList, false))
			{
				ReportSVNError();
			}
			break;
		case Revert:
			sWindowTitle.LoadString(IDS_PROGRS_TITLE_REVERT);
			SetWindowText(sWindowTitle);
			if (!m_pSvn->Revert(m_targetPathList, !!(m_options & ProgOptRecursive)))
			{
				ReportSVNError();
				break;
			}
			break;
		case Resolve:
			{
				ASSERT(m_targetPathList.GetCount() == 1);
				sWindowTitle.LoadString(IDS_PROGRS_TITLE_RESOLVE);
				SetWindowText(sWindowTitle);
				//check if the file may still have conflict markers in it.
				BOOL bMarkers = FALSE;
				try
				{
					if (!m_targetPathList[0].IsDirectory())
					{
						CStdioFile file(m_targetPathList[0].GetWinPath(), CFile::typeBinary | CFile::modeRead);
						CString strLine = _T("");
						while (file.ReadString(strLine))
						{
							if (strLine.Find(_T("<<<<<<<"))==0)
							{
								bMarkers = TRUE;
								break;
							}
						}
						file.Close();
					} // if (!PathIsDirectory(m_sPath)) 
				} 
				catch (CFileException* pE)
				{
					TRACE(_T("CFileException in Resolve!\n"));
					TCHAR error[10000] = {0};
					pE->GetErrorMessage(error, 10000);
					CMessageBox::Show(NULL, error, _T("TortoiseSVN"), MB_ICONERROR);
					pE->Delete();
				}
				if (bMarkers)
				{
					if (CMessageBox::Show(m_hWnd, IDS_PROGRS_REVERTMARKERS, IDS_APPNAME, MB_YESNO | MB_ICONQUESTION)==IDYES)
						m_pSvn->Resolve(m_targetPathList[0], true);
				} // if (bMarkers)
				else
					m_pSvn->Resolve(m_targetPathList[0], true);
			}
			break;
		case Switch:
			{
				ASSERT(m_targetPathList.GetCount() == 1);
				SVNStatus st;
				sWindowTitle.LoadString(IDS_PROGRS_TITLE_SWITCH);
				SetWindowText(sWindowTitle);
				LONG rev = 0;
				if (st.GetStatus(m_targetPathList[0].GetWinPath()) != (-2))
				{
					if (st.status->entry != NULL)
					{
						rev = st.status->entry->revision;
					}
				}
				if (!m_pSvn->Switch(m_targetPathList[0], m_url, m_Revision, true))
				{
					ReportSVNError();
					break;
				}
				m_nUpdateStartRev = rev;
				if ((m_RevisionEnd >= 0)&&(m_nUpdateStartRev >= 0)
					&&((LONG)m_RevisionEnd > (LONG)m_nUpdateStartRev))
					GetDlgItem(IDC_LOGBUTTON)->ShowWindow(SW_SHOW);
			}
			break;
		case Export:
			ASSERT(m_targetPathList.GetCount() == 1);
			sWindowTitle.LoadString(IDS_PROGRS_TITLE_EXPORT);
			sTempWindowTitle = m_url.GetFileOrDirectoryName()+_T(" - ")+sWindowTitle;
			SetWindowText(sTempWindowTitle);
			if (!m_pSvn->Export(m_url.GetSVNPathString(), m_targetPathList[0].GetSVNPathString(), m_Revision))
			{
				ReportSVNError();
			}
			break;
		case Merge:
			{
				ASSERT(m_targetPathList.GetCount() == 1);
				sWindowTitle.LoadString(IDS_PROGRS_TITLE_MERGE);
				SetWindowText(sWindowTitle);
				// Eeek!  m_sMessage is actually a path for this command...
				CTSVNPath urlTo(m_sMessage);
				if (m_url.IsEquivalentTo(urlTo))
				{
					if (!m_pSvn->PegMerge(m_url, m_Revision, m_RevisionEnd, 
						m_url.IsUrl() ? m_Revision : SVNRev(SVNRev::REV_WC), 
						m_targetPathList[0], true, true, false, !!(m_options & ProgOptDryRun)))
					{
						ReportSVNError();
					}
				}
				else
				{
					if (!m_pSvn->Merge(m_url, m_Revision, urlTo, m_RevisionEnd, m_targetPathList[0], 
						true, true, false, !!(m_options & ProgOptDryRun)))
					{
						ReportSVNError();
					}
				}
			}
			break;
		case Copy:
			ASSERT(m_targetPathList.GetCount() == 1);
			sWindowTitle.LoadString(IDS_PROGRS_TITLE_COPY);
			SetWindowText(sWindowTitle);
			if (!m_pSvn->Copy(m_targetPathList[0], m_url, m_Revision, m_sMessage))
			{
				ReportSVNError();
				break;
			}
			CString sTemp;
			sTemp.LoadString(IDS_PROGRS_COPY_WARNING);
			CMessageBox::Show(m_hWnd, sTemp, _T("TortoiseSVN"), MB_OK | MB_ICONINFORMATION);
			break;
	}
	temp.LoadString(IDS_PROGRS_TITLEFIN);
	sWindowTitle = sWindowTitle + _T(" ") + temp;
	SetWindowText(sWindowTitle);

	GetDlgItem(IDCANCEL)->EnableWindow(FALSE);
	GetDlgItem(IDOK)->EnableWindow(TRUE);

	m_bCancelled = TRUE;
	m_bThreadRunning = FALSE;
	POINT pt;
	GetCursorPos(&pt);
	SetCursorPos(pt.x, pt.y);

	CString info = BuildInfoString();
	GetDlgItem(IDC_INFOTEXT)->SetWindowText(info);
	ResizeColumns();

	if (((WORD)CRegStdWORD(_T("Software\\TortoiseSVN\\AutoClose"), FALSE)&&(!(m_options & ProgOptDryRun))
	 ||	m_bCloseOnEnd))
	{
		if (!(WORD)CRegStdWORD(_T("Software\\TortoiseSVN\\AutoCloseNoForReds"), FALSE) || (!m_bConflictsOccurred) || m_bCloseOnEnd) 
			PostMessage(WM_COMMAND, 1, (LPARAM)GetDlgItem(IDOK)->m_hWnd);
	}
	//Don't do anything here which might cause messages to be sent to the window
	//The window thread is probably now blocked in OnOK if we've done an autoclose
	return 0;
}

void CSVNProgressDlg::OnBnClickedLogbutton()
{
	CLogDlg dlg;
	dlg.SetParams(m_updatedPath.GetWinPathString(), m_RevisionEnd, m_nUpdateStartRev);
	dlg.DoModal();
}


void CSVNProgressDlg::OnClose()
{
	if (m_bCancelled)
	{
		TerminateThread(m_pThread->m_hThread, (DWORD)-1);
		m_bThreadRunning = FALSE;
	}
	else
	{
		m_bCancelled = TRUE;
		return;
	}
	GetDlgItem(IDCANCEL)->EnableWindow();
	__super::OnClose();
}

void CSVNProgressDlg::OnOK()
{
	if ((m_bCancelled)&&(!m_bThreadRunning))
	{
		// I have made this wait a sensible amount of time (10 seconds) for the thread to finish
		// You must be careful in the thread that after posting the WM_COMMAND/IDOK message, you 
		// don't do any more operations on the window which might require message passing
		// If you try to send windows messages once we're waiting here, then the thread can't finished
		// because the Window's message loop is blocked at this wait
		WaitForSingleObject(m_pThread->m_hThread, 10000);
		__super::OnOK();
	}
	m_bCancelled = TRUE;
}

void CSVNProgressDlg::OnCancel()
{
	if ((m_bCancelled)&&(!m_bThreadRunning))
		__super::OnCancel();
	m_bCancelled = TRUE;
}

void CSVNProgressDlg::OnNMCustomdrawSvnprogress(NMHDR *pNMHDR, LRESULT *pResult)
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

		ASSERT(pLVCD->nmcd.dwItemSpec <  m_arData.size());
		if(pLVCD->nmcd.dwItemSpec >= m_arData.size())
		{
			return;
		}
		const NotificationData * data = m_arData[pLVCD->nmcd.dwItemSpec];
		ASSERT(data != NULL);
		if (data == NULL)
			return;

		// Store the color back in the NMLVCUSTOMDRAW struct.
		pLVCD->clrText = data->color;
	}
}

void CSVNProgressDlg::OnNMDblclkSvnprogress(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMLISTVIEW pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);
	*pResult = 0;
	if (pNMLV->iItem < 0)
		return;

	const NotificationData * data = m_arData[pNMLV->iItem];
	ASSERT(data != NULL);
	if (data->bConflictedActionItem)
	{
		// We've double-clicked on a conflicted item - do a three-way merge on it
		SVN::StartConflictEditor(data->path);
	}
	else if ((data->action == svn_wc_notify_update_update) && ((data->content_state == svn_wc_notify_state_merged)||(Enum_Merge == m_Command)))
	{
		// This is a modified file which has been merged on update
		// Diff it against base
		CTSVNPath temporaryFile;
		SVN::DiffFileAgainstBase(data->path, temporaryFile);
		if(!temporaryFile.IsEmpty())
		{
			m_templist.AddPath(temporaryFile);
		}
	}
}

void CSVNProgressDlg::OnHdnItemclickSvnprogress(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMHEADER phdr = reinterpret_cast<LPNMHEADER>(pNMHDR);
	if (m_bThreadRunning)
		return;
	if (m_nSortedColumn == phdr->iItem)
		m_bAscending = !m_bAscending;
	else
		m_bAscending = TRUE;
	m_nSortedColumn = phdr->iItem;
	Sort();

	CString temp;
	m_ProgList.SetRedraw(FALSE);
	m_ProgList.DeleteAllItems();
	for (size_t i=0; i<m_arData.size(); i++)
	{
		AddItemToList(m_arData[i]);
	} 
	
	m_ProgList.SetRedraw(TRUE);

	*pResult = 0;
}

bool CSVNProgressDlg::NotificationDataIsAux(const NotificationData* pData)
{
	return pData->bAuxItem;
}

void CSVNProgressDlg::Sort()
{
	if(m_arData.size() < 2)
	{
		return;
	}

	// We need to sort the blocks which lie between the auxiliary entries
	// This is so that any aux data stays where it was
	NotificationDataVect::iterator actionBlockBegin;
	NotificationDataVect::iterator actionBlockEnd = m_arData.begin();	// We start searching from here

	for(;;)
	{
		// Search to the start of the non-aux entry in the next block
		actionBlockBegin = std::find_if(actionBlockEnd, m_arData.end(), std::not1(std::ptr_fun(&CSVNProgressDlg::NotificationDataIsAux)));
		if(actionBlockBegin == m_arData.end())
		{
			// There are no more actions
			break;
		}
		// Now search to find the end of the block
		actionBlockEnd = std::find_if(actionBlockBegin+1, m_arData.end(), std::ptr_fun(&CSVNProgressDlg::NotificationDataIsAux));
		// Now sort the block
		std::sort(actionBlockBegin, actionBlockEnd, &CSVNProgressDlg::SortCompare);
	}
}

bool CSVNProgressDlg::SortCompare(const NotificationData * pData1, const NotificationData * pData2)
{
	int result = 0;
	switch (m_nSortedColumn)
	{
	case 0:		//action column
		result = pData1->sActionColumnText.Compare(pData2->sActionColumnText);
		break;
	case 1:		//path column
		// Compare happens after switch()
		break;
	case 2:		//mime-type column
		result = pData1->mime_type.Compare(pData2->mime_type);
		break;
	default:
		break;
	} // switch (m_nSortedColumn)

	// Sort by path if everything else is equal
	if (result == 0)
	{
		result = CTSVNPath::Compare(pData1->path, pData2->path);
	}

	if (!m_bAscending)
		result = -result;
	return result < 0;
}

BOOL CSVNProgressDlg::OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message)
{
	if (!GetDlgItem(IDOK)->IsWindowEnabled())
	{
		// only show the wait cursor over the list control
		if ((pWnd)&&(pWnd == GetDlgItem(IDC_SVNPROGRESS)))
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

BOOL CSVNProgressDlg::PreTranslateMessage(MSG* pMsg)
{
	if (pMsg->message == WM_KEYDOWN)
	{
		if (pMsg->wParam == 'A')
		{
			if (GetKeyState(VK_CONTROL)&0x8000)
			{
				// Ctrl-A -> select all
				m_ProgList.SetSelectionMark(0);
				for (int i=0; i<m_ProgList.GetItemCount(); ++i)
				{
					m_ProgList.SetItemState(i, LVIS_SELECTED, LVIS_SELECTED);
				}
			}
		}
		if (pMsg->wParam == 'C')
		{
			int selIndex = m_ProgList.GetSelectionMark();
			if (selIndex >= 0)
			{
				if (GetKeyState(VK_CONTROL)&0x8000)
				{
					//Ctrl-C -> copy to clipboard
					CStringA sClipdata;
					POSITION pos = m_ProgList.GetFirstSelectedItemPosition();
					if (pos != NULL)
					{
						while (pos)
						{
							int nItem = m_ProgList.GetNextSelectedItem(pos);
							CString sAction = m_ProgList.GetItemText(nItem, 0);
							CString sPath = m_ProgList.GetItemText(nItem, 1);
							CString sMime = m_ProgList.GetItemText(nItem, 2);
							CString sLogCopyText;
							sLogCopyText.Format(_T("%s: %s  %s\n"),
								(LPCTSTR)sAction, (LPCTSTR)sPath, (LPCTSTR)sMime);
							sClipdata +=  CStringA(sLogCopyText);
						}
						CUtils::WriteAsciiStringToClipboard(sClipdata);
					}
				} // if (GetKeyState(VK_CONTROL)&0x8000)
			} // if (selIndex >= 0)
		} // if (pLVKeyDow->wVKey == 'C')
	} // if (pMsg->message == WM_KEYDOWN)
	return __super::PreTranslateMessage(pMsg);
}

void CSVNProgressDlg::OnContextMenu(CWnd* pWnd, CPoint point)
{
	if (pWnd == &m_ProgList)
	{
		int selIndex = m_ProgList.GetSelectionMark();
		if ((point.x == -1) && (point.y == -1))
		{
			// Menu was invoked from the keyboard rather than by right-clicking
			CRect rect;
			m_ProgList.GetItemRect(selIndex, &rect, LVIR_LABEL);
			m_ProgList.ClientToScreen(&rect);
			point = rect.CenterPoint();
		}

		if ((selIndex >= 0)&&(!m_bThreadRunning)&&(GetDlgItem(IDC_LOGBUTTON)->IsWindowVisible()))
		{
			//entry is selected, thread has finished with updating so show the popup menu
			CMenu popup;
			if (popup.CreatePopupMenu())
			{
				CString temp;
				if (m_ProgList.GetSelectedCount() == 1)
				{
					const NotificationData * data = m_arData[selIndex];
					if ((data)&&(!data->path.IsDirectory())&&(data->action == svn_wc_notify_update_update))
					{
						temp.LoadString(IDS_LOG_POPUP_COMPARE);
						popup.AppendMenu(MF_STRING | MF_ENABLED, ID_COMPARE, temp);

						int cmd = popup.TrackPopupMenu(TPM_RETURNCMD | TPM_LEFTALIGN | TPM_NONOTIFY, point.x, point.y, this, 0);
						GetDlgItem(IDOK)->EnableWindow(FALSE);
						this->SetPromptApp(&theApp);
						theApp.DoWaitCursor(1);
						switch (cmd)
						{
						case ID_COMPARE:
							{
								CTSVNPath tempfile = CUtils::GetTempFilePath(data->path);
								m_templist.AddPath(tempfile);
								SVN svn;
								if (!svn.Cat(data->path, m_nUpdateStartRev, tempfile))
								{
									ReportSVNError();
									GetDlgItem(IDOK)->EnableWindow(TRUE);
									break;
								}
								else
								{
									CString revname, wcname;
									revname.Format(_T("%s Revision %ld"), (LPCTSTR)data->path.GetFileOrDirectoryName(), m_nUpdateStartRev);
									wcname.Format(IDS_DIFF_WCNAME, (LPCTSTR)data->path.GetFileOrDirectoryName());
									CUtils::StartExtDiff(tempfile, data->path, revname, wcname);
								}
							}
							break;
						}
						GetDlgItem(IDOK)->EnableWindow(TRUE);
						theApp.DoWaitCursor(-1);
					}
				}
			}
		}
	}
}
