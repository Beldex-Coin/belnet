#include <catch2/catch.hpp>
#include <dns/cache.hpp>
#include <dns/dns.hpp>
#include <dns/message.hpp>
#include <dns/name.hpp>
#include <dns/rr.hpp>
#include <net/net.hpp>
#include <net/ip.hpp>
#include <util/buffer.hpp>

#include <algorithm>

constexpr auto tld = ".bdx";

TEST_CASE("Test Has TLD", "[dns]")
{
  llarp::dns::Question question;
  question.qname = "a.bdx.";
  CHECK(question.HasTLD(tld));
  question.qname = "a.bdx..";
  CHECK(not question.HasTLD(tld));
  question.qname = "bepis.bdx.";
  CHECK(question.HasTLD(tld));
  question.qname = "bepis.logi.";
  CHECK(not question.HasTLD(tld));
  question.qname = "a.net.";
  CHECK(not question.HasTLD(tld));
  question.qname = "a.boki.";
  CHECK(not question.HasTLD(tld));
  question.qname = "t.co.";
  CHECK(not question.HasTLD(tld));
};

TEST_CASE("Test Is Localhost.bdx", "[dns]")
{
  llarp::dns::Question question;

  question.qname = "localhost.bdx.";
  CHECK(question.IsLocalhost());
  question.qname = "foo.localhost.bdx.";
  CHECK(question.IsLocalhost());
  question.qname = "foo.bar.localhost.bdx.";
  CHECK(question.IsLocalhost());

  question.qname = "something.bdx.";
  CHECK(not question.IsLocalhost());
  question.qname = "localhost.something.bdx.";
  CHECK(not question.IsLocalhost());
  question.qname = "notlocalhost.bdx.";
  CHECK(not question.IsLocalhost());
};

TEST_CASE("Test Get Subdomains" , "[dns]")
{
  llarp::dns::Question question;
  std::string expected;

  question.qname = "localhost.bdx.";
  expected = "";
  CHECK(question.Subdomains() == expected);

  question.qname = "foo.localhost.bdx.";
  expected = "foo";
  CHECK(question.Subdomains() == expected);

  question.qname = "foo.bar.localhost.bdx.";
  expected = "foo.bar";
  CHECK(question.Subdomains() == expected);

  // not legal, but test it anyway
  question.qname = ".localhost.bdx.";
  expected = "";
  CHECK(question.Subdomains() == expected);

  question.qname = ".bdx.";
  expected = "";
  CHECK(question.Subdomains() == expected);

  question.qname = "beldex.";
  expected = "";
  CHECK(question.Subdomains() == expected);

  question.qname = ".";
  expected = "";
  CHECK(question.Subdomains() == expected);

  question.qname = "";
  expected = "";
  CHECK(question.Subdomains() == expected);
};

TEST_CASE("Test PTR records", "[dns]")
{
  llarp::huint128_t expected =
      llarp::net::ExpandV4(llarp::ipaddr_ipv4_bits(10, 10, 10, 1));
  auto ip = llarp::dns::DecodePTR("1.10.10.10.in-addr.arpa.");
  CHECK(ip);
  CHECK(*ip == expected);

  expected.h.upper = 0x0123456789abcdefUL;
  expected.h.lower = 0xeeee888812341234UL;
  ip = llarp::dns::DecodePTR("4.3.2.1.4.3.2.1.8.8.8.8.e.e.e.e.f.e.d.c.b.a.9.8.7.6.5.4.3.2.1.0.ip6.arpa.");
  CHECK(ip);
  CHECK(oxenc::to_hex(std::string_view{reinterpret_cast<char*>(&expected.h), 16}) ==
          oxenc::to_hex(std::string_view{reinterpret_cast<char*>(&ip->h), 16}));
  CHECK(*ip == expected);
}

TEST_CASE("Test Serialize Header", "[dns]")
{
  std::array<byte_t, 1500> data{};
  llarp_buffer_t buf(data);
  llarp::dns::MessageHeader hdr, other;
  hdr.id       = 0x1234;
  hdr.fields   = (1 << 15);
  hdr.qd_count = 1;
  hdr.an_count = 1;
  hdr.ns_count = 0;
  hdr.ar_count = 0;
  
  CHECK(hdr.Encode(&buf));
  CHECK((buf.cur - buf.base) == llarp::dns::MessageHeader::Size);

  // rewind
  buf.cur = buf.base;

  CHECK(other.Decode(&buf));
  CHECK(hdr == other);
  CHECK(other.id == 0x1234);
  CHECK(other.fields == (1 << 15));
}

TEST_CASE("Test Serialize Name" , "[dns]")
{
  const std::string name     = "whatever.tld";
  const std::string expected = "whatever.tld.";
  std::array<byte_t, 1500> data{};
  llarp_buffer_t buf(data);
  
  CHECK(llarp::dns::EncodeNameTo(&buf, name));

  buf.cur = buf.base;
  
  CHECK(buf.base[0] == 8);
  CHECK(buf.base[1] == 'w');
  CHECK(buf.base[2] == 'h');
  CHECK(buf.base[3] == 'a');
  CHECK(buf.base[4] == 't');
  CHECK(buf.base[5] == 'e');
  CHECK(buf.base[6] == 'v');
  CHECK(buf.base[7] == 'e');
  CHECK(buf.base[8] == 'r');
  CHECK(buf.base[9] == 3);
  CHECK(buf.base[10] == 't');
  CHECK(buf.base[11] == 'l');
  CHECK(buf.base[12] == 'd');
  CHECK(buf.base[13] == 0);
  auto other = llarp::dns::DecodeName(&buf);
  CHECK(other);
  CHECK(expected == *other);
}

TEST_CASE("Test serialize question", "[dns]")
{
  const std::string name          = "whatever.tld";
  const std::string expected_name = name + ".";
  llarp::dns::Question q, other;

  std::array<byte_t, 1500> data{};
  llarp_buffer_t buf(data);
  
  q.qname  = name;
  q.qclass = 1;
  q.qtype  = 1;
  CHECK(q.Encode(&buf));

  buf.cur = buf.base;
  
  CHECK(other.Decode(&buf));
  CHECK(other.qname == expected_name);
  CHECK(q.qclass == other.qclass);
  CHECK(q.qtype == other.qtype);
}

TEST_CASE("Test Encode/Decode RData" , "[dns]")
{
  std::array<byte_t, 1500> data{};
  llarp_buffer_t buf(data);

  static constexpr size_t rdatasize = 32;
  llarp::dns::RR_RData_t rdata(rdatasize);
  std::fill(rdata.begin(), rdata.end(), 'a');
  llarp::dns::RR_RData_t other_rdata;

  CHECK(llarp::dns::EncodeRData(&buf, rdata));
  CHECK(buf.cur - buf.base == rdatasize + sizeof(uint16_t));

  buf.cur = buf.base;
  
  CHECK(llarp::dns::DecodeRData(&buf, other_rdata));
  CHECK(rdata == other_rdata);
}

TEST_CASE("Test reserved names", "[dns]")
{
    using namespace llarp::dns;
    CHECK(NameIsReserved("bdx.bdx"));
    CHECK(NameIsReserved("bdx.bdx."));
    CHECK(NameIsReserved("mnode.bdx"));
    CHECK(NameIsReserved("mnode.bdx."));
    CHECK(NameIsReserved("foo.bdx.bdx"));
    CHECK(NameIsReserved("foo.bdx.bdx."));
    CHECK(NameIsReserved("bar.mnode.bdx"));
    CHECK(NameIsReserved("bar.mnode.bdx."));
    CHECK_FALSE(NameIsReserved("barmnode.bdx."));
    CHECK_FALSE(NameIsReserved("barmnode.bdx"));
    CHECK_FALSE(NameIsReserved("allthebdx.bdx"));
    CHECK_FALSE(NameIsReserved("allthebdx.bdx."));
}

TEST_CASE("QueryCache hit after put with TTL rewrite", "[dns][cache]")
{
  using namespace std::literals;
  llarp::dns::QueryCache cache;
  llarp::dns::Question q{"example.com", llarp::dns::qTypeA};

  llarp::dns::Message answer{q};
  answer.AddINReply(llarp::huint128_t{42}, false, 60);

  // miss before put
  CHECK(not cache.Get(q, 1000ms).has_value());
  CHECK(cache.Misses() == 1);

  cache.Put(q, answer, 1000ms);

  // hit 2 seconds later; TTL wound down by elapsed time
  auto hit = cache.Get(q, 3000ms);
  REQUIRE(hit.has_value());
  REQUIRE(hit->answers.size() == 1);
  CHECK(hit->answers[0].ttl == 58);
  CHECK(cache.Hits() == 1);

  // qname matching is case insensitive
  llarp::dns::Question upper{"EXAMPLE.com", llarp::dns::qTypeA};
  CHECK(cache.Get(upper, 3000ms).has_value());

  // different qtype is a different key
  llarp::dns::Question v6{"example.com", llarp::dns::qTypeAAAA};
  CHECK(not cache.Get(v6, 3000ms).has_value());
}

TEST_CASE("QueryCache txid patching is caller's job and content survives", "[dns][cache]")
{
  using namespace std::literals;
  llarp::dns::QueryCache cache;
  llarp::dns::Question q{"txid.example.com", llarp::dns::qTypeA};

  llarp::dns::Message answer{q};
  answer.hdr_id = 0x1234;
  answer.AddINReply(llarp::huint128_t{7}, false, 100);
  cache.Put(q, answer, 0ms);

  auto hit = cache.Get(q, 0ms);
  REQUIRE(hit.has_value());
  // patch txid like the resolver does for the incoming query
  hit->hdr_id = 0x5678;
  CHECK(hit->hdr_id == 0x5678);
  // the cached original is untouched
  auto hit2 = cache.Get(q, 0ms);
  REQUIRE(hit2.has_value());
  CHECK(hit2->hdr_id == 0x1234);
}

TEST_CASE("QueryCache TTL expiry and floors", "[dns][cache]")
{
  using namespace std::literals;
  llarp::dns::QueryCache cache;
  llarp::dns::Question q{"expire.example.com", llarp::dns::qTypeA};

  llarp::dns::Message answer{q};
  answer.AddINReply(llarp::huint128_t{1}, false, 60);
  cache.Put(q, answer, 0ms);

  // still valid just before 60s
  CHECK(cache.Get(q, 59s).has_value());
  // expired after 60s
  CHECK(not cache.Get(q, 61s).has_value());

  // an answer with a tiny TTL is kept for the 5s floor
  llarp::dns::Question q2{"floor.example.com", llarp::dns::qTypeA};
  llarp::dns::Message answer2{q2};
  answer2.AddINReply(llarp::huint128_t{2}, false, 1);
  cache.Put(q2, answer2, 0ms);
  CHECK(cache.Get(q2, 3s).has_value());
  CHECK(not cache.Get(q2, 6s).has_value());
}

TEST_CASE("QueryCache negative caching", "[dns][cache]")
{
  using namespace std::literals;
  llarp::dns::QueryCache cache;
  llarp::dns::Question q{"nxdomain.example.com", llarp::dns::qTypeA};

  // no answer records -> negative cache for 30s
  llarp::dns::Message answer{q};
  cache.Put(q, answer, 0ms);
  CHECK(cache.Get(q, 29s).has_value());
  CHECK(not cache.Get(q, 31s).has_value());
}

TEST_CASE("QueryCache enforces its size cap via LRU", "[dns][cache]")
{
  using namespace std::literals;
  llarp::dns::QueryCache cache;

  const auto fill = llarp::dns::QueryCache::MaxEntries;
  for (size_t i = 0; i <= fill; ++i)
  {
    llarp::dns::Question q{"host" + std::to_string(i) + ".example.com", llarp::dns::qTypeA};
    llarp::dns::Message answer{q};
    answer.AddINReply(llarp::huint128_t{1}, false, 3600);
    cache.Put(q, answer, 0ms);
  }
  CHECK(cache.Size() == fill);
  // the least recently used (first inserted) entry was evicted
  llarp::dns::Question first{"host0.example.com", llarp::dns::qTypeA};
  CHECK(not cache.Get(first, 0ms).has_value());
  // the newest entry is present
  llarp::dns::Question last{"host" + std::to_string(fill) + ".example.com", llarp::dns::qTypeA};
  CHECK(cache.Get(last, 0ms).has_value());
}
