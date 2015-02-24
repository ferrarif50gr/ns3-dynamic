/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2008 INRIA
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
 * Author: Mathieu Lacage <mathieu.lacage@sophia.inria.fr>
 */
#ifndef RANDOM_APP_HELPER_H
#define RANDOM_APP_HELPER_H

#include <stdint.h>
#include <string>
#include "ns3/object-factory.h"
#include "ns3/address.h"
#include "ns3/attribute.h"
#include "ns3/net-device.h"
#include "ns3/node-container.h"
#include "ns3/application-container.h"

namespace ns3 {

/**
 * \brief A helper to make it easier to instantiate an ns3::OnOffApplication
 * on a set of nodes.
 */
class RandomAppHelper
   {
   public:
	  /**
	   * Create an OnOffHelper to make it easier to work with OnOffApplications
	   *
	   * \param protocol the name of the protocol to use to send traffic
	   *        by the applications. This string identifies the socket
	   *        factory type used to create sockets for the applications.
	   *        A typical value would be ns3::UdpSocketFactory.
	   * \param remote the address of the remote node to send traffic
	   *        to.
	   */

     RandomAppHelper (std::string protocol, Address remote);

     /**
      * Helper function used to set the underlying application attributes.
      *
      * \param name the name of the application attribute to set
      * \param value the value of the application attribute to set
      */
     void SetAttribute (std::string name, const AttributeValue &value);

     /**
      * Install an ns3::OnOffApplication on each node of the input container
      * configured with all the attributes set with SetAttribute.
      *
      * \param c NodeContainer of the set of nodes on which an OnOffApplication
      * will be installed.
      * \returns Container of Ptr to the applications installed.
      */

     ApplicationContainer Install (NodeContainer nodes) const;

   private:
     std::string m_protocol;
     Address m_remote;
     ObjectFactory m_factory;
//     RandomVariable m_packetSize;
//     RandomVariable m_delay;
   };

} // namespace ns3

#endif /* RANDOM_APP_HELPER_H */

