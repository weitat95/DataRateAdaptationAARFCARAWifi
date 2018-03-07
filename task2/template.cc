/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
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
 */
#include <iostream>
#include <fstream>
#include "ns3/core-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/random-variable-stream.h"
#include <sstream>

using namespace ns3;
NS_LOG_COMPONENT_DEFINE ("AssignmentTemplate");


void sendOnOff( Ipv4Address rxIpv4Addr, Ptr<Node> rxNode ,uint16_t port , Ptr<Node> txNode, double startTime){

  OnOffHelper onoff ("ns3::UdpSocketFactory", Address ()); //create a new on-off application to send data
  std::string dataRate = "20Mib/s"; //data rate set as a string, see documentation for accepted units
  onoff.SetConstantRate(dataRate, (uint32_t)1024); //set the onoff client application to CBR mode
  AddressValue remoteAddress (InetSocketAddress( rxIpv4Addr, port));
  onoff.SetAttribute ("Remote", remoteAddress);
  ApplicationContainer apps = onoff.Install (txNode);
  apps.Start(Seconds(startTime));
  apps.Stop (Seconds(10.0));
  PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress (rxIpv4Addr, port));
  apps.Add (sink.Install(rxNode));

}

void showPosition (Ptr<Node> node, double deltaTime)
{
  uint32_t nodeId = node->GetId ();
  Ptr<MobilityModel> mobModel = node->GetObject<MobilityModel> ();
  Vector3D pos = mobModel->GetPosition ();
  Vector3D speed = mobModel->GetVelocity ();
  std::cout << "At " << Simulator::Now ().GetSeconds () << " node " << nodeId
            << ": Position(" << pos.x << ", " << pos.y << ", " << pos.z
            << ");   Speed(" << speed.x << ", " << speed.y << ", " << speed.z
            << ")" << std::endl;

  Simulator::Schedule (Seconds (deltaTime), &showPosition, node, deltaTime);
}

std::string
FlowOutput(Ptr<FlowMonitor> flowmon, FlowMonitorHelper &flowmonHelper)
{
  double finalThroughput=0.0;
  std::ostringstream oss;
  flowmon->CheckForLostPackets (); //check all packets have been sent or completely lost
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmonHelper.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = flowmon->GetFlowStats (); // pull stats from flow monitor

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator iter = stats.begin (); iter != stats.end (); ++iter)
  {//iterate through collected flow stats and find the traffic from node 1 to node 0
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (iter->first);
    std::cout << "Flow " << iter->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
    std::cout << "  Tx Packets: " << iter->second.txPackets << "\n";
    std::cout << "  Tx Bytes:   " << iter->second.txBytes << "\n";
    std::cout << "  TxOffered:  " << iter->second.txBytes * 8.0 / 9.0 / 1000 / 1000  << " Mbps\n";
    std::cout << "  Rx Packets: " << iter->second.rxPackets << "\n";
    std::cout << "  Rx Bytes:   " << iter->second.rxBytes << "\n";
    std::cout << "  Throughput: " << iter->second.rxBytes * 8.0 / 9.0 / 1000 / 1000  << " Mbps\n\n";

    finalThroughput += iter->second.rxBytes * 8.0/9.0/1000/1000;

  }
  oss << finalThroughput;
  return oss.str();
}

int
main (int argc, char *argv[])
{
  bool verbose = true;
  bool rayleigh = false;
  int seed= 4;
  double distance = 5.0;
  bool cara =false;
  int nodeNum = 1;
  std::string fileName = "default.txt";
  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell echo applications to log if true", verbose); //parameter name, description, variable that will take the value read from the command line
  cmd.AddValue ("seed", "SEED",seed);
  cmd.AddValue ("file", "Output to file",fileName);
  cmd.AddValue ("distance", "distance between node and AP",distance);
  cmd.AddValue ("cara","activate CARA",cara);
  cmd.AddValue ("rayleigh","activate rayleigh",rayleigh);
  cmd.AddValue ("nodeNum","number of station nodes",nodeNum);
  cmd.Parse (argc,argv);

  if (verbose)
    {
      LogComponentEnable ("PacketSink", LOG_LEVEL_INFO); //packet sink should write all actions to the command line
    }
  SeedManager::SetSeed (seed);//seed number should change between runs if running multiple simulations.

  NodeContainer wifiStaNodes; //create AP Node and (one or more) Station node(s)
  wifiStaNodes.Create (nodeNum);
  NodeContainer wifiApNode;
  wifiApNode.Create(1);

  YansWifiChannelHelper channel; //create helpers for the channel and phy layer and set propagation configuration here.
  channel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  channel.AddPropagationLoss("ns3::LogDistancePropagationLossModel");
  if(rayleigh){
    channel.AddPropagationLoss("ns3::NakagamiPropagationLossModel",
	                           "m0", DoubleValue(1.0), "m1", DoubleValue(1.0), "m2", DoubleValue(1.0)); //combining log and nakagami to have both distance and rayleigh fading (nakami with m0, m1 and m2 =1 is rayleigh)
  }

  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiHelper wifi;  //create helper for the overall wifi setup and configure a station manager
  wifi.SetStandard(ns3::WIFI_PHY_STANDARD_80211g);
  if(cara){
    wifi.SetRemoteStationManager ("ns3::CaraWifiManager");
  }else{
    wifi.SetRemoteStationManager ("ns3::AarfWifiManager");
  }
  NqosWifiMacHelper mac = NqosWifiMacHelper::Default (); //create a mac helper and configure for station and AP and install

  Ssid ssid = Ssid ("example-ssid");
  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install (phy, mac, wifiStaNodes);

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));

  NetDeviceContainer apDevices;
  apDevices = wifi.Install (phy, mac, wifiApNode);
// Mobility Helper for AP (Constant Position at (0,0))
  MobilityHelper mobilityAp;
  mobilityAp.SetPositionAllocator ("ns3::RandomDiscPositionAllocator",
                                  "Theta", StringValue ("ns3::ConstantRandomVariable[Constant=0.0]"),
                                  "Rho", StringValue ("ns3::ConstantRandomVariable[Constant=0.0]"));

  mobilityAp.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobilityAp.Install (wifiApNode);

// Mobility Helper for Stations (Random position over circumference of a 10m radius circle)
  MobilityHelper mobilitySta;
  mobilitySta.SetPositionAllocator ("ns3::RandomDiscPositionAllocator",
                                  "Rho", StringValue ("ns3::ConstantRandomVariable[Constant=10.0]"));
  mobilitySta.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobilitySta.Install (wifiStaNodes);
//Debugging
/*  Ptr<MobilityModel> mobModel = wifiApNode.Get(0)->GetObject<MobilityModel>();
  Vector3D pos = mobModel->GetPosition();
  NS_LOG_UNCOND("AP: Position(" << pos.x << ", "
              << pos.y << ", " << pos.z << ");" << std::endl );
  mobModel = wifiStaNodes.Get(0)->GetObject<MobilityModel>();
  pos = mobModel->GetPosition();
  NS_LOG_UNCOND("Station: Position(" << pos.x << ", "
              << pos.y << ", " << pos.z << ");" << std::endl );
*/

  InternetStackHelper stack; //install the internet stack on both nodes
  stack.Install (wifiApNode);
  stack.Install (wifiStaNodes);

  Ipv4AddressHelper address; //assign IP addresses to all nodes
  address.SetBase ("10.1.1.0", "255.255.255.0");

  Ipv4InterfaceContainer apAddress = address.Assign (apDevices); //we need to keep AP address accessible as we need it later

  Ipv4InterfaceContainer staAddress = address.Assign (staDevices);

  std::cout << "ApAdress: ";
  apAddress.GetAddress(0).Print(std::cout);
  std::cout << std::endl;

  Ptr<UniformRandomVariable> var = CreateObject<UniformRandomVariable> ();

  NodeContainer::Iterator i;
  for (i = wifiStaNodes.Begin (); i != wifiStaNodes.End (); ++i)
  {
    uint32_t staId = (*i)->GetId();
    //std::cout << "Station " << staId << ": " ;
    //staAddress.GetAddress(staId).Print(std::cout);

    //Randomly select who to tx and who to rx between AP and Station
    if(var->GetValue(0,1.0)>0.5){ //Ap Tx , Station Rx
      sendOnOff(staAddress.GetAddress(staId),wifiStaNodes.Get(staId),
                8000, wifiApNode.Get(0), var->GetValue(0,0.1));
      //std::cout<< " , RX";
    }else{ //Ap Rx, Station Tx
      sendOnOff(apAddress.GetAddress(0),wifiApNode.Get(0),
                8000, wifiStaNodes.Get(staId), var->GetValue(0,0.1));
      //std::cout<< " , TX";
    }
    //std::cout << std::endl;
    //Simulator::Schedule (Seconds(0.0), &showPosition, wifiStaNodes.Get(staId),1.0);
  }


  //sendOnOff(rxIpv4Addr, rxNode, port, txNode , startTime)
  // Station 0 to Ap
  //sendOnOff(apAddress.GetAddress(0), wifiApNode.Get(0),
  //          8000, wifiStaNodes.Get(0), var->GetValue(0,0.1));
  // Ap to Station 0
  //sendOnOff(staAddress.GetAddress(0),wifiStaNodes.Get(0),
  //          8000, wifiApNode.Get(0), var->GetValue(0,0.1));


  Simulator::Stop (Seconds (10.0)); //define stop time of simulator

  Ptr<FlowMonitor> flowmon; //create an install a flow monitor to monitor all transmissions around the network
  FlowMonitorHelper flowmonHelper;
  flowmon = flowmonHelper.InstallAll();

  Simulator::Run (); //run the simulation and destroy it once done
  Simulator::Destroy ();

  if(seed==1&&nodeNum==1){
    if(rayleigh){
      NS_LOG_UNCOND("Channel Fading: Rayleigh, ");
    }else{
      NS_LOG_UNCOND("Channel Fading: None, ");
    }
    if(cara){
      NS_LOG_UNCOND("Rate Adaptation Algorithm: CARA");
    }else{
      NS_LOG_UNCOND("Rate Adaptation Algorithm: AARF");
    }
  }
  std::string str = FlowOutput(flowmon, flowmonHelper);
  std::ofstream myfile;
  myfile.open (fileName,std::ios::app);
  //Pretty Print
  if(seed==1){
    //myfile << seed<<", ";
    myfile <<"\n" << nodeNum << ", ";
  }
  myfile << str;
  if(seed!=5){
    myfile<< ", ";
  }
  if(nodeNum==46 &&seed==5){
    myfile<<"\n";
  }
  myfile.close();

  return 0;
}
