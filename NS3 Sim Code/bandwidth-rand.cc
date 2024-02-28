#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/simulator.h"
#include "ns3/traffic-control-module.h"
#include "ns3/udp-echo-helper.h"
#include "ns3/wifi-helper.h"
#include "ns3/wifi-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/yans-wifi-phy.h"
#include "ns3/bridge-module.h"


using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiNetworkWithTrafficControlAndPcap");

int
main(int argc, char* argv[])
{
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    

    // Create WiFi nodes
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(2); // 4 devices
    NodeContainer wifiApNode;
    wifiApNode.Create(1); // WiFi router

    // Set up WiFi channel and PHY
    YansWifiChannelHelper channel;
    channel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    channel.AddPropagationLoss("ns3::FriisPropagationLossModel");
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    WifiMacHelper macSta;
    WifiMacHelper macAp;
    Ssid ssid = Ssid("ns-3-WiFi");

    macSta.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
    macAp.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));

    // Configure WiFi
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);

    NetDeviceContainer staDevices = wifi.Install(phy, macSta, wifiStaNodes);
    NetDeviceContainer apDevices = wifi.Install(phy, macAp, wifiApNode);

    phy.EnablePcap("rand", staDevices);


    
    ///////////////////////// Bandwidth Random Assign /////////////////////////

    // List of valid 802.11b rates
    std::vector<std::string> validRates = {"DsssRate1Mbps",
                                           "DsssRate2Mbps",
                                           "DsssRate5_5Mbps",
                                           "DsssRate11Mbps"};

    Ptr<UniformRandomVariable> randomRateIndex = CreateObject<UniformRandomVariable>();
    randomRateIndex->SetAttribute("Min", DoubleValue(0));
    randomRateIndex->SetAttribute("Max", DoubleValue(validRates.size() - 1));

    for (uint32_t i = 0; i < wifiStaNodes.GetN(); ++i)
    {
        int index = static_cast<int>(randomRateIndex->GetValue());
        wifi.SetRemoteStationManager(
            "ns3::ConstantRateWifiManager",
            "DataMode",
            StringValue(validRates[index]),
            "ControlMode",
            StringValue(validRates[0])); // Use the lowest rate for control packets
        // Reinstall the PHY and MAC layers for each station node with updated configurations
        staDevices = wifi.Install(phy, macSta, wifiStaNodes.Get(i));
    }

    ////////////////////////////////////// Mobility //////////////////////////////////////

    // Set mobility
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX",
                                  DoubleValue(0.0),
                                  "MinY",
                                  DoubleValue(0.0),
                                  "DeltaX",
                                  DoubleValue(5.0),
                                  "DeltaY",
                                  DoubleValue(5.0),
                                  "GridWidth",
                                  UintegerValue(3),
                                  "LayoutType",
                                  StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiStaNodes);
    mobility.Install(wifiApNode);

    //////////////////////////////////// IP assign ////////////////////////////////////////

    // Manually assign IP addresses to each device
    InternetStackHelper stack;
    stack.Install(wifiApNode);
    stack.Install(wifiStaNodes);

    Ipv4AddressHelper address;
    address.SetBase("192.168.239.0", "255.255.255.0");

    Ipv4InterfaceContainer apInterfaces = address.Assign(apDevices);
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);
    

    //////////////////////////////////////////////////////////////////////////////////////////

    //////////////////////////////////// Traffic Assign ////////////////////////////////////////

    uint16_t port = 9;
    UdpEchoClientHelper echoClient(apInterfaces.GetAddress(0), port);

    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(wifiApNode.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(900.0));

    // Configure the first client (Laptop) with moderate traffic
    echoClient.SetAttribute("Interval", TimeValue(Seconds(5))); // Send a packet every 0.5 seconds
    echoClient.SetAttribute("PacketSize", UintegerValue(512));
    ApplicationContainer clientAppsLaptop = echoClient.Install(wifiStaNodes.Get(0));
    clientAppsLaptop.Start(Seconds(200.0));
    clientAppsLaptop.Stop(Seconds(300.0));

    // Configure the second client (Smart TV) with high traffic for video streaming
    echoClient.SetAttribute("Interval", TimeValue(Seconds(0.1)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientAppsSmartTV = echoClient.Install(wifiStaNodes.Get(1));
    clientAppsSmartTV.Start(Seconds(2.5));
    clientAppsSmartTV.Stop(Seconds(100.0));
/*
    // Configure the third client (iPad) also with high traffic for video streaming
    echoClient.SetAttribute("Interval", TimeValue(Seconds(0.1)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientAppsiPad = echoClient.Install(wifiStaNodes.Get(2));
    clientAppsiPad.Start(Seconds(3.0));
    clientAppsiPad.Stop(Seconds(100.0));

    // Configure the fourth client (Cell Phone) with low traffic
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(256));
    ApplicationContainer clientAppsCellPhone = echoClient.Install(wifiStaNodes.Get(3));
    clientAppsCellPhone.Start(Seconds(3.5));
    clientAppsCellPhone.Stop(Seconds(100.0));
*/
    
    
    //////////////////////////////////////////////////////////////////////////////////////

    // Enable PCAP tracing
    phy.EnablePcap("rand", staDevices);


    // Run simulation
    Simulator::Stop(Seconds(900.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
