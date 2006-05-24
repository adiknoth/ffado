/* debugmodule.h
 * Copyright (C) 2005 by Daniel Wagner
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

#ifndef DEBUGMODULE_H
#define DEBUGMODULE_H

#include "../fbtypes.h"

#include <vector>
#include <iostream>

typedef short debug_level_t;

#define debugFatal( format, args... )                               \
                m_debugModule.print( DebugModule::eDL_Fatal,        \
                                     __FILE__,                      \
                                     __FUNCTION__,                  \
                                     __LINE__,                      \
                                     format,                        \
                                     ##args )
#define debugError( format, args... )                               \
                m_debugModule.print( DebugModule::eDL_Error,        \
                                     __FILE__,                      \
                                     __FUNCTION__,                  \
                                     __LINE__,                      \
                                     format,                        \
                                     ##args )
#define debugWarning( format, args... )                             \
                m_debugModule.print( DebugModule::eDL_Warning,      \
                                     __FILE__,                      \
                                     __FUNCTION__,                  \
                                     __LINE__,                      \
                                    format,                         \
                                    ##args )

#define debugFatalShort( format, args... )                          \
                m_debugModule.printShort( DebugModule::eDL_Fatal,   \
                                     format,                        \
                                     ##args )
#define debugErrorShort( format, args... )                          \
                m_debugModule.printShort( DebugModule::eDL_Error,   \
                                     format,                        \
                                     ##args )
#define debugWarningShort( format, args... )                        \
                m_debugModule.printShort( DebugModule::eDL_Warning, \
                                     format,                        \
                                     ##args )

#define DECLARE_DEBUG_MODULE static DebugModule m_debugModule
#define IMPL_DEBUG_MODULE( ClassName, RegisterName, Level )        \
                DebugModule ClassName::m_debugModule =             \
                    DebugModule( #RegisterName, Level )

#define DECLARE_GLOBAL_DEBUG_MODULE extern DebugModule m_debugModule
#define IMPL_GLOBAL_DEBUG_MODULE( RegisterName, Level )            \
                DebugModule m_debugModule =                        \
		    DebugModule( #RegisterName, Level )

#define setDebugLevel( Level ) {                                    \
                m_debugModule.setLevel( Level ); \
                m_debugModule.print( Level,                        \
                                     __FILE__,                     \
                                     __FUNCTION__,                 \
                                     __LINE__,                     \
                                     "Setting debug level to %d\n",  \
                                     Level ); \
				}
#define getDebugLevel( Level )                                     \
                m_debugModule.getLevel( )


#ifdef DEBUG

    #define debugOutput( level, format, args... )                  \
                m_debugModule.print( level,                        \
                                     __FILE__,                     \
                                     __FUNCTION__,                 \
                                     __LINE__,                     \
                                     format,                       \
                                     ##args )

    #define debugOutputShort( level, format, args... )             \
                m_debugModule.printShort( level,                   \
                                     format,                       \
                                     ##args )

#else

    #define debugOutput( level, format, args... )
    #define debugOutputShort( level, format, args... )

#endif

unsigned char toAscii( unsigned char c );
void quadlet2char( fb_quadlet_t quadlet, unsigned char* buff );
void hexDump( unsigned char *data_start, unsigned int length );
void hexDumpQuadlets( quadlet_t *data_start, unsigned int length );

class DebugModule {
public:
    enum {
        eDL_Fatal       = 0,
        eDL_Error       = 1,
        eDL_Warning     = 2,
        eDL_Normal      = 3,
        eDL_Verbose     = 4,
        eDL_VeryVerbose = 5,
    } EDebugLevel;

    DebugModule( std::string name, debug_level_t level );
    virtual ~DebugModule();

    void printShort( debug_level_t level,
                     const char* format,
                     ... ) const;

    void print( debug_level_t level,
                const char*   file,
                const char*   function,
                unsigned int  line,
                const char*   format,
                ... ) const;

    bool setLevel( debug_level_t level )
        { m_level = level; return true; }
    debug_level_t getLevel()
        { return m_level; }
    std::string getName()
        { return m_name; }

protected:
    const char* getPreSequence( debug_level_t level ) const;
    const char* getPostSequence( debug_level_t level ) const;

private:
    std::string   m_name;
    debug_level_t m_level;
};

#define DEBUG_LEVEL_NORMAL          DebugModule::eDL_Normal
#define DEBUG_LEVEL_VERBOSE         DebugModule::eDL_Verbose
#define DEBUG_LEVEL_VERY_VERBOSE    DebugModule::eDL_VeryVerbose


class DebugModuleManager {
public:
    friend class DebugModule;

    static DebugModuleManager* instance();
    bool setMgrDebugLevel( std::string name, debug_level_t level );

protected:
    bool registerModule( DebugModule& debugModule );
    bool unregisterModule( DebugModule& debugModule );

private:
    DebugModuleManager();
    ~DebugModuleManager();

    typedef std::vector< DebugModule* > DebugModuleVector;
    typedef std::vector< DebugModule* >::iterator DebugModuleVectorIterator;


    static DebugModuleManager* m_instance;
    DebugModuleVector          m_debugModules;
};

#endif
