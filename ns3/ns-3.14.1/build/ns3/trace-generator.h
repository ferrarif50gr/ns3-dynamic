/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
//
// Copyright (c) 2006 Georgia Tech Research Corporation
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation;
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
// Author: Angelos Chatzipapas<angelos.chatzipapas@imdea.org>
//

// ns3 - Random Traffic Generator Source Application class
// Angelos Chatzipapas, IMDEA Networks, Summer 2012

#ifndef TRACEGENERATOR_H
#define TRACEGENERATOR_H

#include <iostream>
#include <fstream>

#include "ns3/address.h"
#include "ns3/application.h"
#include "ns3/event-id.h"
#include "ns3/ptr.h"
#include "ns3/data-rate.h"
#include "ns3/random-variable.h"
#include "ns3/traced-callback.h"

namespace ns3 {

class Address;
class RandomVariable;
class Socket;

/**
 * \ingroup trace
 * \defgroup trace TraceApplication
 *
 * This traffic generator follows an real traffic pattern: after
 * Application::StartApplication
 * is called, the application uses as input a real traffic pattern from
 * a real captured file. The parameters needed are the packet interarrival
 * time and the size of the packet. The interarrival time is determined
 * by the difference between two succesive packets and the size of the packet
 * is known inside the file. Thus, it is straight forward to extract the two
 * parameters.
 */
/**
* \ingroup trace
*
* \brief Generate traffic to a single destination according to an
*        input file that is considered as real traffic pattern.
*
* This traffic generator follows an On/Off pattern: after
* Application::StartApplication
* is called, the application uses as input a real traffic pattern from
* a real captured file. The parameters needed are the packet interarrival
* time and the size of the packet. The interarrival time is determined
* by finding the difference between two succesive packets and the size of the packet
* is known inside the file. Thus, it is straight forward to extract the two
* parameters.
*
*/

class TraceGenerator : public Application
   {
   public:
	 static TypeId GetTypeId ();
     TraceGenerator ();
     virtual ~TraceGenerator ();
//     void SetDelay (RandomVariable delay);
//     void SetSize (RandomVariable size);
     void SetRemote (std::string socketType, Address remote);
   private:
     virtual void StartApplication (void);
     virtual void StopApplication (void);
     void DoGenerate (void);
     Time          m_delay;
     uint32_t      m_size;
     std::string   m_tracename;
     std::ifstream m_file; 
     Address       m_peer;         // Peer address
     TypeId        m_tid;
     Ptr<Socket>   m_socket;
     EventId       m_next;
     uint32_t      m_maxPacketLength;

   };

} // namespace ns3

#endif /* TRACEGENERATOR_H */
