#include <catch2/catch.hpp>
#include "config/config.hpp"

#include <router_contact.hpp>
#include <nodedb.hpp>

using llarp_nodedb = llarp::NodeDB;

TEST_CASE("FindClosestTo returns correct number of elements", "[nodedb][dht]")
{
  llarp_nodedb nodeDB{fs::current_path(), nullptr};

  constexpr uint64_t numRCs = 3;
  for (uint64_t i = 0; i < numRCs; ++i)
  {
    llarp::RouterContact rc;
    rc.pubkey[0] = i;
    nodeDB.Put(rc);
  }

  REQUIRE(numRCs == nodeDB.NumLoaded());

  llarp::dht::Key_t key;

  std::vector<llarp::RouterContact> results = nodeDB.FindManyClosestTo(key, 4);

  // we asked for more entries than nodedb had
  REQUIRE(numRCs == results.size());
}

TEST_CASE("FindClosestTo returns properly ordered set", "[nodedb][dht]")
{
  llarp_nodedb nodeDB{fs::current_path(), nullptr};

  // insert some RCs: a < b < c
  llarp::RouterContact a;
  a.pubkey[0] = 1;
  nodeDB.Put(a);

  llarp::RouterContact b;
  b.pubkey[0] = 2;
  nodeDB.Put(b);

  llarp::RouterContact c;
  c.pubkey[0] = 3;
  nodeDB.Put(c);

  REQUIRE(3 == nodeDB.NumLoaded());

  llarp::dht::Key_t key;

  std::vector<llarp::RouterContact> results = nodeDB.FindManyClosestTo(key, 2);
  REQUIRE(2 == results.size());

  // we xor'ed with 0x0, so order should be a,b,c
  REQUIRE(a.pubkey == results[0].pubkey);
  REQUIRE(b.pubkey == results[1].pubkey);

  llarp::dht::Key_t compKey;
  compKey.Fill(0xFF);

  results = nodeDB.FindManyClosestTo(compKey, 2);

  // we xor'ed with 0xF...F, so order should be inverted (c,b,a)
  REQUIRE(c.pubkey == results[0].pubkey);
  REQUIRE(b.pubkey == results[1].pubkey);
}

TEST_CASE("GetWeightedRandom respects weights", "[nodedb]")
{
  llarp_nodedb nodeDB{fs::current_path(), nullptr};

  // two routers: one with 4x the weight of the other
  llarp::RouterContact heavy;
  heavy.pubkey[0] = 1;
  nodeDB.Put(heavy);

  llarp::RouterContact light;
  light.pubkey[0] = 2;
  nodeDB.Put(light);

  REQUIRE(2 == nodeDB.NumLoaded());

  auto acceptAll = [](const llarp::RouterContact&) { return true; };
  auto weightOf = [&](const llarp::RouterContact& rc) -> double {
    return rc.pubkey[0] == 1 ? 4.0 : 1.0;
  };

  constexpr int draws = 10000;
  int heavyPicked = 0;
  for (int i = 0; i < draws; ++i)
  {
    auto maybe = nodeDB.GetWeightedRandom(acceptAll, weightOf);
    REQUIRE(maybe.has_value());
    if (maybe->pubkey[0] == 1)
      ++heavyPicked;
  }
  // expected ratio is 4:1 => heavy picked ~80% of the time
  const double frac = double(heavyPicked) / draws;
  CHECK(frac > 0.75);
  CHECK(frac < 0.85);
}

TEST_CASE("GetWeightedRandom respects filter and empty db", "[nodedb]")
{
  llarp_nodedb nodeDB{fs::current_path(), nullptr};

  auto acceptAll = [](const llarp::RouterContact&) { return true; };
  auto weightOne = [](const llarp::RouterContact&) -> double { return 1.0; };

  // empty db yields nothing
  CHECK(not nodeDB.GetWeightedRandom(acceptAll, weightOne).has_value());

  llarp::RouterContact a;
  a.pubkey[0] = 1;
  nodeDB.Put(a);

  llarp::RouterContact b;
  b.pubkey[0] = 2;
  nodeDB.Put(b);

  // filter that rejects everything yields nothing
  auto rejectAll = [](const llarp::RouterContact&) { return false; };
  CHECK(not nodeDB.GetWeightedRandom(rejectAll, weightOne).has_value());

  // filter that only accepts b always yields b
  auto onlyB = [](const llarp::RouterContact& rc) { return rc.pubkey[0] == 2; };
  for (int i = 0; i < 100; ++i)
  {
    auto maybe = nodeDB.GetWeightedRandom(onlyB, weightOne);
    REQUIRE(maybe.has_value());
    CHECK(maybe->pubkey[0] == 2);
  }
}
