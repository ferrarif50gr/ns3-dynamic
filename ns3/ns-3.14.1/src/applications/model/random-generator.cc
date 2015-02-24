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
// Author: George F. Riley<riley@ece.gatech.edu>
//

// ns3 - On/Off Data Source Application class
// George F. Riley, Georgia Tech, Spring 2007
// Adapted from ApplicationOnOff in GTNetS.

#include "ns3/log.h"
#include "ns3/address.h"
#include "ns3/inet-socket-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/node.h"
#include "ns3/nstime.h"
#include "ns3/data-rate.h"
#include "ns3/random-variable.h"
#include "ns3/socket.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include "ns3/trace-source-accessor.h"
#include "random-generator.h"
#include "ns3/udp-socket-factory.h"
//#include "pcap-file.h"

NS_LOG_COMPONENT_DEFINE ("RandomGeneratorApplication");

//using namespace std;

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (RandomGenerator);

TypeId
RandomGenerator::GetTypeId ()
{
  static TypeId tid = TypeId ("ns3::RandomGenerator")
    .SetParent<Application> ()
    .AddConstructor<RandomGenerator> ()
    .AddAttribute ("Delay",
    		   "The delay between two packets (s)",
                   RandomVariableValue (ConstantVariable (1.0)),
                   MakeRandomVariableAccessor (&RandomGenerator::m_delay),
                   MakeRandomVariableChecker ())
    .AddAttribute ("PacketSize",
    		   "The size of each packet (bytes)",
                   RandomVariableValue (ConstantVariable (1000)),
                   MakeRandomVariableAccessor (&RandomGenerator::m_size),
                   MakeRandomVariableChecker ())
    .AddAttribute ("Remote",
    		   "The address of the destination",
                   AddressValue (),
                   MakeAddressAccessor (&RandomGenerator::m_peer),
                   MakeAddressChecker ())
    .AddAttribute ("Protocol",
    		   "The type of protocol to use.",
                   TypeIdValue (UdpSocketFactory::GetTypeId ()),
                   MakeTypeIdAccessor (&RandomGenerator::m_tid),
                   MakeTypeIdChecker ())
    .AddAttribute ("MaxPacketSize",
                   "The max length of the packet.",
                   UintegerValue(1200),
                   MakeUintegerAccessor (&RandomGenerator::m_maxPacketLength),
                   MakeUintegerChecker<uint32_t> ())
  ;
  return tid;
}

RandomGenerator::RandomGenerator()
{
  NS_LOG_FUNCTION_NOARGS();
  m_socket = 0;
}

RandomGenerator::~RandomGenerator()
{
  NS_LOG_FUNCTION_NOARGS();
}

void
RandomGenerator::SetRemote (std::string socketType, Address remote)
{
  TypeId tid = TypeId::LookupByName (socketType);
  m_socket = Socket::CreateSocket (GetNode (), tid);
  m_socket->Bind ();
  m_socket->ShutdownRecv ();
  m_socket->SetAllowBroadcast (true);
  m_socket->Connect (remote);
}

/*void 
RandomGenerator::DoGenerate (void)
{
  PcapFile myFile;
  myfile.open("/workspace/ns3-3rd-instance/ns-3/ns-3.14.1/scratch/trace.pcap",fstream::binary);
  uint8_t expected[PCAP_SNAPLEN];
  uint32_t tsSec, tsUsec, inclLen, origLen, readLen;
  myFile.Read (expected, sizeof(expected), tsSec, tsUsec, inclLen, origLen, readLen);
  uint8_t *actual = new uint8_t[readLen];
  p->CopyData (actual, readLen);

  uint32_t result = memcmp (actual, expected, readLen);

  delete [] actual;

//  myFile.read();
}*/
void
RandomGenerator::DoGenerate (void)
{
  m_next = Simulator::Schedule (Seconds (m_delay.GetValue ()), &RandomGenerator::DoGenerate, this);
  uint32_t packetSize;
  if (m_size.GetInteger () > m_maxPacketLength)
	  packetSize = m_maxPacketLength;
  else
	  packetSize = m_size.GetInteger ();
  Ptr<Packet> p = Create<Packet> (packetSize);  //initially it was GetIntValue()
  m_socket->Send (p);
}

void
RandomGenerator::StartApplication (void)
{
  NS_LOG_FUNCTION_NOARGS ();

  // Create the socket if not already
  if (!m_socket)
    {
      m_socket = Socket::CreateSocket (GetNode (), m_tid);
      m_socket->Bind ();
      m_socket->Connect (m_peer);
      m_socket->SetAllowBroadcast (true);
      m_socket->ShutdownRecv ();

//	  SetRemote(m_tid, m_peer);
    }

  DoGenerate();
}

void
RandomGenerator::StopApplication (void)
{
  NS_LOG_FUNCTION_NOARGS ();

  if(m_socket != 0)
  {
    m_socket->Close ();
  }
  else
  {
    NS_LOG_WARN ("OnOffApplication found null socket to close in StopApplication");
  }

}

} //namespace ns3
