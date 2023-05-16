#include "common.hpp"

#ifdef MAINNET
  #define FEED_ACCOUNT name("feed.pizza")
#else
  #define FEED_ACCOUNT name("lendfeedctv2")
#endif

namespace pizzafeed {
  struct source {
    uint8_t type;
    std::vector<std::string> args;
  };
  
  struct [[eosio::table]] fetch {
    uint64_t id;
    extended_symbol coin;
    std::vector<source> sources;
    uint32_t interval;
    uint32_t reliable_interval;
    bool updatable;
    bool available;
    std::vector<decimal> prices;
    decimal reliable_price;
    uint32_t fetched_at;
    uint32_t recorded_at;

    uint128_t by_coin() const {
      return raw(coin);
    };

    uint64_t primary_key() const {
      return id;
    }
  };

  typedef eosio::multi_index<
    name("fetch"), fetch,
    indexed_by<
      name("bycoin"),
      const_mem_fun<fetch, uint128_t, &fetch::by_coin>
    >
  > fetch_tlb;

  decimal get_price(extended_symbol coin) {
    fetch_tlb fetchs(FEED_ACCOUNT, FEED_ACCOUNT.value);
    auto idx = fetchs.get_index<name("bycoin")>();
    auto itr = idx.find(raw(coin));
    check(itr != idx.end(), "price not found");
    return itr->reliable_price;
  };
}