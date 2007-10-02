// TortoiseMerge - a Diff/Patch program

// Copyright (C) 2006-2007 - TortoiseSVN

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
#include "StdAfx.h"
#include "Undo.h"

#include "BaseView.h"

CUndo& CUndo::GetInstance()
{
	static CUndo instance;
	return instance;
}

CUndo::CUndo()
{
}

CUndo::~CUndo()
{
}

void CUndo::AddState(const viewstate& leftstate, const viewstate& rightstate, const viewstate& bottomstate, POINT pt)
{
	m_viewstates.push_back(bottomstate);
	m_viewstates.push_back(rightstate);
	m_viewstates.push_back(leftstate);
	m_caretpoints.push_back(pt);
}

bool CUndo::Undo(CBaseView * pLeft, CBaseView * pRight, CBaseView * pBottom)
{
	if (m_viewstates.size() > 0)
	{
		viewstate state = m_viewstates.back();
		Undo(state, pLeft);
		m_viewstates.pop_back();
		state = m_viewstates.back();
		Undo(state, pRight);
		m_viewstates.pop_back();
		state = m_viewstates.back();
		Undo(state, pBottom);
		m_viewstates.pop_back();
		if ((pLeft)&&(pLeft->HasCaret()))
		{
			pLeft->SetCaretPosition(m_caretpoints.back());
			pLeft->EnsureCaretVisible();
		}
		if ((pRight)&&(pRight->HasCaret()))
		{
			pRight->SetCaretPosition(m_caretpoints.back());
			pRight->EnsureCaretVisible();
		}
		if ((pBottom)&&(pBottom->HasCaret()))
		{
			pBottom->SetCaretPosition(m_caretpoints.back());
			pBottom->EnsureCaretVisible();
		}
		return true;
	}
	return false;
}

void CUndo::Undo(const viewstate& state, CBaseView * pView)
{
	if (pView)
	{
		bool bModified = false;
		for (std::list<int>::const_iterator it = state.addedlines.begin(); it != state.addedlines.end(); ++it)
		{
			if (pView->m_pViewData)
				pView->m_pViewData->RemoveData(*it);
			bModified = true;
		}
		for (std::map<int, DWORD>::const_iterator it = state.linelines.begin(); it != state.linelines.end(); ++it)
		{
			if (pView->m_pViewData)
			{
				pView->m_pViewData->SetLineNumber(it->first, it->second);
				bModified = true;
			}
		}
		for (std::map<int, DWORD>::const_iterator it = state.linestates.begin(); it != state.linestates.end(); ++it)
		{
			if (pView->m_pViewData)
			{
				pView->m_pViewData->SetState(it->first, (DiffStates)it->second);
				bModified = true;
			}
		}
		for (std::map<int, CString>::const_iterator it = state.difflines.begin(); it != state.difflines.end(); ++it)
		{
			if (pView->m_pViewData)
			{
				pView->m_pViewData->SetLine(it->first, it->second);
				bModified = true;
			}
		}
		for (std::map<int, viewdata>::const_iterator it = state.removedlines.begin(); it != state.removedlines.end(); ++it)
		{
			if (pView->m_pViewData)
			{
				pView->m_pViewData->InsertData(it->first, it->second.sLine, it->second.state, it->second.linenumber, it->second.ending);
				bModified = true;
			}
		}

		pView->DocumentUpdated();
		if (bModified)
			pView->SetModified();
	}
}