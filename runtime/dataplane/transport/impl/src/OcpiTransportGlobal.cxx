/*
 * This file is protected by Copyright. Please refer to the COPYRIGHT file
 * distributed with this source distribution.
 *
 * This file is part of OpenCPI <http://www.opencpi.org>
 *
 * OpenCPI is free software: you can redistribute it and/or modify it under the
 * terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option) any
 * later version.
 *
 * OpenCPI is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
 * details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Abstract:
 *   This class contains all global data used by the OCPI transport layer.
 *
 * Revision History: 
 * 
 *    Author: John F. Miller
 *    Date: 1/2005
 *    Revision Detail: Created
 *
 */
#include <stdio.h>
#include <string>
#include <OcpiOsAssert.h>
#include <DtHandshakeControl.h>
#include <OcpiTransportGlobal.h>
#include <OcpiTransferController.h>
#include <OcpiTemplateGenerators.h>

#include <OcpiParallelDataDistribution.h>

using namespace OCPI::DataTransport;
using namespace DataTransfer;
using namespace DtI;
using namespace OCPI::Util;
using namespace OCPI::OS;

#define EVENT_START 100
#define EVENT_RANGE 20

#ifdef PROCESS_WIDE_GLOBALS
// Test setup structure
#ifdef TEST_TRANSPORT_STANDALONE
ConnectionTest       TransportGlobal::m_testSetup;
#endif
OCPI::OS::uint32_t      TransportGlobal::m_sharedMemorySize = 500000;
bool    TransportGlobal::m_ooPZeroCopy = false;

VList TransportGlobal::m_localEndpoints;
VList TransportGlobal::m_remoteEndpoints;
#endif



void TransportGlobal::clearGlobalCache()
{
  // remove me

}



/**********************************
 * Init method
 *********************************/
bool TransportGlobal::m_init=false;
void TransportGlobal::init()
{

  if ( ! m_init ) {
    m_init = true;
    m_gen_pat1AFC = NULL;
    m_gen_pat1AFCShadow = NULL;
    m_gen_temp_gen = NULL;
    m_gen_pat1 = NULL;
    m_gen_pat1passive = NULL;
    m_gen_pat2 = NULL;
    m_gen_pat3 = NULL;
    m_gen_pat4 = NULL;
    m_gen_control = NULL;
    m_cont1 = NULL;
    m_cont1passive = NULL;
    m_cont1AFCShadow = NULL;
    m_cont2 = NULL;
    m_cont3 = NULL;
    m_cont4 = NULL;
  }
}


void TransportGlobal::parseArgs( int argc, char **argv )
{
  ( void ) argc;
  ( void ) argv;
}


TransportGlobal::TransportGlobal( int argcp, char **argvp )
  : m_useEvents(false),m_Circuitinit(false), m_event_manager(NULL)
{
  ( void ) argcp;
  ( void ) argvp;
  init();
}



TransportGlobal::TransportGlobal( int event_ordinal, bool asyc )
  : m_useEvents(asyc),m_Circuitinit(false), m_event_manager(NULL)
{
  int low = EVENT_START + event_ordinal*EVENT_RANGE;
  int high = low+EVENT_RANGE-1;
  if ( asyc ) {
    ocpiDebug("GPP: Using events");        
    //        Create a asyc event handler object
    m_event_manager = new DataTransfer::EventManager(low,high);
  }
  else {
    ocpiDebug("GPP: Not Using events");
    m_event_manager = NULL;
  }
        
  init();
}

TransportGlobal::~TransportGlobal()
{
  if ( m_event_manager ) delete m_event_manager;

  delete m_gen_pat1AFC;
  delete m_gen_pat1AFCShadow;
  delete m_gen_temp_gen;
  delete m_gen_pat1;
  delete m_gen_pat1passive;
  delete m_gen_pat2;
  delete m_gen_pat3;
  delete m_gen_pat4;
  delete m_gen_control;
  delete m_cont1;
  delete m_cont1passive;
  delete m_cont1AFCShadow;
  delete m_cont2;
  delete m_cont3;
  delete m_cont4;


  clearGlobalCache();

}

