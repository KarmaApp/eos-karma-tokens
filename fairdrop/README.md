# KARMA Fairdrop

## Jungle Testnet Steps

Steps to run Airdrop with

```
# create a jungle account let's say therealkarmc
# use the jungle faucet to add tokens to therealkarmc
# my cleos alias to junglenet is cleosjn2

# lets buy enough resources to airdrop:
cleosjn2 system buyram therealkarmc therealkarmc "1000.0000 EOS"
cleosjn2 system delegatebw therealkarmc therealkarmc "100.0000 EOS" "8000.0000 EOS"

# deploying contract
cd ~/eos-karma-tokens/eosio.token
eosiocpp -o eosio.token.wast eosio.token.cpp
eosiocpp -g eosio.token.abi eosio.token.cpp
cleosjn2 set contract therealkarmc ../eosio.token

# creating karma token (100 bi max supply - covers 50 years of inflation at 5%)
cleosjn2 push action therealkarmc create '["therealkarmc", "100000000000.0000 KARMA"]' -p therealkarmc

# if using EOSDrops https://github.com/EOSEssentials/EOSDrops
# confirm the config, blacklist and capWhitelist json files
node airdrop.js

# if everything is calculated correct you should see this message:
You are about to airdrop 1912765963.6052 KARMA tokens on 163904 accounts.

# confirm and start the airdrop!
```
