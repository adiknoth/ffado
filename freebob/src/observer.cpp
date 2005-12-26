/* observer.cpp
   Copyright (C) 2004 by Daniel Wagner

   This file is part of FreeBob.

   FreeBob is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   FreeBob is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with FreeBob; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, 
   MA 02111-1307 USA.  */

#include "observer.h"

void 
Subject::attach (Observer* pObserver) 
{
  m_observers.push_back (pObserver);
}

void 
Subject::detach (Observer* pObserver) 
{
  int iCount = m_observers.size ();
  int i;

  for (i = 0; i < iCount; i++) {
    if(m_observers[i] == pObserver)
      break;
  }
 
  if(i < iCount)
    m_observers.erase (m_observers.begin() + i);
}

void 
Subject::notify () 
{
  int iCount = m_observers.size ();

  for (int i = 0; i < iCount; i++)
    (m_observers[i])->update (this);
}
