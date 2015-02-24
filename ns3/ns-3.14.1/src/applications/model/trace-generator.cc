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

// ns3 - This application generates traffic based on input files. We import the file
// that we have from possible real traces and the application extracts from the file
// the interarrival time and the size of the packet and thus, we can simulate real
// traffic.

#include "ns3/string.h"
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
#include "trace-generator.h"
#include "ns3/udp-socket-factory.h"
//#include "pcap-file.h"

NS_LOG_COMPONENT_DEFINE ("TraceGeneratorApplication");

using namespace std;

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (TraceGenerator);

TypeId
TraceGenerator::GetTypeId ()
{
  static TypeId tid = TypeId ("ns3::TraceGenerator")
    .SetParent<Application> ()
    .AddConstructor<TraceGenerator> ()
 /*   .AddAttribute ("Delay",
    		   "The delay between two packets (s)",
                   ConstantVariable (1.0),
                   MakeRandomVariableAccessor (&RandomGenerator::m_delay),
                   MakeRandomVariableChecker ())
    .AddAttribute ("PacketSize",
    		   "The size of each packet (bytes)",
		   UintegerValue (1000),
                   MakeUintegerAccessor (&TraceGenerator::m_size),
                   MakeUintegerChecker<uint32_t> ())
 */   
    .AddAttribute ("Remote",
    		   "The address of the destination",
                   AddressValue (),
                   MakeAddressAccessor (&TraceGenerator::m_peer),
                   MakeAddressChecker ())
    .AddAttribute ("Protocol",
    		   "The type of protocol to use.",
                   TypeIdValue (UdpSocketFactory::GetTypeId ()),
                   MakeTypeIdAccessor (&TraceGenerator::m_tid),
                   MakeTypeIdChecker ())
    .AddAttribute ("MaxPacketSize",
                   "The max length of the packet.",
                   UintegerValue(1200),
                   MakeUintegerAccessor (&TraceGenerator::m_maxPacketLength),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("TraceFile",
                   "Filename.",
                   StringValue("/workspace/ns3-3rd-instance/ns-3/ns-3.14.1/scratch/trace2"),
                   MakeStringAccessor (&TraceGenerator::m_tracename),
                   MakeStringChecker ())
  ;
  return tid;
}

TraceGenerator::TraceGenerator()
{
  NS_LOG_FUNCTION_NOARGS();
  m_socket = 0;
  m_delay = Seconds (0.0);
}

TraceGenerator::~TraceGenerator()
{
  NS_LOG_FUNCTION_NOARGS();
}

void
TraceGenerator::SetRemote (std::string socketType, Address remote)
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
TraceGenerator::DoGenerate (void)
{
  uint32_t packetSize;
  packetSize = m_size;
  Ptr<Packet> p = Create<Packet> (packetSize);  //initially it was GetIntValue()
  m_socket->Send (p);
  
  char* line = new char[1000];

  if(m_file.eof()) {
  //	cout << "rien ne va plus" << endl;
	return;
  }
  m_file.getline(line,999);
  if(m_file.fail())
  {
  //	cout << "oops, this line was no good!" << endl;
	return;
  }
  long double tnext;
  sscanf(line,"%Lf %d",&tnext, &m_size);
  m_size-=54; 
  //cout << "Time for next packet" << endl;
  //printf("%.12Lf\n",tnext); 

/*  cout << ns3::Time::FromDouble (tnext, ns3::Time::Unit(0)) << endl; 
  cout << Seconds(tnext) << endl; 
  cout << m_delay << endl;
  */
  m_next = Simulator::Schedule ( Seconds(tnext) - m_delay, &TraceGenerator::DoGenerate, this);
  m_delay = ns3::Time::FromDouble(tnext, ns3::Time::Unit(0));//Seconds (tnext); 	
}

void
TraceGenerator::StartApplication (void)
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
  
  m_file.open(m_tracename.c_str(), std::ios::in); 
  char* line = new char[1000];
  if(m_file.eof()) {
	cout << "rien ne va plus";
	return;
  }
  m_file.getline(line,999);
  double tnext;
  sscanf(line,"%lf %d",&tnext, &m_size);
  m_size-=54; 
  m_next = Simulator::Schedule ( Seconds(tnext) - m_delay, &TraceGenerator::DoGenerate, this);
  m_delay = Seconds (tnext);  
//  DoGenerate();
}

void
TraceGenerator::StopApplication (void)
{
  NS_LOG_FUNCTION_NOARGS ();
//cout << "That's it!" << endl; 
  m_next.Cancel();
  if(m_socket != 0)
  {
    m_socket->Close ();
  }
  else
  {
    NS_LOG_WARN ("OnOffApplication found null socket to close in StopApplication");
  }
  m_file.close();
}


} //namespace ns3
