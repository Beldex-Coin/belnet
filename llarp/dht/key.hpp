#pragma once

#include <llarp/util/aligned.hpp>
#include <llarp/router_id.hpp>
#include <llarp/util/formattable.hpp>

#include <array>

namespace llarp
{
  namespace dht
  {
    struct Key_t : public AlignedBuffer<32>
    {
      explicit Key_t(const byte_t* buf) : AlignedBuffer<SIZE>(buf)
      {}

      explicit Key_t(const Data& data) : AlignedBuffer<SIZE>(data)
      {}

      explicit Key_t(const AlignedBuffer<SIZE>& data) : AlignedBuffer<SIZE>(data)
      {}

      Key_t() : AlignedBuffer<SIZE>()
      {}

      /// get mnode address string
      std::string
      MNode() const
      {
        const RouterID rid{as_array()};
        return rid.ToString();
      }

      util::StatusObject
      ExtractStatus() const;

      std::string
      ToString() const
      {
        return MNode();
      }

      Key_t
      operator^(const Key_t& other) const
      {
        Key_t dist;
        std::transform(begin(), end(), other.begin(), dist.begin(), std::bit_xor<byte_t>());
        return dist;
      }

      bool
      operator==(const Key_t& other) const
      {
        return as_array() == other.as_array();
      }

      bool
      operator!=(const Key_t& other) const
      {
        return as_array() != other.as_array();
      }

      bool
      operator<(const Key_t& other) const
      {
        return as_array() < other.as_array();
      }

      bool
      operator>(const Key_t& other) const
      {
        return as_array() > other.as_array();
      }
    };
  }  // namespace dht
}  // namespace llarp
