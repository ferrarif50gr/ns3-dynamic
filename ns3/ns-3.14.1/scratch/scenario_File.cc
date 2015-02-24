#include <iostream>
#include <fstream>
#include <string>
#include <cassert>
//#include <stdio.h>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/switched-ethernet-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/pcap-file.h"
//#include "ns3/trace-generator.h"

using namespace std;
using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("SwitchedEthernetPacketSocketExample");

std::ofstream g_os;
//std::ifstream trace;
//PcapFile g_is;
//FILE* g_is; 

static void
SinkRx (std::string path, Ptr<const Packet> p, const Address &address)
{
//  NS_LOG_UNCOND("Accessing receiver file");
  g_os << p->GetSize () << std::endl;
}

/*static void
SinkTx (std::string path, Ptr<const Packet> p, const Address &address)
{
 uint8_t expected[1600];
 uint32_t tsSec, tsUsec, inclLen, origLen, readLen;
 g_is.Read (expected, sizeof(expected), tsSec, tsUsec, inclLen, origLen, readLen);
 NS_LOG_UNCOND("File Opened");
//  g_is.Read
}
*/
int
main (int argc, char *argv[])
{
#if 0
  LogComponentEnable ("SwitchedEthernetPacketSocketExample", LOG_LEVEL_INFO);
#endif

  Config::SetDefault("ns3::DropTailQueue::MaxPackets", UintegerValue(1500));
  Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(1434));
  CommandLine cmd;
  cmd.Parse (argc, argv);

//  g_os.open ("switched-ethernet-packet-socket-sink.tr",std::ios_base::binary | std::ios_base::out);
//  trace.open ("./scratch/trace2", std::ios::in);

//  uint8_t expected[1600];
//  uint32_t tsSec, tsUsec, inclLen, origLen, readLen;
//  g_is.Read (expected, sizeof(expected), tsSec, tsUsec, inclLen, origLen, readLen);


 //cout<< "let's see" << " "<<  expected << " "<< tsSec << " "<< tsUsec << " "<< inclLen <<  " "<< origLen <<  " "<< readLen << endl;  

/*double ttt=2.23; 
int sss=1;
char* line = new char[100000];

for(int j=0; j<3; j++)
{
	trace.getline(line,99999,'\n');

	cout << "before " << line << endl;
	sscanf(line,"%lf %d",&ttt, &sss);
	tsSec = uint32_t(int(ttt));
	tsUsec = uint32_t(int(1000000000*(ttt-int(ttt))));
	inclLen = uint32_t(sss);
	origLen = inclLen;
	readLen = inclLen;

	cout << "reading from file" << endl; 
	cout << ttt << " " << sss << endl;
	cout << tsSec << endl;
	cout << tsUsec << endl;
	cout << inclLen << endl;
	cout << origLen << endl;
	cout << readLen << endl;

	cin.get();
}
*/
 // cout << g_is.Eof() << g_is.Fail() << endl;

 // g_is.Read (expected, sizeof(expected), tsSec, tsUsec, inclLen, origLen, readLen);
//  g_is.Read (expected, sizeof(expected), tsSec, tsUsec, inclLen, origLen, readLen);
//cout<< "let's see" << " "<<  expected << " "<< tsSec << " "<< tsUsec << " "<< inclLen <<  " "<< origLen <<  " "<< readLen << endl;  

//NS_LOG_UNCONs");
  
  NS_LOG_INFO ("Create nodes.");

  //Create two nodes - both can send and receive simultaneously
  NodeContainer nodes;
  nodes.Create (2);

  InternetStackHelper internet;
  internet.Install (nodes);

  // create the two full-duplex channels that are used to send packets to node 0 from nodes 1 and 2
  NS_LOG_INFO ("Create channels.");
  Ptr<SwitchedEthernetChannel> channel = CreateObjectWithAttributes<SwitchedEthernetChannel> (
      "DataRate", DataRateValue (DataRate (1000000000)),
      "Delay", TimeValue (NanoSeconds (10)));
  Time startTime = Seconds (0);
  Time stopTime = Seconds (170);

  // use a helper function to connect our nodes to the full-duplex channel.
  NS_LOG_INFO ("Build Topology.");
  SwitchedEthernetHelper switchedEth;
  switchedEth.SetDeviceAttribute("EncapsulationMode", StringValue ("Llc"));
  switchedEth.SetDeviceAttribute("GoToSleepTime", TimeValue(MicroSeconds(182.0)));
  switchedEth.SetDeviceAttribute("QueueTimer", TimeValue(MicroSeconds(0)));
  switchedEth.SetDeviceAttribute("MaxQueueLength", UintegerValue(1));
  switchedEth.SetDeviceAttribute("DynamicCoalescing", BooleanValue (false));
  switchedEth.SetDeviceAttribute("Additive", BooleanValue (true));
  switchedEth.SetDeviceAttribute("Multiplicative", BooleanValue (true));
  switchedEth.SetDeviceAttribute("TimerStepAdditive", UintegerValue (0));   //Time is given in microseconds
  switchedEth.SetDeviceAttribute("TimerStepMultiplicative", UintegerValue (0));   //The step is given in percentage eg. 5 10 etc
  switchedEth.SetDeviceAttribute("QueueStepAdditive", UintegerValue (0));   //Queue step is given in number of packets
  switchedEth.SetDeviceAttribute("QueueStepMultiplicative", UintegerValue (0));   //The step is given in percentage eg. 5 10 etc

  NetDeviceContainer devs = switchedEth.Install (nodes, channel);

  Ipv4AddressHelper ipv4;
  NS_LOG_INFO ("Assign IP Addresses");
  ipv4.SetBase ("10.0.0.0", "255.255.255.0");
  Ipv4InterfaceContainer i = ipv4.Assign (devs);
  NS_LOG_INFO ("Create Applications.");

  // Create a packet sink on the star "hub" to receive these packets
  uint16_t port = 50000;
  Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), port));
  PacketSinkHelper sinkHelper ("ns3::UdpSocketFactory", sinkLocalAddress);
  ApplicationContainer sinkApp = sinkHelper.Install (nodes.Get(1));
  sinkApp.Start (startTime);
  sinkApp.Stop (stopTime);

//  uint16_t port2 = 10;
//  Address sinkLocalAddress2 (InetSocketAddress (Ipv4Address::GetAny (), port2));
  PacketSinkHelper sinkHelper2 ("ns3::UdpSocketFactory", sinkLocalAddress);
  ApplicationContainer sinkApp2 = sinkHelper2.Install (nodes.Get(0));
  sinkApp2.Start (startTime);
  sinkApp2.Stop (stopTime);

//FromDouble
  TraceAppHelper client = TraceAppHelper ("ns3::UdpSocketFactory", i.GetAddress (1));
  client.SetAttribute ("TraceFile", StringValue("/media/3Tdisk/github-ns3/ns3-dynamic/current_trace_0"));
  ApplicationContainer clientApps;
  AddressValue remoteAddress(InetSocketAddress (i.GetAddress (1), port));
  client.SetAttribute ("Remote", remoteAddress);
  clientApps = client.Install (nodes.Get(0));
  clientApps.Start (startTime);
  clientApps.Stop (stopTime);

  TraceAppHelper client2 = TraceAppHelper ("ns3::UdpSocketFactory", i.GetAddress (0));
  client2.SetAttribute ("TraceFile", StringValue("/media/3Tdisk/github-ns3/ns3-dynamic/current_trace_1"));
  client2.SetAttribute ("MaxPacketSize", UintegerValue(1460));
  ApplicationContainer clientApps2;
  AddressValue remoteAddress2(InetSocketAddress (i.GetAddress (0), port));
  client2.SetAttribute ("Remote", remoteAddress2);
  clientApps2 = client2.Install (nodes.Get(1));
  clientApps2.Start (startTime);
  clientApps2.Stop (stopTime);
  //Config::Connect ("/NodeList/*/ApplicationList/*/$ns3::PacketSink/Tx", MakeCallback (&SinkTx));

  // While the below trace sink is hooked to all nodes (the wildcard "*")
  // only one packet sink (on node 0) is actually added above, so
  // only the receive events on node 0 will be traced
  Config::Connect ("/NodeList/*/ApplicationList/*/$ns3::PacketSink/Rx",
                   MakeCallback (&SinkRx));

  // Configure tracing of all enqueue, dequeue, and NetDevice receive events
  // Trace output will be sent to the switched-ethernet-MyScenario2.tr file
  NS_LOG_INFO ("Configure Tracing.");

  // For more detailed information on what is going on, enable these two log functions
  // in debug mode. Note that this logging will generate a large amount of output.
  //LogComponentEnable ("SwitchedEthernetNetDevice", LOG_LEVEL_ALL);
  //LogComponentEnable ("SwitchedEthernetChannel", LOG_LEVEL_ALL);

  AsciiTraceHelper ascii;
//  switchedEth.EnableAsciiAll (ascii.CreateFileStream ("switched-ethernet-MyScenario1.tr"));
  Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream("scenarioFile.tr");
  stream->GetStream()->precision(10);
  switchedEth.EnableAsciiAll(stream);
//  switchedEth.EnablePcapAll ("scenarioFile", false);

/*  std::ofstream ascii;
  ascii.open ("my2-helper.tr");
  SwitchedEthernetHelper::EnableAsciiAll (ascii);
  SwitchedEthernetHelper::EnablePcapAll ("my2-helper");
  //SwitchedEthernetHelper::EnablePcapAll ("wsn3-helper");
*/
  NS_LOG_INFO ("Run Simulation.");
  Simulator::Stop(stopTime + Seconds(1.0));
  Simulator::Run ();
  Simulator::Destroy ();
  NS_LOG_INFO ("Done.");

  g_os.close ();
 // trace.close ();
  return 0;
}
