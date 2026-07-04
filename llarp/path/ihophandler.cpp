#include "ihophandler.hpp"
#include <llarp/constants/path.hpp>
#include <llarp/router/abstractrouter.hpp>
#include <llarp/util/logging.hpp>

namespace llarp
{
  namespace path
  {
    void
    IHopHandler::EnforceQueueBound(TrafficQueue_t& queue)
    {
      // drop from the FRONT (oldest) so newer traffic gets through; the
      // oldest packets are the ones most likely to be useless by now
      while (queue.size() >= transit_hop_queue_size)
      {
        queue.pop_front();
        ++m_QueueDrops;
      }
    }

    bool
    IHopHandler::DropStale(const TrafficEvent_t& ev, llarp_time_t now)
    {
      if (not TrafficEventIsStale(ev, now, transit_queue_max_age))
        return false;
      const auto drops = ++m_QueueDrops;
      auto last = m_LastDropWarnAt.load();
      if (now.count() - last >= 5000
          and m_LastDropWarnAt.compare_exchange_strong(last, now.count()))
      {
        LogWarn(
            "dropping stale queued traffic (age > ",
            ToString(transit_queue_max_age),
            "), ",
            drops,
            " drops total");
      }
      return true;
    }

    // handle data in upstream direction
    bool
    IHopHandler::HandleUpstream(const llarp_buffer_t& X, const TunnelNonce& Y, AbstractRouter* r)
    {
      EnforceQueueBound(m_UpstreamQueue);
      auto& pkt = m_UpstreamQueue.emplace_back();
      pkt.first.resize(X.sz);
      std::copy_n(X.base, X.sz, pkt.first.begin());
      pkt.second = Y;
      pkt.queuedAt = r->Now();
      r->TriggerPump();
      return true;
    }

    // handle data in downstream direction
    bool
    IHopHandler::HandleDownstream(const llarp_buffer_t& X, const TunnelNonce& Y, AbstractRouter* r)
    {
      EnforceQueueBound(m_DownstreamQueue);
      auto& pkt = m_DownstreamQueue.emplace_back();
      pkt.first.resize(X.sz);
      std::copy_n(X.base, X.sz, pkt.first.begin());
      pkt.second = Y;
      pkt.queuedAt = r->Now();
      r->TriggerPump();
      return true;
    }

    void
    IHopHandler::DecayFilters(llarp_time_t now)
    {
      m_UpstreamReplayFilter.Decay(now);
      m_DownstreamReplayFilter.Decay(now);
    }
  }  // namespace path
}  // namespace llarp
