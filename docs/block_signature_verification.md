# Consensus block signature verification.


## Block proposal ECDSA_PROPOSER_SIGNATURE

Each consensus block proposal includes $$EcdsaProposerSignature$$ object.

$$ EcdsaProposerSignature = EcdsaSign(ProposerNodeEcdsaKey, Proposal) $$

The proposal of signed by the ECDSA private key of the node that made the proposal.

## Block proposal DA_THRESHOLD_SIGNATURE

The purpose of DA (Data Availability) threshoold signature is to guarantee that the proposal has been distributed by the 
node to at least 11 nodes (including itself), before the consensus starts. 

Each time a proposer distributes the proposal to a particular receiving node, the receiving node will sign a signature share confirming that it received the proposal.

Once the proposer receives 11 signature shares, it glues them into DA_THRESHOLD_SIGNATURE

An honest receiving node will only sign a single proposal for a given block from a given proposer. Thefore, a malicious proposer will never 
be able to collect DA_THRESHOLD_SIGNATURE for two different proposals. This guarantees the fact that if a proposal has DA_THRESHOLD_SIGHNATURE, it is unique.



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










