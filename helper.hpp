#include "common.hpp"

#define EOSIO name("eosio")
#define EOSIOTOKEN name("eosio.token")
#define EOS_SYMBOL symbol("EOS", 4)
#define REX_SYMBOL symbol("REX", 4)

struct abihash {
  name owner;
  checksum256 hash;

  uint64_t primary_key()const { return owner.value; }
};

typedef eosio::multi_index<name("abihash"), abihash > abihash_tlb;
abihash_tlb abihashs(name("eosio"), name("eosio").value);

bool is_contract(name account) {
  auto itr = abihashs.find(account.value);
  if (itr == abihashs.end()) return false;
  if (itr->hash == checksum256()) return false;  
  return true;
}

struct currency_stat {
  asset    supply;
  asset    max_supply;
  name     issuer;

  uint64_t primary_key()const { return supply.symbol.code().raw(); }
};
typedef eosio::multi_index<name("stat"), currency_stat > currency_stats;

currency_stat get_currency_stat(extended_symbol sym) {
  currency_stats statstable(sym.get_contract(), sym.get_symbol().code().raw());
  return statstable.get(sym.get_symbol().code().raw(), "currency stat not found");
};

bool is_currency_exist(extended_symbol sym) {
  currency_stats statstable(sym.get_contract(), sym.get_symbol().code().raw());
  auto exist = statstable.find(sym.get_symbol().code().raw());
  return exist != statstable.end();
};

asset get_currency_supply(extended_symbol sym) {
  currency_stats statstable(sym.get_contract(), sym.get_symbol().code().raw());
  auto itr = statstable.find(sym.get_symbol().code().raw());
  if (itr == statstable.end()) return asset(0, sym.get_symbol());
  return itr->supply;
};

struct account {
  asset balance;

  uint64_t primary_key()const { return balance.symbol.code().raw(); }
};

typedef eosio::multi_index< "accounts"_n, account > accounts;

asset get_balance(extended_symbol sym, name account) {

  accounts account_table(sym.get_contract(), account.value);
  auto row = account_table.find(sym.get_symbol().code().raw());
  return row->balance;
}

asset get_eos_balance(name account){

  return get_balance(extended_symbol(symbol("EOS", 4), EOSIOTOKEN), account);
}

struct rex_balance {
  uint8_t version = 0;
  name    owner;
  asset   vote_stake; /// the amount of CORE_SYMBOL currently included in owner's vote
  asset   rex_balance; /// the amount of REX owned by owner
  int64_t matured_rex = 0; /// matured REX available for selling
  std::deque<std::pair<time_point_sec, int64_t>> rex_maturities; /// REX daily maturity buckets

  uint64_t primary_key()const { return owner.value; }
};

typedef eosio::multi_index<name("rexbal"), rex_balance> rexbal_tlb;
rexbal_tlb rexbalances(EOSIO, EOSIO.value);

rex_balance get_rexbalance(name account) {

  auto itr = rexbalances.find(account.value);
  if(itr == rexbalances.end()){
    rex_balance rexbal;
    rexbal.owner = account;
    rexbal.vote_stake = asset(0, EOS_SYMBOL);
    rexbal.rex_balance = asset(0, REX_SYMBOL);
    return rexbal;
  }
  return *itr;
}

struct rex_pool {
  uint8_t    version = 0;
  asset      total_lent; /// total amount of CORE_SYMBOL in open rex_loans
  asset      total_unlent; /// total amount of CORE_SYMBOL available to be lent (connector)
  asset      total_rent; /// fees received in exchange for lent  (connector)
  asset      total_lendable; /// total amount of CORE_SYMBOL that have been lent (total_unlent + total_lent)
  asset      total_rex; /// total number of REX shares allocated to contributors to total_lendable
  asset      namebid_proceeds; /// the amount of CORE_SYMBOL to be transferred from namebids to REX pool
  uint64_t   loan_num = 0; /// increments with each new loan

  uint64_t primary_key()const { return 0; }
};

typedef eosio::multi_index<name("rexpool"), rex_pool> rexpool_tlb;
rexpool_tlb rexpools(EOSIO, EOSIO.value);
