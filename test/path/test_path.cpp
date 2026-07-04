#include <path/path.hpp>
#include <catch2/catch.hpp>

using Path_t   = llarp::path::Path;
using Path_ptr = llarp::path::Path_ptr;
using Set_t    = llarp::path::Path::UniqueEndpointSet_t;
using RC_t     = llarp::RouterContact;

static RC_t
MakeHop(const char name)
{
  RC_t rc;
  rc.pubkey.Fill(name);
  return rc;
}

static Path_ptr
MakePath(std::vector< char > hops)
{
  std::vector< RC_t > pathHops;
  for(const auto& hop : hops)
    pathHops.push_back(MakeHop(hop));
  return std::make_shared< Path_t >(pathHops, std::weak_ptr<llarp::path::PathSet>{}, 0, "test");
}

TEST_CASE("UniqueEndpointSet_t has unique endpoints", "[path]")
{
  Set_t set;
  REQUIRE(set.empty());
  const auto inserted_first =
      set.emplace(MakePath({'a', 'b', 'c', 'd'})).second;
  REQUIRE(inserted_first);
  const auto inserted_again =
      set.emplace(MakePath({'a', 'b', 'c', 'd'})).second;
  REQUIRE(not inserted_again);
  const auto inserted_second =
      set.emplace(MakePath({'d', 'c', 'b', 'a'})).second;
  REQUIRE(inserted_second);
}

TEST_CASE("stale traffic events are detected and newest survive", "[path][aqm]")
{
  using namespace std::literals;
  using llarp::path::TrafficEventIsStale;
  using llarp::path::transit_queue_max_age;

  const llarp_time_t now = 10000ms;

  // build a queue with a mix of stale and fresh events
  llarp::path::IHopHandler::TrafficQueue_t queue;
  for (int i = 0; i < 8; ++i)
  {
    auto& ev = queue.emplace_back();
    // first half stale (older than max age), second half fresh
    ev.queuedAt = i < 4 ? now - transit_queue_max_age - 50ms : now - 10ms;
  }

  size_t stale = 0;
  size_t fresh = 0;
  for (const auto& ev : queue)
  {
    if (TrafficEventIsStale(ev, now, transit_queue_max_age))
      ++stale;
    else
      ++fresh;
  }
  CHECK(stale == 4);
  CHECK(fresh == 4);

  // events with unknown enqueue time are treated as fresh
  llarp::path::TrafficEvent_t unknown;
  CHECK(not TrafficEventIsStale(unknown, now, transit_queue_max_age));

  // an event exactly at the age limit is not yet stale
  llarp::path::TrafficEvent_t edge;
  edge.queuedAt = now - transit_queue_max_age;
  CHECK(not TrafficEventIsStale(edge, now, transit_queue_max_age));
}

TEST_CASE("LatencyStats EWMA and jitter math", "[path][latency]")
{
  using namespace std::literals;
  llarp::path::LatencyStats stats;
  CHECK(stats.ewma == 0s);
  CHECK(stats.jitter == 0s);

  // first sample initializes the ewma with zero jitter
  stats.AddSample(100ms);
  CHECK(stats.ewma == 100ms);
  CHECK(stats.jitter == 0s);
  CHECK(stats.Score() == 100ms);

  // steady samples converge and keep jitter at zero
  for (int i = 0; i < 50; ++i)
    stats.AddSample(100ms);
  CHECK(stats.ewma == 100ms);
  CHECK(stats.jitter == 0ms);

  // a spike moves the ewma by ~alpha (0.3) and produces jitter
  stats.AddSample(200ms);
  CHECK(stats.ewma == 130ms);   // 0.7*100 + 0.3*200
  CHECK(stats.jitter == 30ms);  // 0.7*0 + 0.3*|200-100|
  CHECK(stats.Score() == 130ms + 2 * 30ms);

  // negative garbage is ignored
  const auto before = stats.ewma;
  stats.AddSample(-5ms);
  CHECK(stats.ewma == before);
}

TEST_CASE("LatencyStats reacts to degradation faster than an 8-sample mean", "[path][latency]")
{
  using namespace std::literals;
  llarp::path::LatencyStats stats;
  for (int i = 0; i < 8; ++i)
    stats.AddSample(50ms);
  REQUIRE(stats.ewma == 50ms);

  // path degrades to 500ms; after 3 samples the ewma has crossed most of
  // the gap while a mean of the last 8 samples would still be under 220ms
  stats.AddSample(500ms);
  stats.AddSample(500ms);
  stats.AddSample(500ms);
  CHECK(stats.ewma > 330ms);
  CHECK(stats.jitter > 0ms);
}
