#include "pizzalend.hpp"

namespace pizzalend {
  void pizzalend::on_transfer(name from, name to, asset quantity, std::string s) {
    if (from == _self || to != _self) return;

    memo m = memo(s);
    std::string first = m.get(0);
    if (first == "deposit") {
      _deposit(from, get_first_receiver(), quantity);
    } else if (first == "collateral") {
      _collateral(from, get_first_receiver(), quantity);
    } else if (first == "withdraw") {
      _withdraw_pztoken(from, get_first_receiver(), quantity);
    } else if (first == "repay") {
      _repay(from, get_first_receiver(), quantity);
    } else if (first == "repayfor") {
      name account = name(m.get(1));
      check(is_account(account), "the target must be account");
      _repay(account, get_first_receiver(), quantity);
    } else if (first == "minirepay") {
      _mini_repay(from, get_first_receiver(), quantity, m);
    } else if (first == "borrow") {
      _borrow_with_fee(from, get_first_receiver(), quantity, m);
    } else if (first == "bid") {
      _bid(from, get_first_receiver(), quantity, m);
    } else {
      check(false, "invalid memo");
    }
  };

  void pizzalend::addpztoken(name pzname, extended_symbol pzsymbol, extended_symbol anchor, pztoken_config config) {
    require_auth(permission_level{ADMIN_ACCOUNT, name("manager")});

    _addpztoken(pzname, pzsymbol, anchor, config);
  };

  void pizzalend::setpztoken(name pzname, pztoken_config config) {
    require_auth(permission_level{ADMIN_ACCOUNT, name("manager")});

    check(config.valid(), "pztoken config invalid");
    auto itr = pztokens.find(pzname.value);
    check(itr != pztokens.end(), "pztoken not found");
    pztokens.modify(itr, _self, [&](auto& row) {
      row.config = config;
    });

    _calculate_interest(itr);
  };

  void pizzalend::setrate(std::vector<name> pznames, decimal base_rate, decimal max_rate) {
    require_auth(permission_level{ADMIN_ACCOUNT, name("manager")});

    for (auto itr = pznames.begin(); itr != pznames.end(); itr++) {
      name pzname = *itr;
      auto pztoken_itr = pztokens.find(pzname.value);
      if (pztoken_itr == pztokens.end()) {
        continue;
      }

      pztokens.modify(pztoken_itr, _self, [&](auto& row) {
        if (row.config.base_rate > base_rate) row.config.base_rate = base_rate;
        if (row.config.max_rate > max_rate) row.config.max_rate = max_rate;
      });

      _calculate_interest(pztoken_itr);
    }
  };

  void pizzalend::setfeatures(name pzname, std::vector<feature_perm> perms) {
    require_auth(permission_level{ADMIN_ACCOUNT, name("manager")});
    _setfeatures(pzname, perms);
  };

  void pizzalend::claimearn() {
    require_auth(DAC_ACCOUNT);

    for (auto itr = pztokens.begin(); itr != pztokens.end(); itr++) {
      asset earn = itr->cal_discount_interest();
      print_f("pz: %, earn: % | ", itr->pzname, earn);
      if (earn.amount < 0) {
        print_f("warning!!! %'s earn is negative | ", itr->pzname);
        continue;
      }
      
      check(earn.amount >= 0, "earn of " + itr->pzname.to_string() + " cannot be negative");

      asset got = earn;
      if (got > itr->available_deposit) {
        got = itr->available_deposit;
      }

      if (got.amount > 0) {
        _decr_pztoken_available_deposit(itr->pzname, got);
        _transfer_out(KEEP_ACCOUNT, itr->anchor.get_contract(), got, "system income");
        auto earn_itr = earns.find(itr->pzname.value);
        if (earn_itr == earns.end()) {
          earn_itr = earns.emplace(_self, [&](auto& row) {
            row.pzname = itr->pzname;
            row.received = got;
            row.updated_at = current_millis();
          });
        } else {
          earns.modify(earn_itr, _self, [&](auto& row) {
            row.received += got;
            row.updated_at = current_millis();
          });
        }
      }
    }
  };

  #ifndef MAINNET
  void pizzalend::clear() {
    require_auth(_self);

    auto itr = pztokens.begin();
    while(itr != pztokens.end()) {
      itr = pztokens.erase(itr);
    }

    auto bitr = loans.begin();
    while(bitr != loans.end()) {
      bitr = loans.erase(bitr);
    }

    auto citr = collaterals.begin();
    while(citr != collaterals.end()) {
      citr = collaterals.erase(citr);
    }

    auto litr = liqdtorders.begin();
    while(litr != liqdtorders.end()) {
      litr = liqdtorders.erase(litr);
    }
  };

  void pizzalend::rmpztoken(name pzname) {
    require_auth(_self);
    
    auto loans_bypzname = loans.get_index<name("bypzname")>();
    auto loan_itr = loans_bypzname.lower_bound(pzname.value);
    while(loan_itr != loans_bypzname.end() && loan_itr->pzname == pzname) {
      loan_itr = loans_bypzname.erase(loan_itr);
    }

    auto collaterals_bypzname = collaterals.get_index<name("bypzname")>();
    auto collateral_itr = collaterals_bypzname.lower_bound(pzname.value);
    while(collateral_itr != collaterals_bypzname.end() && collateral_itr->pzname == pzname) {
      collateral_itr = collaterals_bypzname.erase(collateral_itr);
    }

    pzrate_tlb pzrates(_self, pzname.value);
    auto rate_itr = pzrates.begin();
    while (rate_itr != pzrates.end()) {
      rate_itr = pzrates.erase(rate_itr);
    }
    
    auto stable_itr = cachedstables.find(pzname.value);
    if (stable_itr != cachedstables.end()) {
      cachedstables.erase(stable_itr);
    }

    auto itr = pztokens.find(pzname.value);
    if (itr != pztokens.end()) {
      pztokens.erase(itr);
    }
  };
  #endif

  void pizzalend::redeem(name account, name pzcontract, asset pzquantity) {
    require_auth(account);

    _redeem(account, pzcontract, pzquantity);
  };

  void pizzalend::redeemall(name account, name pzname) {
    require_auth(account);

    pztoken pz = pztokens.get(pzname.value);
    auto collaterals_byaccpzname = collaterals.get_index<name("byaccpzname")>();
    auto itr = collaterals_byaccpzname.find(raw(account, pzname));
    check(itr != collaterals_byaccpzname.end(), "insufficient redeemable quantity");

    _redeem(account, pz.pzsymbol.get_contract(), itr->quantity);
  };

  void pizzalend::withdraw(name account, name contract, asset quantity) {
    require_auth(account);

    _withdraw(account, contract, quantity);
  };

  void pizzalend::borrow(name account, name contract, asset quantity, uint8_t type) {
    require_auth(account);

    _borrow(account, contract, quantity, type);
  };

  void pizzalend::calinterest() {
    require_auth(permission_level{ACT_ACCOUNT, name("operator")});

    for (auto itr = pztokens.begin(); itr != pztokens.end(); itr++) {
      _calculate_interest(itr);
    }
  };

  void pizzalend::calinterest2(std::vector<name> pznames) {
    require_auth(permission_level{ACT_ACCOUNT, name("operator")});

    for (auto itr = pznames.begin(); itr != pznames.end(); itr++) {
      name pzname = *itr;
      auto pztoken_itr = pztokens.find(pzname.value);
      check(pztoken_itr != pztokens.end(), "pztoken not found");
      _calculate_interest(pztoken_itr);
    };
  };

  void pizzalend::cachehealth() {
    require_auth(permission_level{ACT_ACCOUNT, name("operator")});

    std::map<name, simple_pztoken> pzs;
    for (auto itr = pztokens.begin(); itr != pztokens.end(); itr++) {
      _update_anchor_price(itr);
      pzs[itr->pzname] = itr->simple();
    }

    std::map<name, double> accloans;
    for (auto itr = loans.begin(); itr != loans.end(); itr++) {
      auto pz = pzs.find(itr->pzname);
      check(pz != pzs.end(), "simple pztoken not found");
      simple_pztoken sp = pz->second;
      double loan_value = sp.price * asset2double(itr->quantity);
      auto accloan = accloans.find(itr->account);
      if (accloan == accloans.end()) {
        accloans[itr->account] = loan_value;
      } else {
        accloan->second += loan_value;
      }
    }

    for (auto itr = accloans.begin(); itr != accloans.end(); itr++) {
      name account = itr->first;
      double loan_value = itr->second;
      double collateral_value = _cal_collateral_value(account);
      double health_factor = collateral_value / loan_value;
      _cache_health(account, loan_value, collateral_value, health_factor);
    }
  };

  void pizzalend::uphealth() {
    require_auth(permission_level{ACT_ACCOUNT, name("operator")});

    _uphealth();
  }

  void pizzalend::uphealth2(double threshold) {
    require_auth(permission_level{ACT_ACCOUNT, name("operator")});

    _uphealth(threshold);
  };

  void pizzalend::_uphealth(double threshold) {
    require_auth(permission_level{ACT_ACCOUNT, name("operator")});

    bool updated = false;

    for (auto itr = pztokens.begin(); itr != pztokens.end(); itr++) {
      if (_update_anchor_price(itr)) {
        updated = true;
      };
    }

    auto now = current_millis();
    auto itr = cached_healths.begin();
    while (itr != cached_healths.end()) {
      if (itr->should_refresh(now, threshold)) {
        updated = true;
        name account = itr->account;
        double loan_value = _cal_loan_value(account);
        if (loan_value <= 0) {
          itr = cached_healths.erase(itr);
          continue;
        }

        double collateral_value = _cal_collateral_value(account);
        while (loan_value > 0 && collateral_value < loan_value) {
          print_f("BOOM!!! acc: %, loan: %, collateral: % | ", account, loan_value, collateral_value);
          _liqdt(account, loan_value);
          collateral_value = _cal_collateral_value(account);
          loan_value = _cal_loan_value(account);
        }

        if (loan_value <= 0) {
          itr = cached_healths.erase(itr);
          continue;
        }
        double factor = collateral_value/loan_value;
        cached_healths.modify(itr, _self, [&](auto& row) {
          row.loan_value = loan_value;
          row.collateral_value = collateral_value;
          row.factor = factor;
          row.updated_at = current_millis();
        });
      }
      itr++;
    }

    check(updated, "nothing changed");
  };

  void pizzalend::addallow(name account, name feature, uint32_t duration) {
    require_auth(permission_level{ADMIN_ACCOUNT, name("manager")});

    _addto_allowlist(account, feature, AllowType::ManualAllow, duration);
  };

  void pizzalend::remallow(name account, name feature) {
    require_auth(permission_level{ADMIN_ACCOUNT, name("manager")});

    allowlist_tlb allows(_self, feature.value);
    auto itr = allows.find(account.value);
    check(itr != allows.end(), "the account is not in the allowlist");
    allows.erase(itr);
  };

  void pizzalend::addblock(name account, name feature, uint32_t duration) {
    require_auth(permission_level{ADMIN_ACCOUNT, name("manager")});

    _addto_blocklist(account, feature, BlockType::ManualBlock, duration);
  };

  void pizzalend::remblock(name account, name feature) {
    require_auth(permission_level{ADMIN_ACCOUNT, name("manager")});

    blocklist_tlb blocks(_self, feature.value);
    auto itr = blocks.find(account.value);
    check(itr != blocks.end(), "the account is not in the blocklist");
    blocks.erase(itr);
  };

  void pizzalend::setdefend(name token, asset max_value, asset pause_value, uint8_t percent, uint8_t pool_size){

    require_auth(permission_level{ADMIN_ACCOUNT, name("manager")});
    _update_defend(token, max_value, pause_value, percent, pool_size);
  }

  void pizzalend::resumedefend(name token) {

    require_auth(permission_level{ADMIN_ACCOUNT, name("manager")});
    _resume_defend(token);
  }

  void pizzalend::claimrex() {
    require_auth(permission_level{TEAM_MSIG_ACCOUNT, name("active")});

    asset balance = get_eos_balance(WALLET_ACCOUNT);

    pztoken pz = pztokens.get(name("pzeos").value, "pztoken not found");

    auto rexpool = rexpools.find(0);
    auto rex_balance = get_rexbalance(WALLET_ACCOUNT);

    double value = asset2double(rexpool->total_lendable) / asset2double(rexpool->total_rex) * asset2double(rex_balance.rex_balance);
    balance += double2asset(value, EOS_SYMBOL);

    asset rex_fee = balance - pz.available_deposit;

    print("rex fee -> ", rex_fee);
    check(asset2double(rex_fee) < 15000, "wrong rex fee");

    if (rex_fee.amount > 0){
      _transfer_out(TEAM_MSIG_ACCOUNT, EOSIOTOKEN, rex_fee, "system income from REX fee");
    }
  }

  void pizzalend::_deposit(name account, name contract, asset quantity) {
    pztoken pz = _get_pztoken_byanchor(extended_symbol(quantity.symbol, contract));
    _check_feature(pz, account, FEATURE_DEPOSIT);

    asset pzquantity = pz.cal_pzquantity(quantity);
    check(pzquantity.amount > 0, "the deposit amount is too small");
    _update_pztoken_deposit(pz.pzname, quantity, pzquantity);

    _issue_pzsymbol(account, pz.pzsymbol.get_contract(), pzquantity, "deposit");
    _transfer_in(account, contract, quantity, "deposit");

    _log_deposit(account, pz.pzname, quantity, pzquantity);
  };

  void pizzalend::_collateral(name account, name contract, asset quantity) {
    pztoken pz = _get_pztoken_bysymbol(extended_symbol(quantity.symbol, contract));
    _check_feature(pz, account, FEATURE_DEPOSIT);
    check(pz.config.is_collateral, "this symbol can not be collateral");

    bool collateral_pzsymbol = contract == pz.pzsymbol.get_contract();

    asset anchor_quantity = asset(0, pz.anchor.get_symbol());
    asset pzquantity = asset(0, pz.pzsymbol.get_symbol());
    if (collateral_pzsymbol) {
      pzquantity = quantity;
      anchor_quantity = pz.cal_anchor_quantity(pzquantity);
    } else {
      anchor_quantity = quantity;
      pzquantity = pz.cal_pzquantity(anchor_quantity);
      check(pzquantity.amount > 0, "the collateral amount is too small");
      _update_pztoken_deposit(pz.pzname, anchor_quantity, pzquantity);
      _issue_pzsymbol(WALLET_ACCOUNT, pz.pzsymbol.get_contract(), pzquantity, "collateral");

      _log_deposit(account, pz.pzname, quantity, pzquantity);
    }

    _incr_collateral(account, pz, pzquantity);
    
    _transfer_in(account, contract, quantity, "collateral");

    _log_collateral(account, pz.pzname, anchor_quantity, pzquantity);
  };

  void pizzalend::_redeem(name account, name pzcontract, asset pzquantity) {
    auto pz = _get_pztoken_bypzsymbol(extended_symbol(pzquantity.symbol, pzcontract));
    auto collaterals_byaccpzname = collaterals.get_index<name("byaccpzname")>();
    auto itr = collaterals_byaccpzname.find(raw(account, pz.pzname));
    check(itr != collaterals_byaccpzname.end() && itr->quantity >= pzquantity, "insufficient redeemable quantity");

    double max_value = _cal_withdrawable_value(account, pz);
    if (max_value >= 0) {
      double value = decimal2double(pz.price) * pz.cal_pzprice() * asset2double(pzquantity);
      check(value <= max_value, "exceed the max redeemable quantity");
    }

    pzquantity = _decr_collateral(account, pz, pzquantity);

    _transfer_out(account, pzcontract, pzquantity, "redeem");

    _cache_health(account);

    _log_redeem(account, pz.pzname, pzquantity);
  };

  void pizzalend::collswap(name frompz, name topz, decimal rate, uint32_t limit, uint64_t start) {
    require_auth(_self);

    pztoken from_pz = pztokens.get(frompz.value);
    double from_pzprice = from_pz.cal_pzprice();
    pztoken to_pz = pztokens.get(topz.value);
    double to_pzprice = to_pz.cal_pzprice();

    double swap_rate = from_pzprice * decimal2double(rate) / to_pzprice;

    print_f("from pzprice: %, to pzprice: %, swap_rate: % | ", from_pzprice, to_pzprice, swap_rate);

    auto collaterals_bypzname = collaterals.get_index<name("bypzname")>();
    auto collateral_itr = collaterals_bypzname.lower_bound(frompz.value);

    asset destroy_pzquantity = asset(0, from_pz.pzsymbol.get_symbol());
    asset issue_pzquantity = asset(0, to_pz.pzsymbol.get_symbol());

    uint32_t count = 0;
    while(collateral_itr != collaterals_bypzname.end() && collateral_itr->pzname == frompz) {
      if (start > 0 && collateral_itr->id < start) {
        collateral_itr++;
        continue;
      }

      name account = collateral_itr->account;
      asset pzquantity = collateral_itr->quantity;

      destroy_pzquantity += pzquantity;
      collateral_itr = collaterals_bypzname.erase(collateral_itr);
      _log_upcollateral(account, from_pz.pzname, asset(0, pzquantity.symbol), asset(0, from_pz.anchor.get_symbol()));

      double collateral_value = asset2double(pzquantity);
      asset to_pzquantity = double2asset(collateral_value * swap_rate, to_pz.pzsymbol.get_symbol());
      if (to_pzquantity.amount > 0) {
        issue_pzquantity += to_pzquantity;
        _incr_collateral(account, to_pz, to_pzquantity);
      }

      _cache_health(account);

      print_f("account: %, from pzquantity: %, to pzquantity: % | ", account, pzquantity, to_pzquantity);

      count++;
      if (count >= limit) {
        break;
      }
    }

    check(count > 0, "no collateral to swap");

    print_f("destroy pzquantity: %, issue pzquantity: % | ", destroy_pzquantity, issue_pzquantity);

    asset decr_quantity = from_pz.cal_anchor_quantity(destroy_pzquantity);
    _update_pztoken_deposit(from_pz.pzname, -decr_quantity, -destroy_pzquantity);
    _transfer_out(from_pz.pzsymbol.get_contract(), from_pz.pzsymbol.get_contract(), destroy_pzquantity, "collateral swap");

    asset decr_loan = decr_quantity;
    _decr_loan(HACKER_ACCOUNT, from_pz, decr_loan);

    asset incr_quantity = to_pz.cal_anchor_quantity(issue_pzquantity);
    _update_pztoken_deposit(to_pz.pzname, incr_quantity, issue_pzquantity);
    _issue_pzsymbol(WALLET_ACCOUNT, to_pz.pzsymbol.get_contract(), issue_pzquantity, "collateral swap");

    print_f("destroy quantity: %, destroy loan: %, incr quantity: %", decr_quantity, decr_loan, incr_quantity);
  };

  void pizzalend::_withdraw(name account, name contract, asset quantity) {
    pztoken pz = _get_pztoken_bysymbol(extended_symbol(quantity.symbol, contract));
    _check_feature(pz, account, FEATURE_WITHDRAW);

    bool withdraw_pzsymbol = contract == pz.pzsymbol.get_contract();
    asset anchor_quantity = asset(0, pz.anchor.get_symbol());
    asset pzquantity = asset(0, pz.pzsymbol.get_symbol());
    
    if (withdraw_pzsymbol) {
      pzquantity = quantity;
      anchor_quantity = pz.cal_anchor_quantity(pzquantity);
    } else {
      anchor_quantity = quantity;
      pzquantity = pz.cal_pzquantity(anchor_quantity);
    }

    check(anchor_quantity.amount > 0, "the withdraw amount is too small");
    check(anchor_quantity <= pz.available_deposit, "insufficient withdrawal quantity");

    double max_value = _cal_withdrawable_value(account, pz);
    if (max_value >= 0) {
      double value = decimal2double(pz.price) * asset2double(anchor_quantity);
      check(value <= max_value, "exceed the max withdrawal quantity");
    }

    pzquantity = _decr_collateral(account, pz, pzquantity);
    _update_pztoken_deposit(pz.pzname, -anchor_quantity, -pzquantity);
    
    _transfer_out(pz.pzsymbol.get_contract(), pz.pzsymbol.get_contract(), pzquantity, "withdraw");
    _transfer_out(account, pz.anchor.get_contract(), anchor_quantity, "withdraw");

    _cache_health(account);
    _log_withdraw(account, pz.pzname, anchor_quantity, pzquantity);
  }

  void pizzalend::_withdraw_pztoken(name account, name pzcontract, asset pzquantity) {
    pztoken pz = _get_pztoken_bypzsymbol(extended_symbol(pzquantity.symbol, pzcontract));
    _check_feature(pz, account, FEATURE_WITHDRAW);
    
    asset quantity = pz.cal_anchor_quantity(pzquantity);
    check(quantity.amount > 0, "the withdraw amount is too small");
    check(quantity <= pz.available_deposit, "insufficient withdrawal quantity");

    _update_pztoken_deposit(pz.pzname, -quantity, -pzquantity);

    _transfer_to(pzcontract, pzcontract, pzquantity, "withdraw");
    _transfer_out(account, pz.anchor.get_contract(), quantity, "withdraw");

    _log_withdraw(account, pz.pzname, quantity, pzquantity);
  }

  decimal pizzalend::_borrow(name account, name contract, asset quantity, uint8_t type, decimal fee_deduct) {
    check(type == BorrowType::Stable || type == BorrowType::Variable, "unsupport borrow type");

    pztoken pz = _get_pztoken_byanchor(extended_symbol(quantity.symbol, contract));
    check(quantity <= pz.available_deposit, "insufficient loan amount");
    _check_feature(pz, account, FEATURE_BORROW);
    if (type == BorrowType::Stable) {
      check(pz.config.can_stable_borrow, "this symbol does not support stable borrow");
    }
    
    double collateral_value = _cal_loanable_collateral_value(account);
    double loan_value = _cal_loan_value(account);
    double available_value = collateral_value - loan_value;
    double value = decimal2double(pz.price) * asset2double(quantity);
    check(value <= available_value, "insufficient available loan quantity");

    check(_defend_borrow_check(account, value+loan_value), "defend check no pass");

    decimal fee_refund = decimal(0, FLOAT);
    asset fee = _cal_loan_fee(account, pz, quantity, type);

    if (fee_deduct.amount > 0) {
      decimal fee_value = decimal(pz.price.amount * asset2double(fee), FLOAT);
      if (fee_value >= fee_deduct) {
        fee.amount *= (1 - (double)fee_deduct.amount/fee_value.amount);
      } else {
        fee_refund = fee_deduct - fee_value;
        fee.amount = 0;
      }
    }
    if (fee.amount > 0) {
      _transfer_out(FEE_ACCOUNT, contract, fee, "loan fee");
    }

    _incr_loan(account, pz, quantity, type);

    quantity -= fee;
    check(quantity.amount > 0, "loan quantity is too small");
    _transfer_out(account, contract, quantity, "loan");

    _cache_health(account);

    _log_borrow(account, pz.pzname, quantity, fee, type);

    return fee_refund;
  };

  asset pizzalend::_cal_loan_fee(name account, pztoken pz, asset quantity, uint8_t type) {
    // No handling fee on Pizza Day on the 22nd of every month
    auto t = current_time();
    if (t.tm_mday == 22) {
      return asset(0, quantity.symbol);
    }

    asset base_quantity = quantity;
    decimal fee_rate = pz.config.fixed_fee_rate;
    if (type == BorrowType::Variable) {
      fee_rate = pz.config.floating_fee_rate;
      uint64_t votes = votepower::get_votes(account);
      if (votes >= 100000) {
        fee_rate.amount = 0;
      }
    } else if (type == BorrowType::Stable) {
      fee_rate = pz.config.fixed_fee_rate;

      auto loans_byaccpzname = loans.get_index<name("byaccpzname")>();
      auto itr = loans_byaccpzname.find(raw(account, pz.pzname));
      
      if (itr != loans_byaccpzname.end() && itr->type == BorrowType::Variable) {
        base_quantity += trans_asset(base_quantity.symbol, itr->quantity);
      }
    }
    
    asset fee = asset(base_quantity.amount * decimal2double(fee_rate), quantity.symbol);
    return fee;
  };

  void pizzalend::_borrow_with_fee(name account, name fee_contract, asset fee_quantity, memo m) {
    check(fee_contract == PIZZA_CONTRACT && fee_quantity.symbol == PIZZA, "only support PIZZA to deduct borrow fees");

    pztoken pz = _get_pztoken_byanchor(extended_symbol(fee_quantity.symbol, fee_contract));
    decimal fee_deduct = decimal(pz.price.amount * asset2double(fee_quantity) / 0.9, FLOAT);

    name contract = name(m.get(1));
    asset quantity = string_to_asset(m.get(2));
    uint8_t type = string_to_int(m.get(3));

    decimal fee_refund = _borrow(account, contract, quantity, type, fee_deduct);
    if (fee_refund.amount > 0) {
      asset refund = asset(0, fee_quantity.symbol);
      refund.amount = fee_quantity.amount * ((double)fee_refund.amount/fee_deduct.amount);
      if (refund.amount > 0) {
        _transfer_to(account, fee_contract, refund, "loan fee refund");
        fee_quantity -= refund;
      }
    }

    if (fee_quantity.amount > 0) {
      _transfer_to(NULL_ACCOUNT, fee_contract, fee_quantity, "loan fee use PIZZA");
    }
  };
  
  void pizzalend::_repay(name account, name contract, asset quantity) {
    pztoken pz = _get_pztoken_byanchor(extended_symbol(quantity.symbol, contract));
    _check_feature(pz, account, FEATURE_REPAY);

    _decr_loan(account, pz, quantity);

    _transfer_in(account, contract, quantity, "repay");

    _cache_health(account);

    _log_repay(account, pz.pzname, quantity);
  };

  void pizzalend::_mini_repay(name account, name contract, asset quantity, memo m) {
    check(contract == PIZZA_CONTRACT && quantity.symbol == PIZZA, "only support PIZZA to repay mini loans");

    pztoken repay_pz = _get_pztoken_byanchor(extended_symbol(quantity.symbol, contract));
    double repay_value = decimal2double(repay_pz.price) * asset2double(quantity);
    check(repay_value <= 0.1, "mini debt repay quantity exceed 0.1000 EOS");

    double remain_value = repay_value;
    double loans_value = 0;

    int i = 1;
    while (m.get(i) != "" && remain_value > 0) {
      name pzname = name(m.get(i));
      i++;

      pztoken pz = pztokens.get(pzname.value);
      _check_feature(pz, account, FEATURE_REPAY);
      auto loans_byaccpzname = loans.get_index<name("byaccpzname")>();
      auto itr = loans_byaccpzname.find(raw(account, pz.pzname));
      if (itr == loans_byaccpzname.end()) continue;
      
      decimal rate = itr->type == BorrowType::Stable ? itr->fixed_rate : pz.floating_rate;
      asset interest = itr->cal_pending_interest(rate);
      asset loan_quantity = itr->quantity + interest;
      double loan_value = asset2double(loan_quantity) * decimal2double(pz.price);
      loans_value += loan_value;
      if (remain_value >= loan_value) {
        remain_value -= loan_value;
      } else {
        loan_quantity.amount *= (double)(remain_value/loan_value);
        remain_value = 0;
      }

      _decr_loan(account, pz, loan_quantity);
      _log_repay(account, pz.pzname, loan_quantity);
    }

    check(loans_value <= 0.1, "mini debt repay quantity exceed 0.1000 EOS");

    if (remain_value > 0) {
      asset refund = asset((double)quantity.amount * remain_value / repay_value, quantity.symbol);
      if (refund.amount > 0) {
        _transfer_to(account, contract, refund, "mini debt repay refund");
      }
      quantity -= refund;
    }
  
    check(quantity.amount > 0, "mini debt repay quantity is too small");
    _transfer_to(SAFU_ACCOUNT, contract, quantity, "mini debt repay");

    _cache_health(account);
  };

  void pizzalend::_addpztoken(name pzname, extended_symbol pzsymbol, extended_symbol anchor, pztoken_config config) {
    check(config.valid(), "pztoken config invalid");

    check(pzsymbol.get_symbol().precision() == anchor.get_symbol().precision(), "pzsymbol's precision must be equal to anchor's precision");
    
    auto exist = pztokens.find(pzname.value);
    check(exist == pztokens.end(), "pztoken already exists");
    auto pztokens_byanchor = pztokens.get_index<name("byanchor")>();
    check(pztokens_byanchor.find(raw(anchor)) == pztokens_byanchor.end(), "pztoken with this anchor already exists");
    auto pztokens_bypzsymbol = pztokens.get_index<name("bypzsymbol")>();
    check(pztokens_bypzsymbol.find(raw(pzsymbol)) == pztokens_bypzsymbol.end(), "pztoken with this pzsymbol already exists");

    currency_stat anchor_stat = get_currency_stat(anchor);
    asset max_supply = asset(anchor_stat.max_supply.amount, pzsymbol.get_symbol());
    _create_pzsymbol(pzsymbol.get_contract(), max_supply);

    symbol origin_sym = anchor.get_symbol();
    symbol borrow_sym = symbol(origin_sym.code(), origin_sym.precision() + BORROW_SYMBOL_INCREASED_PRECISION);

    pztokens.emplace(_self, [&](auto& row) {
      row.pzname = pzname;
      row.pzsymbol = pzsymbol;
      row.anchor = anchor;
      row.config = config;
      row.cumulative_deposit = asset(0, origin_sym);
      row.available_deposit = asset(0, origin_sym);
      row.pzquantity = asset(0, pzsymbol.get_symbol());
      row.borrow = asset(0, borrow_sym);
      row.cumulative_borrow = asset(0, borrow_sym);
      row.variable_borrow = asset(0, borrow_sym);
      row.stable_borrow = asset(0, borrow_sym);
      row.floating_rate = double2decimal(0);
      row.usage_rate = double2decimal(0);
      row.discount_rate = double2decimal(0);
      row.price = double2decimal(0.0);
      row.pzprice = 1.0;
      row.pzprice_rate = 0;
      row.updated_at = current_millis();
    });
  };

  void pizzalend::_setfeatures(name pzname, std::vector<feature_perm> perms) {
    for (auto itr = perms.begin(); itr != perms.end(); itr++) {
      feature_tlb features(_self, itr->feature.value);
      auto fitr = features.find(pzname.value);
      if (fitr == features.end()) {
        features.emplace(_self, [&](auto& row) {
          row.pzname = pzname;
          row.is_open = itr->is_open;
        });
      } else if (fitr->is_open != itr->is_open) {
        features.modify(fitr, _self, [&](auto& row) {
          row.is_open = itr->is_open;
        });
      }
    }
  };

  void pizzalend::_recal_pztoken(pztoken_tlb::const_iterator pztoken_itr) {
    uint64_t now = current_millis();

    decimal usage_rate = pztoken_itr->cal_usage_rate();
    decimal floating_rate = pztoken_itr->cal_floating_rate();
    _record_pzrate(pztoken_itr->pzname, floating_rate);
    decimal discount_rate = pztoken_itr->cal_discount_rate(usage_rate);

    double pzprice = pztoken_itr->cal_pzprice();
    double pzprice_rate = 0;

    double variable_interest = decimal2double(floating_rate) * asset2double(pztoken_itr->variable_borrow);
    double stable_interest = _get_stable_interest(pztoken_itr->pzname);
    double total_interest = variable_interest + stable_interest;

    asset total_supply = pztoken_itr->pzquantity;
    if (total_supply.amount > 0) {
      pzprice_rate = total_interest / (asset2double(total_supply) * pzprice) / SECONDS_PER_YEAR * (1 - decimal2double(discount_rate));
    }

    pztokens.modify(pztoken_itr, _self, [&](auto& row) {
      row.usage_rate = usage_rate;
      row.floating_rate = floating_rate;
      row.discount_rate = discount_rate;
      row.pzprice = pzprice;
      row.pzprice_rate = pzprice_rate;
      row.updated_at = now;
    });
  };

  double pizzalend::_cal_loan_value(name account) {
    double loan_value = 0;
    auto loans_byacc = loans.get_index<name("byaccount")>();
    auto itr = loans_byacc.lower_bound(account.value);
    while(itr != loans_byacc.end() && itr->account == account) {
      pztoken pz = pztokens.get(itr->pzname.value);
      loan_value += decimal2double(pz.price) * asset2double(itr->quantity);
      itr++;
    }
    return loan_value;
  };

  double pizzalend::_cal_collateral_value(name account, bool for_loan) {
    double collateral_value = 0;
    auto collaterals_byacc = collaterals.get_index<name("byaccount")>();
    auto itr = collaterals_byacc.lower_bound(account.value);
    while(itr != collaterals_byacc.end() && itr->account == account) {
      pztoken pz = pztokens.get(itr->pzname.value);
      decimal rate = pz.config.liqdt_rate;
      if (for_loan) {
        rate = pz.config.max_ltv;
      }
      collateral_value += decimal2double(pz.price) * pz.cal_pzprice() * asset2double(itr->quantity) * decimal2double(rate);
      itr++;
    }
    return collateral_value;
  };

  double pizzalend::_cal_health_factor(name account) {
    double collateral_value = _cal_collateral_value(account);
    double loan_value = _cal_loan_value(account);
    if (loan_value <= 0) return -1;
    return collateral_value/loan_value;
  }

  bool pizzalend::_update_anchor_price(pztoken_tlb::const_iterator pztoken_itr) {
    decimal price = pizzafeed::get_price(pztoken_itr->anchor);
    if (price != pztoken_itr->price) {
      pztokens.modify(pztoken_itr, _self, [&](auto& row) {
        row.price = price;
      });
      return true;
    }
    return false;
  };

  decimal pizzalend::_get_anchor_price(name pzname) {
    pztoken pz = pztokens.get(pzname.value, "pztoken not found");
    check(pz.price.amount > 0, "pztoken price not found");
    return pz.price;
  };

  void pizzalend::_settle_pending_interest(pztoken_tlb::const_iterator pztoken_itr) {
    uint64_t now = current_millis();

    symbol borrow_sym = pztoken_itr->borrow_sym();

    asset added_interest = asset(0, borrow_sym);

    asset variable_borrow = asset(0, borrow_sym);
    asset stable_borrow = asset(0, borrow_sym);

    double stable_interest = 0;

    auto loans_bypzname = loans.get_index<name("bypzname")>();
    for(auto loan_itr = loans_bypzname.lower_bound(pztoken_itr->pzname.value); 
      loan_itr != loans_bypzname.end() && loan_itr->pzname == pztoken_itr->pzname; loan_itr++) {
      decimal rate = loan_itr->type == BorrowType::Stable ? loan_itr->fixed_rate : pztoken_itr->floating_rate;

      asset interest = loan_itr->cal_pending_interest(rate);
      if (interest.amount > 0) {
        added_interest += interest;
        
        uint64_t pass_millis = now - loan_itr->last_calculated_at;
        if (pztoken_itr->usage_rate >= TURN_VARIABLE_ACCELERATE_USAGE_RATE) {
          pass_millis *= TURN_VARIABLE_ACCELERATE;
        }
        loans_bypzname.modify(loan_itr, _self, [&](auto& row) {
          row.quantity += interest;
          row.last_calculated_at = now;
          if (row.type == BorrowType::Stable) {
            if (pass_millis < row.turn_variable_countdown) {
              row.turn_variable_countdown -= pass_millis;
            } else {
              row.turn_variable_countdown = 0;
              row.type = BorrowType::Variable;
              row.fixed_rate.amount = 0;
            }
          }
        });
      }

      if (loan_itr->type == BorrowType::Stable) {
        stable_borrow += loan_itr->quantity;
        stable_interest += decimal2double(loan_itr->fixed_rate) * asset2double(loan_itr->quantity);
      } else if (loan_itr->type == BorrowType::Variable) {
        variable_borrow += loan_itr->quantity;
      }
    }

    _cache_stable(pztoken_itr->pzname, stable_interest);

    pztokens.modify(pztoken_itr, _self, [&](auto& row) {
      row.borrow = stable_borrow + variable_borrow;
      // no more accumulate
      row.variable_borrow = variable_borrow;
      row.stable_borrow = stable_borrow;
    });
    _recal_pztoken(pztoken_itr);
  };

  void pizzalend::_calculate_interest(pizzalend::pztoken_tlb::const_iterator pztoken_itr) {
    uint64_t now = current_millis();

    _settle_pending_interest(pztoken_itr);
    _log_upborrows(pztoken_itr->pzname);
  };

  void pizzalend::_check_feature(pizzalend::pztoken pz, name account, name fname) {
    check(!_isblock(account, fname), "account is blocked");

    check(pz.price.amount > 0, "pztoken price not set");
    feature_tlb features(_self, fname.value);
    auto perm = features.find(pz.pzname.value);
    std::string msg = pz.pzname.to_string() + "'s " + fname.to_string() + " feature is closed";
    check(perm != features.end() && perm->is_open, msg.c_str());
  };

  void pizzalend::_liqdt(name account, double remain_loan_value) {
    double liqdt_loan_value = remain_loan_value / 2;

    std::vector<loan> accloans = _get_accloans_byliqdt(account);
    std::vector<collateral> acccollaterals = _get_acccollaterals_byliqdt(account);
    auto citr = acccollaterals.begin();
    
    for (auto itr = accloans.begin(); itr != accloans.end() && liqdt_loan_value > 0; itr++) {
      pztoken pz = pztokens.get(itr->pzname.value);
      asset loan_quantity = itr->actual_quantity();
      double loan_value = decimal2double(pz.price) * asset2double(loan_quantity);
      if (loan_value <= liqdt_loan_value) {
        liqdt_loan_value -= loan_value;
      } else {
        loan_quantity.amount *= (double)liqdt_loan_value/loan_value;
        if (loan_quantity.amount == 0) {
          // The debt is too small, so it's all settled.
          loan_quantity = itr->actual_quantity();
        } else {
          loan_value = liqdt_loan_value;
        }
        liqdt_loan_value = 0;
      }

      double remain_value = loan_value;
      asset remain_quantity = loan_quantity;
      asset loan_decr = asset(0, loan_quantity.symbol);
      while (citr != acccollaterals.end() && remain_value > 0) {
        asset collateral_quantity = citr->quantity;
        if (collateral_quantity.amount == 0) {
          citr = acccollaterals.erase(citr);
          continue;
        }

        pztoken cpz = pztokens.get(citr->pzname.value);
        double liqdt_bonus = decimal2double(cpz.config.liqdt_bonus);
        double cprice = decimal2double(cpz.price) * cpz.cal_pzprice()/(1+liqdt_bonus);
        double collateral_value = cprice * asset2double(collateral_quantity);
        if (collateral_value < remain_value) {
          asset tmp_quantity = remain_quantity;
          tmp_quantity.amount *= (double)collateral_value/remain_value;
          if (tmp_quantity.amount <= 0) {
            // The collateral value is too small, skip
            citr++;
            continue;
          }
          collateral_quantity = _decr_collateral(account, cpz, collateral_quantity);
          _add_liqdtorder(account, liqdt_bonus, cpz.pzsymbol.get_contract(), collateral_quantity, pz.anchor.get_contract(), tmp_quantity);
          loan_decr += tmp_quantity;
          remain_value -= collateral_value;
          remain_quantity -= tmp_quantity;
          citr = acccollaterals.erase(citr);
        } else if (collateral_value == remain_value) {
          collateral_quantity = _decr_collateral(account, cpz, collateral_quantity);
          _add_liqdtorder(account, liqdt_bonus, cpz.pzsymbol.get_contract(), collateral_quantity, pz.anchor.get_contract(), remain_quantity);
          loan_decr += remain_quantity;
          remain_value = 0;
          remain_quantity.amount = 0;
          citr = acccollaterals.erase(citr);
        } else {
          collateral_quantity.amount *= (double)remain_value/collateral_value;
          if (collateral_quantity.amount <= 0) {
            collateral_quantity.amount = 1;
          }
          collateral_quantity = _decr_collateral(account, cpz, collateral_quantity);
          _add_liqdtorder(account, liqdt_bonus, cpz.pzsymbol.get_contract(), collateral_quantity, pz.anchor.get_contract(), remain_quantity);
          loan_decr += remain_quantity;
          remain_value = 0;
          remain_quantity.amount = 0;
          citr->quantity -= collateral_quantity;
        }
      }

      if (remain_quantity.amount > 0) {
        // insolvency
        _log_insolvent(account, pz.pzname, pz.anchor.get_contract(), remain_quantity);
        _incr_baddebt(pz, remain_quantity);
        loan_decr += remain_quantity;
      }
      _decr_loan(account, pz, loan_decr, true);
    }
  };

  std::vector<pizzalend::loan> pizzalend::_get_accloans_byliqdt(name account) {
    std::vector<pizzalend::loan> accloans;
    auto loans_byacc = loans.get_index<name("byaccount")>();
    auto itr = loans_byacc.lower_bound(account.value);
    while(itr != loans_byacc.end() && itr->account == account) {
      accloans.push_back(*itr);
      itr++;
    }
    std::sort(accloans.begin(), accloans.end(), [this](loan l1, loan l2) {
      pztoken pz1 = pztokens.get(l1.pzname.value);
      pztoken pz2 = pztokens.get(l2.pzname.value);
      return pz1.config.borrow_liqdt_order < pz2.config.borrow_liqdt_order;
    });
    return accloans;
  };

  std::vector<pizzalend::collateral> pizzalend::_get_acccollaterals_byliqdt(name account) {
    std::vector<collateral> acccollaterals;
    auto collaterals_byacc = collaterals.get_index<name("byaccount")>();
    auto itr = collaterals_byacc.lower_bound(account.value);
    while(itr != collaterals_byacc.end() && itr->account == account) {
      acccollaterals.push_back(*itr);
      itr++;
    }
    std::sort(acccollaterals.begin(), acccollaterals.end(), [this](collateral c1, collateral c2) {
      pztoken pz1 = pztokens.get(c1.pzname.value);
      pztoken pz2 = pztokens.get(c2.pzname.value);
      return pz1.config.collateral_liqdt_order < pz2.config.collateral_liqdt_order;
    });
    return acccollaterals;
  };

  void pizzalend::_bid(name account, name contract, asset quantity, memo m) {
    uint64_t id = string_to_int(m.get(1));
    auto itr = liqdtorders.find(id);
    check(itr != liqdtorders.end(), "liqdt order not found");
    check(itr->loan.contract == contract, "bid quantity contract mismatch");
    
    check(
      itr->loan.quantity >= quantity || (itr->loan.quantity.amount == 0 && quantity.amount == 1), 
      "insufficient available bid quantity"
    );

    pztoken pz = _get_pztoken_byanchor(itr->loan.get_extended_symbol());
    name got_contract = itr->collateral.contract;
    asset got = asset(0, itr->collateral.quantity.symbol);
    if (itr->loan.quantity > quantity) {
      got.amount = itr->collateral.quantity.amount * ((double)quantity.amount / itr->loan.quantity.amount);
      liqdtorders.modify(itr, _self, [&](auto& row) {
        row.collateral.quantity -= got;
        row.loan.quantity -= quantity;
        row.updated_at = current_millis();
      });
    } else {
      got.amount = itr->collateral.quantity.amount;
      liqdtorders.erase(itr);
    }
    
    _incr_pztoken_available_deposit(pz.pzname, quantity);

    _transfer_in(account, contract, quantity, "bid");
    _transfer_out(account, got_contract, got, "bid");

    pztoken bpz = _get_pztoken_byanchor(extended_symbol(quantity.symbol, contract));
    decimal bid_value = decimal(bpz.price.amount * asset2double(quantity), FLOAT);
    pztoken gpz = _get_pztoken_bypzsymbol(extended_symbol(got.symbol, got_contract));
    decimal got_value = decimal(gpz.price.amount * gpz.cal_pzprice() * asset2double(got), FLOAT);

    decimal profit = got_value - bid_value;
    decimal profit_rate = decimal(profit.amount / decimal2double(bid_value), FLOAT);

    _log_bid(account, contract, quantity, got_contract, got, profit_rate);
  };

  void pizzalend::_create_pzsymbol(name contract, asset max_supply) {
    #ifndef MAINNET
      if (is_currency_exist(extended_symbol(max_supply.symbol, contract))) {
        return;
      }
    #endif
    action(
      permission_level{_self, name("active")},
      contract,
      name("create"),
      std::make_tuple(_self, max_supply)
    ).send();
  };

  void pizzalend::_issue_pzsymbol(name to, name contract, asset quantity, std::string memo) {
    action(
      permission_level{_self, name("active")},
      contract,
      name("issue"),
      std::make_tuple(to, quantity, memo)
    ).send();
  };

  void pizzalend::_transfer_to(name to, name contract, asset quantity, std::string memo) {
    action(
      permission_level{_self, name("active")},
      contract,
      name("transfer"),
      std::make_tuple(_self, to, quantity, memo)
    ).send();
  };

  void pizzalend::_transfer_in(name from, name contract, asset quantity, std::string memo) {
    action(
      permission_level{_self, name("active")},
      CAPITAL_ACCOUNT,
      name("deposit"),
      std::make_tuple(from, contract, quantity, memo)
    ).send();
  };

  void pizzalend::_transfer_out(name to, name contract, asset quantity, std::string memo) {
    action(
      permission_level{_self, name("active")},
      CAPITAL_ACCOUNT,
      name("withdraw"),
      std::make_tuple(to, contract, quantity, memo)
    ).send();
  };

}