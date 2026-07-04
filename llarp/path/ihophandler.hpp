#pragma once

#include <llarp/crypto/types.hpp>
#include <llarp/util/types.hpp>
#include <llarp/crypto/encrypted_frame.hpp>
#include <llarp/util/decaying_hashset.hpp>
#include <llarp/messages/relay.hpp>
#include <vector>

#include <atomic>
#include <list>
#include <memory>
#include <utility>

struct llarp_buffer_t;

namespace llarp
{
  struct AbstractRouter;

  namespace routing
  {
    struct IMessage;
  }

  namespace path
  {
    /// a queued traffic event: payload + nonce, stamped with its enqueue
    /// time so stale packets can be dropped at dequeue (anti-bufferbloat)
    struct TrafficEvent_t : public std::pair<std::vector<byte_t>, TunnelNonce>
    {
      /// when this event was enqueued; 0s means unknown (treated as fresh)
      llarp_time_t queuedAt = 0s;
    };

    /// return true if a traffic event has been sitting in a queue for longer
    /// than maxAge as of now
    inline bool
    TrafficEventIsStale(const TrafficEvent_t& ev, llarp_time_t now, llarp_time_t maxAge)
    {
      return ev.queuedAt > 0s and now > ev.queuedAt and now - ev.queuedAt > maxAge;
    }

    struct IHopHandler
    {
      using TrafficEvent_t = path::TrafficEvent_t;
      using TrafficQueue_t = std::list<TrafficEvent_t>;

      virtual ~IHopHandler() = default;

      virtual PathID_t
      RXID() const = 0;

      void
      DecayFilters(llarp_time_t now);

      virtual bool
      Expired(llarp_time_t now) const = 0;

      virtual bool
      ExpiresSoon(llarp_time_t now, llarp_time_t dlt) const = 0;

      /// send routing message and increment sequence number
      virtual bool
      SendRoutingMessage(const routing::IMessage& msg, AbstractRouter* r) = 0;

      // handle data in upstream direction
      virtual bool
      HandleUpstream(const llarp_buffer_t& X, const TunnelNonce& Y, AbstractRouter*);
      // handle data in downstream direction
      virtual bool
      HandleDownstream(const llarp_buffer_t& X, const TunnelNonce& Y, AbstractRouter*);

      /// return timestamp last remote activity happened at
      virtual llarp_time_t
      LastRemoteActivityAt() const = 0;

      virtual bool
      HandleLRSM(uint64_t status, std::array<EncryptedFrame, 8>& frames, AbstractRouter* r) = 0;

      uint64_t
      NextSeqNo()
      {
        return m_SequenceNum++;
      }

      virtual void
      FlushUpstream(AbstractRouter* r) = 0;

      virtual void
      FlushDownstream(AbstractRouter* r) = 0;

      /// total number of traffic events dropped by queue management
      uint64_t
      QueueDrops() const
      {
        return m_QueueDrops.load();
      }

     protected:
      uint64_t m_SequenceNum = 0;
      TrafficQueue_t m_UpstreamQueue;
      TrafficQueue_t m_DownstreamQueue;
      util::DecayingHashSet<TunnelNonce> m_UpstreamReplayFilter;
      util::DecayingHashSet<TunnelNonce> m_DownstreamReplayFilter;
      /// count of events dropped for being stale or overflowing the queue
      std::atomic<uint64_t> m_QueueDrops{0};
      /// last time we logged about queue drops (rate limited)
      std::atomic<int64_t> m_LastDropWarnAt{0};

      /// drop-from-head when a queue exceeds its bound; increments the drop
      /// counter. call before emplacing a new event.
      void
      EnforceQueueBound(TrafficQueue_t& queue);

      /// return true (and account for it) if this event is too old to be
      /// worth processing; logs a rate-limited warning at most every 5s
      bool
      DropStale(const TrafficEvent_t& ev, llarp_time_t now);

      virtual void
      UpstreamWork(TrafficQueue_t queue, AbstractRouter* r) = 0;

      virtual void
      DownstreamWork(TrafficQueue_t queue, AbstractRouter* r) = 0;

      virtual void
      HandleAllUpstream(std::vector<RelayUpstreamMessage> msgs, AbstractRouter* r) = 0;
      virtual void
      HandleAllDownstream(std::vector<RelayDownstreamMessage> msgs, AbstractRouter* r) = 0;
    };

    using HopHandler_ptr = std::shared_ptr<IHopHandler>;
  }  // namespace path
}  // namespace llarp
