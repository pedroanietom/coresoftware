#include "SingleZdcTriggerInput.h"

#include "Fun4AllPrdfInputTriggerManager.h"
#include "InputManagerType.h"

#include <ffarawobjects/CaloPacketv1.h>
#include <ffarawobjects/CaloPacketContainerv1.h>

#include <phool/PHCompositeNode.h>
#include <phool/PHIODataNode.h>    // for PHIODataNode
#include <phool/PHNode.h>          // for PHNode
#include <phool/PHNodeIterator.h>  // for PHNodeIterator
#include <phool/PHObject.h>        // for PHObject
#include <phool/getClass.h>
#include <phool/phool.h>

#include <Event/Event.h>
#include <Event/EventTypes.h>
#include <Event/Eventiterator.h>
#include <Event/packet.h>  // for Packet

#include <TSystem.h>

#include <cstdint>   // for uint64_t
#include <iostream>  // for operator<<, basic_ostream<...
#include <iterator>  // for reverse_iterator
#include <limits>    // for numeric_limits
#include <memory>
#include <set>
#include <utility>  // for pair

// we have so far 7 packets in the "zdc" file (zdc + smd),
// this number needs to be npackets+1
// so it doesn't trigger the warning and exit. Setting it to 3

static const int NZDCPACKETS = 8;

SingleZdcTriggerInput::SingleZdcTriggerInput(const std::string &name)
  : SingleTriggerInput(name)
{
  SubsystemEnum(InputManagerType::ZDC);
  plist = new Packet *[NZDCPACKETS];  // two packets for the zdc file
}

SingleZdcTriggerInput::~SingleZdcTriggerInput()
{
  CleanupUsedPackets(std::numeric_limits<int>::max());
  delete[] plist;
}

void SingleZdcTriggerInput::FillPool(const unsigned int keep)
{
  if (AllDone())  // no more files and all events read
  {
    return;
  }
  while (GetEventiterator() == nullptr)  // at startup this is a null pointer
  {
    if (!OpenNextFile())
    {
      AllDone(1);
      return;
    }
  }
  while (GetSomeMoreEvents(keep))
  {
    std::unique_ptr<Event> evt(GetEventiterator()->getNextEvent());
    while (!evt)
    {
      fileclose();
      if (!OpenNextFile())
      {
        AllDone(1);
        return;
      }
      evt.reset(GetEventiterator()->getNextEvent());
    }
    if (Verbosity() > 2)
    {
      std::cout << PHWHERE << "Fetching next Event" << evt->getEvtSequence() << std::endl;
    }
    RunNumber(evt->getRunNumber());
    if (GetVerbosity() > 1)
    {
      evt->identify();
    }
    if (evt->getEvtType() != DATAEVENT)
    {
      m_NumSpecialEvents++;
      continue;
    }
    int EventSequence = evt->getEvtSequence();
    int npackets = evt->getPacketList(plist, NZDCPACKETS);
    if (npackets >= NZDCPACKETS)
    {
      std::cout << PHWHERE << " Packets array size " << NZDCPACKETS
		<< " too small for " << Name()
		<< ", increase NZDCPACKETS and rebuild" << std::endl;
      exit(1);
    }

    for (int i = 0; i < npackets; i++)
    {
      int packet_id = plist[i]->getIdentifier();
// The call to  EventNumberOffset(identifier) will initialize it to our default if it wasn't set already
      int CorrectedEventSequence = EventSequence + EventNumberOffset(packet_id);
      if (Verbosity() > 2)
      {
        plist[i]->identify();
      }

      // by default use previous bco clock for gtm bco
      CaloPacket *newhit = new CaloPacketv1();
      uint64_t gtm_bco = plist[i]->lValue(0, "CLOCK");
      int nr_modules = plist[i]->iValue(0,"NRMODULES");
      int nr_channels = plist[i]->iValue(0, "CHANNELS");
      int nr_samples = plist[i]->iValue(0, "SAMPLES");
      if (nr_modules > 3)
      {
	std::cout << PHWHERE << " too many modules, need to adjust arrays" << std::endl;
	gSystem->Exit(1);
      }
      newhit->setNrModules(nr_modules);
      newhit->setNrSamples(nr_samples);
      newhit->setNrChannels(nr_channels);
      newhit->setBCO(gtm_bco);
      newhit->setPacketEvtSequence(plist[i]->iValue(0, "EVTNR"));
      newhit->setIdentifier(packet_id);
      newhit->setHitFormat(plist[i]->getHitFormat());
      newhit->setEvtSequence(EventSequence);
      newhit->setEvenChecksum(plist[i]->iValue(0, "EVENCHECKSUM"));
      newhit->setCalcEvenChecksum(plist[i]->iValue(0, "CALCEVENCHECKSUM"));
      newhit->setOddChecksum(plist[i]->iValue(0, "ODDCHECKSUM"));
      newhit->setCalcOddChecksum(plist[i]->iValue(0, "CALCODDCHECKSUM"));
      newhit->setModuleAddress(plist[i]->iValue(0,"MODULEADDRESS"));
      newhit->setDetId(plist[i]->iValue(0,"DETID"));
      for (int ifem = 0; ifem < nr_modules; ifem++)
      {
        newhit->setFemClock(ifem, plist[i]->iValue(ifem, "FEMCLOCK"));
        newhit->setFemEvtSequence(ifem, plist[i]->iValue(ifem, "FEMEVTNR"));
        newhit->setFemSlot(ifem, plist[i]->iValue(ifem, "FEMSLOT"));
        newhit->setChecksumLsb(ifem, plist[i]->iValue(ifem, "CHECKSUMLSB"));
        newhit->setChecksumMsb(ifem, plist[i]->iValue(ifem, "CHECKSUMMSB"));
        newhit->setCalcChecksumLsb(ifem,plist[i]->iValue(ifem, "CALCCHECKSUMLSB"));
        newhit->setCalcChecksumMsb(ifem,plist[i]->iValue(ifem, "CALCCHECKSUMMSB"));
      }
      for (int ipmt = 0; ipmt < nr_channels; ipmt++)
      {
        // store pre/post only for suppressed channels, the array in the packet routines is not
        // initialized so reading pre/post for not zero suppressed channels returns garbage
        bool isSuppressed = plist[i]->iValue(ipmt,"SUPPRESSED");
        newhit->setSuppressed(ipmt,isSuppressed);
	if (isSuppressed)
	{
	  newhit->setPre(ipmt,plist[i]->iValue(ipmt,"PRE"));
	  newhit->setPost(ipmt,plist[i]->iValue(ipmt,"POST"));
	}
	else
	{
	  for (int isamp = 0; isamp < nr_samples; isamp++)
	  {
	    newhit->setSample(ipmt, isamp, plist[i]->iValue(isamp, ipmt));
	  }
	}
      }
      if (Verbosity() > 2)
      {
        std::cout << PHWHERE << "corrected evtno: " << CorrectedEventSequence
		  << ", original evtno: " << EventSequence
                  << ", bco: 0x" << std::hex << gtm_bco << std::dec
                  << std::endl;
      }
      if (TriggerInputManager())
      {
	if (packet_id == std::clamp(packet_id, 9000, 9999))
	{
	  if (packet_id == 9002)
	  {
	    TriggerInputManager()->AddSEpdPacket(CorrectedEventSequence, newhit);
	  }
	}
	else
	{
//        CorrectedEventSequence = EventSequence;
	  TriggerInputManager()->AddZdcPacket(CorrectedEventSequence, newhit);
	}
      }
      m_ZdcPacketMap[CorrectedEventSequence].push_back(newhit);
      m_EventStack.insert(CorrectedEventSequence);
      if (ddump_enabled())
      {
	ddumppacket(plist[i]);
      }
      delete plist[i];
    }
  }
}

void SingleZdcTriggerInput::Print(const std::string &what) const
{
  if (what == "ALL" || what == "STORAGE")
  {
    for (const auto &bcliter : m_ZdcPacketMap)
    {
      std::cout << PHWHERE << "Event: " << bcliter.first << std::endl;
    }
  }
  if (what == "ALL" || what == "STACK")
  {
    for (auto iter : m_EventStack)
    {
      std::cout << PHWHERE << "stacked event: " << iter << std::endl;
    }
  }
}

void SingleZdcTriggerInput::CleanupUsedPackets(const int eventno)
{
  std::vector<int> toclearevents;
  for (const auto &iter : m_ZdcPacketMap)
  {
    if (iter.first <= eventno)
    {
      if (Verbosity() > 1)
      {
	std::cout << "Deleting event " << iter.first << " from zdc input mgr" << std::endl;
      }
      for (auto pktiter : iter.second)
      {
	if (Verbosity() > 1)
	{
	  std::cout << "Deleting packet " << pktiter->getIdentifier() << std::endl;
	}
        delete pktiter;
      }
      toclearevents.push_back(iter.first);
    }
    else
    {
      break;
    }
  }

  for (auto iter : toclearevents)
  {
    m_EventStack.erase(iter);
    m_ZdcPacketMap.erase(iter);
  }
}

void SingleZdcTriggerInput::ClearCurrentEvent()
{
  // called interactively, to get rid of the current event
  int currentevent = *m_EventStack.begin();
  //  std::cout << PHWHERE << "clearing bclk 0x" << std::hex << currentbclk << std::dec << std::endl;
  CleanupUsedPackets(currentevent);
  return;
}

bool SingleZdcTriggerInput::GetSomeMoreEvents(const unsigned int keep)
{
  if (AllDone())
  {
    return false;
  }
  if (m_ZdcPacketMap.empty())
  {
    return true;
  }

  int first_event = m_ZdcPacketMap.begin()->first;
  int last_event = m_ZdcPacketMap.rbegin()->first;
  if (Verbosity() > 1)
  {
    std::cout << "number of zdc events: " << m_ZdcPacketMap.size() << std::endl;
    std::cout << PHWHERE << "first event: " << first_event
              << " last event: " << last_event
              << std::endl;
  }
  if (keep > 2 && m_ZdcPacketMap.size() < keep)
  {
    return true;
  }
  if (first_event >= last_event)
  {
    return true;
  }
  return false;
}

void SingleZdcTriggerInput::CreateDSTNode(PHCompositeNode *topNode)
{
  PHNodeIterator iter(topNode);
  PHCompositeNode *dstNode = dynamic_cast<PHCompositeNode *>(iter.findFirst("PHCompositeNode", "DST"));
  if (!dstNode)
  {
    dstNode = new PHCompositeNode("DST");
    topNode->addNode(dstNode);
  }
  PHNodeIterator iterDst(dstNode);
  PHCompositeNode *detNode = dynamic_cast<PHCompositeNode *>(iterDst.findFirst("PHCompositeNode", "ZDC"));
  if (!detNode)
  {
    detNode = new PHCompositeNode("ZDC");
    dstNode->addNode(detNode);
  }
  CaloPacketContainer *zdcpacketcont = findNode::getClass<CaloPacketContainer>(detNode, "ZDCPackets");
  if (!zdcpacketcont)
  {
    zdcpacketcont = new CaloPacketContainerv1();
    PHIODataNode<PHObject> *newNode = new PHIODataNode<PHObject>(zdcpacketcont, "ZDCPackets", "PHObject");
    detNode->addNode(newNode);
  }
  detNode = dynamic_cast<PHCompositeNode *>(iterDst.findFirst("PHCompositeNode", "SEPD"));
  if (!detNode)
  {
    detNode = new PHCompositeNode("SEPD");
    dstNode->addNode(detNode);
  }
  CaloPacketContainer *sepdpacketcont = findNode::getClass<CaloPacketContainer>(detNode, "SEPDPackets");
  if (!sepdpacketcont)
  {
    sepdpacketcont = new CaloPacketContainerv1();
    PHIODataNode<PHObject> *newNode = new PHIODataNode<PHObject>(sepdpacketcont, "SEPDPackets", "PHObject");
    detNode->addNode(newNode);
  }
}

void SingleZdcTriggerInput::AdjustPacketMap(int pktid, int evtoffset)
{
  std::cout << PHWHERE << " adjusting local zdc packet map for packet " << pktid
	    << " with offset " << evtoffset << std::endl;
  std::vector<int> eventnumbers;
  for (auto packetmapiter =  m_ZdcPacketMap.rbegin(); packetmapiter != m_ZdcPacketMap.rend(); ++packetmapiter )
  {
      eventnumbers.push_back(packetmapiter->first);
  }

    for (auto evtnumiter : eventnumbers)
    {
    int lastevent = evtnumiter;
    int newevent = lastevent + evtoffset;
//    for (auto pktiter : m_ZdcPacketMap[lastevent])
    for (std::vector<OfflinePacket *>::iterator  pktiter =  m_ZdcPacketMap[lastevent].begin(); pktiter != m_ZdcPacketMap[lastevent].end(); ++pktiter )
    {
      if ((*pktiter)->getIdentifier() == pktid)
      {
	std::cout << PHWHERE << " need to move packet " << (*pktiter)->getIdentifier() << std::endl;
	m_ZdcPacketMap[newevent].push_back(std::move(*pktiter));
	m_ZdcPacketMap[lastevent].erase(pktiter);
//	std::move
	break;
      }
    }
  }
}
