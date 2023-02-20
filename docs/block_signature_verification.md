
# Consensus block signatures spec.

## 1 Block proposal EcdsaProposerSignature

### 1.1 EcdsaProposerSignature description.

Each consensus block proposal includes $EcdsaProposerSignature$ object.

$EcdsaProposerSignature$ is a signature of the proposal using the ECDSA private key $ProposerNodeEcdsaKey$ of the node that made the proposal.

### 1.2 Computing  EcdsaProposerSignature

$EcdsaProposerSignature$ is computed as follows

$$ EcdsaProposerSignature = EcdsaSign(ProposerNodeEcdsaKey, BLAKE3Hash(Proposal)) $$

Here $Blake3Hash$ is 256-bit version of Blake3 hash algorithm.

### 1.3 BlockProposal generation algorithm.

During the proposal phase, the block proposer will:

* generate an unsigned proposal. The unsigned proposal is composed of 
  * proposal JSON header and the list of binary transactions.
  * the list of binary transactions.
* compute $EcdsaProposerSignature$ of the proposal 
* add it to proposal JSON header.


## 2 Block proposal DaThresholdSignature

### 2.1 DaThresholdSignature description.

During the block proposal phase:

* proposal node will submit the proposal to all other nodes
* when a receiving node receives a proposal, it will:
  * sign it, creating $DaThresholdSignatureShare$
  * return $DaThresholdSignatureShare$ to the proposer node.
* when the proposer node receives $2t+1$(11) such signature shares, including its own share, it will combine the  shares into $DaThresholdSignature$.
* the proposer will then send the $DaThresholdSignature$ to all other nodes.

### 2.2 DaThresholdSignature meaning.

* $DaThresholdSignature$ proves data availability of the proposal. In other words, it proves that the proposal has been distributed to at least 11 out of 16 nodes.

* $DaThresholdSignature$ also proves uniqueness of the proposal for a given $blockId$ and proposer. This comes from the fact, that an honest node will only sign a single proposal for a given proposer and block number.

$DaThresholdSignature$ is required for consensus, meaning that a node votes $0$ in consensus if it did not receive $DaThresholdSignature$. This means, that a proposal that wins consensus is guranteeed to have $DaThresholdSignature$

### 2.3 Computing DaThresholdSignatureShare.

A $DaThresholdSignatureShare$ is computed as

$$ DaThresholdSignatureShare = EdDSASign(EddsaSessionKey, BLAKE3Hash(Proposal))$$


### 2.4 Computing EdDSASessionKey.


Each node generates and uses $EdDSASessionKey$ to sign $DaThresholdSignatureShare$.

$EdDSASessionKey$ is a temporary EdDSA key pair. It is stored in RAM of the node. Signining using this key pair
does not involve SGX server, therefore it is really fast.

When $EdDSASessionKey$ is generated, its public key is signed using $EDSAPrivateKey$ of the node over SGX.
This is done to prove that a particular $EdDSASessionKey$ belongs to a particular node.


### 2.3 Computing DaThresholdSignature

A $DaThresholdSignature$ of the proposal is simply composed of 11 $DaThresholdSignatureShare$s of the proposal.


## 3 Committed block BLSThresholdSignature.

## 3.1 Committed block BLSThresholdSignature.

A block is committed by signing it using $BLSThresholdSignature$ algorithm.

## 3.2 BLSThresholdSignatureShare creation and broadcast.

Each time when consensus completes on a particular node and a winning block proposer is determined, the node will
* signs a message specifying the winning block proposer index and the blockID using $BLSKeyShare$ of the node, to create
$BLSThresholdSignatureShare$
* send $BLSThresholdSignatureShare$ to all other nodes.

## 3.3 BLSThresholdSignatureShare collection and BLSThresholdSignature assembly.

When a node collects 11 $BLSThresholdSignatureShare$ objects, it will:
* glue $BLSThresholdSignatureShares$ into $BLSThresholdSignature$ 
* add $BLSThresholdSignature$ this signature to the block proposal, turning it into a committed block.



## 3.4 Computing BLSThresholdSignatureShare


$EcdsaProposerSignature$ is computed as follows

$$ EcdsaProposerSignature = EcdsaSign(ProposerNodeEcdsaKey, BLAKE3Hash(CommitMessage)) $$

Here $CommitMessage$ is a JSON object that includes $chainId$, $blockId$, and winning $proposerIndex$. 

Note that if no proposal won (consessus returned default block), then $proposerIndex$ is set to $0$.

