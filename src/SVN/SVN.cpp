// TortoiseSVN - a Windows shell extension for easy version control

// Copyright (C) 2003-2005 - Tim Kemp and Stefan Kueng

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

#include "StdAfx.h"
#include "TortoiseProc.h"
#include "ProgressDlg.h"
#include "svn.h"
#include "svn_sorts.h"
#include "client.h"
#include "UnicodeUtils.h"
#include "DirFileEnum.h"
#include "TSVNPath.h"
#include "ShellUpdater.h"
#include "Registry.h"
#include "SVNHelpers.h"
#include "SVNStatus.h"
#include "Utils.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

SVN::SVN(void)
{
	parentpool = svn_pool_create(NULL);
	svn_utf_initialize(parentpool);
	const char * deststr = NULL;
	svn_utf_cstring_to_utf8(&deststr, "dummy", parentpool);
	svn_utf_cstring_from_utf8(&deststr, "dummy", parentpool);
	svn_client_create_context(&m_pctx, parentpool);

	Err = svn_config_ensure(NULL, parentpool);
	pool = svn_pool_create (parentpool);
	// set up the configuration
	if (Err == 0)
		Err = svn_config_get_config (&(m_pctx->config), NULL, parentpool);

	if (Err != 0)
	{
		::MessageBox(NULL, this->GetLastErrorMessage(), _T("TortoiseSVN"), MB_ICONERROR);
		svn_pool_destroy (pool);
		svn_pool_destroy (parentpool);
		exit(-1);
	} // if (Err != 0) 

	// set up authentication
	m_prompt.Init(parentpool, m_pctx);

	m_pctx->log_msg_func = svn_cl__get_log_message;
	m_pctx->log_msg_baton = logMessage("");
	m_pctx->notify_func = notify;
	m_pctx->notify_baton = this;
	m_pctx->cancel_func = cancel;
	m_pctx->cancel_baton = this;

	//set up the SVN_SSH param
	CString tsvn_ssh = CRegString(_T("Software\\TortoiseSVN\\SSH"));
	if (tsvn_ssh.IsEmpty())
		tsvn_ssh = CUtils::GetAppDirectory() + _T("TortoisePlink.exe");
	tsvn_ssh.Replace('\\', '/');
	if (!tsvn_ssh.IsEmpty())
	{
		svn_config_t * cfg = (svn_config_t *)apr_hash_get (m_pctx->config, SVN_CONFIG_CATEGORY_CONFIG,
			APR_HASH_KEY_STRING);
		svn_config_set(cfg, SVN_CONFIG_SECTION_TUNNELS, "ssh", CUnicodeUtils::GetUTF8(tsvn_ssh));
	}
	//UseIEProxySettings(ctx.config);
}

SVN::~SVN(void)
{
	svn_pool_destroy (parentpool);
}

CString SVN::CheckConfigFile()
{
	svn_client_ctx_t 			ctx;
	SVNPool						pool;
	svn_error_t *				Err;

	memset (&ctx, 0, sizeof (ctx));

	Err = svn_config_ensure(NULL, pool);
	// set up the configuration
	if (Err == 0)
		Err = svn_config_get_config (&(ctx.config), NULL, pool);
	CString msg;
	CString temp;
	if (Err != NULL)
	{
		svn_error_t * ErrPtr = Err;
		msg = CUnicodeUtils::GetUnicode(ErrPtr->message);
		while (ErrPtr->child)
		{
			ErrPtr = ErrPtr->child;
			msg += _T("\n");
			msg += CUnicodeUtils::GetUnicode(ErrPtr->message);
		}
		if (!temp.IsEmpty())
		{
			msg += _T("\n") + temp;
		}
	}
	return msg;
}

#pragma warning(push)
#pragma warning(disable: 4100)	// unreferenced formal parameter

BOOL SVN::Cancel() {return FALSE;};
BOOL SVN::Notify(const CTSVNPath& path, svn_wc_notify_action_t action, svn_node_kind_t kind, const CString& myme_type, svn_wc_notify_state_t content_state, svn_wc_notify_state_t prop_state, LONG rev) {return TRUE;};
BOOL SVN::Log(LONG rev, const CString& author, const CString& date, const CString& message, LogChangedPathArray * cpaths, apr_time_t time, int filechanges, BOOL copies) {return TRUE;};
BOOL SVN::BlameCallback(LONG linenumber, LONG revision, const CString& author, const CString& date, const CStringA& line) {return TRUE;}
#pragma warning(pop)

struct log_msg_baton
{
  const char *message;  /* the message. */
  const char *message_encoding; /* the locale/encoding of the message. */
  const char *base_dir; /* the base directory for an external edit. UTF-8! */
  const char *tmpfile_left; /* the tmpfile left by an external edit. UTF-8! */
  apr_pool_t *pool; /* a pool. */
};

CString SVN::GetLastErrorMessage()
{
	return GetErrorString(Err);
}

CString SVN::GetErrorString(svn_error_t * Err)
{
	CString msg;
	CString temp;
	char errbuf[256];

	if (Err != NULL)
	{
		svn_error_t * ErrPtr = Err;
		if (ErrPtr->message)
			msg = CUnicodeUtils::GetUnicode(ErrPtr->message);
		else
		{
			/* Is this a Subversion-specific error code? */
			if ((ErrPtr->apr_err > APR_OS_START_USEERR)
				&& (ErrPtr->apr_err <= APR_OS_START_CANONERR))
				msg = svn_strerror (ErrPtr->apr_err, errbuf, sizeof (errbuf));
			/* Otherwise, this must be an APR error code. */
			else
			{
				svn_error_t *temp_err = NULL;
				const char * err_string = NULL;
				temp_err = svn_utf_cstring_to_utf8(&err_string, apr_strerror (ErrPtr->apr_err, errbuf, sizeof (errbuf)), ErrPtr->pool);
				if (temp_err)
				{
					svn_error_clear (temp_err);
					msg = _T("Can't recode error string from APR");
				}
				else
				{
					msg = CUnicodeUtils::GetUnicode(err_string);
				}
			}
		}
		while (ErrPtr->child)
		{
			ErrPtr = ErrPtr->child;
			msg += _T("\n");
			temp = CUnicodeUtils::GetUnicode(ErrPtr->message);
			while (temp.GetLength() > 80)
			{
				int pos=0;
				while ((pos>=0)&&(temp.Find(' ', pos)<80))
				{
					pos = temp.Find(' ', pos+1);
				}
				if (pos==0)
					pos = temp.Find(' ');
				if (pos<0)
				{
					msg += temp;
					temp.Empty();
				}
				else
				{
					msg += temp.Left(pos+1);
					temp = temp.Mid(pos+1);
				}
				msg += _T("\n");
			}
			msg += temp;
		}
		temp.Empty();
		switch (Err->apr_err)
		{
		case SVN_ERR_BAD_FILENAME:
		case SVN_ERR_BAD_URL:
			// please check the path or URL you've entered.
			temp.LoadString(IDS_SVNERR_CHECKPATHORURL);
			break;
		case SVN_ERR_WC_LOCKED:
		case SVN_ERR_WC_NOT_LOCKED:
			// do a "cleanup"
			temp.LoadString(IDS_SVNERR_RUNCLEANUP);
			break;
		case SVN_ERR_WC_NOT_UP_TO_DATE:
		case SVN_ERR_RA_OUT_OF_DATE:
		case SVN_ERR_FS_TXN_OUT_OF_DATE:
			// do an update first
			temp.LoadString(IDS_SVNERR_UPDATEFIRST);
			break;
		case SVN_ERR_WC_CORRUPT:
		case SVN_ERR_WC_CORRUPT_TEXT_BASE:
			// do a "cleanup". If that doesn't work you need to do a fresh checkout.
			temp.LoadString(IDS_SVNERR_CLEANUPORFRESHCHECKOUT);
			break;
		default:
			break;
		}
		if (!temp.IsEmpty())
		{
			msg += _T("\n") + temp;
		}
		return msg;
	}
	return _T("");
}

BOOL SVN::Checkout(const CTSVNPath& moduleName, const CTSVNPath& destPath, SVNRev revision, BOOL recurse)
{
	Err = svn_client_checkout (	NULL,			// we don't need the resulting revision
								moduleName.GetSVNApiPath(),
								destPath.GetSVNApiPath(),
								revision,
								recurse,
								m_pctx,
								pool );

	if(Err != NULL)
	{
		return FALSE;
	}

	return TRUE;
}

BOOL SVN::Remove(const CTSVNPathList& pathlist, BOOL force, CString message)
{
	svn_client_commit_info_t *commit_info = NULL;
	message.Replace(_T("\r"), _T(""));
	m_pctx->log_msg_baton = logMessage(CUnicodeUtils::GetUTF8(message));

	// svn_client_delete needs to run on a sub-pool, so that after it's run, the pool
	// cleanups get run.  For example, after a failure do to an unforced delete on 
	// a modified file, if you don't do a cleanup, the WC stays locked
	SVNPool subPool(pool);
	Err = svn_client_delete (&commit_info, MakePathArray(pathlist), force,
							m_pctx,
							subPool);
	if(Err != NULL)
	{
		return FALSE;
	}

	if (commit_info && SVN_IS_VALID_REVNUM (commit_info->revision))
	{
		for (int i=0; i<pathlist.GetCount(); ++i)
			Notify(pathlist[i], svn_wc_notify_update_completed, svn_node_none, _T(""), svn_wc_notify_state_unknown, svn_wc_notify_state_unknown, commit_info->revision);
	}

	return TRUE;
}

BOOL SVN::Revert(const CTSVNPathList& pathlist, BOOL recurse)
{
	TRACE("Reverting list of %d files\n", pathlist.GetCount());

	Err = svn_client_revert (MakePathArray(pathlist), recurse, m_pctx, pool);

	if(Err != NULL)
	{
		return FALSE;
	}

	return TRUE;
}


BOOL SVN::Add(const CTSVNPathList& pathList, BOOL recurse, BOOL force)
{
	for(int nItem = 0; nItem < pathList.GetCount(); nItem++)
	{
		TRACE(_T("add file %s\n"), pathList[nItem].GetWinPath());

		SVNPool subpool(pool);
		Err = svn_client_add2 (pathList[nItem].GetSVNApiPath(), recurse, force, m_pctx, subpool);
		if(Err != NULL)
		{
			return FALSE;
		}
	}

	return TRUE;
}

BOOL SVN::Update(const CTSVNPath& path, SVNRev revision, BOOL recurse)
{
	Err = svn_client_update(NULL,
							path.GetSVNApiPath(),
							revision,
							recurse,
							m_pctx,
							pool);

	if(Err != NULL)
	{
		return FALSE;
	}

	return TRUE;
}

LONG SVN::Commit(const CTSVNPathList& pathlist, CString message, BOOL recurse)
{
	svn_client_commit_info_t *commit_info = NULL;

	message.Replace(_T("\r"), _T(""));
	m_pctx->log_msg_baton = logMessage(CUnicodeUtils::GetUTF8(message));
	Err = svn_client_commit (&commit_info, 
							MakePathArray(pathlist), 
							!recurse,
							m_pctx,
							pool);
	m_pctx->log_msg_baton = logMessage("");
	if(Err != NULL)
	{
		return 0;
	}

	if (commit_info && SVN_IS_VALID_REVNUM (commit_info->revision))
	{
		Notify(CTSVNPath(), svn_wc_notify_update_completed, svn_node_none, _T(""), svn_wc_notify_state_unknown, svn_wc_notify_state_unknown, commit_info->revision);
		return commit_info->revision;
	}

	return -1;
}

BOOL SVN::Copy(const CTSVNPath& srcPath, const CTSVNPath& destPath, SVNRev revision, CString logmsg)
{
	SVNPool subpool(pool);

	svn_client_commit_info_t *commit_info = NULL;
	logmsg.Replace(_T("\r"), _T(""));
	if (logmsg.IsEmpty())
		m_pctx->log_msg_baton = logMessage(CUnicodeUtils::GetUTF8(_T("made a copy")));
	else
		m_pctx->log_msg_baton = logMessage(CUnicodeUtils::GetUTF8(logmsg));
	Err = svn_client_copy (&commit_info,
							srcPath.GetSVNApiPath(),
							revision,
							destPath.GetSVNApiPath(),
							m_pctx,
							subpool);
	if(Err != NULL)
	{
		return FALSE;
	}
	if (commit_info && SVN_IS_VALID_REVNUM (commit_info->revision))
		Notify(destPath, svn_wc_notify_update_completed, svn_node_none, _T(""), svn_wc_notify_state_unknown, svn_wc_notify_state_unknown, commit_info->revision);
	return TRUE;
}

BOOL SVN::Move(const CTSVNPath& srcPath, const CTSVNPath& destPath, BOOL force, CString message, SVNRev rev)
{
	SVNPool subpool(pool);

	svn_client_commit_info_t *commit_info = NULL;
	message.Replace(_T("\r"), _T(""));
	m_pctx->log_msg_baton = logMessage(CUnicodeUtils::GetUTF8(message));
	Err = svn_client_move (&commit_info,
							srcPath.GetSVNApiPath(),
							rev,
							destPath.GetSVNApiPath(),
							force,
							m_pctx,
							subpool);
	if(Err != NULL)
	{
		return FALSE;
	}
	if (commit_info && SVN_IS_VALID_REVNUM (commit_info->revision))
		Notify(destPath, svn_wc_notify_update_completed, svn_node_none, _T(""), svn_wc_notify_state_unknown, svn_wc_notify_state_unknown, commit_info->revision);

	CShellUpdater::Instance().AddPathForUpdate(srcPath);
	CShellUpdater::Instance().AddPathForUpdate(destPath);

	return TRUE;
}

BOOL SVN::MakeDir(const CTSVNPathList& pathlist, CString message)
{
	svn_client_commit_info_t *commit_info = NULL;
	message.Replace(_T("\r"), _T(""));
	m_pctx->log_msg_baton = logMessage(CUnicodeUtils::GetUTF8(message));
	Err = svn_client_mkdir (&commit_info,
							MakePathArray(pathlist),
							m_pctx,
							pool);
	if(Err != NULL)
	{
		return FALSE;
	}
	if (commit_info && SVN_IS_VALID_REVNUM (commit_info->revision))
	{
		for (int i=0; i<pathlist.GetCount(); ++i)
			Notify(pathlist[i], svn_wc_notify_update_completed, svn_node_none, _T(""), svn_wc_notify_state_unknown, svn_wc_notify_state_unknown, commit_info->revision);
	}

	CShellUpdater::Instance().AddPathsForUpdate(pathlist);

	return TRUE;
}

BOOL SVN::CleanUp(const CTSVNPath& path)
{
	Err = svn_client_cleanup (path.GetSVNApiPath(), m_pctx, pool);

	if(Err != NULL)
	{
		return FALSE;
	}

	CShellUpdater::Instance().AddPathForUpdate(path);

	return TRUE;
}

BOOL SVN::Resolve(const CTSVNPath& path, BOOL recurse)
{
	Err = svn_client_resolved (path.GetSVNApiPath(),
								recurse,
								m_pctx,
								pool);
	if(Err != NULL)
	{
		return FALSE;
	}

	CShellUpdater::Instance().AddPathForUpdate(path);

	return TRUE;
}

BOOL SVN::Export(CString srcPath, CString destPath, SVNRev revision, BOOL force, CProgressDlg * pProgDlg, BOOL extended)
{
	if (revision.IsWorking()&&(pProgDlg))
	{
		// files are special!
		if (!PathIsDirectory(srcPath))
		{
			CopyFile(srcPath, destPath, FALSE);
			return TRUE;
		}
		// our own "export" function with a callback and the ability to export
		// unversioned items too
		if (extended)
		{
			CDirFileEnum lister1(srcPath);
			DWORD maxval = 0;
			DWORD current = 0;
			// first, count all the items we have to copy
			CString srcfile;
			while (lister1.NextFile(srcfile, NULL))
			{
				if (srcfile.Find(_T(SVN_WC_ADM_DIR_NAME))<0)
					maxval++;
			}
			CDirFileEnum lister2(srcPath);
			CString sSVN_ADMIN_DIR = _T("\\");
			sSVN_ADMIN_DIR += _T(SVN_WC_ADM_DIR_NAME);
			while (lister2.NextFile(srcfile, NULL))
			{
				
				if ((srcfile.Find(sSVN_ADMIN_DIR+_T("\\"))>=0)||(srcfile.Right(sSVN_ADMIN_DIR.GetLength()).Compare(sSVN_ADMIN_DIR)==0))
					continue;	// exclude everything inside an admin directory
				current++;
				CString destination = destPath + _T("\\") + srcfile.Mid(srcPath.GetLength());
				pProgDlg->SetProgress(current, maxval);
				pProgDlg->SetLine(2, srcfile, TRUE);
				if (CUtils::FileCopy(srcfile, destination, force)==FALSE)
				{
					LPVOID lpMsgBuf;
					if (!FormatMessageA( 
						FORMAT_MESSAGE_ALLOCATE_BUFFER | 
						FORMAT_MESSAGE_FROM_SYSTEM | 
						FORMAT_MESSAGE_IGNORE_INSERTS,
						NULL,
						GetLastError(),
						MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
						(LPSTR) &lpMsgBuf,
						0,
						NULL ))
					{
						// Handle the error.
						return FALSE;
					}

					Err = svn_error_create(NULL, NULL, (const char *)lpMsgBuf);
					// Free the buffer.
					LocalFree( lpMsgBuf );
					return FALSE;
				}
				if (pProgDlg->HasUserCancelled())
				{
					Err = svn_error_create(NULL, NULL, "user cancelled");
					return FALSE;
				}
			} // while (lister2.NextFile(srcfile))
		}
		else
		{
			const TCHAR * strbuf = NULL;
			svn_wc_status_t * s;
			SVNStatus status;
			if ((s = status.GetFirstFileStatus(srcPath, &strbuf))!=0)
			{
				DWORD maxval = status.GetVersionedCount();
				DWORD current = 0;
				if (SVNStatus::GetMoreImportant(s->text_status, svn_wc_status_unversioned)!=svn_wc_status_unversioned)
				{
					current++;
					CString src = strbuf;
					CString destination = destPath + _T("\\") + src.Mid(srcPath.GetLength());
					pProgDlg->SetProgress(current, maxval);
					pProgDlg->SetLine(2, src, TRUE);
					if (CUtils::FileCopy(src, destination, force)==FALSE)
					{
						LPVOID lpMsgBuf;
						if (!FormatMessageA( 
							FORMAT_MESSAGE_ALLOCATE_BUFFER | 
							FORMAT_MESSAGE_FROM_SYSTEM | 
							FORMAT_MESSAGE_IGNORE_INSERTS,
							NULL,
							GetLastError(),
							MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
							(LPSTR) &lpMsgBuf,
							0,
							NULL ))
						{
							// Handle the error.
							return FALSE;
						}

						Err = svn_error_create(NULL, NULL, (const char *)lpMsgBuf);
						// Free the buffer.
						LocalFree( lpMsgBuf );
						return FALSE;
					}
				}
				while ((s = status.GetNextFileStatus(&strbuf))!=0)
				{
					if ((s->text_status == svn_wc_status_unversioned)||
						(s->text_status == svn_wc_status_ignored)||
						(s->text_status == svn_wc_status_none))
						continue;
					current++;
					if ((s->text_status == svn_wc_status_missing)||
						(s->text_status == svn_wc_status_deleted))
						continue;
					
					CString src = strbuf;
					CString destination = destPath + _T("\\") + src.Mid(srcPath.GetLength());
					pProgDlg->SetProgress(current, maxval);
					pProgDlg->SetLine(2, src, TRUE);
					if (CUtils::FileCopy(src, destination, force)==FALSE)
					{
						LPVOID lpMsgBuf;
						if (!FormatMessageA( 
							FORMAT_MESSAGE_ALLOCATE_BUFFER | 
							FORMAT_MESSAGE_FROM_SYSTEM | 
							FORMAT_MESSAGE_IGNORE_INSERTS,
							NULL,
							GetLastError(),
							MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
							(LPSTR) &lpMsgBuf,
							0,
							NULL ))
						{
							// Handle the error.
							return FALSE;
						}

						Err = svn_error_create(NULL, NULL, (const char *)lpMsgBuf);
						// Free the buffer.
						LocalFree( lpMsgBuf );
						return FALSE;
					}
					if (pProgDlg->HasUserCancelled())
					{
						Err = svn_error_create(NULL, NULL, "user cancelled");
						return FALSE;
					}
				} // while (s = status.GetNextFileStatus(&strbuf))
			} // if (s = status.GetFirstFileStatus(srcPath, &strbuf))
			else
			{
				Err = svn_error_create(status.m_err->apr_err, status.m_err, NULL);
				return FALSE;
			}
		} // else from if (extended)
	}
	else
	{

		preparePath(srcPath);
		preparePath(destPath);

		Err = svn_client_export2(NULL,		//no resulting revision needed
			MakeSVNUrlOrPath(srcPath),
			MakeSVNUrlOrPath(destPath),
			revision,
			force,
			NULL,
			m_pctx,
			pool);
		if(Err != NULL)
		{
			return FALSE;
		}
	}
	return TRUE;
}

BOOL SVN::Switch(const CTSVNPath& path, const CTSVNPath& url, SVNRev revision, BOOL recurse)
{
	Err = svn_client_switch(NULL,
							path.GetSVNApiPath(),
							url.GetSVNApiPath(),
							revision,
							recurse,
							m_pctx,
							pool);
	if(Err != NULL)
	{
		return FALSE;
	}
	
	CShellUpdater::Instance().AddPathForUpdate(path);

	return TRUE;
}

BOOL SVN::Import(const CTSVNPath& path, const CTSVNPath& url, CString message, BOOL recurse)
{
	svn_client_commit_info_t *commit_info = NULL;
	message.Replace(_T("\r"), _T(""));
	m_pctx->log_msg_baton = logMessage(CUnicodeUtils::GetUTF8(message));
	Err = svn_client_import (&commit_info,
							path.GetSVNApiPath(),
							url.GetSVNApiPath(),
							!recurse,
							m_pctx,
							pool);
	m_pctx->log_msg_baton = logMessage("");
	if(Err != NULL)
	{
		return FALSE;
	}
	if (commit_info && SVN_IS_VALID_REVNUM (commit_info->revision))
		Notify(path, svn_wc_notify_update_completed, svn_node_none, _T(""), svn_wc_notify_state_unknown, svn_wc_notify_state_unknown, commit_info->revision);
	return TRUE;
}

BOOL SVN::Merge(const CTSVNPath& path1, SVNRev revision1, const CTSVNPath& path2, SVNRev revision2, const CTSVNPath& localPath, BOOL force, BOOL recurse, BOOL ignoreanchestry, BOOL dryrun)
{
	Err = svn_client_merge (path1.GetSVNApiPath(),
							revision1,
							path2.GetSVNApiPath(),
							revision2,
							localPath.GetSVNApiPath(),
							recurse,
							ignoreanchestry,
							force,
							dryrun,
							m_pctx,
							pool);
	if(Err != NULL)
	{
		return FALSE;
	}

	CShellUpdater::Instance().AddPathForUpdate(CTSVNPath(localPath));

	return TRUE;
}

BOOL SVN::PegMerge(const CTSVNPath& source, SVNRev revision1, SVNRev revision2, SVNRev pegrevision, const CTSVNPath& destpath, BOOL force, BOOL recurse, BOOL ignoreancestry, BOOL dryrun)
{
	Err = svn_client_merge_peg (source.GetSVNApiPath(),
		revision1,
		revision2,
		pegrevision,
		destpath.GetSVNApiPath(),
		recurse,
		ignoreancestry,
		force,
		dryrun,
		m_pctx,
		pool);
	if(Err != NULL)
	{
		return FALSE;
	}

	CShellUpdater::Instance().AddPathForUpdate(destpath);

	return TRUE;
}

BOOL SVN::Diff(const CTSVNPath& path1, SVNRev revision1, const CTSVNPath& path2, SVNRev revision2, BOOL recurse, BOOL ignoreancestry, BOOL nodiffdeleted, CString options, const CTSVNPath& outputfile)
{
	return Diff(path1, revision1, path2, revision2, recurse, ignoreancestry, nodiffdeleted, options, outputfile, CTSVNPath());
}

BOOL SVN::Diff(const CTSVNPath& path1, SVNRev revision1, const CTSVNPath& path2, SVNRev revision2, BOOL recurse, BOOL ignoreancestry, BOOL nodiffdeleted, CString options, const CTSVNPath& outputfile, const CTSVNPath& errorfile)
{
	BOOL del = FALSE;
	apr_file_t * outfile;
	apr_file_t * errfile;
	apr_array_header_t *opts;

	SVNPool localpool(pool);

	opts = svn_cstring_split (CUnicodeUtils::GetUTF8(options), " \t\n\r", TRUE, localpool);

	Err = svn_io_file_open (&outfile, outputfile.GetSVNApiPath(),
							APR_WRITE | APR_CREATE | APR_TRUNCATE | APR_BINARY,
							APR_OS_DEFAULT, localpool);
	if (Err)
		return FALSE;

	CTSVNPath workingErrorFile;
	if (errorfile.IsEmpty())
	{
		workingErrorFile = CUtils::GetTempFilePath();
		del = TRUE;
	}
	else
	{
		workingErrorFile = errorfile;
	}

	Err = svn_io_file_open (&errfile, workingErrorFile.GetSVNApiPath(),
							APR_WRITE | APR_CREATE | APR_TRUNCATE | APR_BINARY,
							APR_OS_DEFAULT, localpool);
	if (Err)
		return FALSE;

	Err = svn_client_diff (opts,
						   path1.GetSVNApiPath(),
						   revision1,
						   path2.GetSVNApiPath(),
						   revision2,
						   recurse,
						   ignoreancestry,
						   nodiffdeleted,
						   outfile,
						   errfile,
						   m_pctx,
						   localpool);
	if (Err)
	{
		return FALSE;
	}
	if (del)
	{
		svn_io_remove_file (workingErrorFile.GetSVNApiPath(), localpool);
	}
	return TRUE;
}

BOOL SVN::PegDiff(const CTSVNPath& path, SVNRev pegrevision, SVNRev startrev, SVNRev endrev, BOOL recurse, BOOL ignoreancestry, BOOL nodiffdeleted, CString options, const CTSVNPath& outputfile)
{
	return PegDiff(path, pegrevision, startrev, endrev, recurse, ignoreancestry, nodiffdeleted, options, outputfile, CTSVNPath());
}

BOOL SVN::PegDiff(const CTSVNPath& path, SVNRev pegrevision, SVNRev startrev, SVNRev endrev, BOOL recurse, BOOL ignoreancestry, BOOL nodiffdeleted, CString options, const CTSVNPath& outputfile, const CTSVNPath& errorfile)
{
	BOOL del = FALSE;
	apr_file_t * outfile;
	apr_file_t * errfile;
	apr_array_header_t *opts;

	SVNPool localpool(pool);

	opts = svn_cstring_split (CUnicodeUtils::GetUTF8(options), " \t\n\r", TRUE, localpool);

	Err = svn_io_file_open (&outfile, outputfile.GetSVNApiPath(),
		APR_WRITE | APR_CREATE | APR_TRUNCATE | APR_BINARY,
		APR_OS_DEFAULT, localpool);
	if (Err)
		return FALSE;

	CTSVNPath workingErrorFile;
	if (errorfile.IsEmpty())
	{
		workingErrorFile = CUtils::GetTempFilePath();
		del = TRUE;
	}
	else
	{
		workingErrorFile = errorfile;
	}

	Err = svn_io_file_open (&errfile, workingErrorFile.GetSVNApiPath(),
		APR_WRITE | APR_CREATE | APR_TRUNCATE | APR_BINARY,
		APR_OS_DEFAULT, localpool);
	if (Err)
		return FALSE;

	Err = svn_client_diff_peg (opts,
		path.GetSVNApiPath(),
		pegrevision,
		startrev,
		endrev,
		recurse,
		ignoreancestry,
		nodiffdeleted,
		outfile,
		errfile,
		m_pctx,
		localpool);
	if (Err)
	{
		return FALSE;
	}
	if (del)
	{
		svn_io_remove_file (workingErrorFile.GetSVNApiPath(), localpool);
	}
	return TRUE;
}

BOOL SVN::ReceiveLog(const CTSVNPathList& pathlist, SVNRev revisionStart, SVNRev revisionEnd, BOOL changed, BOOL strict /* = FALSE */)
{
	Err = svn_client_log (MakePathArray(pathlist), 
						revisionStart, 
						revisionEnd, 
						changed,
						strict,
						logReceiver,	// log_message_receiver
						(void *)this, m_pctx, pool);

	if(Err != NULL)
	{
		return FALSE;
	}
	return TRUE;
}

BOOL SVN::Cat(const CTSVNPath& url, SVNRev revision, const CTSVNPath& localpath)
{
	apr_file_t * file;
	svn_stream_t * stream;
	apr_status_t status;

	CTSVNPath fullLocalPath(localpath);
	if (fullLocalPath.IsDirectory())
	{
		fullLocalPath.AppendPathString(url.GetFileOrDirectoryName());
	}
	::DeleteFile(fullLocalPath.GetWinPath());

	status = apr_file_open(&file, fullLocalPath.GetSVNApiPath(), APR_WRITE | APR_CREATE, APR_OS_DEFAULT, pool);
	if (status)
	{
		Err = svn_error_wrap_apr(status, NULL);
		return FALSE;
	}
	stream = svn_stream_from_aprfile(file, pool);

	Err = svn_client_cat(stream, url.GetSVNApiPath(), revision, m_pctx, pool);

	apr_file_close(file);
	if (Err != NULL)
		return FALSE;
	return TRUE;
}

CString SVN::GetActionText(svn_wc_notify_action_t action, svn_wc_notify_state_t content_state, svn_wc_notify_state_t prop_state)
{
	CString temp = _T(" ");
	switch (action)
	{
		case svn_wc_notify_add:
		case svn_wc_notify_update_add:
			temp.LoadString(IDS_SVNACTION_ADD);
			break;
		case svn_wc_notify_commit_added:
			temp.LoadString(IDS_SVNACTION_ADDING);
			break;
		case svn_wc_notify_copy:
			temp.LoadString(IDS_SVNACTION_COPY);
			break;
		case svn_wc_notify_delete:
		case svn_wc_notify_update_delete:
			temp.LoadString(IDS_SVNACTION_DELETE);
			break;
		case svn_wc_notify_commit_deleted:
			temp.LoadString(IDS_SVNACTION_DELETING);
			break;
		case svn_wc_notify_restore:
			temp.LoadString(IDS_SVNACTION_RESTORE);
			break;
		case svn_wc_notify_revert:
			temp.LoadString(IDS_SVNACTION_REVERT);
			break;
		case svn_wc_notify_resolved:
			temp.LoadString(IDS_SVNACTION_RESOLVE);
			break;
		case svn_wc_notify_update_update:
			if ((content_state == svn_wc_notify_state_conflicted) || (prop_state == svn_wc_notify_state_conflicted))
				temp.LoadString(IDS_SVNACTION_CONFLICTED);
			else if ((content_state == svn_wc_notify_state_merged) || (prop_state == svn_wc_notify_state_merged))
				temp.LoadString(IDS_SVNACTION_MERGED);
			else
				temp.LoadString(IDS_SVNACTION_UPDATE);
			break;
		case svn_wc_notify_update_completed:
			temp.LoadString(IDS_SVNACTION_COMPLETED);
			break;
		case svn_wc_notify_update_external:
			temp.LoadString(IDS_SVNACTION_EXTERNAL);
			break;
		case svn_wc_notify_commit_modified:
			temp.LoadString(IDS_SVNACTION_MODIFIED);
			break;
		case svn_wc_notify_commit_replaced:
			temp.LoadString(IDS_SVNACTION_REPLACED);
			break;
		case svn_wc_notify_commit_postfix_txdelta:
			temp.LoadString(IDS_SVNACTION_POSTFIX);
			break;
		case svn_wc_notify_failed_revert:
			temp.LoadString(IDS_SVNACTION_FAILEDREVERT);
			break;
		case svn_wc_notify_status_completed:
		case svn_wc_notify_status_external:
			temp.LoadString(IDS_SVNACTION_STATUS);
			break;
		case svn_wc_notify_skip:
			if (content_state == svn_wc_notify_state_missing)
				temp.LoadString(IDS_SVNACTION_SKIPMISSING);
			else
				temp.LoadString(IDS_SVNACTION_SKIP);
			break;
		default:
			return _T("???");
	}
	return temp;
}

BOOL SVN::CreateRepository(CString path, CString fstype)
{
	svn_repos_t * repo;
	svn_error_t * err;
	apr_hash_t *config;

	SVNPool localpool;

	const char * deststr = NULL;
	svn_utf_cstring_to_utf8(&deststr, "dummy", localpool);
	svn_utf_cstring_from_utf8(&deststr, "dummy", localpool);
	
	apr_hash_t *fs_config = apr_hash_make (localpool);;

	apr_hash_set (fs_config, SVN_FS_CONFIG_BDB_TXN_NOSYNC,
		APR_HASH_KEY_STRING, "0");

	apr_hash_set (fs_config, SVN_FS_CONFIG_BDB_LOG_AUTOREMOVE,
		APR_HASH_KEY_STRING, "1");

	err = svn_config_get_config (&config, NULL, localpool);
	if (err != NULL)
	{
		return FALSE;
	}
	const char * fs_type = apr_pstrdup(localpool, CStringA(fstype));
	apr_hash_set (fs_config, SVN_FS_CONFIG_FS_TYPE,
		APR_HASH_KEY_STRING,
		fs_type);
	err = svn_repos_create(&repo, MakeSVNUrlOrPath(path), NULL, NULL, config, fs_config, localpool);
	if (err != NULL)
	{
		return FALSE;
	} // if (err != NULL) 
	return TRUE;
}

BOOL SVN::Blame(const CTSVNPath& path, SVNRev startrev, SVNRev endrev)
{
	Err = svn_client_blame ( path.GetSVNApiPath(),
							 startrev,  
							 endrev,  
							 blameReceiver,  
							 (void *)this,  
							 m_pctx,  
							 pool );

	if(Err != NULL)
	{
		return FALSE;
	}
	return TRUE;
}

svn_error_t* SVN::blameReceiver(void* baton,
								apr_off_t line_no,
								svn_revnum_t revision,
								const char * author,
								const char * date,
								const char * line,
								apr_pool_t * pool)
{
	svn_error_t * error = NULL;
	CString author_native;
	CStringA line_native;
	TCHAR date_native[MAX_PATH] = {0};

	SVN * svn = (SVN *)baton;

	if (author)
		author_native = CUnicodeUtils::GetUnicode(author);
	if (line)
		line_native = line;

	if (date && date[0])
	{
		//Convert date to a format for humans.
		apr_time_t time_temp;

		error = svn_time_from_cstring (&time_temp, date, pool);
		if (error)
			return error;

		formatDate(date_native, time_temp, true);
	}
	else
		_tcscat(date_native, _T("(no date)"));

	if (!svn->BlameCallback((LONG)line_no, revision, author_native, date_native, line_native))
	{
		return svn_error_create(SVN_ERR_CANCELLED, NULL, "error in blame callback");
	}
	return error;
}

svn_error_t* SVN::logReceiver(void* baton, 
								apr_hash_t* ch_paths, 
								svn_revnum_t rev, 
								const char* author, 
								const char* date, 
								const char* msg, 
								apr_pool_t* pool)
{
	svn_error_t * error = NULL;
	TCHAR date_native[MAX_PATH] = {0};
	CString author_native;
	CString msg_native;

	SVN * svn = (SVN *)baton;
	author_native = CUnicodeUtils::GetUnicode(author);
	apr_time_t time_temp = NULL;

	if (date && date[0])
	{
		//Convert date to a format for humans.
		error = svn_time_from_cstring (&time_temp, date, pool);
		if (error)
			return error;

		formatDate(date_native, time_temp);
	}
	else
		_tcscat(date_native, _T("(no date)"));

	if (msg == NULL)
		msg = "";

	msg_native = CUnicodeUtils::GetUnicode(msg);
	int filechanges = 0;
	BOOL copies = FALSE;
	LogChangedPathArray * arChangedPaths = new LogChangedPathArray;
	try
	{
		if (ch_paths)
		{
			CString sModifiedStatus, sReplacedStatus, sAddStatus, sDeleteStatus;
			sModifiedStatus.LoadString(IDS_SVNACTION_MODIFIED);
			sReplacedStatus.LoadString(IDS_SVNACTION_REPLACED);
			sAddStatus.LoadString(IDS_SVNACTION_ADD);
			sDeleteStatus.LoadString(IDS_SVNACTION_DELETE);
			apr_array_header_t *sorted_paths;
			sorted_paths = svn_sort__hash(ch_paths, svn_sort_compare_items_as_paths, pool);
			filechanges = sorted_paths->nelts;
			for (int i = 0; i < sorted_paths->nelts; i++)
			{
				LogChangedPath * changedpath = new LogChangedPath;
				svn_sort__item_t *item = &(APR_ARRAY_IDX (sorted_paths, i, svn_sort__item_t));
				CString path_native;
				const char *path = (const char *)item->key;
				svn_log_changed_path_t *log_item = (svn_log_changed_path_t *)apr_hash_get (ch_paths, item->key, item->klen);
				path_native = MakeUIUrlOrPath(path);
				changedpath->sPath = path_native;
				switch (log_item->action)
				{
				case 'M':
					changedpath->sAction = sModifiedStatus;
					break;
				case 'R':
					changedpath->sAction = sReplacedStatus;
					break;
				case 'A':
					changedpath->sAction = sAddStatus;
					break;
				case 'D':
					changedpath->sAction = sDeleteStatus;
				default:
					break;
				}
				if (log_item->copyfrom_path && SVN_IS_VALID_REVNUM (log_item->copyfrom_rev))
				{
					changedpath->sCopyFromPath = MakeUIUrlOrPath(log_item->copyfrom_path);
					changedpath->lCopyFromRev = log_item->copyfrom_rev;
					copies = TRUE;
				}
				else
				{
					changedpath->lCopyFromRev = 0;
				}
				arChangedPaths->Add(changedpath);
			} // for (int i = 0; i < sorted_paths->nelts; i++) 
		} // if (ch_paths)
	}
	catch (CMemoryException * e)
	{
		e->Delete();
	}
#pragma warning(push)
#pragma warning(disable: 4127)	// conditional expression is constant
	SVN_ERR (svn->cancel(baton));
#pragma warning(pop)

	if (svn->Log(rev, author_native, date_native, msg_native, arChangedPaths, time_temp, filechanges, copies))
	{
		return error;
	}
	return error;
}

void SVN::notify( void *baton,
					const char *path,
					svn_wc_notify_action_t action,
					svn_node_kind_t kind,
					const char *mime_type,
					svn_wc_notify_state_t content_state,
					svn_wc_notify_state_t prop_state,
					svn_revnum_t revision)
{
	SVN * svn = (SVN *)baton;
//	WCHAR buf[MAX_PATH*4];
//	if (!MultiByteToWideChar(CP_UTF8, 0, path, -1, buf, MAX_PATH*4))
//		buf[0] = 0;

	CTSVNPath tsvnPath;
	tsvnPath.SetFromSVN(path);

	// Let the shell know (eventually) that we might have changed the state of this path
	CShellUpdater::Instance().AddPathForUpdate(tsvnPath);

	CString mime;
	if (mime_type)
		mime = CUnicodeUtils::GetUnicode(mime_type);

	svn->Notify(tsvnPath, (svn_wc_notify_action_t)action, kind, mime, content_state, prop_state, revision);
}

svn_error_t* SVN::cancel(void *baton)
{
	SVN * svn = (SVN *)baton;
	if (svn->Cancel())
	{
		CStringA temp;
		temp.LoadString(IDS_SVN_USERCANCELLED);
		return svn_error_create(SVN_ERR_CANCELLED, NULL, temp);
	}
	return SVN_NO_ERROR;
}

void * SVN::logMessage (const char * message, char * baseDirectory)
{
	log_msg_baton* baton = (log_msg_baton *) apr_palloc (pool, sizeof (*baton));
	baton->message = apr_pstrdup(pool, message);
	baton->base_dir = baseDirectory ? baseDirectory : "";

	baton->message_encoding = NULL;
	baton->tmpfile_left = NULL;
	baton->pool = pool;

	return baton;
}

void SVN::PathToUrl(CString &path)
{
	path.Trim();
	// convert \ to /
	path.Replace('\\','/');
	// prepend file:///
	if (path.GetAt(0) == '/')
		path.Insert(0, _T("file://"));
	else
		path.Insert(0, _T("file:///"));
	path.TrimRight(_T("/\\"));			//remove trailing slashes
}

void SVN::UrlToPath(CString &url)
{
	//we have to convert paths like file:///c:/myfolder
	//to c:/myfolder
	//and paths like file:////mymachine/c/myfolder
	//to //mymachine/c/myfolder
	url.Trim();
	url.Replace('\\','/');
	url = url.Mid(7);
	if (url.GetAt(1) != '/')
		url = url.Mid(1);
	SVN::preparePath(url);
}

void	SVN::preparePath(CString &path)
{
	path.Trim();
	path.TrimRight(_T("/\\"));			//remove trailing slashes
	path.Replace('\\','/');
	//workaround for Subversions UNC-path bug
	if (path.Left(10).CompareNoCase(_T("file://///"))==0)
	{
		path.Replace(_T("file://///"), _T("file:///\\"));
	}
	else if (path.Left(9).CompareNoCase(_T("file:////"))==0)
	{
		path.Replace(_T("file:////"), _T("file:///\\"));
	}
}

svn_error_t* svn_cl__get_log_message (const char **log_msg,
									const char **tmp_file,
									apr_array_header_t * /*commit_items*/,
									void *baton, 
									apr_pool_t * pool)
{
	log_msg_baton *lmb = (log_msg_baton *) baton;
	*tmp_file = NULL;
	if (lmb->message)
	{
		*log_msg = apr_pstrdup (pool, lmb->message);
	}

	return SVN_NO_ERROR;
}

CString SVN::GetURLFromPath(const CTSVNPath& path)
{
	const char * URL;
	Err = get_url_from_target(&URL, path.GetSVNApiPath());
	if (Err)
		return _T("");
	if (URL==NULL)
		return _T("");
	return MakeUIUrlOrPath(URL);
}

svn_error_t * SVN::get_url_from_target (const char **URL, const char *target)
{
	svn_wc_adm_access_t *adm_access;          
	const svn_wc_entry_t *entry;  
	const char * canontarget = svn_path_canonicalize(target, pool);
	svn_boolean_t is_url = svn_path_is_url (canontarget);

	if (is_url)
		*URL = apr_pstrdup(pool, canontarget);

	else
	{
#pragma warning(push)
#pragma warning(disable: 4127)	// conditional expression is constant
		SVN_ERR (svn_wc_adm_probe_open2 (&adm_access, NULL, canontarget,
			FALSE, 0, pool));
		SVN_ERR (svn_wc_entry (&entry, canontarget, adm_access, FALSE, pool));
		SVN_ERR (svn_wc_adm_close (adm_access));
#pragma warning(pop)

		*URL = entry ? entry->url : NULL;
	}

	return SVN_NO_ERROR;
}

CString SVN::GetUUIDFromPath(const CTSVNPath& path)
{
	const char * UUID;
	Err = get_uuid_from_target(&UUID, path.GetSVNApiPath());
	if (Err)
		return _T("");
	if (UUID==NULL)
		return _T("");
	CString ret = CString(UUID);
	return ret;
}

svn_error_t * SVN::get_uuid_from_target (const char **UUID, const char *target)
{
	svn_wc_adm_access_t *adm_access;          
#pragma warning(push)
#pragma warning(disable: 4127)	// conditional expression is constant
	SVN_ERR (svn_wc_adm_probe_open2 (&adm_access, NULL, target,
		FALSE, 0, pool));
	SVN_ERR (svn_client_uuid_from_path(UUID, target, adm_access, m_pctx, pool));
	SVN_ERR (svn_wc_adm_close (adm_access));
#pragma warning(pop)

	return SVN_NO_ERROR;
}

BOOL SVN::Ls(const CTSVNPath& url, SVNRev revision, CStringArray& entries, BOOL extended, BOOL recursive)
{
	entries.RemoveAll();
	SVNPool subpool(pool);

	apr_hash_t* hash = apr_hash_make(subpool);

	Err = svn_client_ls(&hash, 
						url.GetSVNApiPath(),
						revision,
						recursive, 
						m_pctx,
						subpool);
	if (Err != NULL)
	{
		return FALSE;
	}

	apr_hash_index_t *hi;
    svn_dirent_t* val;
	const char* key;
    for (hi = apr_hash_first(pool, hash); hi; hi = apr_hash_next(hi)) 
	{
        apr_hash_this(hi, (const void**)&key, NULL, (void**)&val);
		CString temp;
		if (val->kind == svn_node_dir)
			temp = "d";
		else if (val->kind == svn_node_file)
			temp = "f";
		else
			temp = "u";
		temp = temp + MakeUIUrlOrPath(key);
		if (extended)
		{
			CString author, revnum, size, dateval;
			TCHAR date_native[_MAX_PATH];
			author = CUnicodeUtils::GetUnicode(val->last_author);
			revnum.Format(_T("%u"), val->created_rev);
			if (val->kind != svn_node_dir)
				size.Format(_T("%u KB"), (val->size+1023)/1024);
			formatDate(date_native, val->time, true);
			dateval.Format(_T("%I64u"), val->time);
			temp = temp + _T("\t") + revnum + _T("\t") + author + _T("\t") + size + _T("\t") + date_native + _T("\t") + dateval;;
		}
		entries.Add(temp);
    }
	return Err == NULL;
}

BOOL SVN::Relocate(const CTSVNPath& path, const CTSVNPath& from, const CTSVNPath& to, BOOL recurse)
{
	Err = svn_client_relocate(
				path.GetSVNApiPath(), 
				from.GetSVNApiPath(), 
				to.GetSVNApiPath(), 
				recurse, m_pctx, pool);
	if (Err != NULL)
		return FALSE;
	return TRUE;
}

BOOL SVN::IsRepository(const CString& strUrl)
{
	svn_repos_t* pRepos;
	CString url = strUrl;
	preparePath(url);
	url += _T("/");
	int pos = url.GetLength();
	while ((pos = url.ReverseFind('/'))>=0)
	{
		url = url.Left(pos);
		Err = svn_repos_open (&pRepos, MakeSVNUrlOrPath(url), pool);
		if ((Err)&&(Err->apr_err == SVN_ERR_FS_BERKELEY_DB))
			return TRUE;
		if (Err == NULL)
			return TRUE;
	}

	return Err == NULL;
}

BOOL SVN::IsBDBRepository(CString url)
{
	preparePath(url);
	url = url.Mid(7);
	url.TrimLeft('/');
	while (!url.IsEmpty())
	{
		if (PathIsDirectory(url + _T("/db")))
		{
			if (PathFileExists(url + _T("/db/fs-type")))
				return FALSE;
			return TRUE;
		}
		if (url.ReverseFind('/')>=0)
			url = url.Left(url.ReverseFind('/'));
		else
			url.Empty();
	}
	return FALSE;
}

CString SVN::GetRepositoryRoot(const CTSVNPath& url)
{
	svn_ra_plugin_t *ra_lib;
	void *ra_baton, *session;
	const char * returl;

	SVNPool localpool(pool);
	/* Get the RA library that handles URL. */
	if (svn_ra_init_ra_libs (&ra_baton, localpool))
		return _T("");
	if (svn_ra_get_ra_library (&ra_lib, ra_baton, url.GetSVNApiPath(), localpool))
		return _T("");

	/* Open a repository session to the URL. */
	if (svn_client__open_ra_session (&session, ra_lib, url.GetSVNApiPath(), NULL, NULL, NULL, FALSE, FALSE, m_pctx, localpool))
		return _T("");

	if (ra_lib->get_repos_root(session, &returl, localpool))
		return _T("");

	return CString(returl);
}

LONG SVN::GetHEADRevision(const CTSVNPath& url)
{
	svn_ra_plugin_t *ra_lib;
	void *ra_baton, *session;
	const char * urla;
	LONG rev;

	SVNPool localpool(pool);
	if (!url.IsUrl())
		SVN::get_url_from_target(&urla, url.GetSVNApiPath());
	else
		urla = url.GetSVNApiPath();

	if (urla == NULL)
		return -1;

	/* Get the RA library that handles URL. */
	if (svn_ra_init_ra_libs (&ra_baton, localpool))
		return -1;
	if (svn_ra_get_ra_library (&ra_lib, ra_baton, urla, localpool))
		return -1;

	/* Open a repository session to the URL. */
	if (svn_client__open_ra_session (&session, ra_lib, urla, NULL, NULL, NULL, FALSE, FALSE, m_pctx, localpool))
		return -1;

	if (ra_lib->get_latest_revnum(session, &rev, localpool))
		return -1;
	return rev;
}

LONG SVN::RevPropertySet(CString sName, CString sValue, CString sURL, SVNRev rev)
{
	svn_revnum_t set_rev;
	svn_string_t*	pval;
	sValue.Replace(_T("\r"), _T(""));
	pval = svn_string_create (MakeSVNUrlOrPath(sValue), pool);
	Err = svn_client_revprop_set(MakeSVNUrlOrPath(sName), 
									pval, 
									MakeSVNUrlOrPath(sURL), 
									rev, 
									&set_rev, 
									FALSE, 
									m_pctx, 
									pool);
	if (Err)
		return 0;
	return set_rev;
}

CString SVN::RevPropertyGet(CString sName, CString sURL, SVNRev rev)
{
	svn_string_t *propval;
	svn_revnum_t set_rev;
	Err = svn_client_revprop_get(MakeSVNUrlOrPath(sName), &propval, MakeSVNUrlOrPath(sURL), rev, &set_rev, m_pctx, pool);
	if (Err)
		return _T("");
	if (propval==NULL)
		return _T("");
	if (propval->len <= 0)
		return _T("");
	return CUnicodeUtils::GetUnicode(propval->data);
}

CTSVNPath SVN::GetPristinePath(const CTSVNPath& wcPath)
{
	svn_error_t * err;
	SVNPool localpool;

	const char * deststr = NULL;
	svn_utf_cstring_to_utf8(&deststr, "dummy", localpool);
	svn_utf_cstring_from_utf8(&deststr, "dummy", localpool);

	const char* pristinePath = NULL;
	CTSVNPath returnPath;

	err = svn_wc_get_pristine_copy_path(svn_path_internal_style(wcPath.GetSVNApiPath(), localpool), &pristinePath, localpool);

	if (err != NULL)
	{
		return returnPath;
	}
	if (pristinePath != NULL)
	{
		returnPath.SetFromSVN(pristinePath);
	}
	return returnPath;
}

BOOL SVN::GetTranslatedFile(CTSVNPath& sTranslatedFile, const CTSVNPath sFile, BOOL bForceRepair /*= TRUE*/)
{
	svn_wc_adm_access_t *adm_access;          
	svn_error_t * err;
	SVNPool localpool;

	const char * deststr = NULL;
	svn_utf_cstring_to_utf8(&deststr, "dummy", localpool);
	svn_utf_cstring_from_utf8(&deststr, "dummy", localpool);

	const char * translatedPath = NULL;
	CStringA temp = sFile.GetSVNApiPath();
	const char * originPath = svn_path_canonicalize(temp, localpool);
	err = svn_wc_adm_probe_open2 (&adm_access, NULL, originPath, FALSE, 0, localpool);
	if (err)
		return FALSE;
	err = svn_wc_translated_file((const char **)&translatedPath, originPath, adm_access, bForceRepair, localpool);
	svn_wc_adm_close(adm_access);
	if (err)
		return FALSE;
	
	sTranslatedFile.SetFromUnknown(MakeUIUrlOrPath(translatedPath));
	return (translatedPath != originPath);
}

BOOL SVN::PathIsURL(const CString& path)
{
	return svn_path_is_url(MakeSVNUrlOrPath(path));
} 

void SVN::formatDate(TCHAR date_native[], apr_time_t& date_svn, bool force_short_fmt)
{
	date_native[0] = '\0';
	apr_time_exp_t exploded_time = {0};
	
	SYSTEMTIME systime;
	TCHAR timebuf[MAX_PATH];
	TCHAR datebuf[MAX_PATH];

	LCID locale = (WORD)CRegStdWORD(_T("Software\\TortoiseSVN\\LanguageID"), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT));
	locale = MAKELCID(locale, SORT_DEFAULT);

	apr_time_exp_lt (&exploded_time, date_svn);
	
	systime.wDay = (WORD)exploded_time.tm_mday;
	systime.wDayOfWeek = (WORD)exploded_time.tm_wday;
	systime.wHour = (WORD)exploded_time.tm_hour;
	systime.wMilliseconds = (WORD)(exploded_time.tm_usec/1000);
	systime.wMinute = (WORD)exploded_time.tm_min;
	systime.wMonth = (WORD)exploded_time.tm_mon+1;
	systime.wSecond = (WORD)exploded_time.tm_sec;
	systime.wYear = (WORD)exploded_time.tm_year+1900;
	if (force_short_fmt || CRegDWORD(_T("Software\\TortoiseSVN\\LogDateFormat")) == 1)
	{
		GetDateFormat(locale, DATE_SHORTDATE, &systime, NULL, datebuf, MAX_PATH);
		GetTimeFormat(locale, 0, &systime, NULL, timebuf, MAX_PATH);
		_tcsncat(date_native, datebuf, MAX_PATH);
		_tcsncat(date_native, _T(" "), MAX_PATH);
		_tcsncat(date_native, timebuf, MAX_PATH);
	}
	else
	{
		GetDateFormat(locale, DATE_LONGDATE, &systime, NULL, datebuf, MAX_PATH);
		GetTimeFormat(locale, 0, &systime, NULL, timebuf, MAX_PATH);
		_tcsncat(date_native, timebuf, MAX_PATH);
		_tcsncat(date_native, _T(", "), MAX_PATH);
		_tcsncat(date_native, datebuf, MAX_PATH);
	}
}

CStringA SVN::MakeSVNUrlOrPath(const CString& UrlOrPath)
{
	CStringA url = CUnicodeUtils::GetUTF8(UrlOrPath);
	if (svn_path_is_url(url))
	{
		if (!CUtils::IsEscaped(url))
			url = CUtils::PathEscape(url);
	}
	return url;
}

CString SVN::MakeUIUrlOrPath(CStringA UrlOrPath)
{
	if (svn_path_is_url(UrlOrPath))
	{
		CUtils::Unescape(UrlOrPath.GetBuffer());
		UrlOrPath.ReleaseBuffer();
	}
	CString url = CUnicodeUtils::GetUnicode(UrlOrPath);
	return url;
}

BOOL SVN::EnsureConfigFile()
{
	svn_error_t * err;
	SVNPool localpool;
	err = svn_config_ensure(NULL, localpool);
	if (err)
	{
		return FALSE;
	}
	return TRUE;
}

void SVN::UseIEProxySettings(apr_hash_t * cfg)
{
	CStringA exceptions;
	CStringA port;
	CStringA proxy;
	CStringA temp;
	const char * valuep;
	INTERNET_PER_CONN_OPTION_LIST    List;
	INTERNET_PER_CONN_OPTION         Option[5];
	unsigned long                    nSize = sizeof(INTERNET_PER_CONN_OPTION_LIST);

	Option[0].dwOption = INTERNET_PER_CONN_AUTOCONFIG_URL;
	Option[1].dwOption = INTERNET_PER_CONN_AUTODISCOVERY_FLAGS;
	Option[2].dwOption = INTERNET_PER_CONN_FLAGS;
	Option[3].dwOption = INTERNET_PER_CONN_PROXY_BYPASS;
	Option[4].dwOption = INTERNET_PER_CONN_PROXY_SERVER;

	List.dwSize = sizeof(INTERNET_PER_CONN_OPTION_LIST);
	List.pszConnection = NULL;
	List.dwOptionCount = 5;
	List.dwOptionError = 0;
	List.pOptions = Option;

	CString server = CRegString(_T("Software\\Tigris.org\\Subversion\\Servers\\global\\http-proxy-host"), _T(""));
	if (!server.IsEmpty())
		return;

	if(!InternetQueryOption(NULL, INTERNET_OPTION_PER_CONNECTION_OPTION, &List, &nSize))
		return;

	if((Option[2].Value.dwValue & PROXY_TYPE_AUTO_PROXY_URL) == PROXY_TYPE_AUTO_PROXY_URL)
		goto ERROR_LABEL;

	if((Option[2].Value.dwValue & PROXY_TYPE_AUTO_DETECT) == PROXY_TYPE_AUTO_DETECT)
		goto ERROR_LABEL;

	exceptions = CStringA(Option[3].Value.pszValue);
	exceptions.Replace(';', ',');
	proxy = CStringA(Option[4].Value.pszValue);
	if (proxy.Find(';')>=0)
	{
		if (proxy.Find("http=")>=0)
		{
			temp = proxy.Mid(proxy.Find("http=")+5);
			temp = temp.Left(temp.Find(';'));
		}
		if ((temp.IsEmpty())&&(proxy.Find("https=")>=0))
		{
			temp = proxy.Mid(proxy.Find("https=")+6);
			temp = temp.Left(temp.Find(';'));
		}
		proxy = temp;
	}
	if (proxy.Find(':')>=0)
	{
		port = proxy.Mid(proxy.Find(':')+1);
		proxy = proxy.Left(proxy.Find(':'));
	}
	if (proxy.IsEmpty())
		goto ERROR_LABEL;

	svn_config_t * opt = (svn_config_t *)apr_hash_get (cfg, SVN_CONFIG_CATEGORY_SERVERS,
		APR_HASH_KEY_STRING);
	svn_config_get(opt, &valuep, SVN_CONFIG_SECTION_GLOBAL, "http-proxy-exceptions", "");
	exceptions += ",";
	exceptions += valuep;
	svn_config_set(opt, SVN_CONFIG_SECTION_GLOBAL, "http-proxy-exceptions", exceptions);
	
	svn_config_set(opt, SVN_CONFIG_SECTION_GLOBAL, "http-proxy-host", proxy);
	
	svn_config_set(opt, SVN_CONFIG_SECTION_GLOBAL, "http-proxy-port", port);



ERROR_LABEL:
	if(Option[0].Value.pszValue != NULL)
		GlobalFree(Option[0].Value.pszValue);

	if(Option[3].Value.pszValue != NULL)
		GlobalFree(Option[3].Value.pszValue);

	if(Option[4].Value.pszValue != NULL)
		GlobalFree(Option[4].Value.pszValue);
	return;
}

/** 
* Set the parent window of an authentication prompt dialog
*/
void SVN::SetPromptParentWindow(HWND hWnd)
{
	m_prompt.SetParentWindow(hWnd);
}
/** 
* Set the MFC Application object for a prompt dialog
*/
void SVN::SetPromptApp(CWinApp* pWinApp)
{
	m_prompt.SetApp(pWinApp);
}

apr_array_header_t * SVN::MakePathArray(const CTSVNPathList& pathList)
{
	apr_array_header_t *targets = apr_array_make (pool,pathList.GetCount(),sizeof(const char *));

	for(int nItem = 0; nItem < pathList.GetCount(); nItem++)
	{
		const char * target = apr_pstrdup (pool, pathList[nItem].GetSVNApiPath());
		(*((const char **) apr_array_push (targets))) = target;
	}
	return targets;
}

void SVN::StartConflictEditor(const CTSVNPath& conflictedFilePath)
{
	CTSVNPath merge = conflictedFilePath;
	CTSVNPath directory = merge.GetDirectory();
	CTSVNPath theirs(directory);
	CTSVNPath mine(directory);
	CTSVNPath base(directory);

	//we have the conflicted file (%merged)
	//now look for the other required files
	SVNStatus stat;
	stat.GetStatus(merge.GetSVNPathString());
	if (stat.status && stat.status->entry)
	{
		if (stat.status->entry->conflict_new)
		{
			theirs.AppendPathString(CUnicodeUtils::GetUnicode(stat.status->entry->conflict_new));
		}
		if (stat.status->entry->conflict_old)
		{
			base.AppendPathString(CUnicodeUtils::GetUnicode(stat.status->entry->conflict_old));
		}
		if (stat.status->entry->conflict_wrk)
		{
			mine.AppendPathString(CUnicodeUtils::GetUnicode(stat.status->entry->conflict_wrk));
		}
	}
	CUtils::StartExtMerge(base,theirs,mine,merge);
}

BOOL SVN::DiffFileAgainstBase(const CTSVNPath& filePath, CTSVNPath& temporaryFile)
{
	CTSVNPath basePath(GetPristinePath(filePath));
	CTSVNPath wcPath;
	// If necessary, convert the line-endings on the file before diffing
	if ((!CRegDWORD(_T("Software\\TortoiseSVN\\DontConvertBase"), TRUE))&&(GetTranslatedFile(wcPath, filePath)))
	{
		temporaryFile = wcPath;
	}
	else
	{
		temporaryFile.Reset();
		wcPath = filePath;
	}
	CString name = wcPath.GetFilename();
	CString n1, n2;
	n1.Format(IDS_DIFF_WCNAME, (LPCTSTR)name);
	n2.Format(IDS_DIFF_BASENAME, (LPCTSTR)name);
	return CUtils::StartExtDiff(basePath, wcPath, n2, n1, TRUE);
}

