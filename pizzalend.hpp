#include "common.hpp"
#include "helper.hpp"
#include "memo.hpp"

#include "pizzafeed.hpp"
#include "votepower.hpp"

#ifdef MAINNET
  #define PZTOKEN_CONTRACT name("pztken.pizza")
  #define ADMIN_ACCOUNT name("admin.pizza")
  #define ACT_ACCOUNT name("active.pizza")
  #define CAPITAL_ACCOUNT name("share.pizza")
  #define WALLET_ACCOUNT name("vault.pizza")
  #define FEE_ACCOUNT name("fee.pizza")
  #define PIZZA_CONTRACT name("pizzatotoken")
  #define PIZZA symbol("PIZZA", 4)
  #define LOG_CONTRACT name("log.pizza")
  #define SAFU_ACCOUNT name("safu.pizza")
  #define NULL_ACCOUNT name("null.pizza")
  #define DAC_ACCOUNT name("pizzalenddac")
  #define KEEP_ACCOUNT name("pzalendkeeps")
  #define TEAM_MSIG_ACCOUNT name("msig.pizza")

  #define HACKER_ACCOUNT name("itsspiderma1")
#else
  #define PZTOKEN_CONTRACT name("pzafabi33333")
  #define ADMIN_ACCOUNT name("lendadmacc11")
  #define ACT_ACCOUNT name("lendactacc11")
  #define CAPITAL_ACCOUNT name("lendcapital2")
  #define WALLET_ACCOUNT name("lendwallet11")
  #define FEE_ACCOUNT name("lendfeeacc11")
  #define PIZZA_CONTRACT name("jiangtest123")
  #define PIZZA symbol("PIZZA", 4)
  #define LOG_CONTRACT name("logcontract2")
  #define SAFU_ACCOUNT name("lendsafuacc1")
  #define NULL_ACCOUNT name("nullaccount1")
  #define DAC_ACCOUNT name("lenddacacc11")
  #define KEEP_ACCOUNT name("keepaccount1")
  #define TEAM_MSIG_ACCOUNT name("msigdotpizza")

  #define HACKER_ACCOUNT name("lenduser1111")
#endif

#define BORROW_SYMBOL_INCREASED_PRECISION 4

#define SECONDS_PER_YEAR 31536000

// 30d
#define TURN_VARIABLE_COUNTDOWN 2592000000

#define TURN_VARIABLE_ACCELERATE 8

// 0.95
#define TURN_VARIABLE_ACCELERATE_USAGE_RATE decimal(95000000, FLOAT)

// 1 second
#define INTEREST_CALCULATE_TTL 1

// features
#define FEATURE_DEPOSIT name("deposit")
#define FEATURE_WITHDRAW name("withdraw")
#define FEATURE_BORROW name("borrow")
#define FEATURE_REPAY name("repay")

#define ALL name("all")

namespace pizzalend {
  struct pztoken_config {
    decimal base_rate;
    decimal max_rate;
    decimal base_discount_rate;
    decimal max_discount_rate;
    decimal best_usage_rate;
    decimal floating_fee_rate;
    decimal fixed_fee_rate;
    decimal liqdt_rate;
    decimal liqdt_bonus;
    decimal max_ltv;
    decimal floating_rate_power;
    bool is_collateral;
    bool can_stable_borrow;
    uint8_t borrow_liqdt_order;
    uint8_t collateral_liqdt_order;

    bool valid() const {
      // 0<= base_rate <=30
      double _base_rate = decimal2double(base_rate);
      if (_base_rate < 0 || _base_rate > 30) return false;
      // 0 <= max_rate <= 2000
      double _max_rate = decimal2double(max_rate);
      if (_max_rate < 0 || _max_rate > 2000) return false;
      // 0 <= base_discount_rate < 1
      double _base_discount_rate = decimal2double(base_discount_rate);
      if (_base_discount_rate < 0 || _base_discount_rate >= 1) return false;
      // 0 <= best_usage_rate < 1
      double _best_usage_rate = decimal2double(best_usage_rate);
      if (_best_usage_rate < 0 || _best_usage_rate >= 1) return false;
      // (base_discount_rate / best_usage_rate) <= max_discount_rate < 1
      double _max_discount_rate = decimal2double(max_discount_rate);
      if (_max_discount_rate < (_base_discount_rate/_best_usage_rate) || _max_discount_rate >= 1) return false;
      // 0 <= floating_fee_rate <= 0.1
      double _floating_fee_rate = decimal2double(floating_fee_rate);
      if (_floating_fee_rate < 0 || _floating_fee_rate > 0.1) return false;
      // 0 <= fixed_fee_rate <= 0.1
      double _fixed_fee_rate = decimal2double(fixed_fee_rate);
      if (_fixed_fee_rate < 0 || _fixed_fee_rate > 0.1) return false;
      // 0.1 <= liqdt_rate <= 0.95
      double _liqdt_rate = decimal2double(liqdt_rate);
      if (_liqdt_rate < 0.1 || _liqdt_rate > 0.95) return false;
      // 0 <= liqdt_bonus <= 0.5
      double _liqdt_bonus = decimal2double(liqdt_bonus);
      if (_liqdt_bonus < 0 || _liqdt_bonus > 0.5) return false;
      // 0.1 <= max_ltv <= liqdt_rate
      double _max_ltv = decimal2double(max_ltv);
      if (_max_ltv < 0.1 || _max_ltv > _liqdt_rate) return false;
      // 1 <= floating_rate_power <= 40
      double _floating_rate_power = decimal2double(floating_rate_power);
      if (_floating_rate_power < 1 || _floating_rate_power > 40) return false;
      return true;
    };
  };

  // feature permission
  struct feature_perm {
    name feature;
    bool is_open;
  };

  class [[eosio::contract]] pizzalend : public contract {
  public:
    pizzalend(name self, name first_receiver, datastream<const char*> ds) : 
      contract(self, first_receiver, ds), pztokens(self, self.value), 
      collaterals(self, self.value), loans(self, self.value), liqdtorders(self, self.value),
      baddebts(self, self.value), cached_healths(self, self.value), cachedstables(self, self.value),
      earns(self, self.value) {}

    [[eosio::action]]
    void addpztoken(name pzname, extended_symbol pzsymbol, extended_symbol anchor, pztoken_config config);

    [[eosio::action]]
    void setpztoken(name pzname, pztoken_config config);

    [[eosio::action]]
    void setrate(std::vector<name> pznames, decimal base_rate, decimal max_rate);

    [[eosio::action]]
    void setfeatures(name pzname, std::vector<feature_perm> perms);

    [[eosio::action]]
    void claimearn();

    [[eosio::on_notify("*::transfer")]]
    void on_transfer(name from, name to, asset quantity, std::string memo);

    [[eosio::action]]
    void redeem(name account, name pzcontract, asset pzquantity);

    [[eosio::action]]
    void redeemall(name account, name pzname);

    [[eosio::action]]
    void withdraw(name account, name contract, asset quantity);

    [[eosio::action]]
    void borrow(name account, name contract, asset quantity, uint8_t type);

    [[eosio::action]]
    void collswap(name frompz, name topz, decimal rate, uint32_t limit, uint64_t start);

    // every 10 mins
    [[eosio::action]]
    void calinterest();

    // every 10 mins
    [[eosio::action]]
    void calinterest2(std::vector<name> pznames);

    // every 1 min
    [[eosio::action]]
    void uphealth();

    [[eosio::action]]
    void uphealth2(double threshold);

    [[eosio::action]]
    void cachehealth();

    [[eosio::action]]
    void addallow(name account, name feature, uint32_t duration);

    [[eosio::action]]
    void remallow(name account, name feature);

    [[eosio::action]]
    void addblock(name account, name feature, uint32_t duration);

    [[eosio::action]]
    void remblock(name account, name feature);

    [[eosio::action]]
    void setdefend(name token, asset max_value, asset pause_value, uint8_t percent, uint8_t pool_size);

    [[eosio::action]]
    void resumedefend(name token);

    [[eosio::action]]
    void claimrex();

    #ifndef MAINNET
    [[eosio::action]]
    void clear();

    [[eosio::action]]
    void rmpztoken(name pzname);
    #endif

  private:
    void _log(name event, std::vector<std::string> args) {
      uint64_t millis = current_millis();
      action(
        permission_level{_self, name("active")},
        LOG_CONTRACT,
        name("log"),
        std::make_tuple(_self, event, args, millis)
      ).send();
    };

    void _log_deposit(name account, name pzname, asset quantity, asset pzquantity) {
      std::vector<std::string> args = {account.to_string(), pzname.to_string(), quantity.to_string(), pzquantity.to_string()};
      _log(name("deposit"), args);
    };

    void _log_collateral(name account, name pzname, asset quantity, asset pzquantity) {
      std::vector<std::string> args = {account.to_string(), pzname.to_string(), quantity.to_string(), pzquantity.to_string()};
      _log(name("collateral"), args);
    };

    void _log_upcollateral(name account, name pzname, asset pzquantity, asset quantity) {
      std::vector<std::string> args = {account.to_string(), pzname.to_string(), pzquantity.to_string(), quantity.to_string()};
      _log(name("upcollateral"), args);
    };

    void _log_redeem(name account, name pzname, asset pzquantity) {
      std::vector<std::string> args = {account.to_string(), pzname.to_string(), pzquantity.to_string()};
      _log(name("redeem"), args);
    };

    void _log_withdraw(name account, name pzname, asset quantity, asset pzquantity) {
      std::vector<std::string> args = {account.to_string(), pzname.to_string(), quantity.to_string(), pzquantity.to_string()};
      _log(name("withdraw"), args);
    };

    void _log_borrow(name account, name pzname, asset quantity, asset fee, uint8_t type) {
      std::vector<std::string> args = {account.to_string(), pzname.to_string(), quantity.to_string(), fee.to_string(), std::to_string(type)};
      _log(name("borrow"), args);
    };

    void _log_upborrow(name account, name pzname, asset quantity) {
      std::vector<std::string> args = {account.to_string(), pzname.to_string(), quantity.to_string()};
      _log(name("upborrow"), args);
    };

    void _log_upborrows(name pzname) {
      std::vector<std::string> args;
      args.push_back(pzname.to_string());
      _log(name("upborrows"), args);
    };

    void _log_repay(name account, name pzname, asset quantity) {
      std::vector<std::string> args = {account.to_string(), pzname.to_string(), quantity.to_string()};
      _log(name("repay"), args);
    };

    void _log_liqdt(name account, name collateral_contract, asset collateral, name loan_contract, asset loan) {
      std::vector<std::string> args = {account.to_string(), collateral_contract.to_string(), collateral.to_string(), loan_contract.to_string(), loan.to_string()};
      _log(name("liqdt"), args);
    };

    void _log_bid(name account, name bid_contract, asset bid, name got_contract, asset got, decimal profit_rate) {
      std::vector<std::string> args = {account.to_string(), bid_contract.to_string(), bid.to_string(), got_contract.to_string(), got.to_string(), profit_rate.to_string()};
      _log(name("bid"), args);
    };

    void _log_insolvent(name account, name pzname, name contract, asset quantity) {
      std::vector<std::string> args = {account.to_string(), pzname.to_string(), contract.to_string(), quantity.to_string()};
      _log(name("insolvent"), args);
    };

    double _cal_loan_value(name account);
    
    double _cal_collateral_value(name account, bool for_loan = false);

    double _cal_loanable_collateral_value(name account) {
      return _cal_collateral_value(account, true);
    };

    double _cal_health_factor(name account);

    struct simple_pztoken {
      name pzname;
      double price;
      double pzprice;
      double liqdt_rate;
    };

    struct [[eosio::table]] pztoken {
      name pzname;
      extended_symbol pzsymbol;
      extended_symbol anchor;
      asset cumulative_deposit;
      asset available_deposit;
      asset pzquantity;
      asset borrow;
      asset cumulative_borrow;
      asset variable_borrow;
      asset stable_borrow;
      decimal usage_rate;
      decimal floating_rate;
      decimal discount_rate;
      decimal price;
      double pzprice;
      double pzprice_rate;
      uint64_t updated_at;
      pztoken_config config;

      uint128_t by_pzsymbol() const {
        return raw(pzsymbol);
      }

      uint128_t by_anchor() const {
        return raw(anchor);
      }

      uint64_t by_borrow_liqdt_order() const {
        return config.borrow_liqdt_order;
      }

      uint64_t by_collateral_liqdt_order() const {
        return config.collateral_liqdt_order;
      }

      uint64_t primary_key() const { return pzname.value; }

      symbol origin_sym() const {
        return anchor.get_symbol();
      }

      symbol borrow_sym() const {
        return symbol(origin_sym().code(), origin_sym().precision() + BORROW_SYMBOL_INCREASED_PRECISION);
      }

      decimal cal_usage_rate(int64_t incr_borrow_amount = 0) const {
        asset incr_borrow = asset(incr_borrow_amount, borrow.symbol);
        asset decr_deposit = trans_asset(available_deposit.symbol, incr_borrow);

        asset updated_deposit = available_deposit - decr_deposit;
        double d = asset2double(updated_deposit);

        asset updated_borrow = borrow + incr_borrow;
        double b = asset2double(updated_borrow);

        if (b + d == 0) return double2decimal(0.0);
        double rate = b / (b + d);
        return double2decimal(rate);
      }

      decimal cal_floating_rate(int64_t incr_borrow_amount = 0) const {
        double base_rate = decimal2double(config.base_rate);
        double max_rate = decimal2double(config.max_rate);
        double floating_rate_power = decimal2double(config.floating_rate_power);
        double d = base_rate + max_rate*pow(decimal2double(cal_usage_rate(incr_borrow_amount)), floating_rate_power);
        return double2decimal(d);
      }

      decimal cal_discount_rate(decimal usage_rate) const {
        double max_discount_rate = decimal2double(config.max_discount_rate);
        double base_discount_rate = decimal2double(config.base_discount_rate);
        double best_usage_rate = decimal2double(config.best_usage_rate);
        double _usage_rate = decimal2double(usage_rate);
        double usage_rate_diff = best_usage_rate - _usage_rate;
        if (usage_rate_diff < 0) {
          usage_rate_diff = -usage_rate_diff;
        }
        double d = max_discount_rate - base_discount_rate * usage_rate_diff/best_usage_rate;
        return double2decimal(d);
      };

      double cal_pzprice() const {
        uint64_t now = current_millis();
        uint64_t secs = (now - updated_at) / 1000;
        return pzprice * (1 + pzprice_rate * secs);
      };

      asset cal_pzquantity(asset quantity) {
        check(quantity.symbol == anchor.get_symbol(), "attempt to calculate pzquantity with different anchor symbol");
        asset pzquantity = asset(0, pzsymbol.get_symbol());
        pzquantity.amount = asset2double(quantity) * pow(10, pzquantity.symbol.precision()) / cal_pzprice();
        return pzquantity;
      }

      asset cal_anchor_quantity(asset pzquantity) {
        check(pzquantity.symbol == pzsymbol.get_symbol(), "attempt to calculate anchor quantity with different pz symbol");
        asset quantity = asset(0, anchor.get_symbol());
        quantity.amount = asset2double(pzquantity) * pow(10, quantity.symbol.precision()) * cal_pzprice();
        return quantity;
      }

      asset cal_discount_interest() const {
        asset borrowed = trans_asset(available_deposit.symbol, borrow);
        int64_t undrawn_amount = (double)pzquantity.amount * cal_pzprice();
        asset undrawn = asset(undrawn_amount, available_deposit.symbol);
        print_f("borrowed: %, available deposit: %, undrawn: %, ", borrowed, available_deposit, undrawn);
        return borrowed + available_deposit - undrawn;
      };

      simple_pztoken simple() const {
        simple_pztoken sp;
        sp.pzname = pzname;
        sp.price = decimal2double(price);
        sp.pzprice = cal_pzprice();
        sp.liqdt_rate = decimal2double(config.liqdt_rate);
        return sp;
      }
    };

    typedef eosio::multi_index<
      name("pztoken"), pztoken,
      indexed_by<name("bypzsymbol"), const_mem_fun<pztoken, uint128_t, &pztoken::by_pzsymbol>>,
      indexed_by<name("byanchor"), const_mem_fun<pztoken, uint128_t, &pztoken::by_anchor>>,
      indexed_by<name("sortbyliqdt"), const_mem_fun<pztoken, uint64_t, &pztoken::by_borrow_liqdt_order>>,
      indexed_by<name("sortbycoll"), const_mem_fun<pztoken, uint64_t, &pztoken::by_collateral_liqdt_order>>
    > pztoken_tlb;
    pztoken_tlb pztokens;

    void _addpztoken(name pzname, extended_symbol pzsymbol, extended_symbol anchor, pztoken_config config);

    void _recal_pztoken(pztoken_tlb::const_iterator pztoken_itr);

    pztoken _get_pztoken_byanchor(extended_symbol anchor) {
      auto pztokens_byanchor = pztokens.get_index<name("byanchor")>();
      std::string msg = "pztoken with anchor " + anchor.get_symbol().code().to_string() + " not found";
      return pztokens_byanchor.get(raw(anchor), msg.c_str());
    };

    pztoken _get_pztoken_bypzsymbol(extended_symbol pzsymbol) {
      auto pztokens_bypzsymbol = pztokens.get_index<name("bypzsymbol")>();
      std::string msg = "pztoken with pzsymbol " + pzsymbol.get_symbol().code().to_string() + " not found";
      return pztokens_bypzsymbol.get(raw(pzsymbol), msg.c_str());
    };

    pztoken _get_pztoken_bysymbol(extended_symbol sym) {
      if (sym.get_contract() == PZTOKEN_CONTRACT) {
        return _get_pztoken_bypzsymbol(sym);
      }
      return _get_pztoken_byanchor(sym);
    };

    void _incr_pztoken_available_deposit(name pzname, asset quantity) {
      auto itr = pztokens.find(pzname.value);
      check(itr != pztokens.end(), "pztoken not found");
      pztokens.modify(itr, _self, [&](auto& row) {
        row.available_deposit += quantity;
      });
      // _settle_pending_interest(itr, true);
      _recal_pztoken(itr);
    };

    void _decr_pztoken_available_deposit(name pzname, asset quantity) {
      auto itr = pztokens.find(pzname.value);
      check(itr != pztokens.end(), "pztoken not found");
      check(itr->available_deposit >= quantity, "insufficient available deposit");
      pztokens.modify(itr, _self, [&](auto& row) {
        row.available_deposit -= quantity;
      });
      _recal_pztoken(itr);
    };

    void _update_pztoken_deposit(name pzname, asset quantity, asset pzquantity) {
      auto itr = pztokens.find(pzname.value);
      check(itr != pztokens.end(), "pztoken not found");
      pztokens.modify(itr, _self, [&](auto& row) {
        // no more accumulate
        // if (quantity.amount > 0) {
        //   row.cumulative_deposit += quantity;
        // }
        row.available_deposit += quantity;
        row.pzquantity += pzquantity;
      });

      _addto_defendlist(itr->pzname, itr->pzquantity, pzquantity);

      _recal_pztoken(itr);
    };

    void _update_pztoken_borrow(name pzname, asset quantity, asset borrow_quantity, uint8_t type, bool is_liqdt = false) {
      auto itr = pztokens.find(pzname.value);
      check(itr != pztokens.end(), "pztoken not found");

      pztokens.modify(itr, _self, [&](auto& row) {
        if (!is_liqdt) {
          row.available_deposit -= quantity;
        }
        row.borrow += borrow_quantity;
        // no more accumulate
        // if (borrow_quantity.amount > 0) {
        //   row.cumulative_borrow += borrow_quantity;
        // }
        if (type == BorrowType::Variable) {
          row.variable_borrow += borrow_quantity;
        }
        if (type == BorrowType::Stable) {
          row.stable_borrow += borrow_quantity;
        }
      });
      _recal_pztoken(itr);
    };

    void _switch_pztoken_borrow_type(name pzname, asset quantity, uint8_t new_type) {
      auto pztoken_itr = pztokens.find(pzname.value);
      check(pztoken_itr != pztokens.end(), "pztoken not found");

      pztokens.modify(pztoken_itr, _self, [&](auto& row) {
        if (new_type == BorrowType::Variable) {
          row.variable_borrow += quantity;
          row.stable_borrow -= quantity;
        }
        if (new_type == BorrowType::Stable) {
          row.stable_borrow += quantity;
          row.variable_borrow -= quantity;
        }
      });
    };

    void _uphealth(double threshold = 0);

    void _settle_pending_interest(pztoken_tlb::const_iterator pztoken_itr);

    void _calculate_interest(pztoken_tlb::const_iterator pztoken_itr);

    bool _update_anchor_price(pztoken_tlb::const_iterator pztoken_itr);

    decimal _get_anchor_price(name pzname);

    double _cal_withdrawable_value(name account, pztoken pz) {
      double loan_value = _cal_loan_value(account);
      if (loan_value <= 0) {
        return -1;
      }

      double collateral_value = _cal_collateral_value(account);
      if (collateral_value <= 0) {
        return 0;
      };

      double max_reduce_rate = 1 - 1.1/(collateral_value/loan_value);
      double max_value = collateral_value * max_reduce_rate / decimal2double(pz.config.liqdt_rate);
      return max_value;
    };

    struct [[eosio::table]] pzrate {
      decimal rate;
      uint64_t time;

      uint64_t primary_key() const { return time; }
    };
    typedef eosio::multi_index<name("pzrate"), pzrate> pzrate_tlb;

    void _record_pzrate(name pzname, decimal rate, uint64_t time = 0) {
      pzrate_tlb pzrates(_self, pzname.value);

      if (time == 0) {
        time = current_hour();
      }

      auto itr = pzrates.find(time);
      if (itr == pzrates.end()) {
        pzrates.emplace(_self, [&](auto& row) {
          row.rate = rate;
          row.time = time;
        });
      } else if (itr->rate < rate) {
        pzrates.modify(itr, _self, [&](auto& row) {
          row.rate = rate;
        });
      }
    };

    decimal cal_fixed_rate(pztoken pz, int64_t incr_borrow_amount = 0) {
      decimal latest_rate = pz.cal_floating_rate(incr_borrow_amount);
      _record_pzrate(pz.pzname, latest_rate);

      uint64_t now = current_hour();
      pzrate_tlb pzrates(_self, pz.pzname.value);
      std::vector<decimal> rates;
      
      auto itr = pzrates.begin();
      if (itr == pzrates.end()) {
        return latest_rate;
      }

      decimal begin_rate;
      uint64_t begin_time = now - 8 * 3600;
      if (itr->time >= begin_time) {
        begin_rate = itr->rate;
        begin_time = itr->time;
      } else {
        // delete records from 8 hours ago
        while(itr != pzrates.end() && itr->time <= begin_time) {
          begin_rate = itr->rate;
          if (itr->time < begin_time) {
            itr = pzrates.erase(itr);
          } else {
            break;
          }
        }
      }

      decimal current_rate = begin_rate;
      uint64_t latest_time = begin_time;
    
      while(latest_time <= now) {
        auto itr = pzrates.find(latest_time);
        if (itr != pzrates.end()) {
          current_rate = itr->rate;
        } else {
          _record_pzrate(pz.pzname, current_rate, latest_time);
        }
        rates.push_back(current_rate);
        latest_time += 3600;
      }

      std::sort(rates.begin(), rates.end());
      decimal fixed_rate = rates[rates.size()/2];
      print_f("latest floating rate: %, fixed rate: %, ", latest_rate, fixed_rate);
      if (fixed_rate.amount > latest_rate.amount * 1.5) {
        fixed_rate.amount = latest_rate.amount * 1.5;
      }
      print_f("fixed rate: % | ", fixed_rate);
      return fixed_rate;
    };

    struct [[eosio::table]] collateral {
      uint64_t id;
      name account;
      name pzname;
      asset quantity;
      uint64_t updated_at;

      uint64_t by_account() const {
        return account.value;
      }

      uint64_t by_pzname() const {
        return pzname.value;
      }

      uint128_t by_acc_pzname() const {
        return raw(account, pzname);
      }

      uint64_t primary_key() const { return id; }
    };

    typedef eosio::multi_index<
      name("collateral"), collateral,
      indexed_by<name("byaccount"), const_mem_fun<collateral, uint64_t, &collateral::by_account>>,
      indexed_by<name("bypzname"), const_mem_fun<collateral, uint64_t, &collateral::by_pzname>>,
      indexed_by<name("byaccpzname"), const_mem_fun<collateral, uint128_t, &collateral::by_acc_pzname>>
    > collateral_tlb;
    collateral_tlb collaterals;

    void _incr_collateral(name account, pztoken pz, asset pzquantity) {
      check(pzquantity.amount > 0, "collateral quantity must be positive");

      auto collaterals_byaccpzname = collaterals.get_index<name("byaccpzname")>();
      auto itr = collaterals_byaccpzname.find(raw(account, pz.pzname));
      if (itr != collaterals_byaccpzname.end()) {
        collaterals_byaccpzname.modify(itr, _self, [&](auto& row) {
          row.quantity += pzquantity;
          row.updated_at = current_millis();
        });
        _log_upcollateral(account, pz.pzname, itr->quantity, pz.cal_anchor_quantity(itr->quantity));
      } else {
        collaterals.emplace(_self, [&](auto& row) {
          row.id = collaterals.available_primary_key();
          row.account = account;
          row.pzname = pz.pzname;
          row.quantity = pzquantity;
          row.updated_at = current_millis();
        });
        _log_upcollateral(account, pz.pzname, pzquantity, pz.cal_anchor_quantity(pzquantity));
      }
    };

    // return:
    //   actual reduction
    asset _decr_collateral(name account, pztoken pz, asset pzquantity) {
      check(pzquantity.amount > 0, "collateral quantity must be positive");

      auto collaterals_byaccpzname = collaterals.get_index<name("byaccpzname")>();
      auto itr = collaterals_byaccpzname.find(raw(account, pz.pzname));
      check(itr != collaterals_byaccpzname.end(), "collateral not found");
      check(itr->quantity >= pzquantity, "insufficient collateral quantity");

      asset exact_quantity = pzquantity;
      asset remain = itr->quantity - exact_quantity;
      if (remain.amount == 1) {
        // accuracy loss compensation
        exact_quantity = itr->quantity;
      }

      if (itr->quantity == exact_quantity) {
        collaterals_byaccpzname.erase(itr);
        _log_upcollateral(account, pz.pzname, asset(0, exact_quantity.symbol), asset(0, pz.anchor.get_symbol()));
      } else {
        collaterals_byaccpzname.modify(itr, _self, [&](auto& row) {
          row.quantity -= exact_quantity;
          row.updated_at = current_millis();
        });
        _log_upcollateral(account, pz.pzname, itr->quantity, pz.cal_anchor_quantity(itr->quantity));
      }
      return exact_quantity;
    };

    std::vector<collateral> _get_acccollaterals_byliqdt(name account);

    enum BorrowType {
      Variable = 1,
      Stable = 2
    };

    struct [[eosio::table]] loan {
      uint64_t id;
      name account;
      name pzname;
      asset principal;
      asset quantity;
      uint8_t type;
      decimal fixed_rate;
      uint64_t turn_variable_countdown;
      uint64_t last_calculated_at;
      uint64_t updated_at;

      uint64_t by_account() const {
        return account.value;
      }

      uint64_t by_pzname() const {
        return pzname.value;
      }

      uint128_t by_acc_pzname() const {
        return raw(account, pzname);
      }

      uint64_t primary_key() const { return id; }

      asset cal_pending_interest(decimal rate) const {
        asset interest = asset(0, quantity.symbol);
        if (rate.amount > 0) {
          uint64_t now = current_millis();
          int64_t passed_secs = (now - last_calculated_at)/1000;
          if (passed_secs >= INTEREST_CALCULATE_TTL) {
            interest.amount = int64_t(quantity.amount * decimal2double(rate) * passed_secs / SECONDS_PER_YEAR);
          }
        }
        return interest;
      }

      asset actual_quantity() const {
        return trans_asset(principal.symbol, quantity);
      };
    };

    typedef eosio::multi_index<
      name("loan"), loan,
      indexed_by<name("byaccount"), const_mem_fun<loan, uint64_t, &loan::by_account>>,
      indexed_by<name("bypzname"), const_mem_fun<loan, uint64_t, &loan::by_pzname>>,
      indexed_by<name("byaccpzname"), const_mem_fun<loan, uint128_t, &loan::by_acc_pzname>>
    > loan_tlb;
    loan_tlb loans;

    std::vector<loan> _get_accloans_byliqdt(name account);

    void _incr_loan(name account, pztoken pz, asset quantity, uint8_t type) {
      check(quantity.amount > 0, "loan quantity must be positive");

      auto loans_byaccpzname = loans.get_index<name("byaccpzname")>();
      auto itr = loans_byaccpzname.find(raw(account, pz.pzname));

      uint64_t now = current_millis();
      asset exact_quantity = trans_asset(pz.borrow_sym(), quantity);

      double old_stable_interest = 0;
      double new_stable_interest = 0;

      if (itr != loans_byaccpzname.end()) {
        if (itr->type == BorrowType::Stable) {
          old_stable_interest = decimal2double(itr->fixed_rate) * asset2double(itr->quantity);
        }

        if (itr->type != type) {
          _switch_pztoken_borrow_type(pz.pzname, itr->quantity, type);
        }

        decimal rate = itr->type == BorrowType::Stable ? itr->fixed_rate : pz.floating_rate;
        asset interest = itr->cal_pending_interest(rate);
        exact_quantity += interest;
        loans_byaccpzname.modify(itr, _self, [&](auto& row) {
          row.principal += quantity;
          row.quantity += exact_quantity;
          row.type = type;
          if (type == BorrowType::Stable) {
            row.fixed_rate = cal_fixed_rate(pz, exact_quantity.amount);
            row.turn_variable_countdown = TURN_VARIABLE_COUNTDOWN;
          } else {
            row.fixed_rate = decimal(0, FLOAT);
            row.turn_variable_countdown = 0;
          }
          row.last_calculated_at = now;
          row.updated_at = now;
        });

        if (type == BorrowType::Stable) {
          new_stable_interest = decimal2double(itr->fixed_rate) * asset2double(itr->quantity);
        }

        _log_upborrow(account, pz.pzname, itr->quantity);
      } else {
        auto loan_itr = loans.emplace(_self, [&](auto& row) {
          row.id = loans.available_primary_key();
          row.account = account;
          row.pzname = pz.pzname;
          row.principal = quantity;
          row.quantity = exact_quantity;
          row.type = type;
          if (type == BorrowType::Stable) {
            row.fixed_rate = cal_fixed_rate(pz, exact_quantity.amount);
            row.turn_variable_countdown = TURN_VARIABLE_COUNTDOWN;
          } else {
            row.fixed_rate = decimal(0, FLOAT);
            row.turn_variable_countdown = 0;
          }
          row.last_calculated_at = now;
          row.updated_at = now;
        });

        if (type == BorrowType::Stable) {
          new_stable_interest = decimal2double(loan_itr->fixed_rate) * asset2double(loan_itr->quantity);
        }
        _log_upborrow(account, pz.pzname, exact_quantity);
      }

      if (new_stable_interest != old_stable_interest) {
        _change_stable_interest(pz.pzname, new_stable_interest - old_stable_interest);
      }

      _update_pztoken_borrow(pz.pzname, quantity, exact_quantity, type);
    };

    void _decr_loan(name account, pztoken pz, asset quantity, bool is_liqdt = false) {
      check(quantity.amount > 0, "loan quantity must be positive");

      auto loans_byaccpzname = loans.get_index<name("byaccpzname")>();
      auto itr = loans_byaccpzname.find(raw(account, pz.pzname));
      check(itr != loans_byaccpzname.end(), "loan not found");

      double old_stable_interest = 0;
      double new_stable_interest = 0;
      if (itr->type == BorrowType::Stable) {
        old_stable_interest = decimal2double(itr->fixed_rate) * asset2double(itr->quantity);
      }

      decimal rate = itr->type == BorrowType::Stable ? itr->fixed_rate : pz.floating_rate;
      asset interest = itr->cal_pending_interest(rate);

      asset raw_quantity = trans_asset(pz.anchor.get_symbol(), quantity);
      asset exact_quantity = trans_asset(pz.borrow_sym(), quantity);

      asset remain = itr->quantity - exact_quantity + interest;
      check(remain.amount >= 0, "insufficient loan quantity");

      double remain_ratio = 1 - asset2double(exact_quantity) / (asset2double(itr->quantity) + asset2double(interest));

      asset principal_remain = asset(0, itr->principal.symbol);
      principal_remain.amount = itr->principal.amount * remain_ratio;
      asset actual_remain = trans_asset(itr->principal.symbol, remain);

      uint8_t type = itr->type;

      uint64_t now = current_millis();
      if (actual_remain.amount > 0) {
        uint64_t pass_millis = now - itr->last_calculated_at;
        loans_byaccpzname.modify(itr, _self, [&](auto& row) {
          row.quantity = remain;
          row.principal = principal_remain;
          row.last_calculated_at = now;
          if (row.type == BorrowType::Stable) {
            if (pass_millis < row.turn_variable_countdown) {
              row.turn_variable_countdown -= pass_millis;
            } else {
              row.turn_variable_countdown = 0;
            }
          }
          row.updated_at = now;
        });
        if (itr->type == BorrowType::Stable) {
          new_stable_interest = decimal2double(itr->fixed_rate) * asset2double(itr->quantity);
        }
        _log_upborrow(account, pz.pzname, itr->quantity);
      } else {
        exact_quantity = itr->quantity;
        loans_byaccpzname.erase(itr);
        _log_upborrow(account, pz.pzname, asset(0, exact_quantity.symbol));
      }

      if (new_stable_interest != old_stable_interest) {
        _change_stable_interest(pz.pzname, new_stable_interest - old_stable_interest);
      }

      _update_pztoken_borrow(itr->pzname, -raw_quantity, -(exact_quantity-interest), type, is_liqdt);
    };

    asset _cal_loan_fee(name account, pztoken pz, asset quantity, uint8_t type);

    struct [[eosio::table]] feature {
      name pzname;
      bool is_open;

      uint64_t primary_key() const { return pzname.value; }
    };

    typedef eosio::multi_index<name("feature"), feature> feature_tlb;

    void _setfeatures(name pzname, std::vector<feature_perm> perms);

    void _check_feature(pztoken pz, name account, name fname);

    struct [[eosio::table]] liqdtorder {
      uint64_t id;
      name account;
      extended_asset collateral;
      extended_asset loan;
      uint64_t liqdted_at;
      uint64_t updated_at;

      uint64_t primary_key() const { return id; }
    };

    typedef eosio::multi_index<name("liqdtorder"), liqdtorder> liqdtorder_tlb;
    liqdtorder_tlb liqdtorders;

    void _add_liqdtorder(name account, double liqdt_bonus, name collateral_contract, asset collateral, name loan_contract, asset loan) {
      int64_t risk_amount = collateral.amount * liqdt_bonus / (1 + liqdt_bonus) / 3;
      asset risk_fund = asset(risk_amount, collateral.symbol);
      if (risk_fund.amount > 0) {
        _transfer_out(SAFU_ACCOUNT, collateral_contract, risk_fund, "safe asset fund for users");
      }

      liqdtorders.emplace(_self, [&](auto& row) {
        row.id = liqdtorders.available_primary_key();
        row.account = account;
        row.collateral = extended_asset(collateral - risk_fund, collateral_contract);
        row.loan = extended_asset(loan, loan_contract);
        row.liqdted_at = current_millis();
        row.updated_at = current_millis();
      });
      _log_liqdt(account, collateral_contract, collateral, loan_contract, loan);
    };

    struct [[eosio::table]] cached_health {
      name account;
      double loan_value;
      double collateral_value;
      double factor;
      uint64_t updated_at;
      
      uint64_t primary_key() const { return account.value; }

      bool should_refresh(uint64_t now, double threshold = 0) const {
        if (threshold > 0 && factor > threshold) {
          return false;
        }
        if (now <= updated_at) return false;
        uint64_t passed_secs = (now - updated_at) / 1000;
        if (factor < 1.25) return passed_secs >= 360;
        if (factor < 1.5) return passed_secs >= 900;
        if (factor < 1.7) return passed_secs >= 4200;
        if (factor < 2) return passed_secs >= 6000;
        return passed_secs >= 15000;
      };
    };

    typedef eosio::multi_index<name("cachedhealth"), cached_health> cached_health_tlb;
    cached_health_tlb cached_healths;

    void _cache_health(name account) {
      double loan_value = _cal_loan_value(account);
      if (loan_value <= 0) {
        _uncache_health(account);
        return;
      }

      double collateral_value = _cal_collateral_value(account);
      double factor = collateral_value/loan_value;
      _cache_health(account, loan_value, collateral_value, factor);
    };

    void _uncache_health(name account) {
      auto itr = cached_healths.find(account.value);
      if (itr != cached_healths.end()) {
        cached_healths.erase(itr);
      }
    };

    void _cache_health(name account, double loan_value, double collateral_value, double factor) {
      if (loan_value <= 0) {
        _uncache_health(account);
        return;
      }
      auto itr = cached_healths.find(account.value);
      if (itr == cached_healths.end()) {
        cached_healths.emplace(_self, [&](auto& row) {
          row.account = account;
          row.loan_value = loan_value;
          row.collateral_value = collateral_value;
          row.factor = factor;
          row.updated_at = current_millis();
        });
      } else {
        cached_healths.modify(itr, _self, [&](auto& row) {
          row.loan_value = loan_value;
          row.collateral_value = collateral_value;
          row.factor = factor;
          row.updated_at = current_millis();
        });
      }
    };

    struct [[eosio::table]] baddebt {
      name pzname;
      extended_asset quantity;

      uint64_t primary_key() const { return pzname.value; }
    };

    typedef eosio::multi_index<name("baddebt"), baddebt> baddebt_tlb;
    baddebt_tlb baddebts;

    void _incr_baddebt(pztoken pz, asset quantity) {
      auto itr = baddebts.find(pz.pzname.value);
      if (itr == baddebts.end()) {
        baddebts.emplace(_self, [&](auto& row) {
          row.pzname = pz.pzname;
          row.quantity = extended_asset(quantity, pz.anchor.get_contract());
        });
      } else {
        baddebts.modify(itr, _self, [&](auto& row) {
          row.pzname = pz.pzname;
          row.quantity += extended_asset(quantity, pz.anchor.get_contract());
        });
      }
    };

    struct [[eosio::table]] cached_stable {
      name pzname;
      double interest;
      uint64_t updated_at;

      uint64_t primary_key() const { return pzname.value; }
    };

    typedef eosio::multi_index<name("cachedstable"), cached_stable> cachedstable_tlb;
    cachedstable_tlb cachedstables;

    void _change_stable_interest(name pzname, double delta) {
      auto itr = cachedstables.find(pzname.value);
      if (itr == cachedstables.end()) {
        itr = cachedstables.emplace(_self, [&](auto& row) {
          row.pzname = pzname;
          row.interest = delta;
          row.updated_at = current_millis();
        });
      } else {
        cachedstables.modify(itr, _self, [&](auto& row) {
          row.interest += delta;
          row.updated_at = current_millis();
        });
      }
      check(itr->interest >= 0, "cached stable interest must be positive");
    };

    void _cache_stable(name pzname, double interest) {
      auto itr = cachedstables.find(pzname.value);
      if (itr == cachedstables.end()) {
        cachedstables.emplace(_self, [&](auto& row) {
          row.pzname = pzname;
          row.interest = interest;
          row.updated_at = current_millis();
        });
      } else {
        cachedstables.modify(itr, _self, [&](auto& row) {
          row.interest = interest;
          row.updated_at = current_millis();
        });
      }
    };

    double _get_stable_interest(name pzname) {
      auto itr = cachedstables.find(pzname.value);
      if (itr != cachedstables.end()) {
        return itr->interest;
      }

      pztoken pz = pztokens.get(pzname.value, "pztoken not found");
      double stable_interest = 0;
      auto loans_bypzname = loans.get_index<name("bypzname")>();
      auto loan_itr = loans_bypzname.lower_bound(pzname.value);
      while(loan_itr != loans_bypzname.end() && loan_itr->pzname == pzname) {
        if (loan_itr->type == BorrowType::Stable) {
          stable_interest += decimal2double(loan_itr->fixed_rate) * asset2double(loan_itr->quantity);
        }
        loan_itr++;
      }

      if (itr == cachedstables.end()) {
        itr = cachedstables.emplace(_self, [&](auto& row) {
          row.pzname = pzname;
          row.interest = stable_interest;
          row.updated_at = current_millis();
        });
      } else {
        cachedstables.modify(itr, _self, [&](auto& row) {
          row.interest = stable_interest;
          row.updated_at = current_millis();
        });
      }
      return stable_interest;
    };

    struct [[eosio::table]] earn {
      name pzname;
      asset received;
      uint64_t updated_at;

      uint64_t primary_key() const { return pzname.value; }
    };

    typedef eosio::multi_index<name("earn"), earn> earn_tlb;
    earn_tlb earns;

    enum AllowType {
      ManualAllow = 1
    };

    // allowlist, priority is higher than blocklist
    struct [[eosio::table]] allowlist {
      name account;
      uint8_t type;
      uint64_t expired_at;

      uint64_t primary_key() const { return account.value; }
    };
    typedef eosio::multi_index<name("allowlist"), allowlist> allowlist_tlb;

    bool _in_allowlist(name account, name feature = ALL) {
      allowlist_tlb allows(_self, feature.value);
      auto itr = allows.find(account.value);
      if (itr == allows.end()) return false;
      if (itr->expired_at > 0 && itr->expired_at <= current_millis()) {
        allows.erase(itr);
        return false;
      }
      return true;
    };

    void _addto_allowlist(name account, name feature = ALL, uint8_t type = AllowType::ManualAllow, uint32_t duration = 0) {
      uint64_t expired_at = 0;
      if (duration > 0) {
        expired_at = current_millis() + duration * 1000;
      }
      allowlist_tlb allows(_self, feature.value);
      auto itr = allows.find(account.value);
      if (itr == allows.end()) {
        allows.emplace(_self, [&](auto& row) {
          row.account = account;
          row.type = type;
          row.expired_at = expired_at;
        });
      } else {
        allows.modify(itr, _self, [&](auto& row) {
          row.type = type;
          row.expired_at = expired_at;
        });
      }
    };

    enum BlockType {
      ManualBlock = 1,
      ContractAcc = 2
    };

    struct [[eosio::table]] blocklist {
      name account;
      uint8_t type;
      uint64_t expired_at;

      uint64_t primary_key() const { return account.value; }
    };
    typedef eosio::multi_index<name("blocklist"), blocklist> blocklist_tlb;

    bool _in_blocklist(name account, name feature = ALL) {
      blocklist_tlb bloks(_self, feature.value);
      auto itr = bloks.find(account.value);
      if (itr == bloks.end()) return false;
      if (itr->expired_at > 0 && itr->expired_at <= current_millis()) {
        bloks.erase(itr);
        return false;
      }
      return true;
    };

    void _addto_blocklist(name account, name feature = ALL, uint8_t type = BlockType::ManualBlock, uint32_t duration = 0) {
      uint64_t expired_at = 0;
      if (duration > 0) {
        expired_at = current_millis() + duration * 1000;
      }
      blocklist_tlb blocks(_self, feature.value);
      auto itr = blocks.find(account.value);
      if (itr == blocks.end()) {
        blocks.emplace(_self, [&](auto& row) {
          row.account = account;
          row.type = type;
          row.expired_at = expired_at;
        });
      } else {
        blocks.modify(itr, _self, [&](auto& row) {
          row.type = type;
          row.expired_at = expired_at;
        });
      }
    };

    bool _isblock(name account, name feature) {
      if (_in_allowlist(ALL, ALL)) return false;
      if (feature != ALL) {
        if (_in_allowlist(ALL, feature)) return false;
      }
      if (_in_allowlist(account, ALL)) return false;
      if (feature != ALL) {
        if (_in_allowlist(account, feature)) return false;
      }
      
      if (_in_blocklist(ALL, ALL)) return true;
      if (feature != ALL) {
        if (_in_blocklist(ALL, feature)) return true;
      }
      if (_in_blocklist(account, ALL)) return true;
      if (feature != ALL) {
        if (_in_blocklist(account, feature)) return true;
      }

      if (is_contract(account)) return true;

      return false;
    };

    struct [[eosio::table]] defendlist {
      name token;
      uint8_t pool_size;
      uint8_t percent;
      asset max_value;
      asset mid_pool;
      std::vector<asset> pools;
      asset pause_value;
      uint32_t pause_at;
      uint32_t updated_at;
      uint64_t primary_key() const { return token.value; }
    };

    typedef eosio::multi_index<name("defendlist"), defendlist> defendlist_tlb;

    void _addto_defendlist(name token, asset quantity, asset delta){

      uint32_t tt = current_secs();
      defendlist_tlb defendlist(_self, ALL.value);
      auto itr = defendlist.find(token.value);

      if (itr == defendlist.end()) {
        asset max_value = string_to_asset("2000.0000 EOS");
        asset pause_value = string_to_asset("100000.0000 EOS");
        std::vector<asset> pools;
        pools.push_back(quantity);

        defendlist.emplace(_self, [&](auto& row) {
          row.token = token;
          row.pool_size = 7;
          row.percent = 50;
          row.max_value = max_value;
          row.pools = pools;
          row.mid_pool = quantity;
          row.pause_value = pause_value;
          row.updated_at = tt;
        });
      } else {
        auto pools = itr->pools;
        uint32_t day = tt - tt % 86400;
        if (itr->updated_at > day){
          pools.pop_back();
        }

        if (pools.size() >= itr->pool_size){
          pools.erase(pools.begin(), pools.begin()+1);
        }

        pools.push_back(quantity);

        std::vector<asset> tempPools(pools);
        std::sort(tempPools.begin(), tempPools.end());
        auto mid = tempPools[tempPools.size()/2];

        auto pause_time = itr->pause_at;
        if (delta.amount > 0){

          pztoken pz = pztokens.get(token.value);
          auto value = decimal2double(pz.price) * pz.cal_pzprice() * asset2double(delta);

          if (value >= asset2double(itr->pause_value)){

            pause_time = tt + 3600 * 36;
          }
        }

        defendlist.modify(itr, _self, [&](auto& row){

          row.pools = pools;
          row.mid_pool = mid;
          row.updated_at = tt;
          row.pause_at = pause_time;
        });

      }
    }

    bool _defend_borrow_check(name account, double value){

      defendlist_tlb defendlist(_self, ALL.value);

      double defend_value = 0;
      uint32_t tt = current_secs();
      auto collaterals_byacc = collaterals.get_index<name("byaccount")>();
      auto itr = collaterals_byacc.lower_bound(account.value);

      while(itr != collaterals_byacc.end() && itr->account == account) {
        pztoken pz = pztokens.get(itr->pzname.value);
        auto defend = defendlist.find(itr->pzname.value);

        decimal rate = pz.config.max_ltv;
        double user_value = asset2double(itr->quantity);
        auto collateral_value = decimal2double(pz.price) * pz.cal_pzprice() * user_value * decimal2double(rate);
        if (defend == defendlist.end()){
          defend_value += collateral_value;
          itr++;
          continue;
        }
        
        if (tt < defend->pause_at){
          itr++;
          continue;
        }

        double total_value = asset2double(pz.pzquantity);
        double base_value = asset2double(defend->max_value);
        double mid_value = asset2double(defend->mid_pool) * decimal2double(pz.price) * pz.cal_pzprice();

        double step1 = user_value / total_value * std::max(base_value, mid_value) * decimal2double(rate);
        defend_value += std::min(step1, collateral_value);

        itr++;
      }

      return value < defend_value;
    }

    void _update_defend(name token, asset max_value, asset pause_value, uint8_t percent, uint8_t pool_size){

      defendlist_tlb defendlist(_self, ALL.value);
      auto itr = defendlist.find(token.value);
      check(itr != defendlist.end(), "error token");

      check(max_value.symbol == EOS_SYMBOL, "incorrect max_value");
      check(pause_value.symbol == EOS_SYMBOL, "incorrect pause_value");

      defendlist.modify(itr, _self, [&](auto& row){

        row.max_value = max_value;
        row.pause_value = pause_value;
        row.pool_size = pool_size;
        row.percent = percent;
      });
    }

    void _resume_defend(name token){

      defendlist_tlb defendlist(_self, ALL.value);
      auto itr = defendlist.find(token.value);
      check(itr != defendlist.end(), "error token");

      defendlist.modify(itr, _self, [&](auto& row){
        row.pause_at = 0;
      });
    }

    void _deposit(name account, name contract, asset quantity);

    void _collateral(name account, name contract, asset quantity);

    void _redeem(name account, name pzcontract, asset pzquantity);

    void _withdraw(name account, name contract, asset quantity);

    void _withdraw_pztoken(name account, name pzcontract, asset pzquantity);

    decimal _borrow(name account, name contract, asset quantity, uint8_t type, decimal fee_deduct = decimal(0, FLOAT));

    void _borrow_with_fee(name account, name fee_contract, asset fee_quantity, memo m);

    void _repay(name account, name contract, asset quantity);

    void _mini_repay(name account, name contract, asset quantity, memo m);

    void _bid(name account, name contract, asset quantity, memo m);

    void _create_pzsymbol(name contract, asset max_supply);
    void _issue_pzsymbol(name to, name contract, asset quantity, std::string memo);
    void _transfer_to(name to, name contract, asset quantity, std::string memo);
    void _transfer_in(name from, name contract, asset quantity, std::string memo);
    void _transfer_out(name to, name contract, asset quantity, std::string memo);

    void _liqdt(name account, double remain_loan_value);

    struct acc_value {
      double loan_value;
      double collateral_value;

      acc_value() : loan_value(0), collateral_value(0) {}

      acc_value(double l, double c) {
        loan_value = l;
        collateral_value = c;
      }
    };
  };
}