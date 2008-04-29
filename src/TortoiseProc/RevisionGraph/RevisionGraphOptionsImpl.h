// TortoiseSVN - a Windows shell extension for easy version control

// Copyright (C) 2003-2008 - TortoiseSVN

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
#pragma once

/** Default implementation class for interfaces
* that derive from IRevisionGraphOption.
*/

template<class Base>
class IRevisionGraphOptionImpl : public Base
{
private:

    /// data members

    int priority;
    WORD id;
    bool selected;

protected:

    /// construction / destruction

    IRevisionGraphOptionImpl ( CRevisionGraphOptionList& list
                             , int priority
                             , WORD id);
    virtual ~IRevisionGraphOptionImpl() {};

public:

    /// implement IRevisionGraphOption

    virtual WORD CommandID() const;
    virtual int Priority() const; 

    virtual bool IsAvailable() const;
    virtual bool IsSelected() const;
    virtual bool IsActive() const; 

    virtual void ToggleSelection();
};

// construction

template<class Base>
IRevisionGraphOptionImpl<Base>::IRevisionGraphOptionImpl 
    ( CRevisionGraphOptionList& list
    , int priority
    , WORD id)
    : priority (priority)
    , id (id)
    , selected (false)
{
    list.Add (this);
}

// implement IRevisionGraphOption

template<class Base>
WORD IRevisionGraphOptionImpl<Base>::CommandID() const
{
    return id;
}

template<class Base>
int IRevisionGraphOptionImpl<Base>::Priority() const
{
    return priority;
}

template<class Base>
bool IRevisionGraphOptionImpl<Base>::IsAvailable() const
{
    return true;
}

template<class Base>
bool IRevisionGraphOptionImpl<Base>::IsSelected() const
{
    return selected;
}

template<class Base>
bool IRevisionGraphOptionImpl<Base>::IsActive() const
{
    return IsSelected();
}

template<class Base>
void IRevisionGraphOptionImpl<Base>::ToggleSelection()
{
    selected = !selected;
}

/** Implement a simple boolean option.
*/

template<class Base, int Prio, WORD ID>
class CRevisionGraphOptionImpl : public IRevisionGraphOptionImpl<Base>
{
public:

    /// construction / destruction

    CRevisionGraphOptionImpl (CRevisionGraphOptionList& list)
        : IRevisionGraphOptionImpl<Base>(list, Prio, ID)
    {
    }
};

