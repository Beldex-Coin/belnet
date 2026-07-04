#include <llarp/profiling.hpp>

#include <oxenc/bt_producer.h>
#include <oxenc/bt_serialize.h>

#include <catch2/catch.hpp>

using namespace std::literals;

TEST_CASE("RouterProfile latency fields round trip", "[profiling]")
{
  llarp::RouterProfile profile;
  profile.connectGoodCount = 5;
  profile.connectTimeoutCount = 1;
  profile.pathSuccessCount = 42;
  profile.pathFailCount = 3;
  profile.pathTimeoutCount = 2;
  profile.latencyAccum = 400ms;
  profile.latencySamples = 4;
  profile.lastUpdated = 123456ms;

  REQUIRE(profile.EstLatency() == 100ms);

  std::string buf;
  buf.resize(llarp::RouterProfile::MaxSize);
  oxenc::bt_dict_producer producer{buf.data(), buf.size()};
  profile.BEncode(producer);
  buf.resize(producer.end() - buf.data());

  llarp::RouterProfile decoded{oxenc::bt_dict_consumer{buf}};
  CHECK(decoded.connectGoodCount == profile.connectGoodCount);
  CHECK(decoded.connectTimeoutCount == profile.connectTimeoutCount);
  CHECK(decoded.pathSuccessCount == profile.pathSuccessCount);
  CHECK(decoded.pathFailCount == profile.pathFailCount);
  CHECK(decoded.pathTimeoutCount == profile.pathTimeoutCount);
  CHECK(decoded.latencyAccum == profile.latencyAccum);
  CHECK(decoded.latencySamples == profile.latencySamples);
  CHECK(decoded.lastUpdated == profile.lastUpdated);
  CHECK(decoded.EstLatency() == 100ms);
}

TEST_CASE("RouterProfile decodes old profiles without latency keys", "[profiling]")
{
  // encode with the pre-latency key set only, as older belnet versions do
  std::string buf;
  buf.resize(llarp::RouterProfile::MaxSize);
  oxenc::bt_dict_producer producer{buf.data(), buf.size()};
  producer.append("g", 7);
  producer.append("p", 11);
  producer.append("q", 0);
  producer.append("s", 1);
  producer.append("t", 0);
  producer.append("u", 98765);
  producer.append("v", 0);
  buf.resize(producer.end() - buf.data());

  llarp::RouterProfile decoded{oxenc::bt_dict_consumer{buf}};
  CHECK(decoded.connectGoodCount == 7);
  CHECK(decoded.pathSuccessCount == 11);
  CHECK(decoded.pathFailCount == 1);
  CHECK(decoded.lastUpdated == 98765ms);
  CHECK(decoded.latencySamples == 0);
  CHECK(decoded.latencyAccum == 0s);
  CHECK(decoded.EstLatency() == 0s);
}
