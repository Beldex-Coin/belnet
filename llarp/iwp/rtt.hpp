#pragma once

#include <llarp/util/types.hpp>

#include <algorithm>

namespace llarp
{
  namespace iwp
  {
    /// smoothed RTT estimator per RFC 6298, used to derive the fragment
    /// retransmission interval of a session instead of a hard-coded timer.
    /// this is sender-side scheduling only and makes no wire format changes.
    struct RttEstimator
    {
      /// lower bound of the derived resend interval
      static constexpr llarp_time_t MinResendInterval = 50ms;
      /// upper bound of the derived resend interval (the old fixed abandon
      /// timer, so behavior is never worse than before)
      static constexpr llarp_time_t MaxResendInterval = 500ms;
      /// upper bound of the exponentially backed off resend interval
      static constexpr llarp_time_t MaxBackoffInterval = 2s;

      llarp_time_t srtt = 0s;
      llarp_time_t rttvar = 0s;

      /// feed a new RTT sample. samples from retransmitted messages must be
      /// excluded by the caller passing retransmitted=true (Karn's rule).
      void
      Update(llarp_time_t sample, bool retransmitted = false)
      {
        if (retransmitted or sample < 0s)
          return;
        if (srtt == 0s)
        {
          // first sample
          srtt = sample;
          rttvar = sample / 2;
          return;
        }
        const auto delta = srtt > sample ? srtt - sample : sample - srtt;
        rttvar = (rttvar * 3 + delta) / 4;
        srtt = (srtt * 7 + sample) / 8;
      }

      /// base retransmission interval: clamp(srtt + 4 * rttvar, 50ms, 500ms).
      /// with no samples yet we stay at the conservative maximum.
      llarp_time_t
      ResendInterval() const
      {
        if (srtt == 0s)
          return MaxResendInterval;
        return std::clamp(srtt + rttvar * 4, MinResendInterval, MaxResendInterval);
      }

      /// resend interval doubled once per successive retransmission of the
      /// same fragment, capped at MaxBackoffInterval
      llarp_time_t
      BackedOffResendInterval(uint16_t retransmits) const
      {
        auto interval = ResendInterval();
        while (retransmits > 0 and interval < MaxBackoffInterval)
        {
          interval *= 2;
          --retransmits;
        }
        return std::min(interval, MaxBackoffInterval);
      }
    };
  }  // namespace iwp
}  // namespace llarp
