/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */

#include "eosio.token.hpp"

//inflation parameters
const double   continuous_rate       = 0.04879;          // 5% annual rate
const double   staking_share         = 0.8;              // 80% goes to staking reward - this will shrink to 15% when app deploys
const uint32_t seconds_per_year      = 52*7*24*3600;
const uint64_t useconds_per_year     = seconds_per_year*1000000ll;
//claim parameters
const uint32_t seconds_claim_delay   = 7*24*3600;       // 7 days
const uint64_t useconds_claim_delay  = seconds_claim_delay*1000000ll;
//refund parameters
static constexpr time   seconds_refund_delay = 3*24*3600; // 3 days
const uint64_t         useconds_refund_delay = seconds_refund_delay*1000000ll;

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
  eosio_assert( sym == token::SYMBOL, "you can only power up with KARMA" );
  eosio_assert( quantity.amount > 0, "must specify positive quantity" );
  power owner_power( _self, owner );
  sub_balance( owner, quantity );
  auto to = owner_power.find( quantity.symbol.name() );
  if( to == owner_power.end() ) {
     //first powerup
     owner_power.emplace( owner, [&]( auto& a ){
       a.weight = quantity;
       a.last_claim_time = current_time();
     });
  } else {
     //do a prorated claim to reward current power, then increase power.
     //claim timer resets, will be increased reward at that time
     do_claim(owner, true);
     owner_power.modify( to, 0, [&]( auto& a ) {
       a.weight += quantity;
       a.last_claim_time = current_time();
     });
  }
  //finally increase global power
  _global.total_power += quantity;
}
void token::powerdown( account_name owner, asset quantity ) {
  require_auth( owner );
  auto sym = quantity.symbol;
  eosio_assert( sym.is_valid(), "invalid symbol name" );
  eosio_assert( sym == token::SYMBOL, "you can only power down KARMA" );
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

  //decrease global power
  _global.total_power -= quantity;

  //create a refunding state
  refunding owner_refund( _self, owner );
  auto to = owner_refund.find( quantity.symbol.name() );
  if( to == owner_refund.end() ) {
     owner_refund.emplace( owner, [&]( auto& a ){
       a.quantity = quantity;
       a.request_time = current_time();
     });
  } else {
     owner_refund.modify( to, 0, [&]( auto& a ) {
       a.quantity += quantity;
       a.request_time = current_time();
     });
  }

  //send deferred refund action
  eosio::transaction out;
  out.actions.emplace_back( permission_level{ _self, N(notify) }, _self, N(refund), owner );
  out.delay_sec = seconds_refund_delay;
  cancel_deferred( owner ); // TODO: Remove this line when replacing deferred trxs is fixed
  out.send( owner, owner, true );
}

void token::claim( account_name owner ) {
  require_auth( owner );
  do_claim( owner, false );
}

void token::refund( account_name owner ) {
  //require_auth( owner );
  //eosio::print("Attempting refund"); //TODO Remove
  refunding owner_refund( _self, owner );
  const auto& from = owner_refund.get( token::SYMBOL.name(), "no KARMA refund found" );
  eosio_assert(from.request_time + useconds_refund_delay <= current_time(), "refund not available yet");
  eosio_assert(from.quantity.amount > 0, "refund must be positive");
  add_balance(owner, from.quantity, owner);
  owner_refund.erase( from );
  //eosio::print("\nRefund success"); //TODO Remove
}

void token::do_claim( account_name owner, bool prorate ) {
  power owner_power( _self, owner );
  const auto& from = owner_power.get( token::SYMBOL.name(), "no KARMA power found" );
  const auto memo = std::string(prorate ? "Thanks for POWERING UP more KARMA! Here's your prorated POWER UP reward." : "Thanks for POWERING UP with KARMA! Here's your POWER UP reward.");

  auto ct = current_time();
  double proration = 1.0;

  // eosio::print("Current time: ", ct,"\n"); //TODO Remove
  // eosio::print("Last claim time: ", from.last_claim_time,"\n"); //TODO Remove
  // eosio::print("Delay time: ", useconds_claim_delay,"\n"); //TODO Remove
  // eosio::print("Shortage: ", ct - from.last_claim_time,"\n"); //TODO Remove

  if(_global.last_filled_time == 0) {
    _global.last_filled_time = ct;
  }

  //check timing vs proration
  if(!prorate) {
    eosio_assert(ct - from.last_claim_time > useconds_claim_delay, "you must wait seven days from last claim or powerup");
  } else {
    proration = double(ct - from.last_claim_time) / double(useconds_claim_delay);
  }

  owner_power.modify( from, 0, [&]( auto& a ) {
    a.last_claim_time = ct;
  });

  // eosio::print("Proration: ", proration, "\n");

  const auto usecs_since_last_fill = ct - _global.last_filled_time;
  const asset token_supply         = get_supply(token::SYMBOL.name());

  //create inflation
  auto new_tokens = static_cast<uint64_t>( (staking_share * continuous_rate * double(token_supply.amount) * double(usecs_since_last_fill)) / double(useconds_per_year) );

  _global.power_pool += asset(new_tokens,token::SYMBOL);
  _global.last_filled_time = ct;

  // eosio::print("Create inflation: ", new_tokens,"\n"); //TODO Remove
  // eosio::print("Pool: ", _global.power_pool,"\n"); //TODO Remove

  //award tokens
  auto reward = static_cast<uint64_t>((proration * double(from.weight.amount) * double(_global.power_pool.amount)) / double(_global.total_power.amount));
  eosio_assert(reward > 0, "you must have a reward greater than zero"); //extreme edge case that probably will never happen
  add_balance(owner,asset(reward,token::SYMBOL),owner);

  //reduce pool
  eosio_assert(reward <= _global.power_pool.amount, "power pool too small"); //extreme edge case that probably will never happen
  _global.power_pool -= asset(reward,token::SYMBOL);

  //we will remove inflation from self
  //this is because the pre-app staking isn't real inflation
  //but is being funded from the KARMA supply
  sub_balance(_self, asset(reward,token::SYMBOL));
  // eosio::print("Reward: ", reward,"\n"); //TODO Remove

  action(
    permission_level{ _self, N(notify) },
    _self, N(rewarded),
    std::make_tuple(owner, asset(reward,token::SYMBOL), memo)
  ).send();
}

void token::rewarded( account_name to, asset quantity, string memo ){};

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

EOSIO_ABI( token, (create)(unlock)(issue)(transfer)(powerup)(powerdown)(claim)(refund)(rewarded) )
