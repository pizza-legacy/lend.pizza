#include "common.hpp"

#ifdef MAINNET
  #define VOTE_ACCOUNT name("vote.pizza")
#else
  #define VOTE_ACCOUNT name("pzvotectcode")
#endif

namespace votepower {
  struct voter {
    name account;
    uint64_t votes;

    uint64_t primary_key() const {
      return account.value;
    }
  };

  typedef eosio::multi_index<name("voter"), voter> voter_tlb;
  voter_tlb voters(VOTE_ACCOUNT, VOTE_ACCOUNT.value);

  uint64_t get_votes(name account) {
    auto itr = voters.find(account.value);
    if (itr != voters.end()) {
      return itr->votes;
    }
    return 0;
  };
}