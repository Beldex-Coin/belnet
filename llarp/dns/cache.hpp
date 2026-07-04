#pragma once

#include "message.hpp"
#include "question.hpp"

#include <llarp/util/types.hpp>

#include <atomic>
#include <list>
#include <map>
#include <mutex>
#include <optional>
#include <utility>

namespace llarp::dns
{
  /// a TTL-respecting LRU cache of upstream DNS answers keyed by
  /// (lowercased qname, qtype). answering repeated queries locally avoids a
  /// full onion round trip per query (audit finding D7).
  class QueryCache
  {
   public:
    /// maximum number of cached answers
    static constexpr size_t MaxEntries = 4096;
    /// floor for how long we keep an answer, even if its records say less
    static constexpr auto MinCacheTTL = 5s;
    /// ceiling for how long we keep an answer
    static constexpr auto MaxCacheTTL = 1h;
    /// how long we cache negative answers (NXDOMAIN / NODATA / errors)
    static constexpr auto NegativeCacheTTL = 30s;

    /// look up a cached answer for this question. on a hit, returns a copy
    /// of the cached message with answer TTLs wound down by the elapsed
    /// time; the caller is responsible for patching the txid (hdr_id) to
    /// match the incoming query.
    std::optional<Message>
    Get(const Question& q, llarp_time_t now);

    /// store an upstream answer for this question. no-op for non-IN
    /// classes and truncated responses.
    void
    Put(const Question& q, const Message& answer, llarp_time_t now);

    uint64_t
    Hits() const
    {
      return m_Hits.load();
    }

    uint64_t
    Misses() const
    {
      return m_Misses.load();
    }

    size_t
    Size() const;

   private:
    using Key_t = std::pair<std::string, QType_t>;

    struct Entry
    {
      Message msg;
      llarp_time_t storedAt;
      llarp_time_t expiresAt;
      std::list<Key_t>::iterator lruIt;

      Entry(
          Message m, llarp_time_t stored, llarp_time_t expires, std::list<Key_t>::iterator lru)
          : msg{std::move(m)}, storedAt{stored}, expiresAt{expires}, lruIt{lru}
      {}
    };

    static Key_t
    MakeKey(const Question& q);

    mutable std::mutex m_Mutex;
    std::map<Key_t, Entry> m_Cache;
    /// front = most recently used
    std::list<Key_t> m_LRU;
    std::atomic<uint64_t> m_Hits{0};
    std::atomic<uint64_t> m_Misses{0};
  };
}  // namespace llarp::dns
