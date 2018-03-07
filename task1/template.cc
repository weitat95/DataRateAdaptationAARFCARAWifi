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
  std::ostringstream oss;
  flowmon->CheckForLostPackets (); //check all packets have been sent or completely lost
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmonHelper.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = flowmon->GetFlowStats (); // pull stats from flow monitor

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator iter = stats.begin (); iter != stats.end (); ++iter)
  {//iterate through collected flow stats and find the traffic from node 1 to node 0
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (iter->first);
    if (t.sourceAddress == Ipv4Address("10.1.1.1") && t.destinationAddress == Ipv4Address("10.1.1.2")) //use the node addresses to select the flow (or flows) you want
    {
      oss//<<"Throughput in Kib/s over the run time: " //normal c++ output streams can be used to output this information to a file in place of the terminal
              //<< iter->second.rxBytes * 8.0/9.0/1000/1000 <<": "//bits per byte/run time over bits per Kib
                << iter->second.rxBytes * 8.0 / (10 * 1024);
      NS_LOG_UNCOND(oss.str()); //flowmonitor records a number of different statistics about each flow, however we are only interested in rxBytes

        //std::ofstream myfile;
        //myfile.open (fileName,std::ios::app);
        //myfile << "Writing this to a file." << iter->second.rxBytes * 8.0 / (10 * 1024) <<"\n";
        //myfile.close();
    }
  }
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
  std::string fileName = "default.txt";
  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell echo applications to log if true", verbose); //parameter name, description, variable that will take the value read from the command line
  cmd.AddValue ("seed", "SEED",seed);
  cmd.AddValue ("file", "Output to file",fileName);
  cmd.AddValue ("distance", "distance between node and AP",distance);
  cmd.AddValue ("cara","activate CARA",cara);
  cmd.AddValue ("rayleigh","activate rayleigh",rayleigh);
  cmd.Parse (argc,argv);

  if (verbose)
    {
      LogComponentEnable ("PacketSink", LOG_LEVEL_INFO); //packet sink should write all actions to the command line
    }

  SeedManager::SetSeed (seed);//seed number should change between runs if running multiple simulations.

  NodeContainer wifiStaNodes; //create AP Node and (one or more) Station node(s)
  wifiStaNodes.Create (1);
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

  MobilityHelper mobility;

  mobility.SetPositionAllocator ("ns3::GridPositionAllocator", //places nodes in a grid with distance between nodes and grid width defined below, for 2 nodes can be used to easily place them at a set distance apart
                                 "MinX", DoubleValue (0.0), //first node positioned at x = 0, y= 0
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (distance), //nodes will be place 5m apart in x plane
                                 "DeltaY", DoubleValue (0.0),
                                 "GridWidth", UintegerValue (3),
                                 "LayoutType", StringValue ("RowFirst"));


  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiApNode);
//Mobility of 1m/s (Not needed)
/*
  mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                             "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=1.0]"),
                             "Bounds", StringValue ("-100|100|-100|100"));
*/
  mobility.Install (wifiStaNodes);

  InternetStackHelper stack; //install the internet stack on both nodes
  stack.Install (wifiApNode);
  stack.Install (wifiStaNodes);

  Ipv4AddressHelper address; //assign IP addresses to all nodes
  address.SetBase ("10.1.1.0", "255.255.255.0");
  address.Assign (staDevices);
  Ipv4InterfaceContainer apAddress = address.Assign (apDevices); //we need to keep AP address accessible as we need it later

  OnOffHelper onoff ("ns3::UdpSocketFactory", Address ()); //create a new on-off application to send data
  std::string dataRate = "20Mib/s"; //data rate set as a string, see documentation for accepted units
  onoff.SetConstantRate(dataRate, (uint32_t)1024); //set the onoff client application to CBR mode

  AddressValue remoteAddress (InetSocketAddress (apAddress.GetAddress (0), 8000)); //specify address and port of the AP as the destination for on-off application's packets
  onoff.SetAttribute ("Remote", remoteAddress);
  ApplicationContainer apps = onoff.Install (wifiStaNodes.Get (0));//install onoff application on stanode 0 and configure start/stop times
  Ptr<UniformRandomVariable> var = CreateObject<UniformRandomVariable> ();
  apps.Start(Seconds(var->GetValue(0, 0.1)));
  apps.Stop (Seconds (10.0));

  PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress (apAddress.GetAddress (0), 8000)); //create packet sink on AP node with address and port that on-off app is sending to
  apps.Add(sink.Install(wifiApNode.Get(0)));

  Simulator::Stop (Seconds (10.0)); //define stop time of simulator

  Ptr<FlowMonitor> flowmon; //create an install a flow monitor to monitor all transmissions around the network
  FlowMonitorHelper flowmonHelper;
  flowmon = flowmonHelper.InstallAll();

  Simulator::Schedule (Seconds(0.0), &showPosition, wifiStaNodes.Get(0),1.0);
  Simulator::Run (); //run the simulation and destroy it once done
  Simulator::Destroy ();

  if(seed==1&&distance==5.0){
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
  //Pretty Printing
  if(seed==1){
    //myfile << seed<<", ";
    myfile <<"\n" << distance << ", ";
  }
  myfile << str;
  if(seed!=5){
    myfile<< ", ";
  }
  if(distance==100.0 &&seed==5){
    myfile<<"\n";
  }
  myfile.close();
  return 0;
}
