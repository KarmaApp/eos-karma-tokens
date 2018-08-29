#!/bin/bash

cleos create account eosio therealkarma EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV
cleos set contract therealkarma . eosio.token.wasm eosio.token.abi
cleos push action create '{"issuer":"therealkarma","maximum_supply":"1000000000.0000 KARMA"}' -p therealkarma
cleos push action issue '{"to":"therealkarma","quantity":"10000.0000 KARMA","memo":"Issue"}' -p therealkarma
