#include <llarp/iwp/rtt.hpp>

#include <catch2/catch.hpp>

using namespace std::literals;
using llarp::iwp::RttEstimator;

TEST_CASE("RttEstimator first sample initializes srtt/rttvar", "[iwp][rtt]")
{
  RttEstimator rtt;
  CHECK(rtt.ResendInterval() == RttEstimator::MaxResendInterval);

  rtt.Update(100ms);
  CHECK(rtt.srtt == 100ms);
  CHECK(rtt.rttvar == 50ms);
  // srtt + 4 * rttvar = 300ms, within bounds
  CHECK(rtt.ResendInterval() == 300ms);
}

TEST_CASE("RttEstimator converges and clamps to lower bound", "[iwp][rtt]")
{
  RttEstimator rtt;
  // steady 10ms link: rto = srtt + 4*rttvar shrinks below the floor
  for (int i = 0; i < 100; ++i)
    rtt.Update(10ms);
  CHECK(rtt.srtt <= 12ms);
  CHECK(rtt.ResendInterval() == RttEstimator::MinResendInterval);
}

TEST_CASE("RttEstimator clamps to upper bound", "[iwp][rtt]")
{
  RttEstimator rtt;
  for (int i = 0; i < 10; ++i)
    rtt.Update(2000ms);
  CHECK(rtt.ResendInterval() == RttEstimator::MaxResendInterval);
}

TEST_CASE("RttEstimator exponential backoff caps at 2s", "[iwp][rtt]")
{
  RttEstimator rtt;
  rtt.Update(100ms);
  const auto base = rtt.ResendInterval();
  REQUIRE(base == 300ms);
  CHECK(rtt.BackedOffResendInterval(0) == 300ms);
  CHECK(rtt.BackedOffResendInterval(1) == 600ms);
  CHECK(rtt.BackedOffResendInterval(2) == 1200ms);
  // 2400ms would exceed the cap
  CHECK(rtt.BackedOffResendInterval(3) == RttEstimator::MaxBackoffInterval);
  CHECK(rtt.BackedOffResendInterval(100) == RttEstimator::MaxBackoffInterval);
}

TEST_CASE("RttEstimator excludes retransmitted samples (Karn)", "[iwp][rtt]")
{
  RttEstimator rtt;
  rtt.Update(100ms);
  const auto srttBefore = rtt.srtt;
  const auto rttvarBefore = rtt.rttvar;

  // samples from retransmitted messages must not change the estimate
  rtt.Update(5000ms, /*retransmitted=*/true);
  CHECK(rtt.srtt == srttBefore);
  CHECK(rtt.rttvar == rttvarBefore);

  // negative garbage is ignored too
  rtt.Update(-5ms);
  CHECK(rtt.srtt == srttBefore);
}
