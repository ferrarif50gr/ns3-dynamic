/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2012 Jeff Young
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
 * 			This code is based on csma-channel created by 
 *	   	Emmanuelle Laprise <emmanuelle.laprise@bluekazoo.ca>
 */

#include "switched-ethernet-channel.h"
#include "switched-ethernet-net-device.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/log.h"

NS_LOG_COMPONENT_DEFINE ("SwitchedEthernetChannel");

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (SwitchedEthernetChannel);

  TypeId 
SwitchedEthernetChannel::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::SwitchedEthernetChannel")
    .SetParent<Channel> ()
    .AddConstructor<SwitchedEthernetChannel> ()
    .AddAttribute ("DataRate", 
                   "The transmission data rate to be provided to devices connected to the channel",
                   DataRateValue (DataRate (0xffffffff)),
                   MakeDataRateAccessor (&SwitchedEthernetChannel::m_bps),
                   MakeDataRateChecker ())
    .AddAttribute ("Delay", "Transmission delay through the channel",
                   TimeValue (Seconds (0)),
                   MakeTimeAccessor (&SwitchedEthernetChannel::m_delay),
                   MakeTimeChecker ())
    ;
  return tid;
}

SwitchedEthernetChannel::SwitchedEthernetChannel ()
: 
  Channel ()
{
  NS_LOG_FUNCTION_NOARGS ();
  m_State[0] = IDLE_STATE;
  m_State[1] = IDLE_STATE;
  endpoints[0] = 0;
  endpoints[1] = 0;
  m_deviceList.clear();
}
/*
SwitchedEthernetLink::SwitchedEthernetLink ()
:
  SwitchedEthernetChannel ()
{
	endpoints[0]=0;
	endpoints[1]=0;
}
*/
int32_t
SwitchedEthernetChannel::Attach (Ptr<SwitchedEthernetNetDevice> device)
{
	if(endpoints[0] == 0)
	{
		//NS_LOG_UNCOND("SwitchedEthernetChannel::Attach() if(endpoints[0] == 0");
		endpoints[0] = device;
//		NS_LOG_UNCOND("SwitchedEthernetChannel::Attach() if(endpoints[0] == 0, endpoint[0]: " << endpoints[0] << ", endpoint[1]: " << endpoints[1] << ", device: " << device);
	}
	else if(endpoints[1] == 0)
	{

		endpoints[1] = device;
		endpoints[0]->SetOtherLinkEnd(endpoints[1]);
		endpoints[1]->SetOtherLinkEnd(endpoints[0]);
		NS_LOG_UNCOND("SwitchedEthernetChannel::Attach()2 if(endpoints[1] == 0, endpoint[0]: " << endpoints[0] << ", endpoint[1]: " << endpoints[1] << ", device: " << m_deviceList.size());
	}
	else
	{
		NS_LOG_UNCOND("Error: too many devices for a point-to-point link!");
		exit(-1);
	}

  NS_LOG_FUNCTION (this << device);
  NS_ASSERT (device != 0);

  //For full-duplex links we can only attach two devices to a channel
  //since there is no backoff
  NS_ASSERT(m_deviceList.size() < 2);

  SwitchedEthernetDeviceRec rec (device);
  
  m_deviceList.push_back (rec);
  return (m_deviceList.size () - 1);
}

  bool
SwitchedEthernetChannel::Reattach (Ptr<SwitchedEthernetNetDevice> device)
{
  NS_LOG_FUNCTION (this << device);
  NS_ASSERT (device != 0);

  std::vector<SwitchedEthernetDeviceRec>::iterator it;
  for (it = m_deviceList.begin (); it < m_deviceList.end( ); it++) 
    {
      if (it->devicePtr == device) 
        {
          if (!it->active) 
            {
              it->active = true;
              return true;
            } 
          else 
            {
              return false;
            }
        }
    }
  return false;
}

  bool
SwitchedEthernetChannel::Reattach (uint32_t deviceId)
{
  NS_LOG_FUNCTION (this << deviceId);

  if (deviceId < m_deviceList.size ())
    {
      return false;
    }

  if (m_deviceList[deviceId].active)
    {
      return false;
    } 
  else 
    {
      m_deviceList[deviceId].active = true;
      return true;
    }
}

  bool
SwitchedEthernetChannel::Detach (uint32_t deviceId)
{
  NS_LOG_FUNCTION (this << deviceId);

  if (deviceId < m_deviceList.size ())
    {
      if (!m_deviceList[deviceId].active)
        {
          NS_LOG_WARN ("SwitchedEthernetChannel::Detach(): Device is already detached (" << deviceId << ")");
          return false;
        }

      m_deviceList[deviceId].active = false;

      if ((m_State[deviceId] == TRANSMITTING_STATE) && (m_currentSrc[deviceId] == deviceId))
      {
          NS_LOG_WARN ("SwitchedEthernetChannel::Detach(): Device is currently" << "transmitting (" << deviceId << ")");
      }

      return true;
    } 
  else 
    {
      return false;
    }
}

  bool
SwitchedEthernetChannel::Detach (Ptr<SwitchedEthernetNetDevice> device)
{
  NS_LOG_FUNCTION (this << device);
  NS_ASSERT (device != 0);

  std::vector<SwitchedEthernetDeviceRec>::iterator it;
  for (it = m_deviceList.begin (); it < m_deviceList.end (); it++) 
    {
      if ((it->devicePtr == device) && (it->active)) 
        {
          it->active = false;
          return true;
        }
    }
  return false;
}

  bool
SwitchedEthernetChannel::TransmitStart (Ptr<Packet> p, uint32_t srcId)
{
  NS_LOG_FUNCTION (this << p << srcId);
  NS_LOG_INFO ("UID is " << p->GetUid () << ")");

  if (m_State[srcId] != IDLE_STATE)
  {
    NS_LOG_WARN ("SwitchedEthernetChannel::TransmitStart(): State is not IDLE for device "<<srcId);
    return false;
  }

  if (!IsActive(srcId))
    {
      NS_LOG_ERROR ("SwitchedEthernetChannel::TransmitStart(): Selected source is not currently attached to network");
      return false;
    }

  NS_LOG_LOGIC ("switch to TRANSMITTING_STATE");
  m_currentPkt[srcId] = p;
  m_currentSrc[srcId] = srcId;
  m_State[srcId] = TRANSMITTING_STATE;
  return true;
}


  bool
SwitchedEthernetChannel::IsActive(uint32_t deviceId) 
{
  return (m_deviceList[deviceId].active);
}

  bool
SwitchedEthernetChannel::TransmitEnd(uint32_t srcId)
{
  NS_LOG_FUNCTION (this << m_currentPkt[srcId] << m_currentSrc[srcId]);
  NS_LOG_INFO ("UID is " << m_currentPkt[srcId]->GetUid () << ")");

  NS_ASSERT (m_State[srcId] == TRANSMITTING_STATE);
  m_State[srcId] = PROPAGATING_STATE;

  bool retVal = true;

  if (!IsActive (m_currentSrc[srcId]))
    {
      NS_LOG_ERROR ("SwitchedEthernetChannel::TransmitEnd(): Seclected source was detached before the end of the transmission");
      retVal = false;
    }

  NS_LOG_LOGIC ("Schedule event in " << m_delay.GetSeconds () << " sec");


  NS_LOG_LOGIC ("Receive");
  
  std::vector<SwitchedEthernetDeviceRec>::iterator it;
  uint32_t devId = 0;
  for (it = m_deviceList.begin (); it < m_deviceList.end(); it++) 
    {
	  //Check to see if the device is active and also that the device is not the sender
      if (it->IsActive () && (devId != m_currentSrc[srcId]))
      {
          // schedule reception events
          Simulator::ScheduleWithContext (it->devicePtr->GetNode ()->GetId (),
                                          m_delay,
                                          &SwitchedEthernetNetDevice::Receive, it->devicePtr,
                                          m_currentPkt[srcId]->Copy (), m_deviceList[m_currentSrc[srcId]].devicePtr);
        }
      devId++;
    }

  // also schedule for the tx side to go back to IDLE_STATE
  Simulator::Schedule (m_delay, &SwitchedEthernetChannel::PropagationCompleteEvent,
                       this, srcId);
  return retVal;
}


  void
SwitchedEthernetChannel::PropagationCompleteEvent(uint32_t deviceId)
{
//  NS_LOG_UNCOND("SwitchedEthernetChannel::PropagationCompleteEvent(), endpoint[0]: " << endpoints[0] << ", endpoint[1]: " << endpoints[1] << ", device: " << deviceId);
  NS_LOG_FUNCTION (this << m_currentPkt[deviceId]);
  NS_LOG_INFO ("UID is " << m_currentPkt[deviceId]->GetUid () << ")");

  NS_ASSERT (m_State[deviceId] == PROPAGATING_STATE);
  m_State[deviceId] = IDLE_STATE;
}

  uint32_t 
SwitchedEthernetChannel::GetNumActDevices (void)
{
  int numActDevices = 0;
  std::vector<SwitchedEthernetDeviceRec>::iterator it;
  for (it = m_deviceList.begin (); it < m_deviceList.end (); it++) 
    {
      if (it->active)
        {
          numActDevices++;
        }
    }
  return numActDevices;
}

  uint32_t 
SwitchedEthernetChannel::GetNDevices (void) const
{
  return (m_deviceList.size ());
}

  Ptr<SwitchedEthernetNetDevice>
SwitchedEthernetChannel::GetSwitchedEthernetDevice (uint32_t i) const
{
  Ptr<SwitchedEthernetNetDevice> netDevice = m_deviceList[i].devicePtr;
  return netDevice;
}

  int32_t
SwitchedEthernetChannel::GetDeviceNum (Ptr<SwitchedEthernetNetDevice> device)
{
  std::vector<SwitchedEthernetDeviceRec>::iterator it;
  int i = 0;
  for (it = m_deviceList.begin (); it < m_deviceList.end (); it++) 
    {
      if (it->devicePtr == device)
        {
          if (it->active) 
            {
              return i;
            } 
          else 
            {
              return -2;
            }
        }
      i++;
    }
  return -1;
}

  bool 
SwitchedEthernetChannel::IsBusy (uint32_t deviceId)
{
  if (m_State[deviceId] == IDLE_STATE) 
    {
      return false;
    } 
  else 
    {
      return true;
    }
}

  DataRate
SwitchedEthernetChannel::GetDataRate (void)
{
  return m_bps;
}

  Time
SwitchedEthernetChannel::GetDelay (void)
{
  return m_delay;
}

  FullDuplexWireState
SwitchedEthernetChannel::GetState (uint32_t deviceId)
{
  return m_State[deviceId];
}

  Ptr<NetDevice>
SwitchedEthernetChannel::GetDevice (uint32_t i) const
{
  return GetSwitchedEthernetDevice (i);
}

SwitchedEthernetDeviceRec::SwitchedEthernetDeviceRec ()
{
  active = false;
}

SwitchedEthernetDeviceRec::SwitchedEthernetDeviceRec (Ptr<SwitchedEthernetNetDevice> device)
{
  devicePtr = device; 
  active = true;
}

  bool
SwitchedEthernetDeviceRec::IsActive () 
{
  return active;
}

} // namespace ns3
