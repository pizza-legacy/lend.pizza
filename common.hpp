#pragma once

#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <eosio/symbol.hpp>
#include <eosio/system.hpp>
#include <libc/stdint.h>

#include <math.h>
#include <time.h>

using namespace eosio;

#define MAINNET

// exact float number
#define FLOAT symbol("F", 8)

typedef asset decimal;

decimal double2decimal(double a) {
  double tmp = a * pow(10, FLOAT.precision());
  return asset(int64_t(tmp), FLOAT);
};

double decimal2double(decimal d) {
  return (double)d.amount / pow(10, d.symbol.precision());
};

double asset2double(asset a) {
  return (double)a.amount / pow(10, a.symbol.precision());
};

asset double2asset(double a, symbol s) {
  double tmp = a * pow(10, s.precision());
  return asset(int64_t(tmp), s);
};

template <typename KeyT, typename ValueT>
ValueT get_or(std::map<KeyT, ValueT> m, KeyT key, ValueT default_value) {
  auto itr = m.find(key);
  if (itr != m.end()) {
    return itr->second;
  }
  return default_value;
};

uint32_t current_secs() {
  time_point tp = current_time_point();
  return tp.sec_since_epoch();
};

uint32_t current_hour() {
  uint32_t secs = current_secs();
  return secs - secs%3600;
}

// current millis
tm current_time() {
  time_t t = current_time_point().sec_since_epoch();
  return *gmtime(&t);
}

// 当前毫秒时间戳
uint64_t current_millis() {
  time_point tp = current_time_point();
  return tp.time_since_epoch().count()/1000;
};

uint128_t raw(name n1, name n2) {
  return (uint128_t)n1.value << 64 | n2.value;
};

uint128_t raw(extended_symbol sym) {
  return (uint128_t)sym.get_contract().value << 64 | sym.get_symbol().raw();
};

asset trans_asset(symbol sym, asset quantity) {
  if (sym == quantity.symbol) return quantity;
  int64_t amount = quantity.amount * pow(10, (int8_t)sym.precision() - quantity.symbol.precision());
  return asset(amount, sym);
};

std::vector<std::string> split_string(const std::string& s, const std::string& delimiter = " ") {
  auto last_pos = s.find_first_not_of(delimiter, 0);
  auto pos = s.find_first_of(delimiter, last_pos);

  std::vector<std::string> ss;
  while(std::string::npos != pos || std::string::npos != last_pos) {
    ss.push_back(s.substr(last_pos, pos - last_pos));
    last_pos = s.find_first_not_of(delimiter, pos);
    pos = s.find_first_of(delimiter, last_pos);
  }
  return ss;
}; 

int string_to_int(const std::string& s) {
  return atoi(s.c_str());
};

// 4,EOS
symbol string_to_symbol(const std::string& s) {
  check(!s.empty(), "creating symbol from empty string");
  auto comma_pos = s.find(',');
  check(comma_pos != std::string::npos, "missing comma in symbol");
  auto prec_part = s.substr(0, comma_pos);
  uint8_t p = string_to_int(prec_part);
  check(p <= 18, "precision should be <= 18");
  std::string name_part = s.substr(comma_pos + 1);
  return symbol(name_part, p);
};

asset string_to_asset(const std::string& s) {
  auto space_pos = s.find(' ');
  check(space_pos != std::string::npos, "asset's amount and symbol should be separated with space");
  auto symbol_str = s.substr(space_pos + 1);
  auto amount_str = s.substr(0, space_pos);

  auto dot_pos = amount_str.find('.');
  if (dot_pos != std::string::npos) {
    check(dot_pos != amount_str.size() - 1, "missing decimal fraction after decimal point");
  }

  std::string precision_digit_str = "0";
  if (dot_pos != std::string::npos) {
    precision_digit_str = std::to_string(amount_str.size() - dot_pos - 1);
  }

  std::string symbol_part = precision_digit_str + ',' + symbol_str;
  symbol sym = string_to_symbol(symbol_part);

  int64_t int_part, fract_part;
  if (dot_pos != std::string::npos) {
    int_part = atoll(amount_str.substr(0, dot_pos).c_str());
    fract_part = atoll(amount_str.substr(dot_pos + 1).c_str());
    if (amount_str[0] == '-') fract_part *= -1;
  } else {
    int_part = atoll(amount_str.c_str());
  }

  int64_t amount = int_part;
  amount *= pow(10, sym.precision());
  amount += fract_part;

  return asset(amount, sym);
};