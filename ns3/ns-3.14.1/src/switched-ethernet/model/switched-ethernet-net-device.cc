/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2012 Jeffrey Young
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: 	Jeff Young <jyoung9@gatech.edu>
 * 			This code is based on csma-net-device created by 
 * 		  Emmanuelle Laprise <emmanuelle.laprise@bluekazoo.ca>
 * 		   
 */

#include "ns3/log.h"
#include "ns3/queue.h"
#include "ns3/simulator.h"
#include "ns3/ethernet-header.h"
#include "ns3/ethernet-trailer.h"
#include "ns3/llc-snap-header.h"
#include "ns3/error-model.h"
#include "ns3/enum.h"
#include "ns3/boolean.h"
#include "ns3/uinteger.h"
#include "ns3/pointer.h"
#include "ns3/trace-source-accessor.h"
#include "switched-ethernet-net-device.h"
#include "switched-ethernet-channel.h"
#include "ns3/nstime.h"
//RngRun

NS_LOG_COMPONENT_DEFINE ("SwitchedEthernetNetDevice");
using namespace std;
namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (SwitchedEthernetNetDevice);

  TypeId 
SwitchedEthernetNetDevice::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::SwitchedEthernetNetDevice")
    .SetParent<NetDevice> ()
    .AddConstructor<SwitchedEthernetNetDevice> ()
    .AddAttribute ("Address", 
                   "The MAC address of this device.",
                   Mac48AddressValue (Mac48Address ("ff:ff:ff:ff:ff:ff")),
                   MakeMac48AddressAccessor (&SwitchedEthernetNetDevice::m_address),
                   MakeMac48AddressChecker ())
    .AddAttribute ("Mtu", "The MAC-level Maximum Transmission Unit",
                   UintegerValue (DEFAULT_MTU),
                   MakeUintegerAccessor (&SwitchedEthernetNetDevice::SetMtu,
                                         &SwitchedEthernetNetDevice::GetMtu),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("EncapsulationMode", 
                   "The link-layer encapsulation type to use.",
                   EnumValue (DIX),
                   MakeEnumAccessor (&SwitchedEthernetNetDevice::SetEncapsulationMode),
                   MakeEnumChecker (DIX, "Dix",
                                    LLC, "Llc"))
    .AddAttribute ("SendEnable", 
                   "Enable or disable the transmitter section of the device.",
                   BooleanValue (true),
                   MakeBooleanAccessor (&SwitchedEthernetNetDevice::m_sendEnable),
                   MakeBooleanChecker ())
    .AddAttribute ("ReceiveEnable",
                   "Enable or disable the receiver section of the device.",
                   BooleanValue (true),
                   MakeBooleanAccessor (&SwitchedEthernetNetDevice::m_receiveEnable),
                   MakeBooleanChecker ())
    .AddAttribute ("ReceiveErrorModel", 
                   "The receiver error model used to simulate packet loss",
                   PointerValue (),
                   MakePointerAccessor (&SwitchedEthernetNetDevice::m_receiveErrorModel),
                   MakePointerChecker<ErrorModel> ())
    
    //
    // Transmit queueing discipline for the device which includes its own set
    // of trace hooks.
    //
    .AddAttribute ("TxQueue", 
                   "A queue to use as the transmit queue in the device.",
                   PointerValue (),
                   MakePointerAccessor (&SwitchedEthernetNetDevice::m_queue),
                   MakePointerChecker<Queue> ())

    .AddAttribute ("QueueTimer",
    			   "A timer that we add to the queue in order to accumulate packets until this timer expires.",
    			   TimeValue (MilliSeconds(6.0)),
    			   MakeTimeAccessor (&SwitchedEthernetNetDevice::m_queueTimer),
    			   MakeTimeChecker() )

    .AddAttribute ("WakeUpTime",
    		  	   "The time that the NetDevice needs until it powers up and starts transmitting packets (LPI->Active).",
    		  	   TimeValue(MicroSeconds(16.0)),
    		  	   MakeTimeAccessor(&SwitchedEthernetNetDevice::m_wakeUpTime),
    		  	   MakeTimeChecker())

    .AddAttribute ("GoToSleepTime",
       		  	   "The time that the NetDevice needs in order to go to LPI mode. The transition time between Active->LPI.",
       		  	   TimeValue(MicroSeconds(182.0)),
       		  	   MakeTimeAccessor(&SwitchedEthernetNetDevice::m_goToSleepTime),
       		  	   MakeTimeChecker())

    .AddAttribute ("MaxQueueLength",
    		   	   "The length of the queue.",
    		   	   UintegerValue(100),
    		   	   MakeUintegerAccessor (&SwitchedEthernetNetDevice::m_maxNumberOfPackets),
    		   	   MakeUintegerChecker<uint32_t> ())

    .AddAttribute ("DynamicCoalescing",
                   "Enable or disable the dynamic coalescing algorithm for EEE.",
                   BooleanValue (true),
                   MakeBooleanAccessor (&SwitchedEthernetNetDevice::m_dynamicEnable),
                   MakeBooleanChecker ())

    .AddAttribute ("TimerStepAdditive",
       		  	   "The step of the timer when dynamic coalescing is applied. This step is either add or subtract a value. The time is given in microseconds",
       		  	   UintegerValue(500),
       		  	   MakeUintegerAccessor(&SwitchedEthernetNetDevice::m_timerStepAdditive),
       		  	   MakeUintegerChecker<uint16_t> ())

    .AddAttribute ("TimerStepMultiplicative",
                   "The step of the timer when dynamic coalescing is applied. This step multiplies by a percentage value the timer (e.g. 10%, 30% etc).",
                   UintegerValue(10),
                   MakeUintegerAccessor (&SwitchedEthernetNetDevice::m_timerStepMultiplicative),
                   MakeUintegerChecker<uint16_t> ())

    .AddAttribute ("QueueStepAdditive",
    		   	   "The increase or decrease of the queue length in dynamic coalescing. In this case it is additive (fixed value of packets).",
    		   	   UintegerValue(10),
    		   	   MakeUintegerAccessor (&SwitchedEthernetNetDevice::m_queueStepAdditive),
    		   	   MakeUintegerChecker<uint16_t> ())

    .AddAttribute ("QueueStepMultiplicative",
                   "The increase or decrease of the queue length in dynamic coalescing. in this case it is multiplicative (percentage of packets).",
                   UintegerValue(10),
                   MakeUintegerAccessor (&SwitchedEthernetNetDevice::m_queueStepMultiplicative),
                   MakeUintegerChecker<uint16_t> ())

    .AddAttribute ("Additive",
                   "Enable or disable how to apply the step. Either by adding and subtracting the step in EEE value either not.",
                   BooleanValue (true),
                   MakeBooleanAccessor (&SwitchedEthernetNetDevice::m_additiveEnable),
                   MakeBooleanChecker ())

    .AddAttribute ("Multiplicative",
                   "Enable or disable the multiplicative functinality of the dynamic coalescing algorithm for EEE. The step is added in percentage and thus it is not constant",
                   BooleanValue (false),
                   MakeBooleanAccessor (&SwitchedEthernetNetDevice::m_multiplicativeEnable),
                   MakeBooleanChecker ())

/*    .AddAttribute ("DynamicQueueOnly",
                   "Option to select if we want to maintain a constant TimeOut and use a variable queue size. We have to avoid using that with DynamicTimerOnly",
                   BooleanValue (false),
                   MakeBooleanAccessor (&SwitchedEthernetNetDevice::m_dynamicQueueOnly),
                   MakeBooleanChecker ())

    .AddAttribute ("DynamicTimerOnly",
                   "Option to select if we want to maintain a constant queue and use a variable TimeOut. We have to avoid using that with DynamicQueueOnly",
                   BooleanValue (false),
                   MakeBooleanAccessor (&SwitchedEthernetNetDevice::m_dynamicTimerOnly),
                   MakeBooleanChecker ())
*/
    .AddAttribute ("MaxTimeout",
                   "The maximum TimeOut that the NetDevice can suffer before opening the queue.",
                   TimeValue(MilliSeconds(50.0)),
                   MakeTimeAccessor(&SwitchedEthernetNetDevice::m_maxTimeout),
                   MakeTimeChecker())

    			    			  //
    // Trace sources at the "top" of the net device, where packets transition
    // to/from higher layers.
    //
    .AddTraceSource ("MacTx", 
                     "Trace source indicating a packet has arrived for transmission by this device",
                     MakeTraceSourceAccessor (&SwitchedEthernetNetDevice::m_macTxTrace))
    .AddTraceSource ("MacTxDrop", 
                     "Trace source indicating a packet has been dropped by the device before transmission",
                     MakeTraceSourceAccessor (&SwitchedEthernetNetDevice::m_macTxDropTrace))
    .AddTraceSource ("MacPromiscRx", 
                     "A packet has been received by this device, has been passed up from the physical layer "
                     "and is being forwarded up the local protocol stack.  This is a promiscuous trace,",
                     MakeTraceSourceAccessor (&SwitchedEthernetNetDevice::m_macPromiscRxTrace))
    .AddTraceSource ("MacRx", 
                     "A packet has been received by this device, has been passed up from the physical layer "
                     "and is being forwarded up the local protocol stack.  This is a non-promiscuous trace,",
                     MakeTraceSourceAccessor (&SwitchedEthernetNetDevice::m_macRxTrace))
#if 0
    // Not currently implemented in this device
    .AddTraceSource ("MacRxDrop", 
                     "Trace source indicating a packet was received, but dropped before being forwarded up the stack",
                     MakeTraceSourceAccessor (&SwitchedEthernetNetDevice::m_macRxDropTrace))
#endif
    //
    // Trace souces at the "bottom" of the net device, where packets transition
    // to/from the channel.
    //
    .AddTraceSource ("PhyTxBegin", 
                     "Trace source indicating a packet has begun transmitting over the channel",
                     MakeTraceSourceAccessor (&SwitchedEthernetNetDevice::m_phyTxBeginTrace))
    .AddTraceSource ("PhyTxEnd", 
                     "Trace source indicating a packet has been completely transmitted over the channel",
                     MakeTraceSourceAccessor (&SwitchedEthernetNetDevice::m_phyTxEndTrace))
    .AddTraceSource ("PhyTxDrop", 
                     "Trace source indicating a packet has been dropped by the device during transmission",
                     MakeTraceSourceAccessor (&SwitchedEthernetNetDevice::m_phyTxDropTrace))
#if 0
    // Not currently implemented in this device
    .AddTraceSource ("PhyRxBegin", 
                     "Trace source indicating a packet has begun being received by the device",
                     MakeTraceSourceAccessor (&SwitchedEthernetNetDevice::m_phyRxBeginTrace))
#endif
    .AddTraceSource ("PhyRxEnd", 
                     "Trace source indicating a packet has been completely received by the device",
                     MakeTraceSourceAccessor (&SwitchedEthernetNetDevice::m_phyRxEndTrace))
    .AddTraceSource ("PhyRxDrop", 
                     "Trace source indicating a packet has been dropped by the device during reception",
                     MakeTraceSourceAccessor (&SwitchedEthernetNetDevice::m_phyRxDropTrace))
    //
    // Trace sources designed to simulate a packet sniffer facility (tcpdump). 
    //
    .AddTraceSource ("Sniffer", 
                     "Trace source simulating a non-promiscuous packet sniffer attached to the device",
                     MakeTraceSourceAccessor (&SwitchedEthernetNetDevice::m_snifferTrace))
    .AddTraceSource ("PromiscSniffer", 
                     "Trace source simulating a promiscuous packet sniffer attached to the device",
                     MakeTraceSourceAccessor (&SwitchedEthernetNetDevice::m_promiscSnifferTrace))
    ;
  return tid;
}

SwitchedEthernetNetDevice::SwitchedEthernetNetDevice ()
 : m_linkUp (false)
{
  NS_LOG_FUNCTION (this);
  m_txMachineState = READY;
  m_tInterframeGap = Seconds (0);
  m_channel = 0;
  simulation_start = Seconds (0);
  simulation_end = Seconds (0);

  // 
  // We would like to let the attribute system take care of initializing the 
  // packet encapsulation stuff, but we also don't want to get caught up in
  // initialization order changes.  So we'll get the three problem variables
  // into a consistent state here before the attribute calls, and then depend
  // on the semantics of the setters to preserve a consistent state.  This 
  // really doesn't have to be the same set of values as the initial values 
  // set by the attributes, but it does have to be a consistent set.  That is,
  // you can just change the default encapsulation mode above without having 
  // to change it here.
  //
  m_encapMode = DIX;
}

SwitchedEthernetNetDevice::~SwitchedEthernetNetDevice()
{
  NS_LOG_FUNCTION_NOARGS ();
  m_queue = 0;
}

  void 
SwitchedEthernetNetDevice::DoDispose ()
{
  NS_LOG_FUNCTION_NOARGS ();
  m_channel = 0;
  m_node = 0;
  NetDevice::DoDispose ();
}

  void 
SwitchedEthernetNetDevice::SetEncapsulationMode (enum EncapsulationMode mode)
{
  NS_LOG_FUNCTION (mode);

  m_encapMode = mode;

  NS_LOG_LOGIC ("m_encapMode = " << m_encapMode);
  NS_LOG_LOGIC ("m_mtu = " << m_mtu);
}

  SwitchedEthernetNetDevice::EncapsulationMode
SwitchedEthernetNetDevice::GetEncapsulationMode (void)
{
  NS_LOG_FUNCTION_NOARGS ();
  return m_encapMode;
}
  
bool
SwitchedEthernetNetDevice::SetMtu (uint16_t mtu)
{
  NS_LOG_FUNCTION (this << mtu);
  m_mtu = mtu;

  NS_LOG_LOGIC ("m_encapMode = " << m_encapMode);
  NS_LOG_LOGIC ("m_mtu = " << m_mtu);

  return true;
}

  uint16_t
SwitchedEthernetNetDevice::GetMtu (void) const
{
  NS_LOG_FUNCTION_NOARGS ();
  return m_mtu;
}


  void
SwitchedEthernetNetDevice::SetSendEnable (bool sendEnable)
{
  NS_LOG_FUNCTION (sendEnable);
  m_sendEnable = sendEnable;
}

  void
SwitchedEthernetNetDevice::SetReceiveEnable (bool receiveEnable)
{
  NS_LOG_FUNCTION (receiveEnable);
  m_receiveEnable = receiveEnable;
}

  bool
SwitchedEthernetNetDevice::IsSendEnabled (void)
{
  NS_LOG_FUNCTION_NOARGS ();
  return m_sendEnable;
}

  bool
SwitchedEthernetNetDevice::IsReceiveEnabled (void)
{
  NS_LOG_FUNCTION_NOARGS ();
  return m_receiveEnable;
}

  void 
SwitchedEthernetNetDevice::SetInterframeGap (Time t)
{
  NS_LOG_FUNCTION (t);
  m_tInterframeGap = t;
}

  void 
SwitchedEthernetNetDevice::AddHeader (Ptr<Packet> p,   Mac48Address source,  Mac48Address dest,  uint16_t protocolNumber)
{
  NS_LOG_FUNCTION (p << source << dest << protocolNumber);

  EthernetHeader header (false);
  header.SetSource (source);
  header.SetDestination (dest);

  EthernetTrailer trailer;

  NS_LOG_LOGIC ("p->GetSize () = " << p->GetSize ());
  NS_LOG_LOGIC ("m_encapMode = " << m_encapMode);
  NS_LOG_LOGIC ("m_mtu = " << m_mtu);

  uint16_t lengthType = 0;
  switch (m_encapMode) 
    {
    case DIX:
      NS_LOG_LOGIC ("Encapsulating packet as DIX (type interpretation)");
      //
      // This corresponds to the type interpretation of the lengthType field as
      // in the old Ethernet Blue Book.
      //
      lengthType = protocolNumber;

      //
      // All Ethernet frames must carry a minimum payload of 46 bytes.  We need
      // to pad out if we don't have enough bytes.  These must be real bytes 
      // since they will be written to pcap files and compared in regression 
      // trace files.
      //
      if (p->GetSize () < 46)
        {
          uint8_t buffer[46];
          memset (buffer, 0, 46);
          Ptr<Packet> padd = Create<Packet> (buffer, 46 - p->GetSize ());
          p->AddAtEnd (padd);
        }
      break;
    case LLC: 
      {
        NS_LOG_LOGIC ("Encapsulating packet as LLC (length interpretation)");

        LlcSnapHeader llc;
        llc.SetType (protocolNumber);
        p->AddHeader (llc);

        //
        // All Ethernet frames must carry a minimum payload of 46 bytes.  The 
        // LLC SNAP header counts as part of this payload.  We need to padd out
        // if we don't have enough bytes.  These must be real bytes since they 
        // will be written to pcap files and compared in regression trace files.
        //
        if (p->GetSize () < 46)
          {
            uint8_t buffer[46];
            memset (buffer, 0, 46);
            Ptr<Packet> padd = Create<Packet> (buffer, 46 - p->GetSize ());
            p->AddAtEnd (padd);
          }

        //
        // This corresponds to the length interpretation of the lengthType field,
        // but with an LLC/SNAP header added to the payload as in IEEE 802.2
        //      
        lengthType = p->GetSize ();
        NS_ASSERT_MSG (lengthType <= GetMtu (),
          "SwitchedEthernetNetDevice::AddHeader(): 802.3 Length/Type field with LLC/SNAP: "
          "length interpretation must not exceed device frame size minus overhead");
      }
      break;
    case ILLEGAL:
    default:
      NS_FATAL_ERROR ("SwitchedEthernetNetDevice::AddHeader(): Unknown packet encapsulation mode");
      break;
    }

  NS_LOG_LOGIC ("header.SetLengthType (" << lengthType << ")");
  header.SetLengthType (lengthType);
  p->AddHeader (header);

  if (Node::ChecksumEnabled ())
    {
      trailer.EnableFcs (true);
    }
  trailer.CalcFcs (p);
  p->AddTrailer (trailer);
}

#if 0
  bool 
SwitchedEthernetNetDevice::ProcessHeader (Ptr<Packet> p, uint16_t & param)
{
  NS_LOG_FUNCTION (p << param);

  EthernetTrailer trailer;
  p->RemoveTrailer (trailer);

  EthernetHeader header (false);
  p->RemoveHeader (header);

  if ((header.GetDestination () != GetBroadcast ()) &&
      (header.GetDestination () != GetAddress ()))
    {
      return false;
    }

  switch (m_encapMode)
    {
    case DIX:
      param = header.GetLengthType ();
      break;
    case LLC: 
      {
        LlcSnapHeader llc;
        p->RemoveHeader (llc);
        param = llc.GetType ();
      } 
      break;
    case ILLEGAL:
    default:
      NS_FATAL_ERROR ("SwitchedEthernetNetDevice::ProcessHeader(): Unknown packet encapsulation mode");
      break;
    }
  return true;
}
#endif

  void
SwitchedEthernetNetDevice::TransmitStart (void)
{
  NS_LOG_FUNCTION_NOARGS ();

  //
  // This function is called to start the process of transmitting a packet.  We 
  // expect that the packet to transmit will be found in m_currentPkt.
  //
  NS_ASSERT_MSG (m_currentPkt != 0, "SwitchedEthernetNetDevice::TransmitStart(): m_currentPkt not set");

  NS_LOG_LOGIC ("m_currentPkt = " << m_currentPkt);
  NS_LOG_LOGIC ("UID = " << m_currentPkt->GetUid ());
  NS_LOG_LOGIC ("Device ID = " << m_deviceId);

  //Remove    NS_LOG_UNCOND("SwitchedEthernetNetDevice::TransmitStart() m_deviceId: " << m_deviceId << ", at: " << Simulator::Now());  // Remove
  //
  // Only transmit if the send side of net device is enabled
  //
  if (IsSendEnabled () == false)
    {
      m_phyTxDropTrace (m_currentPkt);
      m_currentPkt = 0;
      return;
    }

  //
  // Somebody has called here telling us to start transmitting a packet.  They 
  // can only do this if the state machine is in the READY or BACKOFF state.
  // Specifically, if we are ready to start transmitting, we cannot already
  // be transmitting (i.e., BUSY)
  //
  NS_ASSERT_MSG ((m_txMachineState == READY) || (m_txMachineState == BACKOFF), 
                 "Must be READY to transmit. Tx state is: " << m_txMachineState);

  //
  // Now we have to sense the state of the medium and either start transmitting
  // if it is idle or wait
  
  if(m_channel->GetState(m_deviceId) != IDLE_STATE)
  {
      //
      // The channel is busy, but we don't do backoff in the traditional CSMA
      // manner since this device connects to a full-duplex channel     
      m_txMachineState = BACKOFF;

      NS_LOG_LOGIC ("Channel busy!");
      
  } 
  else 
  {
      //
      // The channel is free, transmit the packet
      //
      //TX
     
		if (m_channel->TransmitStart (m_currentPkt, m_deviceId) == false)
		{
			NS_LOG_WARN ("Channel TransmitStart returns an error");
			NS_LOG_UNCOND ("Channel TransmitStart returns an error");
			m_phyTxDropTrace (m_currentPkt);
			m_currentPkt = 0;
			m_txMachineState = READY;
		} 
		else 
		{
			//
			// Transmission succeeded, reset the backoff time parameters and
			// schedule a transmit complete event.
			//
			m_txMachineState = BUSY;
			m_phyTxBeginTrace (m_currentPkt);

			Time tEvent = Seconds (m_bps.CalculateTxTime (m_currentPkt->GetSize ()));
			NS_LOG_LOGIC ("Schedule TransmitCompleteEvent in " << tEvent.GetSeconds () << "sec");
			Simulator::Schedule (tEvent, &SwitchedEthernetNetDevice::TransmitCompleteEvent, this);
		}
	 
  }
}

  void
SwitchedEthernetNetDevice::TransmitAbort (void)
{
  NS_LOG_FUNCTION_NOARGS ();

  //
  // When we started the process of transmitting the current packet, it was 
  // placed in m_currentPkt.  So we had better find one there.
  //
    
  NS_ASSERT_MSG (m_currentPkt != 0, "SwitchedEthernetNetDevice::TransmitAbort(): m_currentPkt zero");
  NS_LOG_LOGIC ("m_currentPkt=" << m_currentPkt);
  NS_LOG_LOGIC ("Pkt UID is " << m_currentPkt->GetUid () << ")");

  m_phyTxDropTrace (m_currentPkt);
  m_currentPkt = 0;

  NS_ASSERT_MSG (m_txMachineState == BACKOFF, "Must be in BACKOFF state to abort.  Tx state is: " << m_txMachineState);

  // 
  // We're done with that one, so reset the backoff algorithm and ready the
  // transmit state machine.
  //
    m_txMachineState = READY;

  //
  // If there is another packet on the input queue, we need to start trying to 
  // get that out.  If the queue is empty we just wait until someone puts one
  // in.
  //
  if (m_queue->IsEmpty ())
    {
      return;
    }
  else
    {
      m_currentPkt = m_queue->Dequeue ();
      NS_ASSERT_MSG (m_currentPkt != 0, "SwitchedEthernetNetDevice::TransmitAbort(): IsEmpty false but no Packet on queue?");
      m_snifferTrace (m_currentPkt);
      m_promiscSnifferTrace (m_currentPkt);
      TransmitStart ();
    }
}

  void
SwitchedEthernetNetDevice::TransmitCompleteEvent (void)
{
  NS_LOG_FUNCTION_NOARGS ();

  //
  // This function is called to finish the  process of transmitting a packet.
  // We need to tell the channel that we've stopped wiggling the wire and
  // schedule an event that will be executed when it's time to re-enable
  // the transmitter after the interframe gap.
  //
  NS_ASSERT_MSG (m_txMachineState == BUSY, "SwitchedEthernetNetDevice::transmitCompleteEvent(): Must be BUSY if transmitting");
  
  NS_ASSERT (m_channel->GetState(m_deviceId) == TRANSMITTING_STATE);
  
  m_txMachineState = GAP;

  //
  // When we started transmitting the current packet, it was placed in 
  // m_currentPkt.  So we had better find one there.
  //
  NS_ASSERT_MSG (m_currentPkt != 0, "SwitchedEthernetNetDevice::TransmitCompleteEvent(): m_currentPkt zero");
  NS_LOG_LOGIC ("m_currentPkt=" << m_currentPkt);
  NS_LOG_LOGIC ("Pkt UID is " << m_currentPkt->GetUid () << ")");
  NS_LOG_LOGIC ("Device ID = " << m_deviceId);

  m_channel->TransmitEnd(m_deviceId); 
      
  m_phyTxEndTrace (m_currentPkt);
  m_currentPkt = 0;

  NS_LOG_LOGIC ("Schedule TransmitReadyEvent in " << m_tInterframeGap.GetSeconds () << "sec");

  id_gap = Simulator::Schedule (m_tInterframeGap, &SwitchedEthernetNetDevice::TransmitReadyEvent, this);
}

  void
SwitchedEthernetNetDevice::CancelGap (void)
{
  Simulator::Cancel(id_gap);
}

  uint16_t
SwitchedEthernetNetDevice::GetTxMachineState (void)
  {
	return m_txMachineState;
  }

  void
SwitchedEthernetNetDevice::TransmitReadyEvent (void)
{
  NS_LOG_FUNCTION_NOARGS ();
  
  //
  // This function is called to enable the transmitter after the interframe
  // gap has passed.  If there are pending transmissions, we use this opportunity
  // to start the next transmit.
  //
  NS_ASSERT_MSG (m_txMachineState == GAP || (m_txMachineState == SLEEP && m_queue->IsEmpty () && m_otherEndLink->GetQueue()->IsEmpty() && !ChannelIsActive()), "SwitchedEthernetNetDevice::TransmitReadyEvent(): Must be in interframe gap. " << Simulator::Now());

  if(m_txMachineState == GAP)
	  m_txMachineState = READY;

  //
  // We expect that the packet we had been transmitting was cleared when the 
  // TransmitCompleteEvent() was executed.
  //
  NS_ASSERT_MSG (m_currentPkt == 0, "SwitchedEthernetNetDevice::TransmitReadyEvent(): m_currentPkt nonzero");

  //
  // Get the next packet from the queue for transmitting
  //

	if (m_queue->IsEmpty () && m_otherEndLink->GetQueue()->IsEmpty() && !ChannelIsActive() && m_otherEndLink->GetTxMachineState() != 2)
    {
	  EnableSleep();
	  m_otherEndLink->EnableSleep();


    }
  else if (m_queue->IsEmpty () && ChannelIsActive())
    {
	  m_txMachineState = READY;
	  return;

    }
  else if(!m_queue->IsEmpty())
  {
	  m_currentPkt = m_queue->Dequeue ();
      NS_ASSERT_MSG (m_currentPkt != 0, "SwitchedEthernetNetDevice::TransmitReadyEvent(): IsEmpty false but no Packet on queue?");
      m_snifferTrace (m_currentPkt);
      m_promiscSnifferTrace (m_currentPkt);
      TransmitStart ();
  }
}

  void
SwitchedEthernetNetDevice::CheckQueue(void)
{
  if (!m_queue->IsEmpty ())
  {
	m_txMachineState = READY;
    m_currentPkt = m_queue->Dequeue ();
    NS_ASSERT_MSG (m_currentPkt != 0, "SwitchedEthernetNetDevice::TransmitAbort(): IsEmpty false but no Packet on queue?");
    m_snifferTrace (m_currentPkt);
    m_promiscSnifferTrace (m_currentPkt);
    TransmitStart ();
  }
  return;
}

  void
SwitchedEthernetNetDevice::Sleep(void)
  {
	  if (Simulator::Now() > simulation_end)
	  {
	    simulation_end = Simulator::Now();
	    simulation_total = simulation_end - simulation_start;
        cout << "total:" << simulation_total.GetSeconds() << ", LPI: " << total_LPI.GetSeconds() <<" , Power Save: "<< total_LPI.GetSeconds()/simulation_total.GetSeconds()*100 << " %, Average Queue: " << meanQueue/n << " pkts, Average TimeOut: " << meanTimer/n << " s, " << endl;
	  }
	  if (m_txMachineState == SLEEP)
	  {
		  m_txMachineState = LPI;
		  enter_LPI = Simulator::Now();
	  }
	  else
	  {
//		  NS_LOG_UNCOND("SwitchedEthernetNetDevice::Sleep() inside else");
	  }

  }

  void
SwitchedEthernetNetDevice::LPIState (void)
{
  if (m_queue->IsEmpty() && m_otherEndLink->GetQueue()->IsEmpty())
  {
	m_txMachineState = SLEEP;
	Simulator::ScheduleNow(&SwitchedEthernetNetDevice::Sleep, this);
  }
  else
  {
	m_txMachineState = WAKEUP;
	Simulator::Schedule(m_wakeUpTime, &SwitchedEthernetNetDevice::WakeUp, this);
  }
}

  void
SwitchedEthernetNetDevice::ExitLPI (void)
{
  NS_ASSERT(m_txMachineState == LPI);
  m_txMachineState = WAKEUP;
  if (m_dynamicEnable)
  {
	//At this point we increase the timer
    if (m_queueTimer < m_maxTimeout )
	{
	  if (m_additiveEnable && !m_multiplicativeEnable)
	  {
        double beta = double(m_timerStepAdditive)/1000000;
        m_queueTimer = ns3::Time::FromDouble(m_queueTimer.ToDouble(ns3::Time::Unit(0))+beta, ns3::Time::Unit(0));
	    if (m_queueTimer > m_maxTimeout)
		  m_queueTimer = m_maxTimeout;
        cout << "new timeout1 (incremented): " << m_queueTimer << endl;
      }
	  else if (!m_additiveEnable && m_multiplicativeEnable)
	  {
        double gama = 1+(double(m_timerStepMultiplicative)/100);
        m_queueTimer = ns3::Time::FromDouble(m_queueTimer.ToDouble(ns3::Time::Unit(0))*gama, ns3::Time::Unit(0));
        if (m_queueTimer > m_maxTimeout)
          m_queueTimer = m_maxTimeout;
        cout << "new timeout2 (incremented): " << m_queueTimer << endl;
      }
	  //in this case we have additive increase multiplicative decrease
      else if (m_additiveEnable && m_multiplicativeEnable)
      {
        double giota = double(m_timerStepAdditive)/1000000;
        m_queueTimer = ns3::Time::FromDouble(m_queueTimer.ToDouble(ns3::Time::Unit(0))+giota, ns3::Time::Unit(0));
        if (m_queueTimer > m_maxTimeout)
          m_queueTimer = m_maxTimeout;
        cout << "new timeout3 (incremented): " << m_queueTimer << endl;
      }

	  else
	    cout << "SwitchedEthernetNetDevice::ExitLPI() ERROR: both variables are false!";
	}
	// At this point we decrease the queueSize
    if (m_maxNumberOfPackets > 1)
    {
      if (m_additiveEnable && !m_multiplicativeEnable)
      {
        uint16_t epsilon = m_queueStepAdditive;
	    if (epsilon >= m_maxNumberOfPackets)
		  m_maxNumberOfPackets = 1;
        else
		  m_maxNumberOfPackets -= epsilon;
        if (m_maxNumberOfPackets < 1)
		  m_maxNumberOfPackets = 1;
        cout << "new QSize1 (decremented): " << m_maxNumberOfPackets << endl;
      }
      else if (!m_additiveEnable && m_multiplicativeEnable)
      {
        double zeta = 1.0-double(m_queueStepMultiplicative)/100;
        m_maxNumberOfPackets = int(double(m_maxNumberOfPackets)*zeta+0.5);
        if (m_maxNumberOfPackets<1)
		  m_maxNumberOfPackets=1;
        cout << "new QSize2 (decremented): " << m_maxNumberOfPackets << endl;
      }
      else if (m_additiveEnable && m_multiplicativeEnable)
      {
        double kapa = 1.0-double(m_queueStepMultiplicative)/100;
        m_maxNumberOfPackets = int(double(m_maxNumberOfPackets)*kapa + 0.5);
        if (m_maxNumberOfPackets < 1)
		  m_maxNumberOfPackets=1;
        cout << "new QSize3 (decremented): " << m_maxNumberOfPackets << endl;
      }
      else
	  {
        cout << "SwitchedEthernetNetDevice::ExitLPI() ERROR: both variables are false!";
	  }
      if (m_maxNumberOfPackets < 1)
	    m_maxNumberOfPackets=1;
    }
	n++;
	meanQueue+=m_maxNumberOfPackets;
	meanTimer+=m_queueTimer.ToDouble(ns3::Time::Unit(0));
	cout << "Average queue: " << meanQueue/n << " , Average TimeOut: " << meanTimer/n << endl;
  }
    Simulator::Schedule(m_wakeUpTime, &SwitchedEthernetNetDevice::WakeUp, this);

}

  void
SwitchedEthernetNetDevice::WakeUp (void)
{
  m_txMachineState = READY;
  exit_LPI = Simulator::Now();
  total_LPI = total_LPI + (exit_LPI - enter_LPI - m_wakeUpTime);

  if (!m_queue->IsEmpty())
  {
  m_currentPkt = m_queue->Dequeue();
  NS_ASSERT_MSG (m_currentPkt != 0, "SwitchedEthernetNetDevice::WakeUp(): IsEmpty false but no Packet on queue?");
  m_promiscSnifferTrace (m_currentPkt);
  m_snifferTrace (m_currentPkt);
  TransmitStart();
  }
  return;
}

  bool
SwitchedEthernetNetDevice::Attach (Ptr<SwitchedEthernetChannel> ch)
{
  NS_LOG_FUNCTION (this << &ch);

  m_channel = ch;

  m_deviceId = m_channel->Attach (this);

  NS_LOG_FUNCTION ("Device ID is "<< m_deviceId);

  //
  // The channel provides us with the transmitter data rate.
  //
  m_bps = m_channel->GetDataRate ();

  //
  // We use the Ethernet interframe gap of 96 bit times.
  //
  m_tInterframeGap = Seconds (m_bps.CalculateTxTime (96/8));

  //
  // This device is up whenever a channel is attached to it.
  //
  NotifyLinkUp ();
    
  return true;
}

  void
SwitchedEthernetNetDevice::SetQueue (Ptr<Queue> q)
{
  NS_LOG_FUNCTION (q);
  m_queue = q;
}

  void
SwitchedEthernetNetDevice::SetReceiveErrorModel (Ptr<ErrorModel> em)
{
  NS_LOG_FUNCTION (em);
  m_receiveErrorModel = em; 
}

  void
SwitchedEthernetNetDevice::Receive (Ptr<Packet> packet, Ptr<SwitchedEthernetNetDevice> senderDevice)
{
  NS_LOG_FUNCTION (packet << senderDevice);
  NS_LOG_LOGIC ("UID is " << packet->GetUid ());
  NS_LOG_LOGIC ("Device ID = " << m_deviceId);
  //
  // We never forward up packets that we sent.  Real devices don't do this since
  // their receivers are disabled during send, so we don't.
  // 
  if (senderDevice == this)
    {
      return;
    }

  //
  // Hit the trace hook.  This trace will fire on all packets received from the
  // channel except those originated by this device.
  //
  m_phyRxEndTrace (packet);

  // 
  // Only receive if the send side of net device is enabled
  //
  if (IsReceiveEnabled () == false)
    {
      m_phyRxDropTrace (packet);
      return;
    }

  if (m_receiveErrorModel && m_receiveErrorModel->IsCorrupt (packet) )
    {
      NS_LOG_LOGIC ("Dropping pkt due to error model ");
      m_phyRxDropTrace (packet);
    }

  //
  // Trace sinks will expect complete packets, not packets without some of the
  // headers.
  //
  Ptr<Packet> originalPacket = packet->Copy ();

  EthernetTrailer trailer;
  packet->RemoveTrailer (trailer);
  if (Node::ChecksumEnabled ())
    {
      trailer.EnableFcs (true);
    }

  trailer.CheckFcs (packet);
  bool crcGood = trailer.CheckFcs (packet);
  if (!crcGood)
    {
      NS_LOG_INFO ("CRC error on Packet " << packet);
      m_phyRxDropTrace (packet);
      return;
    }

  EthernetHeader header (false);
  packet->RemoveHeader (header);

  NS_LOG_LOGIC ("Pkt source is " << header.GetSource ());
  NS_LOG_LOGIC ("Pkt destination is " << header.GetDestination ());

  uint16_t protocol;
  //
  // If the length/type is less than 1500, it corresponds to a length 
  // interpretation packet.  In this case, it is an 802.3 packet and 
  // will also have an 802.2 LLC header.  If greater than 1500, we
  // find the protocol number (Ethernet type) directly.
  //
  if (header.GetLengthType () <= 1500)
    {
      LlcSnapHeader llc;
      packet->RemoveHeader (llc);
      protocol = llc.GetType ();
    }
  else
   {
      protocol = header.GetLengthType ();
   }

  //
  // Classify the packet based on its destination.
  //
  PacketType packetType;

  if (header.GetDestination ().IsBroadcast ())
    {
      packetType = PACKET_BROADCAST;
    }
  else if (header.GetDestination ().IsGroup ())
    {
      packetType = PACKET_MULTICAST;          
    }
  else if (header.GetDestination () == m_address)
    {
      packetType = PACKET_HOST;
    }
  else
    {
      packetType = PACKET_OTHERHOST;
    }

  // 
  // For all kinds of packetType we receive, we hit the promiscuous sniffer
  // hook and pass a copy up to the promiscuous callback.  Pass a copy to 
  // make sure that nobody messes with our packet.
  //
  m_promiscSnifferTrace (originalPacket);
  if (!m_promiscRxCallback.IsNull ())
    {
      m_macPromiscRxTrace (originalPacket);
      m_promiscRxCallback (this, packet, protocol, header.GetSource (), header.GetDestination (), packetType);
    }

  //
  // If this packet is not destined for some other host, it must be for us
  // as either a broadcast, multicast or unicast.  We need to hit the mac
  // packet received trace hook and forward the packet up the stack.
  //
  if (packetType != PACKET_OTHERHOST)
    {
      m_snifferTrace (originalPacket);
      m_macRxTrace (originalPacket);
      m_rxCallback (this, packet, protocol, header.GetSource ());
    }
}


  Ptr<Queue>
SwitchedEthernetNetDevice::GetQueue (void) const 
{ 
  NS_LOG_FUNCTION_NOARGS ();
  return m_queue;
}

  void
SwitchedEthernetNetDevice::NotifyLinkUp (void)
{
  NS_LOG_FUNCTION_NOARGS ();
  m_linkUp = true;
  m_linkChangeCallbacks ();
}

  void 
SwitchedEthernetNetDevice::SetIfIndex (const uint32_t index)
{
  NS_LOG_FUNCTION (index);
  m_ifIndex = index;
}

  uint32_t 
SwitchedEthernetNetDevice::GetIfIndex (void) const
{
  NS_LOG_FUNCTION_NOARGS ();
  return m_ifIndex;
}

  Ptr<Channel> 
SwitchedEthernetNetDevice::GetChannel (void) const
{
  NS_LOG_FUNCTION_NOARGS ();
  return m_channel;
}

  void
SwitchedEthernetNetDevice::SetAddress (Address address)
{
  NS_LOG_FUNCTION_NOARGS ();
  m_address = Mac48Address::ConvertFrom (address);
}

  Address 
SwitchedEthernetNetDevice::GetAddress (void) const
{
  NS_LOG_FUNCTION_NOARGS ();
  return m_address;
}

  bool 
SwitchedEthernetNetDevice::IsLinkUp (void) const
{
  NS_LOG_FUNCTION_NOARGS ();
  return m_linkUp;
}

  void 
SwitchedEthernetNetDevice::AddLinkChangeCallback (Callback<void> callback)
{
  NS_LOG_FUNCTION (&callback);
  m_linkChangeCallbacks.ConnectWithoutContext (callback);
}

  bool 
SwitchedEthernetNetDevice::IsBroadcast (void) const
{
  NS_LOG_FUNCTION_NOARGS ();
  return true;
}

  Address
SwitchedEthernetNetDevice::GetBroadcast (void) const
{
  NS_LOG_FUNCTION_NOARGS ();
  return Mac48Address ("ff:ff:ff:ff:ff:ff");
}

  bool 
SwitchedEthernetNetDevice::IsMulticast (void) const
{
  NS_LOG_FUNCTION_NOARGS ();
  return true;
}

  Address 
SwitchedEthernetNetDevice::GetMulticast (Ipv4Address multicastGroup) const
{
  NS_LOG_FUNCTION (multicastGroup);

  Mac48Address ad = Mac48Address::GetMulticast (multicastGroup);

  //
  // Implicit conversion (operator Address ()) is defined for Mac48Address, so
  // use it by just returning the EUI-48 address which is automagically converted
  // to an Address.
  //
  NS_LOG_LOGIC ("multicast address is " << ad);

  return ad;
}

  bool 
SwitchedEthernetNetDevice::IsPointToPoint (void) const
{
  NS_LOG_FUNCTION_NOARGS ();
  return false;
}

  bool 
SwitchedEthernetNetDevice::IsBridge (void) const
{
  NS_LOG_FUNCTION_NOARGS ();
  return false;
}

  bool
SwitchedEthernetNetDevice::Send (Ptr<Packet> packet,const Address& dest, uint16_t protocolNumber)
{
  NS_LOG_FUNCTION (packet << dest << protocolNumber);
  return SendFrom (packet, m_address, dest, protocolNumber);
}

  bool
SwitchedEthernetNetDevice::ChannelIsActive(void)
{
  if (m_channel->GetState(0) == (TRANSMITTING_STATE || PROPAGATING_STATE) || m_channel->GetState(1) == (TRANSMITTING_STATE || PROPAGATING_STATE))
	  return true;
  else
	  return false;
}

  void
SwitchedEthernetNetDevice::EnableSleep (void)
{
	  if(m_txMachineState == SLEEP) return; //already sleeping
	  m_txMachineState = SLEEP;
	  id_sleep = Simulator::Schedule(m_goToSleepTime, &SwitchedEthernetNetDevice::Sleep, this);
}


  void
SwitchedEthernetNetDevice::CancelSleep (void)
{
  m_txMachineState = READY;
  Simulator::Cancel(id_sleep);
}

  void
SwitchedEthernetNetDevice::CancelLPI (void)
{
  Simulator::Cancel(id_lpi);
  m_txMachineState = WAKEUP;
  Simulator::Schedule(m_wakeUpTime, &SwitchedEthernetNetDevice::WakeUp, this);
  if (m_dynamicEnable)
  {
    //At this point we decrease the timer
    if (m_queueTimer >= MilliSeconds(1.0) )
    {
      if (m_additiveEnable && !m_multiplicativeEnable)
      {
        double alpha = double(m_timerStepAdditive)/1000000;
	if (alpha >= m_queueTimer)
	  m_queueTimer = MilliSeconds(1.0);
	else
          m_queueTimer = ns3::Time::FromDouble(m_queueTimer.ToDouble(ns3::Time::Unit(0))-alpha, ns3::Time::Unit(0));
	if (m_queueTimer < MilliSeconds(1.0))
	  m_queueTimer = MilliSeconds (1.0);
        cout << "new timeout4 (decremented): " << m_queueTimer << endl;
      }
      else if (!m_additiveEnable && m_multiplicativeEnable)
      {
        double delta = 1-(double(m_timerStepMultiplicative)/100);
        m_queueTimer = ns3::Time::FromDouble(m_queueTimer.ToDouble(ns3::Time::Unit(0))*delta, ns3::Time::Unit(0));
	if (m_queueTimer < MilliSeconds(1.0))
          m_queueTimer = MilliSeconds (1.0);
        cout << "new timeout5 (decremented): " << m_queueTimer << endl;
      }
      else if (m_additiveEnable && m_multiplicativeEnable)
      {
        double lamda = 1-(double(m_timerStepMultiplicative)/100);
        m_queueTimer = ns3::Time::FromDouble(m_queueTimer.ToDouble(ns3::Time::Unit(0))*lamda, ns3::Time::Unit(0));
        if (m_queueTimer < MilliSeconds(1.0))
          m_queueTimer = MilliSeconds (1.0);
        cout << "new timeout6 (decremented): " << m_queueTimer << endl;
      }
      else
        cout << "SwitchedEthernetNetDevice::ExitLPI() ERROR: both variables are false!";
    }
    //At this point we increase the queueSize
    if (m_maxNumberOfPackets < 200)
    {
      if (m_additiveEnable && !m_multiplicativeEnable)
      {
        uint16_t eta = m_queueStepAdditive;
        m_maxNumberOfPackets += eta;
	if (m_maxNumberOfPackets > 200)
	  m_maxNumberOfPackets = 200;
        cout << "new QSize4 (incremented): " << m_maxNumberOfPackets << endl;
      }
      else if (!m_additiveEnable && m_multiplicativeEnable)
      {
        double theta = 1.0+double(m_queueStepMultiplicative)/100;
        m_maxNumberOfPackets = int(double(m_maxNumberOfPackets)*theta+0.5);
        if (m_maxNumberOfPackets > 200)
          m_maxNumberOfPackets = 200;
        cout << "new QSize5 (incremented): " << m_maxNumberOfPackets << endl;
      }
      else if (m_additiveEnable && m_multiplicativeEnable)
      {
        uint16_t mi = m_queueStepAdditive;
        m_maxNumberOfPackets += mi;
        if (m_maxNumberOfPackets > 200)
          m_maxNumberOfPackets = 200;
        cout << "new QSize6 (incremented): " << m_maxNumberOfPackets << endl;
      }
      else
      {
        cout << "SwitchedEthernetNetDevice::CancelLPI() ERROR: both variables are false!";
      }
    }
    n++;
	meanQueue+=m_maxNumberOfPackets;
	meanTimer+=m_queueTimer.ToDouble(ns3::Time::Unit(0));
	cout << "Average queue: " << meanQueue/n << " , Average TimeOut: " << meanTimer/n << endl;
  }
}

  bool
SwitchedEthernetNetDevice::SendFrom (Ptr<Packet> packet, const Address& src, const Address& dest, uint16_t protocolNumber)
{
  NS_LOG_FUNCTION (packet << src << dest << protocolNumber);
  NS_LOG_LOGIC ("packet =" << packet);
  NS_LOG_LOGIC ("UID is " << packet->GetUid () << ")");
  NS_LOG_LOGIC ("Device ID = " << m_deviceId);


  NS_ASSERT (IsLinkUp ());

  if (simulation_start == Seconds (0))
  {
    simulation_start = Simulator::Now();
 static int first=0; //v
first++; //
  }
  //
  // Only transmit if send side of net device is enabled
  //
  if (IsSendEnabled () == false)
    {
      m_macTxDropTrace (packet);
      return false;
    }

  Mac48Address destination = Mac48Address::ConvertFrom (dest);
  Mac48Address source = Mac48Address::ConvertFrom (src);
  AddHeader (packet, source, destination, protocolNumber);

  m_macTxTrace (packet);

  //
  // Place the packet to be sent on the send queue.  Note that the 
  // queue may fire a drop trace, but we will too.
  //
  if (m_queue->Enqueue(packet) == false)
    {
      m_macTxDropTrace (packet);
      return false;
    }

  NS_ASSERT(!m_queue->IsEmpty());


  if (m_txMachineState == SLEEP)
  {
	  CancelSleep();
	  m_otherEndLink->CancelSleep();
  }

  //
  // If the device is idle, we need to start a transmission. Otherwise,
  // the transmission will be started when the current packet finished
  // transmission (see TransmitCompleteEvent)
  //
  if (m_txMachineState == READY) 
  {
	  m_currentPkt = m_queue->Dequeue ();
	  NS_ASSERT_MSG (m_currentPkt != 0, "SwitchedEthernetNetDevice::SendFrom(): IsEmpty false but no Packet on queue?");
	  m_promiscSnifferTrace (m_currentPkt);
	  m_snifferTrace (m_currentPkt);
	  TransmitStart ();
  }

  //Added by Angelos START
  else if (m_txMachineState == LPI)
  {
		if( m_queue->GetNPackets() >= m_maxNumberOfPackets )
		{
			CancelLPI();
			m_otherEndLink->CancelLPI();
		}
		else if( m_queue->GetNPackets() == 1 && m_otherEndLink->GetQueue()->IsEmpty())
		{
			id_lpi=Simulator::Schedule(m_queueTimer, &SwitchedEthernetNetDevice::ExitLPI, this);
			m_otherEndLink->Set_id_lpi(Simulator::Schedule(m_queueTimer, &SwitchedEthernetNetDevice::ExitLPI, m_otherEndLink));
		}

  }
  //Added by Angelos END

  return true;
}

  void
SwitchedEthernetNetDevice::SetOtherLinkEnd(Ptr<SwitchedEthernetNetDevice> P)
{
  m_otherEndLink = P;
}


  EventId
SwitchedEthernetNetDevice::Get_id_sleep(void)
{
  NS_LOG_FUNCTION_NOARGS ();
  return id_sleep;
}

  void
SwitchedEthernetNetDevice::Set_id_sleep(EventId ID)
{
//  NS_LOG_FUNCTION (ID);
  id_sleep = ID;
}

  EventId
SwitchedEthernetNetDevice::Get_id_lpi(void)
{
  NS_LOG_FUNCTION_NOARGS ();
  return id_lpi;
}

  void
SwitchedEthernetNetDevice::Set_id_lpi(EventId ID)
{
//  NS_LOG_FUNCTION (ID);
  id_lpi = ID;
}


  Ptr<Node> 
SwitchedEthernetNetDevice::GetNode (void) const
{
  NS_LOG_FUNCTION_NOARGS ();
  return m_node;
}

  void 
SwitchedEthernetNetDevice::SetNode (Ptr<Node> node)
{
  NS_LOG_FUNCTION (node);

  m_node = node;
}

  bool 
SwitchedEthernetNetDevice::NeedsArp (void) const
{
  NS_LOG_FUNCTION_NOARGS ();
  return true;
}

  void 
SwitchedEthernetNetDevice::SetReceiveCallback (NetDevice::ReceiveCallback cb)
{
  NS_LOG_FUNCTION (&cb);
  m_rxCallback = cb;
}

Address SwitchedEthernetNetDevice::GetMulticast (Ipv6Address addr) const
{
  Mac48Address ad = Mac48Address::GetMulticast (addr);

  NS_LOG_LOGIC("MAC IPv6 multicast address is " << ad);
  return ad;
}

  void 
SwitchedEthernetNetDevice::SetPromiscReceiveCallback (NetDevice::PromiscReceiveCallback cb)
{
  NS_LOG_FUNCTION (&cb);
  m_promiscRxCallback = cb;
}

  bool 
SwitchedEthernetNetDevice::SupportsSendFrom () const
{
  NS_LOG_FUNCTION_NOARGS ();
  return true;
}

} // namespace ns3
