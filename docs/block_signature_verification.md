# Consensus block signatures spec.


## 1 Block proposal EcdsaProposerSignature


### 1.1 EcdsaProposerSignature description.

Each consensus block proposal includes $EcdsaProposerSignature$ object.

$EcdsaProposerSignature$ is a signature of the proposal using the ECDSA private key ProposerNodeEcdsaKey of the node that made the proposal.

### 1.2 Computing  EcdsaProposerSignature

$EcdsaProposerSignature$ is computed as follows

$$ EcdsaProposerSignature = EcdsaSign(ProposerNodeEcdsaKey, BLAKE3Hash(Proposal)) $$

Here $Blake3Hash$ is 256-bit version of Blake3 hash algorithm.

### 1.3 BlockProposal generation algorithm.

During the proposal phase, the block proposer will:

* generates an unsigned proposal. The unsigned proposal is composed of proposal JSON header and the list of binary transactions.
* compute $EcdsaProposerSignature$ of the proposal 
* add it to proposal JSON header.


## 2 Block proposal DaThresholdSignature

### 2.1 DaThresholdSignature description.

During the block proposal phase, each time a node receives a proposal from another node, it will sign and return to the proposer node a signature share that verifies receipt.

When a block proposer receives 11 such signature shares  (including its own signature share), it will combine the  shares into an object DaThresholdSignature.

The object proves the fact that the proposal has been distributed to at least 11 out of 16 nodes. It also proves the fact, that the proposal is unique, since an honest receiving node will only sign a single proposal for a given block number and proposer index.

## 2.2 Two step distribution of block proposals.

A proposer will distribute its proposal to other nodes in two steps:

1. Distribute the proposal to at least 11 nodes, including itself.

2. Create a DaThresholdSignature by gluing the 11 signature shares it received back.

3. Distribute DaThresholdSignature to at least 11 out of 16 nodes.

Node, that a node will not vote for consensus for a particular proposal, unless it received both the proposal itself and its DaThresholdSignature.


## 2.3 Algorithm used by DaThresholdSignature.

A DaThresholdSignature of the proposal is simply 11 EdDSA signatures of the proposal. For each signature, a session EdDSA key is used instead of SGX key.
to improve performance.






