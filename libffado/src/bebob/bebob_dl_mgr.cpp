/*
 * Copyright (C) 2005-2007 by Daniel Wagner
 *
 * This file is part of FFADO
 * FFADO = Free Firewire (pro-)audio drivers for linux
 *
 * FFADO is based upon FreeBoB
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software Foundation;
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA
 */

#include "bebob_dl_mgr.h"
#include "bebob_dl_codes.h"
#include "bebob_dl_bcd.h"

#include "libieee1394/configrom.h"
#include "libieee1394/ieee1394service.h"

#include "libavc/avc_serialize.h"

#include <netinet/in.h>

#include <cstdio>

namespace BeBoB {
    enum {
        AddrRegInfo    = 0xffffc8020000ULL,
        AddrRegReq     = 0xffffc8021000ULL,
        AddrRegReqBuf  = 0xffffc8021040ULL,
        AddrRegResp    = 0xffffc8029000ULL,
        AddrRegRespBuf = 0xffffc8029040ULL,
    };

    enum {
        RegInfoManufactorIdOffset =      0x00,
        RegInfoProtocolVersionOffset =   0x08,
        RegInfoBootloaderVersionOffset = 0x0c,
        RegInfoGUID =                    0x10,
        RegInfoHardwareModelId =         0x18,
        RegInfoHardwareRevision =        0x1c,
        RegInfoSoftwareDate =            0x20,
        RegInfoSoftwareTime =            0x28,
        RegInfoSoftwareId =              0x30,
        RegInfoSoftwareVersion =         0x34,
        RegInfoBaseAddress =             0x38,
        RegInfoMaxImageLen =             0x3c,
        RegInfoBootloaderDate =          0x40,
        RegInfoBootloaderTime =          0x48,
        RegInfoDebuggerDate =            0x50,
        RegInfoDebuggerTime =            0x58,
        RegInfoDebuggerId =              0x60,
        RegInfoDebuggerVersion =         0x64
    };

    enum {
        MaxRetries = 10,
    };

    IMPL_DEBUG_MODULE( BootloaderManager, BootloaderManager, DEBUG_LEVEL_NORMAL );
}

BeBoB::BootloaderManager::BootloaderManager(Ieee1394Service& ieee1349service,
                                            fb_nodeid_t nodeId )
    : m_ieee1394service( &ieee1349service )
    , m_protocolVersion( eBPV_Unknown )
    , m_isAppRunning( false )
    , m_forceEnabled( false )
    , m_bStartBootloader( true )
{
    memset( &m_cachedInfoRegs, 0, sizeof( m_cachedInfoRegs ) );

    m_configRom = new ConfigRom( *m_ieee1394service, nodeId );
    // XXX throw exception if initialize fails!
    m_configRom->initialize();
    if ( !cacheInfoRegisters() ) {
        debugError( "BootloaderManager: could not cache info registers\n" );
    }

    switch(  m_cachedInfoRegs.m_protocolVersion ) {
    case 1:
        m_protocolVersion = eBPV_V1;
        break;
    case 3:
        m_protocolVersion = eBPV_V3;
        break;
    default:
        // exception?
        break;
    }

    pthread_mutex_init( &m_mutex, 0 );
    pthread_cond_init( &m_cond, 0 );

    m_functor = new MemberFunctor0< BeBoB::BootloaderManager*,
                void (BeBoB::BootloaderManager::*)() >
                ( this, &BeBoB::BootloaderManager::busresetHandler, false );
    m_ieee1394service->addBusResetHandler( m_functor );
}

BeBoB::BootloaderManager::~BootloaderManager()
{
    m_ieee1394service->remBusResetHandler( m_functor );
    delete( m_functor );

    delete m_configRom;

    pthread_cond_destroy( &m_cond );
    pthread_mutex_destroy( &m_mutex );
}

bool
BeBoB::BootloaderManager::cacheInfoRegisters()
{
    if ( !m_configRom->updatedNodeId() ) {
        debugError( "cacheInfoRegisters: did not find device anymore\n" );
        return false;
    }

    if ( !m_ieee1394service->read(
             0xffc0 | m_configRom->getNodeId(),
             AddrRegInfo,
             sizeof( m_cachedInfoRegs )/4,
             reinterpret_cast<fb_quadlet_t*>( &m_cachedInfoRegs ) ) )
    {
        return false;
    }

    if ( m_cachedInfoRegs.m_bootloaderVersion != 0x0 ) {
        m_isAppRunning = false;
    } else {
        m_isAppRunning = true;
    }

    m_cachedInfoRegs.m_guid = ( m_cachedInfoRegs.m_guid >> 32 )
                              | ( m_cachedInfoRegs.m_guid << 32 );

    return true;
}

bool
BeBoB::BootloaderManager::cacheInfoRegisters( int retries )
{
    for ( int i = 0; i < retries; ++i ) {
        if ( cacheInfoRegisters() ) {
            return true;
        }
        sleep( 1 );
    }

    return false;
}

void
BeBoB::BootloaderManager::printInfoRegisters()
{
    using namespace std;

    if ( !cacheInfoRegisters() ) {
        debugError( "Could not read info registers\n" );
        return;
    }

    printf( "Info Registers\n" );
    printf( "\tManufactors Id:\t\t%s\n",
            makeString( m_cachedInfoRegs.m_manId ).c_str() );
    printf( "\tProtocol Version:\t0x%08x\n",
            m_cachedInfoRegs.m_protocolVersion );
    printf( "\tBootloader Version:\t0x%08x\n",
            m_cachedInfoRegs.m_bootloaderVersion );
    printf( "\tGUID:\t\t\t0x%08x%08x\n",
            ( unsigned int )( m_cachedInfoRegs.m_guid >> 32 ),
            ( unsigned int )( m_cachedInfoRegs.m_guid & 0xffffffff ) );
    printf( "\tHardware Model ID:\t0x%08x\n",
            m_cachedInfoRegs.m_hardwareModelId );
    printf( "\tHardware Revision:\t0x%08x\n",
            m_cachedInfoRegs.m_hardwareRevision );
    if (  m_cachedInfoRegs.m_softwareDate
          && m_cachedInfoRegs.m_softwareTime )
    {
        printf( "\tSoftware Date:\t\t%s, %s\n",
                makeDate( m_cachedInfoRegs.m_softwareDate ).c_str(),
                makeTime( m_cachedInfoRegs.m_softwareTime ).c_str() );
    }
    printf( "\tSoftware Id:\t\t0x%08x\n", m_cachedInfoRegs.m_softwareId );
    printf( "\tSoftware Version:\t0x%08x\n",
            m_cachedInfoRegs.m_softwareVersion );
    printf( "\tBase Address:\t\t0x%08x\n", m_cachedInfoRegs.m_baseAddress );
    printf( "\tMax. Image Len:\t\t0x%08x\n", m_cachedInfoRegs.m_maxImageLen );
    if ( m_cachedInfoRegs.m_bootloaderDate
         && m_cachedInfoRegs.m_bootloaderTime )
    {
        printf( "\tBootloader Date:\t%s, %s\n",
                makeDate( m_cachedInfoRegs.m_bootloaderDate ).c_str(),
                makeTime( m_cachedInfoRegs.m_bootloaderTime ).c_str() );
    }
    if ( m_cachedInfoRegs.m_debuggerDate
         && m_cachedInfoRegs.m_debuggerTime )
    {
        printf( "\tDebugger Date:\t\t%s, %s\n",
                makeDate( m_cachedInfoRegs.m_debuggerDate ).c_str(),
                makeTime( m_cachedInfoRegs.m_debuggerTime ).c_str() );
    }
    printf( "\tDebugger Id:\t\t0x%08x\n", m_cachedInfoRegs.m_debuggerId );
    printf( "\tDebugger Version:\t0x%08x\n",
            m_cachedInfoRegs.m_debuggerVersion );
}

bool
BeBoB::BootloaderManager::downloadFirmware( std::string filename )
{
    using namespace std;

    printf( "parse BCD file\n" );
    std::auto_ptr<BCD> bcd = std::auto_ptr<BCD>( new BCD( filename ) );
    if ( !bcd.get() ) {
        debugError( "downloadFirmware: Could not open or parse BCD '%s'\n",
                    filename.c_str() );
        return false;
    }
    if ( !bcd->parse() ) {
        debugError( "downloadFirmware: BCD parsing failed\n" );
        return false;
    }

    printf( "check firmware device compatibility... " );
    if ( !m_forceEnabled ) {
        if ( !checkDeviceCompatibility( *bcd ) ) {
            printf( "failed.\n" );
            return false;
        }
        printf( "ok\n" );
    } else {
        printf( "forced\n" );
    }

    if ( m_bStartBootloader ) {
        printf( "prepare for download (start bootloader)\n" );
        if ( !startBootloaderCmd() ) {
            debugError( "downloadFirmware: Could not start bootloader\n" );
            return false;
        }
    }

    printf( "start downloading protocol for application image\n" );
    if ( !downloadObject( *bcd, eOT_Application ) ) {
        debugError( "downloadFirmware: Firmware download failed\n" );
        return false;
    }

    printf( "start downloading protocol for CnE\n" );
    if ( !downloadObject( *bcd, eOT_CnE ) ) {
        debugError( "downloadFirmware: CnE download failed\n" );
        return false;
    }

    printf( "setting CnE to factory default settings\n" );
    if ( !initializeConfigToFactorySettingCmd() ) {
        debugError( "downloadFirmware: Could not reinitalize CnE\n" );
        return false;
    }

    printf( "start application\n" );
    if ( !startApplicationCmd() ) {
        debugError( "downloadFirmware: Could not restart application\n" );
        return false;
    }

    return true;
}

bool
BeBoB::BootloaderManager::downloadCnE( std::string filename )
{
    using namespace std;

    printf( "parse BCD file\n" );
    std::auto_ptr<BCD> bcd = std::auto_ptr<BCD>( new BCD( filename ) );
    if ( !bcd.get() ) {
        debugError( "downloadCnE: Could not open or parse BCD '%s'\n",
                    filename.c_str() );
        return false;
    }
    if ( !bcd->parse() ) {
        debugError( "downloadCnE: BCD parsing failed\n" );
        return false;
    }

    printf( "check firmware device compatibility... " );
    if ( !m_forceEnabled ) {
        if ( !checkDeviceCompatibility( *bcd ) ) {
            printf( "failed.\n" );
            return false;
        }
        printf( "ok\n" );
    } else {
        printf( "forced\n" );
    }

    if ( m_bStartBootloader ) {
        printf( "prepare for download (start bootloader)\n" );
        if ( !startBootloaderCmd() ) {
            debugError( "downloadCnE: Could not start bootloader\n" );
            return false;
        }
    }

    printf( "start downloading protocol for CnE\n" );
    if ( !downloadObject( *bcd, eOT_CnE ) ) {
        debugError( "downloadCnE: CnE download failed\n" );
        return false;
    }

    printf( "setting CnE to factory default settings\n" );
    if ( !initializeConfigToFactorySettingCmd() ) {
        debugError( "downloadFirmware: Could not reinitalize CnE\n" );
        return false;
    }

    printf( "start application\n" );
    if ( !startApplicationCmd() ) {
        debugError( "downloadCnE: Could not restart application\n" );
        return false;
    }

    return true;
}


bool
BeBoB::BootloaderManager::downloadObject( BCD& bcd, EObjectType eObject )
{
    using namespace std;

    CommandCodesDownloadStart::EObject eCCDSObject;
    fb_quadlet_t baseAddress;
    fb_quadlet_t imageLength;
    fb_quadlet_t crc;
    fb_quadlet_t offset;

    switch ( eObject ) {
    case eOT_Application:
        eCCDSObject = CommandCodesDownloadStart::eO_Application;
        baseAddress = bcd.getImageBaseAddress();
        imageLength = bcd.getImageLength();
        crc = bcd.getImageCRC();
        offset = bcd.getImageOffset();
        break;
    case eOT_CnE:
        eCCDSObject = CommandCodesDownloadStart::eO_Config;
        baseAddress = 0;
        imageLength = bcd.getCnELength();
        crc = bcd.getCnECRC();
        offset = bcd.getCnEOffset();
        break;
    default:
        return false;
    }

    CommandCodesDownloadStart ccDStart ( m_protocolVersion, eCCDSObject );
    ccDStart.setDate( bcd.getSoftwareDate() );
    ccDStart.setTime( bcd.getSoftwareTime() );
    ccDStart.setId( bcd.getSoftwareId() );
    ccDStart.setVersion( bcd.getSoftwareVersion() );
    ccDStart.setBaseAddress( baseAddress );
    ccDStart.setLength( imageLength );
    ccDStart.setCRC( crc );

    if ( !writeRequest( ccDStart ) ) {
        debugError( "downloadObject: start command write request failed\n" );
        return false;
    }

    // bootloader erases the flash, have to wait until is ready
    // to answer our next request
    printf( "wait until flash ereasing has terminated\n" );
    sleep( 30 );

    if ( !readResponse( ccDStart ) ) {
        debugError( "downloadObject: (start) command read request failed\n" );
        return false;
    }

    if ( ccDStart.getMaxBlockSize() < 0 ) {
        debugError( "downloadObject: (start) command reported error %d\n",
                    ccDStart.getMaxBlockSize() );
        return false;
    }

    unsigned int maxBlockSize = m_configRom->getAsyMaxPayload();
    unsigned int i = 0;
    fb_quadlet_t address = 0;
    bool result = true;
    int totalBytes = imageLength;
    int downloadedBytes = 0;
    while ( imageLength > 0 ) {
        unsigned int blockSize = imageLength > maxBlockSize ?
                        maxBlockSize  : imageLength;

        fb_byte_t block[blockSize];
        if ( !bcd.read( offset, block, blockSize ) ) {
            result = false;
            break;
        }

        if ( !get1394Serivce()->write(
                 0xffc0 | getConfigRom()->getNodeId(),
                 AddrRegReqBuf,
                 ( blockSize + 3 ) / 4,
                 reinterpret_cast<fb_quadlet_t*>( block ) ) )
        {
            debugError( "downloadObject: Could not write to node %d\n",
                        getConfigRom()->getNodeId() );
            return false;
        }

        CommandCodesDownloadBlock ccBlock( m_protocolVersion );
        ccBlock.setSeqNumber( i );
        ccBlock.setAddress( baseAddress + address );
        ccBlock.setNumberBytes( blockSize );

        if ( !writeRequest( ccBlock ) ) {
            debugError( "downloadObject: (block) write request failed" );
            result = false;
            break;
        }
        usleep( 100 );

        if ( !readResponse( ccBlock ) ) {
            debugError( "downloadObject: (block) read request failed\n" );
            result = false;
            break;
        }

        if ( i != ccBlock.getRespSeqNumber() ) {
            debugError( "downloadObject: (block) wrong sequence number "
                        "reported. %d expected, %d reported\n",
                        i, ccBlock.getRespSeqNumber() );
            result = false;
            break;
        }
        if ( ccBlock.getRespErrorCode() != 0 ) {
            debugError( "downloadObject: (block) failed download with "
                        "error code 0x%08x\n", ccBlock.getRespErrorCode() );
            result = false;
            break;
        }

        downloadedBytes += blockSize;
        if ( ( i % 100 ) == 0 ) {
           printf( "%10d/%d bytes downloaded\n",
                   downloadedBytes, totalBytes );
        }

        imageLength -= blockSize;
        address += blockSize;
        offset += blockSize;
        i++;
    }
    printf( "%10d/%d bytes downloaded\n",
            downloadedBytes, totalBytes );

    if ( !result ) {
        debugError( "downloadObject: seqNumber = %d, "
                    "address = 0%08x, offset = 0x%08x, "
                    "restImageLength = 0x%08x\n",
                    i, address, offset, imageLength );
    }

    CommandCodesDownloadEnd ccEnd( m_protocolVersion );
    if ( !writeRequest( ccEnd ) ) {
        debugError( "downloadObject: (end) command write failed\n" );
    }

    printf( "wait for transaction completion\n" );
    sleep( 10 );

    if ( !readResponse( ccEnd ) ) {
        debugError( "downloadObject: (end) command read failed\n" );
    }

    if ( result ) {
        if ( ccEnd.getRespIsValid() ) {
            if ( ccEnd.getRespCrc32() == crc ) {
                debugOutput( DebugModule::eDL_Normal,
                             "downloadObject: CRC match\n" );
            } else {
                debugError( "downloadObject: CRC mismatch. 0x%08x expected, "
                            "0x%08x reported",
                            crc, ccEnd.getRespCrc32() );
                result = false;
            }
        } else {
            debugError( "downloadObject: (end) object is not valid\n" );
            result = false;
        }
    }
    printf( "download protocol successfuly completed\n" );
    return result;
}

bool
BeBoB::BootloaderManager::programGUID( fb_octlet_t guid )
{
    if ( m_bStartBootloader ) {
        if ( !startBootloaderCmd() ) {
            debugError( "programGUID: Could not start bootloader\n" );
            return false;
        }
    }

    if ( !programGUIDCmd( guid ) ) {
        debugError( "programGUID: Could not program guid\n" );
        return false;
    }

    if ( !startApplicationCmd() ) {
        debugError( "Could not restart application\n");
        return false;
    }

    return true;
}

void
BeBoB::BootloaderManager::busresetHandler()
{
    pthread_cond_signal( &m_cond );
}

void
BeBoB::BootloaderManager::waitForBusReset()
{
    pthread_cond_wait( &m_cond, &m_mutex );
}

bool
BeBoB::BootloaderManager::writeRequest( CommandCodes& cmd )
{
    unsigned char buf[ ( ( cmd.getMaxSize()+3 )/4 ) * 4 ];
    memset( buf, 0, sizeof( buf ) );

    BufferSerialize se( buf,  sizeof( buf ) );
    if ( !cmd.serialize( se ) ) {
        debugError( "writeRequest: Could not serialize command code %d\n",
                    cmd.getCommandCode() );
        return false;
    }

    if ( !get1394Serivce()->write(
             0xffc0 | getConfigRom()->getNodeId(),
             AddrRegReq,
             sizeof( buf )/4,
             reinterpret_cast<fb_quadlet_t*>( buf ) ) )
    {
        debugError( "writeRequest: Could not ARM write to node %d\n",
                    getConfigRom()->getNodeId() );
        return false;
    }

    return true;
}

bool
BeBoB::BootloaderManager::readResponse( CommandCodes& writeRequestCmd )
{
    const size_t buf_length = 0x40;
    unsigned char raw[buf_length];
    if ( !get1394Serivce()->read(
             0xffc0 | getConfigRom()->getNodeId(),
             AddrRegResp,
             writeRequestCmd.getRespSizeInQuadlets(),
             reinterpret_cast<fb_quadlet_t*>( raw ) ) )
    {
        return false;
    }

    BufferDeserialize de( raw, buf_length );
    if ( !writeRequestCmd.deserialize( de ) ) {
        debugError( "readResponse: deserialize failed\n" );
        return false;
    }

    bool result =
        writeRequestCmd.getProtocolVersion()
        == writeRequestCmd.getRespProtocolVersion();
    result &=
        writeRequestCmd.getCommandId()
        == writeRequestCmd.getRespCommandId();
    result &=
        writeRequestCmd.getCommandCode()
        == writeRequestCmd.getRespCommandCode();
    #ifdef DEBUG
       if ( !result ) {
           debugError( "readResponse: protocol version: %d expected, "
                       " %d reported\n",
                       writeRequestCmd.getProtocolVersion(),
                       writeRequestCmd.getRespProtocolVersion() );
           debugError( "readResponse: command id: %d expected, "
                       " %d reported\n",
                       writeRequestCmd.getCommandId(),
                       writeRequestCmd.getRespCommandId() );
           debugError( "readResponse: command code: %d expected, "
                       " %d reported\n",
                       writeRequestCmd.getCommandCode(),
                       writeRequestCmd.getRespCommandCode() );
       }
    #endif
    return result;
}

bool
BeBoB::BootloaderManager::startBootloaderCmd()
{
    CommandCodesReset cmd( m_protocolVersion,
                           CommandCodesReset::eSM_Bootloader ) ;
    if ( !writeRequest( cmd ) ) {
        debugError( "startBootloaderCmd: writeRequest failed\n" );
        return false;
    }

    waitForBusReset();
    if ( !cacheInfoRegisters( MaxRetries ) ) {
        debugError( "startBootloaderCmd: Could not read info registers\n" );
        return false;
    }

    // wait for bootloader finish startup sequence
    // there is no way to find out when it has finished
    sleep( 10 );

    return true;
}

bool
BeBoB::BootloaderManager::startApplicationCmd()
{
    CommandCodesGo cmd( m_protocolVersion,
                           CommandCodesGo::eSM_Application ) ;
    if ( !writeRequest( cmd ) ) {
        debugError( "startApplicationCmd: writeRequest failed\n" );
        return false;
    }

    return true;
}

bool
BeBoB::BootloaderManager::programGUIDCmd( fb_octlet_t guid )
{
    CommandCodesProgramGUID cmd( m_protocolVersion, guid );
    if ( !writeRequest( cmd ) ) {
        debugError( "programGUIDCmd: writeRequest failed\n" );
        return false;
    }

    sleep( 1 );

    return true;
}

bool
BeBoB::BootloaderManager::initializePersParamCmd()
{
    CommandCodesInitializePersParam cmd( m_protocolVersion );
    if ( !writeRequest( cmd ) ) {
        debugError( "initializePersParamCmd: writeRequest failed\n" );
        return false;
    }

    sleep( 1 );

    return true;
}

bool
BeBoB::BootloaderManager::initializeConfigToFactorySettingCmd()
{
    CommandCodesInitializeConfigToFactorySetting cmd( m_protocolVersion );
    if ( !writeRequest( cmd ) ) {
        debugError( "initializeConfigToFactorySettingCmd: writeRequest failed\n" );
        return false;
    }

    sleep( 5 );

    return true;
}

bool
BeBoB::BootloaderManager::checkDeviceCompatibility( BCD& bcd )
{
    fb_quadlet_t vendorOUI = (  m_cachedInfoRegs.m_guid >> 40 );


    if ( ( vendorOUI == bcd.getVendorOUI() )
         && ( m_cachedInfoRegs.m_softwareId == bcd.getSoftwareId() ) )
    {
        return true;
    }

    printf( "vendorOUI = 0x%08x\n", vendorOUI );
    printf( "BCD vendorOUI = 0x%08x\n", bcd.getVendorOUI() );
    printf( "software ID = 0x%08x\n", m_cachedInfoRegs.m_softwareId );
    printf( "BCD software ID = 0x%08x\n", bcd.getSoftwareId() );

    return false;
}
