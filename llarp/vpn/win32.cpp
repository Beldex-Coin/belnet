#include "vpn/win32.hpp"

namespace llarp::win32
{

  void
  VPNPlatform::Route(std::string ip, std::string gw, std::string cmd)
  {
    llarp::win32::Exec(
        "route.exe", fmt::format("{} {} MASK 255.255.255.255 {} METRIC {}", cmd, ip, gw, m_Metric));
  }

  void
  VPNPlatform::DefaultRouteViaInterface(NetworkInterface& vpn, std::string cmd)
  {
    // route hole for loopback bacause god is dead on windows
    llarp::win32::Exec(
        "route.exe", fmt::format("{} 127.0.0.0 MASK 255.0.0.0 0.0.0.0 METRIC {}", cmd, m_Metric));
    // set up ipv4 routes
    auto lower = RouteViaInterface(vpn, "0.0.0.0", "128.0.0.0", cmd);
    auto upper = RouteViaInterface(vpn, "128.0.0.0", "128.0.0.0", cmd);
  }

  OneShotExec
  VPNPlatform::RouteViaInterface(
      NetworkInterface& vpn, std::string addr, std::string mask, std::string cmd)
  {
    const auto& info = vpn.Info();
    auto index = info.index;
    if (index == 0)
    {
      if (auto maybe_idx = net::Platform::Default_ptr()->GetInterfaceIndex(info[0]))
        index = *maybe_idx;
    }

    auto ifaddr = ip_to_string(info[0]);
    // this changes the last 1 to a 0 so that it routes over the interface
    // this is required because windows is idiotic af
    ifaddr.back()--;
    if (index)
    {
      return OneShotExec{
          "route.exe",
          fmt::format(
              "{} {} MASK {} {} IF {} METRIC {}", cmd, addr, mask, ifaddr, info.index, m_Metric)};
    }
    else
    {
      return OneShotExec{
          "route.exe",
          fmt::format("{} {} MASK {} {} METRIC {}", cmd, addr, mask, ifaddr, m_Metric)};
    }
  }

  void
  VPNPlatform::AddRoute(net::ipaddr_t ip, net::ipaddr_t gateway)
  {
    Route(ip_to_string(ip), ip_to_string(gateway), "ADD");
  }

  void
  VPNPlatform::DelRoute(net::ipaddr_t ip, net::ipaddr_t gateway)
  {
    Route(ip_to_string(ip), ip_to_string(gateway), "DELETE");
  }

  void
  VPNPlatform::AddRouteViaInterface(NetworkInterface& vpn, IPRange range)
  {
    RouteViaInterface(vpn, range.BaseAddressString(), range.NetmaskString(), "ADD");
  }

  void
  VPNPlatform::DelRouteViaInterface(NetworkInterface& vpn, IPRange range)
  {
    RouteViaInterface(vpn, range.BaseAddressString(), range.NetmaskString(), "DELETE");
  }

  std::vector<net::ipaddr_t>
  VPNPlatform::GetGatewaysNotOnInterface(NetworkInterface& vpn)
  {
    std::vector<net::ipaddr_t> gateways;

    auto idx = vpn.Info().index;
    using UInt_t = decltype(idx);
    for (const auto& iface : Net().AllNetworkInterfaces())
    {
      if (static_cast<UInt_t>(iface.index) == idx)
        continue;
      if (iface.gateway)
        gateways.emplace_back(*iface.gateway);
    }
    return gateways;
  }

  void
  VPNPlatform::AddDefaultRouteViaInterface(NetworkInterface& vpn)
  {
    // kill ipv6
    llarp::win32::Exec(
        "WindowsPowerShell\\v1.0\\powershell.exe",
        "-Command (Disable-NetAdapterBinding -Name \"* \" -ComponentID ms_tcpip6)");

    DefaultRouteViaInterface(vpn, "ADD");
    llarp::win32::Exec("ipconfig.exe", "/flushdns");
  }

  void
  VPNPlatform::DelDefaultRouteViaInterface(NetworkInterface& vpn)
  {
    // restore ipv6
    llarp::win32::Exec(
        "WindowsPowerShell\\v1.0\\powershell.exe",
        "-Command (Enable-NetAdapterBinding -Name \"* \" -ComponentID ms_tcpip6)");

    DefaultRouteViaInterface(vpn, "DELETE");
    llarp::win32::Exec("netsh.exe", "winsock reset");
    llarp::win32::Exec("ipconfig.exe", "/flushdns");
  }

  std::shared_ptr<NetworkInterface>
  VPNPlatform::ObtainInterface(InterfaceInfo info, AbstractRouter* router)
  {
    return wintun::make_interface(std::move(info), router);
  }

  std::shared_ptr<I_Packet_IO>
  VPNPlatform::create_packet_io(unsigned int ifindex)
  {
    // we only want do this on all interfaes with windivert
    if (ifindex)
      throw std::invalid_argument{
          "cannot create packet io on explicitly specified interface, not currently supported on "
          "windows (yet)"};

    std::string filter{"outbound and ( udp.DstPort == 53 or tcp.DstPort == 53 )"};

    if (auto dscp = _ctx->router->GetConfig()->dns.m_queryDSCP.value_or(0))
    {
      // DSCP is the first 6 bits of the TOS field (the last 2 are ECN).
      auto tos = dscp << 2;
      fmt::format_to(std::back_inserter(filter), " and ip.TOS != {}", tos);
    }
    return WinDivert::make_interceptor(filter, [router = _ctx->router] { router->TriggerPump(); });
  }

};

}  // namespace llarp::win32