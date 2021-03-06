/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2012
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
 * Author: Jeff Young 
 *         Based on csma-helper by Mathieu Lacage <mathieu.lacage@sophia.inria.fr>
 */
#ifndef SWITCHED_ETHERNET_HELPER_H
#define SWITCHED_ETHERNET_HELPER_H

#include <string>

#include "ns3/attribute.h"
#include "ns3/object-factory.h"
#include "ns3/net-device-container.h"
#include "ns3/node-container.h"
#include "ns3/switched-ethernet-channel.h"
#include "ns3/deprecated.h"
#include "ns3/trace-helper.h"

namespace ns3 {

class Packet;

/**
 * \brief build a set of SwitchedEthernetNetDevice objects
 *
 * Normally we eschew multiple inheritance, however, the classes 
 * PcapUserHelperForDevice and AsciiTraceUserHelperForDevice are
 * treated as "mixins".  A mixin is a self-contained class that
 * encapsulates a general attribute or a set of functionality that
 * may be of interest to many other classes.
 */
class SwitchedEthernetHelper : public PcapHelperForDevice, public AsciiTraceHelperForDevice
{
public:
  /**
   * Construct a SwitchedEthernetHelper.
   */
  SwitchedEthernetHelper ();
  virtual ~SwitchedEthernetHelper () {}

  /**
   * \param type the type of queue
   * \param n1 the name of the attribute to set on the queue
   * \param v1 the value of the attribute to set on the queue
   * \param n2 the name of the attribute to set on the queue
   * \param v2 the value of the attribute to set on the queue
   * \param n3 the name of the attribute to set on the queue
   * \param v3 the value of the attribute to set on the queue
   * \param n4 the name of the attribute to set on the queue
   * \param v4 the value of the attribute to set on the queue
   *
   * Set the type of queue to create and associated to each
   * SwitchedEthernetNetDevice created through SwitchedEthernetHelper::Install.
   */
  void SetQueue (std::string type,
                 std::string n1 = "", const AttributeValue &v1 = EmptyAttributeValue (),
                 std::string n2 = "", const AttributeValue &v2 = EmptyAttributeValue (),
                 std::string n3 = "", const AttributeValue &v3 = EmptyAttributeValue (),
                 std::string n4 = "", const AttributeValue &v4 = EmptyAttributeValue ());

  /**
   * \param n1 the name of the attribute to set
   * \param v1 the value of the attribute to set
   *
   * Set these attributes on each ns3::SwitchedEthernetNetDevice created
   * by SwitchedEthernetHelper::Install
   */
  void SetDeviceAttribute (std::string n1, const AttributeValue &v1);

  /**
   * \param n1 the name of the attribute to set
   * \param v1 the value of the attribute to set
   *
   * Set these attributes on each ns3::SwitchedEthernetChannel created
   * by SwitchedEthernetHelper::Install
   */
  void SetChannelAttribute (std::string n1, const AttributeValue &v1);

  /**
   * This method creates an ns3::SwitchedEthernetChannel with the attributes configured by
   * SwitchedEthernetHelper::SetChannelAttribute, an ns3::SwitchedEthernetNetDevice with the attributes
   * configured by SwitchedEthernetHelper::SetDeviceAttribute and then adds the device
   * to the node and attaches the channel to the device.
   *
   * \param node The node to install the device in
   * \returns A container holding the added net device.
   */
  NetDeviceContainer Install (Ptr<Node> node) const;

  /**
   * This method creates an ns3::SwitchedEthernetChannel with the attributes configured by
   * SwitchedEthernetHelper::SetChannelAttribute, an ns3::SwitchedEthernetNetDevice with the attributes
   * configured by SwitchedEthernetHelper::SetDeviceAttribute and then adds the device
   * to the node and attaches the channel to the device.
   *
   * \param name The name of the node to install the device in
   * \returns A container holding the added net device.
   */
  NetDeviceContainer Install (std::string name) const;

  /**
   * This method creates an ns3::SwitchedEthernetNetDevice with the attributes configured by
   * SwitchedEthernetHelper::SetDeviceAttribute and then adds the device to the node and 
   * attaches the provided channel to the device.
   *
   * \param node The node to install the device in
   * \param channel The channel to attach to the device.
   * \returns A container holding the added net device.
   */
  NetDeviceContainer Install (Ptr<Node> node, Ptr<SwitchedEthernetChannel> channel) const;

  /**
   * This method creates an ns3::SwitchedEthernetNetDevice with the attributes configured by
   * SwitchedEthernetHelper::SetDeviceAttribute and then adds the device to the node and 
   * attaches the provided channel to the device.
   *
   * \param node The node to install the device in
   * \param channelName The name of the channel to attach to the device.
   * \returns A container holding the added net device.
   */
  NetDeviceContainer Install (Ptr<Node> node, std::string channelName) const;

  /**
   * This method creates an ns3::SwitchedEthernetNetDevice with the attributes configured by
   * SwitchedEthernetHelper::SetDeviceAttribute and then adds the device to the node and 
   * attaches the provided channel to the device.
   *
   * \param nodeName The name of the node to install the device in
   * \param channel The channel to attach to the device.
   * \returns A container holding the added net device.
   */
  NetDeviceContainer Install (std::string nodeName, Ptr<SwitchedEthernetChannel> channel) const;

  /**
   * This method creates an ns3::SwitchedEthernetNetDevice with the attributes configured by
   * SwitchedEthernetHelper::SetDeviceAttribute and then adds the device to the node and 
   * attaches the provided channel to the device.
   *
   * \param nodeName The name of the node to install the device in
   * \param channelName The name of the channel to attach to the device.
   * \returns A container holding the added net device.
   */
  NetDeviceContainer Install (std::string nodeName, std::string channelName) const;

  /**
   * This method creates an ns3::SwitchedEthernetChannel with the attributes configured by
   * SwitchedEthernetHelper::SetChannelAttribute.  For each Ptr<node> in the provided
   * container: it creates an ns3::SwitchedEthernetNetDevice (with the attributes 
   * configured by SwitchedEthernetHelper::SetDeviceAttribute); adds the device to the 
   * node; and attaches the channel to the device.
   *
   * \param c The NodeContainer holding the nodes to be changed.
   * \returns A container holding the added net devices.
   */
  NetDeviceContainer Install (const NodeContainer &c) const;

  /**
   * For each Ptr<node> in the provided container, this method creates an 
   * ns3::SwitchedEthernetNetDevice (with the attributes configured by 
   * SwitchedEthernetHelper::SetDeviceAttribute); adds the device to the node; and attaches 
   * the provided channel to the device.
   *
   * \param c The NodeContainer holding the nodes to be changed.
   * \param channel The channel to attach to the devices.
   * \returns A container holding the added net devices.
   */
  NetDeviceContainer Install (const NodeContainer &c, Ptr<SwitchedEthernetChannel> channel) const;

  /**
   * For each Ptr<node> in the provided container, this method creates an 
   * ns3::SwitchedEthernetNetDevice (with the attributes configured by 
   * SwitchedEthernetHelper::SetDeviceAttribute); adds the device to the node; and attaches 
   * the provided channel to the device.
   *
   * \param c The NodeContainer holding the nodes to be changed.
   * \param channelName The name of the channel to attach to the devices.
   * \returns A container holding the added net devices.
   */
  NetDeviceContainer Install (const NodeContainer &c, std::string channelName) const;

private:
  /*
   * \internal
   */
  Ptr<NetDevice> InstallPriv (Ptr<Node> node, Ptr<SwitchedEthernetChannel> channel) const;

  /**
   * \brief Enable pcap output on the indicated net device.
   * \internal
   *
   * NetDevice-specific implementation mechanism for hooking the trace and
   * writing to the trace file.
   *
   * \param prefix Filename prefix to use for pcap files.
   * \param nd Net device for which you want to enable tracing.
   * \param promiscuous If true capture all possible packets available at the device.
   * \param explicitFilename Treat the prefix as an explicit filename if true
   */
  virtual void EnablePcapInternal (std::string prefix, Ptr<NetDevice> nd, bool promiscuous, bool explicitFilename);

  /**
   * \brief Enable ascii trace output on the indicated net device.
   * \internal
   *
   * NetDevice-specific implementation mechanism for hooking the trace and
   * writing to the trace file.
   *
   * \param stream The output stream object to use when logging ascii traces.
   * \param prefix Filename prefix to use for ascii trace files.
   * \param nd Net device for which you want to enable tracing.
   */
  virtual void EnableAsciiInternal (Ptr<OutputStreamWrapper> stream, 
                                    std::string prefix, 
                                    Ptr<NetDevice> nd,
                                    bool explicitFilename);

  ObjectFactory m_queueFactory;
  ObjectFactory m_deviceFactory;
  ObjectFactory m_channelFactory;
};

} // namespace ns3

#endif /* SWITCHED_ETHERNET_HELPER_H */
