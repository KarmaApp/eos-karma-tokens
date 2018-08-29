# KARMA TOKEN

## Now with POWERUPS!
KARMA provides a mechanism for POWERING UP (or staking) so that you can vote on creative
content with more weight. To help everyone get used to this and test it out we are providing
KARMA POWER rewards immediately!

### How to POWER UP
POWERING UP gives you votes more weight and earns you rewards!

* While you're POWERED up you cannot trade or transfer your KARMA

Run the following command

```cleos push action therealkarma powerup '{"owner":"myusername","quantity":"10000.0000 KARMA"}'```

### How to POWER DOWN
Powering down takes time before your KARMA is available again
* On `testnet` powering down takes 15 minutes
* On `mainnet` powering down takes 3 days
* Each powerdown resets the timer

Run the following command

```cleos push action therealkarma powerdown '{"owner":"myusername","quantity":"1000.0000 KARMA"}'```

### How to claim POWER rewards
KARMA has an inflation model of a maximum 5% annually of the current supply

A portion of this rewards people who POWER UP with KARMA

Prior to DAPP deployment this reward will come from the existing supply instead of inflation and
will be awarded at a higher rate!

* On `testnet` you can claim rewards every 30 minutes
* On `mainnet` you can claim rewards every 7 days

Run the following command

```cleos push action therealkarma claim '{"owner":"myusername"}'```

### How to refund your KARMA
This action should be automatic thanks to deferred actions

However, this may occasionally fail for unforeseen network reasons

Run the following command

```cleos push action therealkarma refund '{"owner":"myusername"}'```

### How to view your POWER

```cleos get table therealkarma myusername power```

### How to view your refunding amount

```cleos get table therealkarma myusername refunding```

### How to the POWER reward state

```cleos get table therealkarma therealkarma global```
