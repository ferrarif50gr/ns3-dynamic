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
 * Author: Angelos Chatzipapas <angelos.chatzipapas@imdeaa.org>
 */

#include "trace-app-helper.h"
#include "ns3/inet-socket-address.h"
#include "ns3/packet-socket-address.h"
#include "ns3/string.h"
#include "ns3/names.h"
#include "ns3/trace-generator.h"
#include "ns3/log.h"

namespace ns3 {

TraceAppHelper::TraceAppHelper (std::string protocol, Address remote)
{
	NS_LOG_UNCOND("TraceAppHelper::TraceAppHelper ()");
  m_factory.SetTypeId (TraceGenerator::GetTypeId () /*"ns3::OnOffApplication"*/);
  m_factory.Set ("Protocol", StringValue (protocol));
  m_factory.Set ("Remote", AddressValue (remote));
}

void
TraceAppHelper::SetAttribute (std::string name, const AttributeValue &value)
{
  m_factory.Set (name, value);
}
/*
ApplicationContainer
RandomAppHelper::Install (Ptr<Node> node) const
{
  return ApplicationContainer (InstallPriv (node));
}

ApplicationContainer
OnOffHelper::Install (NodeContainer nodes) const
{
  ApplicationContainer apps;
  for (NodeContainer::Iterator i = nodes.Begin (); i != nodes.End (); ++i)
    {
      apps.Add (InstallPriv (*i));
    }

  return apps;
}


Ptr<Application>
RandomAppHelper::InstallPriv (Ptr<Node> node) const
{
  Ptr<Application> app = m_factory.Create<Application> ();
  node->AddApplication (app);

  return app;
}
*/

ApplicationContainer
TraceAppHelper::Install (NodeContainer nodes) const
{
  ApplicationContainer applications;
  for (NodeContainer::Iterator i = nodes.Begin (); i != nodes.End (); ++i)
  {
      Ptr<TraceGenerator> app = m_factory.Create<TraceGenerator> ();
//      app->SetRemote (m_socketType, m_remote);
      (*i)->AddApplication (app);
      applications.Add (app);
  }
  return applications;
}

} //namespace ns3
