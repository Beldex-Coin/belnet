#include "cache.hpp"
#include "dns.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <limits>

namespace llarp::dns
{
  QueryCache::Key_t
  QueryCache::MakeKey(const Question& q)
  {
    std::string name = q.Name();
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char ch) {
      return std::tolower(ch);
    });
    return {std::move(name), q.qtype};
  }

  std::optional<Message>
  QueryCache::Get(const Question& q, llarp_time_t now)
  {
    std::lock_guard lock{m_Mutex};
    auto itr = m_Cache.find(MakeKey(q));
    if (itr == m_Cache.end())
    {
      ++m_Misses;
      return std::nullopt;
    }
    auto& entry = itr->second;
    if (now >= entry.expiresAt)
    {
      m_LRU.erase(entry.lruIt);
      m_Cache.erase(itr);
      ++m_Misses;
      return std::nullopt;
    }
    // move to the front of the LRU
    m_LRU.splice(m_LRU.begin(), m_LRU, entry.lruIt);
    ++m_Hits;

    Message msg{entry.msg};
    // wind answer TTLs down by the time the entry spent in the cache
    const auto elapsed = static_cast<RR_TTL_t>(
        std::chrono::duration_cast<std::chrono::seconds>(now - entry.storedAt).count());
    if (elapsed > 0)
    {
      for (auto& rr : msg.answers)
        rr.ttl = rr.ttl > elapsed ? rr.ttl - elapsed : 1;
    }
    return msg;
  }

  void
  QueryCache::Put(const Question& q, const Message& answer, llarp_time_t now)
  {
    // only cache IN-class questions
    if (q.qclass != qClassIN)
      return;
    // don't cache truncated responses
    if (answer.hdr_fields & (1 << 9))
      return;

    // negative answers (error rcode or no answer records) get a short
    // fixed lifetime; positive answers live for their minimum record TTL,
    // clamped to [MinCacheTTL, MaxCacheTTL]
    llarp_time_t lifetime;
    const auto rcode = answer.hdr_fields & 0xf;
    if (rcode != 0 or answer.answers.empty())
      lifetime = NegativeCacheTTL;
    else
    {
      RR_TTL_t minTTL = std::numeric_limits<RR_TTL_t>::max();
      for (const auto& rr : answer.answers)
        minTTL = std::min(minTTL, rr.ttl);
      lifetime = std::clamp(
          llarp_time_t{std::chrono::seconds{minTTL}},
          llarp_time_t{MinCacheTTL},
          llarp_time_t{MaxCacheTTL});
    }

    std::lock_guard lock{m_Mutex};
    auto key = MakeKey(q);
    // replace any existing entry for this key
    if (auto itr = m_Cache.find(key); itr != m_Cache.end())
    {
      m_LRU.erase(itr->second.lruIt);
      m_Cache.erase(itr);
    }
    // enforce the size cap by evicting the least recently used entries
    while (m_Cache.size() >= MaxEntries)
    {
      m_Cache.erase(m_LRU.back());
      m_LRU.pop_back();
    }
    m_LRU.push_front(key);
    m_Cache.emplace(
        std::move(key), Entry{Message{answer}, now, now + lifetime, m_LRU.begin()});
  }

  size_t
  QueryCache::Size() const
  {
    std::lock_guard lock{m_Mutex};
    return m_Cache.size();
  }
}  // namespace llarp::dns
