/* streamprocess.h
 * Copyright (C) 2004 by Daniel Wagner
 *
 * This file is part of FreeBob.
 *
 * FreeBob is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * FreeBob is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with FreeBob; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA.
 */

#ifndef STREAMPROCESS_H
#define STREAMPROCESS_H

#include "cmhandler.h"
#include "debugmodule.h"

/** StreamProcess
 * Simulates a higherlayer application using the FreeBob driver.
 */
class StreamProcess {
 public:
    StreamProcess();
    ~StreamProcess();

    void run( int timeToListen );
 private:
    CMHandler* m_pCMHandler;
    
    DECLARE_DEBUG_MODULE;
};



#endif
