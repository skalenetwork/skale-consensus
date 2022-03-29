# Sync Node Spec


## Intro

A sync node is a skaled node with reduced functionality.

A sync node 

* does not participate in consensus
* does not accept new transactions
* does not broadcast transactions
* does not sign IMA messages

The only things that a sync node does is:

* process blocks through catchup
* keeps a full replica of the EVM and smartcontracts
* responds to read requests, for example eth_call and eth_getBlockNumber


## Network connection

A sync node connects to schain nodes using catchup port.

SyncManager keeps the list of IP addresses of sync nodes.

Schain nodes allow connections to catchup port ONLY from these
addresses

## Sync node config

Sync node config is a regulat skaled config that
does not include SGX server information since
sync nodes do not connect to SGX.

## Sync node config generation

Sync node config is regenerated from template each time 
sync nodes starts.

During generation, BLS and ECDSA public key information,
including rotation key history
is read from SkaleManager and inserted into config.

## Sync node behavior during rotation.

* After rotation, a block BLS signature will not verify,
since it uses the new BLS key. 
* This will cause skaled to
exit with error code ```SYNC_NODE_EXIT_CODE = 314```
* This will cause config regeneration, so the new config 
will include the new key. 
* After that sync node will start with new config.









