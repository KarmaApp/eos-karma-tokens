/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */

#include "eosio.token.hpp"

void token::create( account_name issuer,
                    asset        maximum_supply )
{
    require_auth( _self );

    auto sym = maximum_supply.symbol;
    eosio_assert( sym.is_valid(), "invalid symbol name" );
    eosio_assert( maximum_supply.is_valid(), "invalid supply");
    eosio_assert( maximum_supply.amount > 0, "max-supply must be positive");

    stats statstable( _self, sym.name() );
    auto existing = statstable.find( sym.name() );
    eosio_assert( existing == statstable.end(), "token with symbol already exists" );

    statstable.emplace( _self, [&]( auto& s ) {
       s.supply.symbol = maximum_supply.symbol;
       s.max_supply    = maximum_supply;
       s.issuer        = issuer;
    });
}

void token::unlock( asset unlock ) {
    eosio_assert( unlock.symbol.is_valid(), "invalid symbol name" );
    auto sym_name = unlock.symbol.name();
    stats statstable( _self, sym_name );
    auto token = statstable.find( sym_name );
    eosio_assert( token != statstable.end(), "token with symbol does not exist, create token before unlock" );
    const auto& st = *token;
    require_auth( st.issuer );

    statstable.modify( st, 0, [&]( auto& s ) {
        s.transfer_locked = false;
    });
}


void token::issue( account_name to, asset quantity, string memo )
{
    auto sym = quantity.symbol;
    eosio_assert( sym.is_valid(), "invalid symbol name" );
    eosio_assert( memo.size() <= 256, "memo has more than 256 bytes" );

    auto sym_name = sym.name();
    stats statstable( _self, sym_name );
    auto existing = statstable.find( sym_name );
    eosio_assert( existing != statstable.end(), "token with symbol does not exist, create token before issue" );
    const auto& st = *existing;

    require_auth( st.issuer );
    eosio_assert( quantity.is_valid(), "invalid quantity" );
    eosio_assert( quantity.amount > 0, "must issue positive quantity" );

    eosio_assert( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );
    eosio_assert( quantity.amount <= st.max_supply.amount - st.supply.amount, "quantity exceeds available supply");

    statstable.modify( st, 0, [&]( auto& s ) {
       s.supply += quantity;
    });

    add_balance( st.issuer, quantity, st.issuer );

    if( to != st.issuer ) {
       SEND_INLINE_ACTION( *this, transfer, {st.issuer,N(active)}, {st.issuer, to, quantity, memo} );
    }
}

void token::transfer( account_name from,
                      account_name to,
                      asset        quantity,
                      string       memo )
{
    eosio_assert( from != to, "cannot transfer to self" );
    require_auth( from );
    eosio_assert( is_account( to ), "to account does not exist");
    auto sym = quantity.symbol.name();
    stats statstable( _self, sym );
    const auto& st = statstable.get( sym );

    if ( st.transfer_locked ) {
        require_auth ( st.issuer );
    }

    require_recipient( from );
    require_recipient( to );

    eosio_assert( quantity.is_valid(), "invalid quantity" );
    eosio_assert( quantity.amount > 0, "must transfer positive quantity" );
    eosio_assert( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );
    eosio_assert( memo.size() <= 256, "memo has more than 256 bytes" );


    sub_balance( from, quantity );
    add_balance( to, quantity, from );
}

void token::powerup( account_name owner, asset quantity ) {
  require_auth( owner );
  auto sym = quantity.symbol;
  eosio_assert( sym.is_valid(), "invalid symbol name" );
  eosio_assert( sym == token::_symbol, "you can only power up with KARMA" );
  eosio_assert( quantity.amount > 0, "must specify positive quantity" );
  power owner_power( _self, owner );
  sub_balance( owner, quantity );
  auto to = owner_power.find( quantity.symbol.name() );
  if( to == owner_power.end() ) {
     //first powerup
     owner_power.emplace( owner, [&]( auto& a ){
       a.weight = quantity;
       a.last_claim_time = now();
     });
  } else {
     //do a prorated claim to reward current power, then increase power.
     //claim timer resets, will be increased reward at that time
     do_claim(owner, true);
     owner_power.modify( to, 0, [&]( auto& a ) {
       a.weight += quantity;
       a.last_claim_time = now();
     });
  }
}
void token::powerdown( account_name owner, asset quantity ) {
  require_auth( owner );
  auto sym = quantity.symbol;
  eosio_assert( sym.is_valid(), "invalid symbol name" );
  eosio_assert( sym == token::_symbol, "you can only power down KARMA" );
  eosio_assert( quantity.amount > 0, "must specify positive quantity" );
  power owner_power( _self, owner );
  const auto& from = owner_power.get( quantity.symbol.name(), "no KARMA power found" );
  eosio_assert( from.weight.amount >= quantity.amount, "you dont have that much KARMA power" );
  if( from.weight.amount == quantity.amount ) {
     //no prorated claim... it's forfeit for not waiting to claim
     owner_power.erase( from );
  } else {
     owner_power.modify( from, owner, [&]( auto& a ) {
         //we don't do a prorated claim for powerdown
         //when they can claim they get reduced rewards... fair for powering down
         a.weight -= quantity;
     });
  }

  //create a refunding state
  refunding owner_refund( _self, owner );
  auto to = owner_refund.find( quantity.symbol.name() );
  if( to == owner_refund.end() ) {
     owner_refund.emplace( owner, [&]( auto& a ){
       a.quantity = quantity;
       a.request_time = now();
     });
  } else {
     owner_refund.modify( to, 0, [&]( auto& a ) {
       a.quantity += quantity;
       a.request_time = now();
     });
  }

  //send deferred refund action
  eosio::transaction out;
  out.actions.emplace_back( permission_level{ owner, N(active) }, _self, N(refund), owner );
  out.delay_sec = token::refund_delay;
  cancel_deferred( owner ); // TODO: Remove this line when replacing deferred trxs is fixed
  out.send( owner, owner, true );
}
void token::claim( account_name owner ) {
  require_auth( owner );
  do_claim( owner, false );
}

void token::refund( account_name owner ) {
  require_auth( owner );
  refunding owner_refund( _self, owner );
  const auto& from = owner_refund.get( token::_symbol.name(), "no KARMA refund found" );
  eosio_assert(now() <= from.request_time + token::refund_delay, "refund not available yet");
  eosio_assert(from.quantity.amount > 0, "refund must be positive");
  add_balance(owner, from.quantity, owner);
  owner_refund.erase( from );

}

void token::do_claim( account_name owner, bool prorate ) {
  //create inflation
}

void token::sub_balance( account_name owner, asset value ) {
   accounts from_acnts( _self, owner );

   const auto& from = from_acnts.get( value.symbol.name(), "no balance object found" );
   eosio_assert( from.balance.amount >= value.amount, "overdrawn balance" );


   if( from.balance.amount == value.amount ) {
      from_acnts.erase( from );
   } else {
      from_acnts.modify( from, owner, [&]( auto& a ) {
          a.balance -= value;
      });
   }
}

void token::add_balance( account_name owner, asset value, account_name ram_payer )
{
   accounts to_acnts( _self, owner );
   auto to = to_acnts.find( value.symbol.name() );
   if( to == to_acnts.end() ) {
      to_acnts.emplace( ram_payer, [&]( auto& a ){
        a.balance = value;
      });
   } else {
      to_acnts.modify( to, 0, [&]( auto& a ) {
        a.balance += value;
      });
   }
}

EOSIO_ABI( token, (create)(unlock)(issue)(transfer) )
