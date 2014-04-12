// TortoiseSVN - a Windows shell extension for easy version control

// Copyright (C) 2008-2010, 2012 - TortoiseSVN

// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
//
#include "stdafx.h"
#include "TortoiseProc.h"
#include "MergeWizard.h"
#include "MergeWizardReintegrate.h"

#include "AppUtils.h"
#include "PathUtils.h"
#include "LogDialog\LogDlg.h"

IMPLEMENT_DYNAMIC(CMergeWizardReintegrate, CMergeWizardBasePage)

CMergeWizardReintegrate::CMergeWizardReintegrate()
    : CMergeWizardBasePage(CMergeWizardReintegrate::IDD)
    , m_URL(_T(""))
    , m_pLogDlg(NULL)
    , m_pLogDlg2(NULL)
{
    m_psp.dwFlags |= PSP_DEFAULT|PSP_USEHEADERTITLE|PSP_USEHEADERSUBTITLE;
    m_psp.pszHeaderTitle = MAKEINTRESOURCE(IDS_MERGEWIZARD_REINTEGRATETITLE);
    m_psp.pszHeaderSubTitle = MAKEINTRESOURCE(IDS_MERGEWIZARD_REINTEGRATESUBTITLE);
}

CMergeWizardReintegrate::~CMergeWizardReintegrate()
{
    if (m_pLogDlg)
    {
        m_pLogDlg->DestroyWindow();
        delete m_pLogDlg;
    }
    if (m_pLogDlg2)
    {
        m_pLogDlg2->DestroyWindow();
        delete m_pLogDlg2;
    }
}

void CMergeWizardReintegrate::DoDataExchange(CDataExchange* pDX)
{
    CMergeWizardBasePage::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_URLCOMBO, m_URLCombo);
    DDX_Control(pDX, IDC_WCEDIT, m_WCPath);
}


BEGIN_MESSAGE_MAP(CMergeWizardReintegrate, CMergeWizardBasePage)
    ON_MESSAGE(WM_TSVN_MAXREVFOUND, &CMergeWizardReintegrate::OnWCStatus)
    ON_BN_CLICKED(IDC_SHOWMERGELOG, &CMergeWizardReintegrate::OnBnClickedShowmergelog)
    ON_BN_CLICKED(IDC_BROWSE, &CMergeWizardReintegrate::OnBnClickedBrowse)
    ON_BN_CLICKED(IDC_SHOWLOGWC, &CMergeWizardReintegrate::OnBnClickedShowlogwc)
    ON_CBN_EDITCHANGE(IDC_URLCOMBO, &CMergeWizardReintegrate::OnCbnEditchangeUrlcombo)
END_MESSAGE_MAP()


BOOL CMergeWizardReintegrate::OnInitDialog()
{
    CMergeWizardBasePage::OnInitDialog();
    CMergeWizard * pWizard = (CMergeWizard*)GetParent();

    CString sRegKey = _T("Software\\TortoiseSVN\\History\\repoURLS\\MergeURLFor") + ((CMergeWizard*)GetParent())->wcPath.GetSVNPathString();
    CString sMergeUrlForWC = CRegString(sRegKey);

    CString sUUID = ((CMergeWizard*)GetParent())->sUUID;
    m_URLCombo.SetURLHistory(true, false);
    m_URLCombo.LoadHistory(_T("Software\\TortoiseSVN\\History\\repoURLS\\")+sUUID, _T("url"));
    if (!(DWORD)CRegDWORD(_T("Software\\TortoiseSVN\\MergeWCURL"), FALSE))
        m_URLCombo.SetCurSel(0);
    else if (!sMergeUrlForWC.IsEmpty())
        m_URLCombo.SetWindowText(CPathUtils::PathUnescape(sMergeUrlForWC));
    if (m_URLCombo.GetString().IsEmpty())
        m_URLCombo.SetWindowText(CPathUtils::PathUnescape(pWizard->url));
    if (!pWizard->URL1.IsEmpty())
        m_URLCombo.SetWindowText(CPathUtils::PathUnescape(pWizard->URL1));
    GetDlgItem(IDC_BROWSE)->EnableWindow(!m_URLCombo.GetString().IsEmpty());

    SetDlgItemText(IDC_WCEDIT, ((CMergeWizard*)GetParent())->wcPath.GetWinPath());

    AddAnchor(IDC_MERGEREINTEGRATEFROMGROUP, TOP_LEFT, TOP_RIGHT);
    AddAnchor(IDC_URLCOMBO, TOP_LEFT, TOP_RIGHT);
    AddAnchor(IDC_BROWSE, TOP_RIGHT);
    AddAnchor(IDC_SHOWMERGELOG, TOP_RIGHT);
    AddAnchor(IDC_MERGEREINTEGRATEWCGROUP, TOP_LEFT, TOP_RIGHT);
    AddAnchor(IDC_WCEDIT, TOP_LEFT, TOP_RIGHT);
    AddAnchor(IDC_SHOWLOGWC, TOP_RIGHT);

    StartWCCheckThread(((CMergeWizard*)GetParent())->wcPath);

    return TRUE;
}

BOOL CMergeWizardReintegrate::CheckData(bool bShowErrors /* = true */)
{
    UNREFERENCED_PARAMETER(bShowErrors);

    if (!UpdateData(TRUE))
        return FALSE;

    if (::IsWindow(m_pLogDlg->GetSafeHwnd())&&(m_pLogDlg->IsWindowVisible()))
    {
        m_pLogDlg->SendMessage(WM_CLOSE);
        return FALSE;
    }
    if (::IsWindow(m_pLogDlg2->GetSafeHwnd())&&(m_pLogDlg2->IsWindowVisible()))
    {
        m_pLogDlg2->SendMessage(WM_CLOSE);
        return FALSE;
    }


    m_URLCombo.SaveHistory();
    m_URL = m_URLCombo.GetString();

    CString sRegKey = _T("Software\\TortoiseSVN\\History\\repoURLS\\MergeURLFor") + ((CMergeWizard*)GetParent())->wcPath.GetSVNPathString();
    CRegString regMergeUrlForWC = CRegString(sRegKey);
    regMergeUrlForWC = m_URL;


    ((CMergeWizard*)GetParent())->URL1 = m_URL;

    UpdateData(FALSE);
    return TRUE;
}

LRESULT CMergeWizardReintegrate::OnWizardNext()
{
    StopWCCheckThread();

    if (!CheckData(true))
        return -1;

    return IDD_MERGEWIZARD_OPTIONS;
}

LRESULT CMergeWizardReintegrate::OnWizardBack()
{
    if (::IsWindow(m_pLogDlg->GetSafeHwnd())&&(m_pLogDlg->IsWindowVisible()))
    {
        m_pLogDlg->SendMessage(WM_CLOSE);
        return -1;
    }
    if (::IsWindow(m_pLogDlg2->GetSafeHwnd())&&(m_pLogDlg2->IsWindowVisible()))
    {
        m_pLogDlg2->SendMessage(WM_CLOSE);
        return -1;
    }

    return IDD_MERGEWIZARD_START;
}

BOOL CMergeWizardReintegrate::OnSetActive()
{
    CPropertySheet* psheet = (CPropertySheet*) GetParent();
    psheet->SetWizardButtons(PSWIZB_NEXT|PSWIZB_BACK);
    SetButtonTexts();

    return CMergeWizardBasePage::OnSetActive();
}


void CMergeWizardReintegrate::OnBnClickedShowmergelog()
{
    if (::IsWindow(m_pLogDlg->GetSafeHwnd())&&(m_pLogDlg->IsWindowVisible()))
        return;
    StopWCCheckThread();
    CString url;
    m_URLCombo.GetWindowText(url);

    if (!url.IsEmpty())
    {
        CTSVNPath wcPath = ((CMergeWizard*)GetParent())->wcPath;
        if (m_pLogDlg)
            m_pLogDlg->DestroyWindow();
        delete m_pLogDlg;
        m_pLogDlg = new CLogDlg();
        m_pLogDlg->SetDialogTitle(CString(MAKEINTRESOURCE(IDS_MERGE_SELECTRANGE)));

        m_pLogDlg->SetSelect(true);
        m_pLogDlg->m_pNotifyWindow = this;
        m_pLogDlg->SetParams(CTSVNPath(url), SVNRev::REV_HEAD, SVNRev::REV_HEAD, 1, TRUE, FALSE);
        m_pLogDlg->SetProjectPropertiesPath(wcPath);
        m_pLogDlg->SetMergePath(wcPath);
        m_pLogDlg->Create(IDD_LOGMESSAGE, this);
        m_pLogDlg->ShowWindow(SW_SHOW);
    }
}

void CMergeWizardReintegrate::OnBnClickedBrowse()
{
    SVNRev rev(SVNRev::REV_HEAD);
    CAppUtils::BrowseRepository(m_URLCombo, this, rev);
}

void CMergeWizardReintegrate::OnBnClickedShowlogwc()
{
    StopWCCheckThread();
    CTSVNPath wcPath = ((CMergeWizard*)GetParent())->wcPath;
    if (m_pLogDlg2)
        m_pLogDlg2->DestroyWindow();
    delete m_pLogDlg2;
    m_pLogDlg2 = new CLogDlg();
    m_pLogDlg2->SetDialogTitle(CString(MAKEINTRESOURCE(IDS_MERGE_SELECTRANGE)));

    m_pLogDlg2->m_pNotifyWindow = NULL;
    m_pLogDlg2->SetParams(wcPath, SVNRev::REV_HEAD, SVNRev::REV_HEAD, 1, TRUE, FALSE);
    m_pLogDlg2->SetProjectPropertiesPath(wcPath);
    m_pLogDlg2->SetMergePath(wcPath);
    m_pLogDlg2->Create(IDD_LOGMESSAGE, this);
    m_pLogDlg2->ShowWindow(SW_SHOW);
}

void CMergeWizardReintegrate::OnCbnEditchangeUrlcombo()
{
    GetDlgItem(IDC_BROWSE)->EnableWindow(!m_URLCombo.GetString().IsEmpty());
}

LPARAM CMergeWizardReintegrate::OnWCStatus(WPARAM wParam, LPARAM /*lParam*/)
{
    if (wParam)
    {
        ShowEditBalloon(IDC_WCEDIT, IDS_MERGE_WCDIRTY, IDS_WARN_WARNING, TTI_WARNING);
    }
    return 0;
}

bool CMergeWizardReintegrate::OkToCancel()
{
    StopWCCheckThread();
    if (::IsWindow(m_pLogDlg->GetSafeHwnd())&&(m_pLogDlg->IsWindowVisible()))
    {
        m_pLogDlg->SendMessage(WM_CLOSE);
        return false;
    }
    if (::IsWindow(m_pLogDlg2->GetSafeHwnd())&&(m_pLogDlg2->IsWindowVisible()))
    {
        m_pLogDlg2->SendMessage(WM_CLOSE);
        return false;
    }
    return true;
}