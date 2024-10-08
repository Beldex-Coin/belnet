#pragma once

#include <optional>
#include <string>
#include <vector>
#include <llarp/util/formattable.hpp>
#include "ip_range.hpp"

namespace llarp::net
{
  /// info about a network interface belnet does not own
  struct InterfaceInfo
  {
    /// human readable name of interface
    std::string name;
    /// interface's index
    int index;
    /// the addresses owned by this interface
    std::vector<IPRange> addrs;
    /// a gateway we can use if it exists
    std::optional<ipaddr_t> gateway;

    std::string
    ToString() const;
  };
}  // namespace llarp::net

template <>
inline constexpr bool llarp::IsToStringFormattable<llarp::net::InterfaceInfo> = true;