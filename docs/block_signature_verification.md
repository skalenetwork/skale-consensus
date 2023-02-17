# Consensus block signature verification.


## Block proposal EcdsaProposerSignature

Each consensus block proposal includes EcdsaProposerSignature object.

EcdsaProposerSignature is a signature of the proposal using the ECDSA private key of the node that made the proposal.

$$ EcdsaProposerSignature = EcdsaSign(ProposerNodeEcdsaKey, Proposal) $$

## Block proposal DaThresholdSignature

During the block proposal phase, each time a node receives a proposal from another node, it will sign and return to the proposer node a signature share that verifies receipt.

When a block proposer receives 11 such signature shares  (including its own signature share), it will combine the  shares into an object DaThresholdSignature.

The object proves the fact that the proposal has been distributed to at least 11 out of 16 nodes. It also proves the fact, that the proposal is unique, since an honest receiving node will only sign a single proposal for a given block number and proposer index.

## Two step distribution of block proposals.

A proposer will distribute its proposal to other nodes in two steps:

1. Distribute the proposal to at least 11 nodes, including itself.

2. Create a DaThresholdSignature by gluing the 11 signature shares it received back.

3. Distribute DaThresholdSignature to at least 11 out of 16 nodes.

Node, that a node will not vote for consensus for a particular proposal, unless it received both the proposal itself and its DaThresholdSignature.




$$ DA_SIGNATURE (Data Availability) 11-out-16 threshold signature of the proposal.

The DA 




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

* Sync admin will detect rotation, regenerate the config and restart skaled with new config










