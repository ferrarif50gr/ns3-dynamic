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
 * Author: Jeff Young <jyoung9@gatech.edu
 * 			This code is based on csma-net-device created by 
 * 			Emmanuelle Laprise <emmanuelle.laprise@bluekazoo.ca>
 * 
 */

#ifndef SWITCHED_ETHERNET_NET_DEVICE_H
#define SWITCHED_ETHERNET_NET_DEVICE_H

#include <string.h>
#include "ns3/node.h"
#include "ns3/address.h"
#include "ns3/net-device.h"
#include "ns3/callback.h"
#include "ns3/packet.h"
#include "ns3/traced-callback.h"
#include "ns3/nstime.h"
#include "ns3/data-rate.h"
#include "ns3/ptr.h"
#include "ns3/random-variable.h"
#include "ns3/mac48-address.h"
#include "ns3/simulator.h"

namespace ns3 {

class Queue;
class SwitchedEthernetChannel;
class ErrorModel;

/**
 * \class SwitchedEthernetNetDevice
 * \brief A Device for a SwitchedEthernet Network Link.
 *
 * The SwitchedEthernet net device class is analogous to layer 1 and 2 of the
 * TCP stack. The NetDevice takes a raw packet of bytes and creates a
 * protocol specific packet from them. 
 */
class SwitchedEthernetNetDevice : public NetDevice 
{
public:
  static TypeId GetTypeId (void);

  /**
   * Enumeration of the types of packets supported in the class.
   */
  enum EncapsulationMode {
    ILLEGAL,     /**< Encapsulation mode not set */
    DIX,         /**< DIX II / Ethernet II packet */
    LLC,         /**< 802.2 LLC/SNAP Packet*/  
  };

  /**
   * Construct a SwitchedEthernetNetDevice
   *
   * This is the default constructor for a SwitchedEthernetNetDevice.
   */
  SwitchedEthernetNetDevice ();

  /**
   * Destroy a SwitchedEthernetNetDevice
   *
   * This is the destructor for a SwitchedEthernetNetDevice.
   */
  virtual ~SwitchedEthernetNetDevice ();

  /**
   * Set the interframe gap used to separate packets.  The interframe gap
   * defines the minimum space required between packets sent by this device.
   * As in Ethernet, it defaults to 96 bit times.
   *
   * \param t the interframe gap time
   */
  void SetInterframeGap (Time t);
 
  /**
   * Attach the device to a channel.
   *
   * The function Attach is used to add a SwitchedEthernetNetDevice to a SwitchedEthernetChannel.
   *
   * \see SetDataRate ()
   * \see SetInterframeGap ()
   * \param ch a pointer to the channel to which this object is being attached.
   */
  bool Attach (Ptr<SwitchedEthernetChannel> ch);

  /**
   * Attach a queue to the SwitchedEthernetNetDevice.
   *
   * The SwitchedEthernetNetDevice "owns" a queue.  This queue may be set by higher
   * level topology objects to implement a particular queueing method such as
   * DropTail or RED.  
   *
   * \see Queue
   * \see DropTailQueue
   * \param queue a Ptr to the queue for being assigned to the device.
   */
  void SetQueue (Ptr<Queue> queue);

  /**
   * Get a copy of the attached Queue.
   *
   * \return a pointer to the queue.
   */
  Ptr<Queue> GetQueue (void) const; 

  /**
   * Attach a receive ErrorModel to the SwitchedEthernetNetDevice.
   *
   * The SwitchedEthernetNetDevice may optionally include an ErrorModel in
   * the packet receive chain to simulate data errors in during transmission.
   *
   * \see ErrorModel
   * \param em a pointer to the ErrorModel 
   */
  void SetReceiveErrorModel (Ptr<ErrorModel> em);

  /**
   * Receive a packet from a connected SwitchedEthernetChannel.
   *
   * The SwitchedEthernetNetDevice receives packets from its connected channel
   * and forwards them up the protocol stack.  This is the public method
   * used by the channel to indicate that the last bit of a packet has 
   * arrived at the device.
   *
   * \see SwitchedEthernetChannel
   * \param p a reference to the received packet
   * \param sender the SwitchedEthernetNetDevice that transmitted the packet in the first place
   */
  void Receive (Ptr<Packet> p, Ptr<SwitchedEthernetNetDevice> sender);

  /**
   * Is the send side of the network device enabled?
   *
   * \returns True if the send side is enabled, otherwise false.
   */
  bool IsSendEnabled (void);

  /**
   * Enable or disable the send side of the network device.
   *
   * \param enable Enable the send side if true, otherwise disable.
   */
  void SetSendEnable (bool enable);

  /**
   * Is the receive side of the network device enabled?
   *
   * \returns True if the receiver side is enabled, otherwise false.
   */
  bool IsReceiveEnabled (void);

  /**
   * Enable or disable the receive side of the network device.
   *
   * \param enable Enable the receive side if true, otherwise disable.
   */
  void SetReceiveEnable (bool enable);

  /**
   * Set the encapsulation mode of this device.
   *
   * \param mode The encapsulation mode of this device.
   *
   */
  void SetEncapsulationMode (SwitchedEthernetNetDevice::EncapsulationMode mode);

  /**
   * Get the encapsulation mode of this device.
   *
   * \returns The encapsulation mode of this device.
   */
  SwitchedEthernetNetDevice::EncapsulationMode  GetEncapsulationMode (void);

  //
  // The following methods are inherited from NetDevice base class.
  //
  virtual void SetIfIndex (const uint32_t index);
  virtual uint32_t GetIfIndex (void) const;
  virtual Ptr<Channel> GetChannel (void) const;
  virtual bool SetMtu (const uint16_t mtu);
  virtual uint16_t GetMtu (void) const;
  virtual void SetAddress (Address address);
  virtual Address GetAddress (void) const;
  virtual bool IsLinkUp (void) const;
  virtual void AddLinkChangeCallback (Callback<void> callback);
  virtual bool IsBroadcast (void) const;
  virtual Address GetBroadcast (void) const;
  virtual bool IsMulticast (void) const;

  /**
   * \brief Make and return a MAC multicast address using the provided
   *        multicast group
   *
   * RFC 1112 says that an Ipv4 host group address is mapped to an Ethernet 
   * multicast address by placing the low-order 23-bits of the IP address into 
   * the low-order 23 bits of the Ethernet multicast address 
   * 01-00-5E-00-00-00 (hex).
   *
   * This method performs the multicast address creation function appropriate
   * to an EUI-48-based SwitchedEthernet device.  This MAC address is encapsulated in an
   *  abstract Address to avoid dependencies on the exact address format.
   *
   * \param multicastGroup The IP address for the multicast group destination
   * of the packet.
   * \return The MAC multicast Address used to send packets to the provided
   * multicast group.
   *
   * \see Ipv4Address
   * \see Mac48Address
   * \see Address
   */
  virtual Address GetMulticast (Ipv4Address multicastGroup) const;

  /**
   * Is this a point to point link?
   * \returns false.
   */
  virtual bool IsPointToPoint (void) const;

  /**
   * Is this a bridge?
   * \returns false.
   */
  virtual bool IsBridge (void) const;

  /**
   * Start sending a packet down the channel.
   * \param packet packet to send
   * \param dest layer 2 destination address
   * \param protocolNumber protocol number
   * \return true if successfull, false otherwise (drop, ...)
   */
  virtual bool Send (Ptr<Packet> packet, const Address& dest, 
    uint16_t protocolNumber);

  void EnableSleep (void);
  void CancelSleep (void);
  void CancelLPI (void);
  void ExitLPI (void);
  void CancelGap (void);


  /**
   * Start sending a packet down the channel, with MAC spoofing
   * \param packet packet to send
   * \param source layer 2 source address
   * \param dest layer 2 destination address
   * \param protocolNumber protocol number
   * \return true if successfull, false otherwise (drop, ...)
   */
  virtual bool SendFrom (Ptr<Packet> packet, const Address& source, const Address& dest, 
                         uint16_t protocolNumber);

  /**
   * Get the node to which this device is attached.
   *
   * \returns Ptr to the Node to which the device is attached.
   */
  virtual Ptr<Node> GetNode (void) const;

  /**
   * Set the node to which this device is being attached.
   *
   * \param node Ptr to the Node to which the device is being attached.
   */
  virtual void SetNode (Ptr<Node> node);

  /**
   * Does this device need to use the address resolution protocol?
   *
   * \returns True if the encapsulation mode is set to a value that requires
   * ARP (IP_ARP or LLC).
   */
  virtual bool NeedsArp (void) const;

  /**
   * Set the callback to be used to notify higher layers when a packet has been
   * received.
   *
   * \param cb The callback.
   */
  virtual void SetReceiveCallback (NetDevice::ReceiveCallback cb);

  /**
   * \brief Get the MAC multicast address corresponding
   * to the IPv6 address provided.
   * \param addr IPv6 address
   * \return the MAC multicast address
   * \warning Calling this method is invalid if IsMulticast returns not true.
   */
  virtual Address GetMulticast (Ipv6Address addr) const;


  virtual void SetPromiscReceiveCallback (PromiscReceiveCallback cb);
  virtual bool SupportsSendFrom (void) const;

  void SetOtherLinkEnd(Ptr<SwitchedEthernetNetDevice> P);


  EventId Get_id_sleep(void);
  void Set_id_sleep(EventId ID);
  EventId Get_id_lpi(void);
  void Set_id_lpi(EventId ID);
  void PrintMessage(void);
  void CheckQueue(void);
  Ptr<SwitchedEthernetNetDevice> m_otherEndLink;


protected:
  /**
   * Perform any object release functionality required to break reference 
   * cycles in reference counted objects held by the device.
   */
  virtual void DoDispose (void);

  /**
   * Adds the necessary headers and trailers to a packet of data in order to
   * respect the packet type
   *
   * \param p Packet to which header should be added
   * \param source MAC source address from which packet should be sent
   * \param dest MAC destination address to which packet should be sent
   * \param protocolNumber In some protocols, identifies the type of
   * payload contained in this packet.
   */
  void AddHeader (Ptr<Packet> p, Mac48Address source, Mac48Address dest, uint16_t protocolNumber);

private:


  /**
   * Operator = is declared but not implemented.  This disables the assignment
   * operator for SwitchedEthernetNetDevice objects.
   * \param o object to copy
   */
  SwitchedEthernetNetDevice &operator = (const SwitchedEthernetNetDevice &o);

  /**
   * Copy constructor is declared but not implemented.  This disables the
   * copy constructor for SwitchedEthernetNetDevice objects.
   * \param o object to copy
   */
  SwitchedEthernetNetDevice (const SwitchedEthernetNetDevice &o);

  /**
   * Initialization function used during object construction.
   * \param sendEnable if device will be allowed to send
   * \param receiveEnable if device will be allowed to receive
   */
  void Init (bool sendEnable, bool receiveEnable);

  /**
   * Start Sending a Packet Down the Wire.
   *
   * The TransmitStart method is the method that is used internally in
   * the SwitchedEthernetNetDevice to begin the process of sending a packet
   * out on the channel.  A corresponding method is called on the
   * channel to let it know that the physical device this class
   * represents has actually started sending signals, this causes the
   * channel to enter the BUSY state.  An event is scheduled for the time at
   * which the bits have been completely transmitted. 
   *
   * If the channel is found to be BUSY, this method reschedules itself for
   * execution at a later time (within the backoff period).
   *
   * \see SwitchedEthernetChannel::TransmitStart ()
   * \see TransmitCompleteEvent ()
   */
  void TransmitStart ();

  /**
   * Stop Sending a Packet Down the Wire and Begin the Interframe Gap.
   *
   * The TransmitCompleteEvent method is used internally to finish the process
   * of sending a packet out on the channel.  During execution of this method
   * the TransmitEnd method is called on the channel to let it know that the
   * physical device this class represents has finished sending simulated
   * signals.  The channel uses this event to begin its speed of light delay
   * timer after which it notifies the Net Device(s) at the other end of the 
   * link that new bits have arrived (it delivers the Packet).  During this 
   * method, the net device also schedules the TransmitReadyEvent at which
   * time the transmitter becomes ready to send the next packet.
   *
   * \see SwitchedEthernetChannel::TransmitEnd ()
   * \see TransmitReadyEvent ()
   */
  void TransmitCompleteEvent (void);

  /**
   * Cause the Transmitter to Become Ready to Send Another Packet.
   *
   * The TransmitReadyEvent method is used internally to re-enable the 
   * transmit machine of the net device.  It is scheduled after a suitable
   * interframe gap after the completion of the previous transmission.
   * The queue is checked at this time, and if there is a packet waiting on
   * the queue, the transmission process is begun.
   *
   * If a packet is in the queue, it is extracted for the queue as the
   * next packet to be transmitted by the net device.
   *
   * \see TransmitStart ()
   */
  void TransmitReadyEvent (void);

  /**
   * Aborts the transmission of the current packet
   *
   * If the net device has tried to transmit a packet for more times
   * than the maximum allowed number of retries (channel always busy)
   * then the packet is dropped.
   */
  void TransmitAbort (void);

  /**
   * Notify any interested parties that the link has come up.
   */
  void NotifyLinkUp (void);

  void Sleep(void);
  void LPIState(void);
  void WakeUp(void);
  bool ChannelIsActive(void);
  bool m_dynamicEnable;
  uint16_t m_timerStepAdditive;
  uint16_t m_timerStepMultiplicative;
  uint16_t m_queueStepAdditive;
  uint16_t m_queueStepMultiplicative;
  bool m_additiveEnable;
  bool m_multiplicativeEnable;
//  bool m_dynamicQueueOnly;
//  bool m_dynamicTimerOnly;
  double meanQueue;
  double meanTimer;
  uint32_t n;

  /** 
   * Device ID returned by the attached functions. It is used by the
   * mp-channel to identify each net device to make sure that only
   * active net devices are writing to the channel
   */
  uint32_t m_deviceId; 

  /**
   * Enable net device to send packets. True by default
   */
  bool m_sendEnable;

  /**
   * Enable net device to receive packets. True by default
   */
  bool m_receiveEnable;

  /**
   * Enumeration of the states of the transmit machine of the net device.
   */
  enum TxMachineState
    {
      READY, /**< The transmitter is ready to begin transmission of a packet */
      BUSY,  /**< The transmitter is busy transmitting a packet */
      GAP,    /**< The transmitter is in the interframe gap time */
      BACKOFF,    /**< The transmitter is waiting for the channel to be free */
      SLEEP,
      LPI,
      WAKEUP
    };

  /**
   * The state of the Net Device transmit state machine.
   * \see TxMachineState
   */
  TxMachineState m_txMachineState;
  uint16_t GetTxMachineState (void);
  /**
   * The type of packet that should be created by the AddHeader
   * function and that should be processed by the ProcessHeader
   * function.
   */
  EncapsulationMode m_encapMode;

  /**
   * The data rate that the Net Device uses to simulate packet transmission
   * timing.
   * \see class DataRate
   */
  DataRate m_bps;

  /**
   * The interframe gap that the Net Device uses insert time between packet
   * transmission
   * \see class Time
   */
  Time m_tInterframeGap;

  /**
   * Next packet that will be transmitted (if transmitter is not
   * currently transmitting) or packet that is currently being
   * transmitted.
   */
  Ptr<Packet> m_currentPkt;

  /**
   * The SwitchedEthernetChannel to which this SwitchedEthernetNetDevice has been
   * attached.
   * \see class SwitchedEthernetChannel
   */
  Ptr<SwitchedEthernetChannel> m_channel;

  /**
   * The Queue which this SwitchedEthernetNetDevice uses as a packet source.
   * Management of this Queue has been delegated to the SwitchedEthernetNetDevice
   * and it has the responsibility for deletion.
   * \see class Queue
   * \see class DropTailQueue
   */
  Ptr<Queue> m_queue;

  /**
   * Error model for receive packet events.  When active this model will be
   * used to model transmission errors by marking some of the packets 
   * received as corrupt.
   */
  Ptr<ErrorModel> m_receiveErrorModel;

  /**
   * The trace source fired when packets come into the "top" of the device
   * at the L3/L2 transition, before being queued for transmission.
   *
   * \see class CallBackTraceSource
   */
  TracedCallback<Ptr<const Packet> > m_macTxTrace;

  /**
   * The trace source fired when packets coming into the "top" of the device
   * at the L3/L2 transition are dropped before being queued for transmission.
   *
   * \see class CallBackTraceSource
   */
  TracedCallback<Ptr<const Packet> > m_macTxDropTrace;

  /**
   * The trace source fired for packets successfully received by the device
   * immediately before being forwarded up to higher layers (at the L2/L3 
   * transition).  This is a promiscuous trace.
   *
   * \see class CallBackTraceSource
   */
  TracedCallback<Ptr<const Packet> > m_macPromiscRxTrace;

  /**
   * The trace source fired for packets successfully received by the device
   * immediately before being forwarded up to higher layers (at the L2/L3 
   * transition).  This is a non-promiscuous trace.
   *
   * \see class CallBackTraceSource
   */
  TracedCallback<Ptr<const Packet> > m_macRxTrace;

  /**
   * The trace source fired for packets successfully received by the device
   * but dropped before being forwarded up to higher layers (at the L2/L3 
   * transition).
   *
   * \see class CallBackTraceSource
   */
  TracedCallback<Ptr<const Packet> > m_macRxDropTrace;
  
  /**
   * The trace source fired when a packet begins the transmission process on
   * the medium.
   *
   * \see class CallBackTraceSource
   */
  TracedCallback<Ptr<const Packet> > m_phyTxBeginTrace;

  /**
   * The trace source fired when a packet ends the transmission process on
   * the medium.
   *
   * \see class CallBackTraceSource
   */
  TracedCallback<Ptr<const Packet> > m_phyTxEndTrace;

  /**
   * The trace source fired when the phy layer drops a packet as it tries
   * to transmit it.
   *
   * \see class CallBackTraceSource
   */
  TracedCallback<Ptr<const Packet> > m_phyTxDropTrace;

  /**
   * The trace source fired when a packet begins the reception process from
   * the medium.
   *
   * \see class CallBackTraceSource
   */
  TracedCallback<Ptr<const Packet> > m_phyRxBeginTrace;

  /**
   * The trace source fired when a packet ends the reception process from
   * the medium.
   *
   * \see class CallBackTraceSource
   */
  TracedCallback<Ptr<const Packet> > m_phyRxEndTrace;

  /**
   * The trace source fired when the phy layer drops a packet it has received.
   *
   * \see class CallBackTraceSource
   */
  TracedCallback<Ptr<const Packet> > m_phyRxDropTrace;

  /**
   * A trace source that emulates a non-promiscuous protocol sniffer connected 
   * to the device.  Unlike your average everyday sniffer, this trace source 
   * will not fire on PACKET_OTHERHOST events.
   *
   * On the transmit size, this trace hook will fire after a packet is dequeued
   * from the device queue for transmission.  In Linux, for example, this would
   * correspond to the point just before a device hard_start_xmit where 
   * dev_queue_xmit_nit is called to dispatch the packet to the PF_PACKET 
   * ETH_P_ALL handlers.
   *
   * On the receive side, this trace hook will fire when a packet is received,
   * just before the receive callback is executed.  In Linux, for example, 
   * this would correspond to the point at which the packet is dispatched to 
   * packet sniffers in netif_receive_skb.
   *
   * \see class CallBackTraceSource
   */
  TracedCallback<Ptr<const Packet> > m_snifferTrace;

  /**
   * A trace source that emulates a promiscuous mode protocol sniffer connected
   * to the device.  This trace source fire on packets destined for any host
   * just like your average everyday packet sniffer.
   *
   * On the transmit size, this trace hook will fire after a packet is dequeued
   * from the device queue for transmission.  In Linux, for example, this would
   * correspond to the point just before a device hard_start_xmit where 
   * dev_queue_xmit_nit is called to dispatch the packet to the PF_PACKET 
   * ETH_P_ALL handlers.
   *
   * On the receive side, this trace hook will fire when a packet is received,
   * just before the receive callback is executed.  In Linux, for example, 
   * this would correspond to the point at which the packet is dispatched to 
   * packet sniffers in netif_receive_skb.
   *
   * \see class CallBackTraceSource
   */
  TracedCallback<Ptr<const Packet> > m_promiscSnifferTrace;

  /**
   * The Node to which this device is attached.
   */
  Ptr<Node> m_node;

  /**
   * The MAC address which has been assigned to this device.
   */
  Mac48Address m_address;

  /**
   * The callback used to notify higher layers that a packet has been received.
   */
  NetDevice::ReceiveCallback m_rxCallback;

  /**
   * The callback used to notify higher layers that a packet has been received in promiscuous mode.
   */
  NetDevice::PromiscReceiveCallback m_promiscRxCallback;

  /**
   * The interface index (really net evice index) that has been assigned to 
   * this network device.
   */
  uint32_t m_ifIndex;

  /**
   * Flag indicating whether or not the link is up.  In this case,
   * whether or not the device is connected to a channel.
   */
  bool m_linkUp;

  /**
   * List of callbacks to fire if the link changes state (up or down).
   */
  TracedCallback<> m_linkChangeCallbacks;

  static const uint16_t DEFAULT_MTU = 1500;

  /**
   * The Maximum Transmission Unit.  This corresponds to the maximum 
   * number of bytes that can be transmitted as seen from higher layers.
   * This corresponds to the 1500 byte MTU size often seen on IP over 
   * Ethernet.
   */
  uint32_t m_mtu;

  Time m_maxTimeout;
  Time m_queueTimer;
  Time m_wakeUpTime;
  Time m_goToSleepTime;
  uint32_t m_maxNumberOfPackets;
  EventId id_lpi;
  EventId id_sleep;
  EventId id_gap;
  Time exit_LPI;
  Time enter_LPI;
  Time total_LPI;
  Time simulation_start;
  Time simulation_end;
  Time simulation_total;


};

}; // namespace ns3

#endif /* SWITCHED_ETHERNET_NET_DEVICE_H */

