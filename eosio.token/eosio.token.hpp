/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */
#pragma once

#include <eosiolib/asset.hpp>
#include <eosiolib/eosio.hpp>
#include <eosiolib/singleton.hpp>
#include <eosiolib/transaction.hpp>
#include <string>

using namespace eosio;
using std::string;

class token : public contract {

  static constexpr time refund_delay = 3*24*3600;   // 3 days
  const uint64_t REQUIRED_STAKE_DURATION = 7*24*3600; // 7 days

  public:
      token( account_name self ):contract(self),_global_singleton(_self,_self){
        _global = _global_singleton.get();
      }

      void create( account_name issuer,
                  asset        maximum_supply);

      void unlock( asset unlock );

      void issue( account_name to, asset quantity, string memo );

      void transfer( account_name from,
                    account_name to,
                    asset        quantity,
                    string       memo );


      void powerup( account_name owner, asset quantity );
      void powerdown( account_name owner, asset quantity );
      void claim( account_name owner );
      void refund( account_name owner );

      inline asset get_supply( symbol_name sym )const;

      inline asset get_balance( account_name owner, symbol_name sym )const;

  private:
      symbol_type _symbol = S(4,KARMA);
      // @abi table accounts i64
      struct account {
        asset    balance;

        uint64_t primary_key()const { return balance.symbol.name(); }
      };

      // @abi table stat i64
      struct stat {
        asset          supply;
        asset          max_supply;
        account_name   issuer;
        bool           transfer_locked = true;

        uint64_t primary_key()const { return supply.symbol.name(); }
      };

      // @abi table power i64
      struct power_st {
        asset     weight;
        time      last_claim_time;
        uint64_t primary_key()const { return weight.symbol.name(); }
      };

      // @abi table refunding i64
      struct refund_st {
        asset     quantity;
        time      request_time;
        uint64_t primary_key()const { return quantity.symbol.name(); }
      };

      // @abi table global i64
      struct global {
        asset     power_pool;
        asset     total_power;
        time      last_filled_time;
      };

      typedef eosio::multi_index<N(accounts), account> accounts;
      typedef eosio::multi_index<N(stat), stat> stats;
      typedef eosio::multi_index<N(power), power_st> power;
      typedef eosio::multi_index<N(refunding), refund_st> refunding;

      typedef eosio::singleton<N(global), global> global_singleton;

      global_singleton _global_singleton;
      global           _global;

      void sub_balance( account_name owner, asset value );
      void add_balance( account_name owner, asset value, account_name ram_payer );
      void do_claim( account_name owner, bool prorate = false );
  public:
      struct transfer_args {
        account_name  from;
        account_name  to;
        asset         quantity;
        string        memo;
      };
};

asset token::get_supply( symbol_name sym )const
{
  stats statstable( _self, sym );
  const auto& st = statstable.get( sym );
  return st.supply;
}

asset token::get_balance( account_name owner, symbol_name sym )const
{
  accounts accountstable( _self, owner );
  const auto& ac = accountstable.get( sym );
  return ac.balance;
}
