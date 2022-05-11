# Consensus Block Proposal Spec

*VERSION 1.1 16 Nov 2020* 

## Item queue
There is a  dedicated item queue for each destination node.

Each item queue has a dedicated sending thread (ST)

## Items
Items in the queue can be either BlockProposal or DAProof.

## Send loop
Sending thread:

a) waits on the queue until it is nonempty

b) pops an item from the queue

c) forever attempts to send the item until the item is sent without an error

d) goes to a)

## Sent item
An item is sent when:

   a) the destination node returns success

   b) the destination node specifies that it does not need the item 


## BlockProposal and DAProof submission algorithm
 

   1. node creates a BlockProposal
   2. node pushes n-1 copies of it to n-1 destination queues
   3. proposal is sent to a node and BLS sig share of the proposal is received back
   4. when 2 t + 1  sig shares are received, BLS signature is glued.  The signature of the proposal is embedded into DAProof object.
   5.node pushes n-1 copies of the DAProof to n-1 destination queues
   6.the proofs are sent to destinations
    
   NOTE As you can see from above, the client will never send a DAProof for a blockproposal, before the block proposal itself is sent.

## Consensus start on DAProofs receipt
 
   a) BlockProposalServer agent waits for 2 t + 1 DAProofs. For  each proof, BLSSig is verified

   b) When the BlockProposalServer receives 2t + 1 DAProofs, it creates a consensus proposal vector

   c) In the proposal vector there are 2 t + 1  “Ones” and t “Zeros”.

   d) for each item in the proposal vector, a binary consensus is started

## Consensus start on timeout
 
BlockProposalServer should always receive 2 t + 1 proofs. But to protect against, e.g., intermittent network problem.

a) As the last resort, if BlockProposal Server does not receive 2 t + 1 proofs after BLOCK_PROPOSAL_TIMEOUT = 60 seconds

b) BlockProposalServer will start consensus with currently received DAProofs (it means that that the proposal vector will have more “Zeroes”, which will increase probability of an default empty block)

NOTE default empty block is committed if all 3t + 1 consensuses terminate with “Zero”

NOTE due to 6 and 7 the system can never get stuck during the proposal phase.

## Ignoring of old items
 
If BlockProposalServer is sent an item with old block id, which is  less or equal to the lastCommittedBlockID, it will ignore the item and report success.

## Behavior on proposer crash
 
Each block proposal is saved by the proposer. If a block proposer crashes in a middle of consensus, and then is restarted

it will re-do procedure *BlockProposal and DAProof submission algorithm* after the crash by reading the proposal from LevelDB

## Queue size
The send item queue size has fixed length (currently 8 items). This means that  it can hold things for 4 blocks_id (since each block_id has one block and one DA proof).

When the other party is not responding, the older items are dropped from the queue, so its size is constant.

This means that if a node is far behind in consensus (4 blocks behind), it does not make sense to send the stale items to it. 

Instead, it will catch up baked blocks.

One can  make the queue size even smaller (1 or two blocks).  There is no point proposing blocks, that 2t + 1 of nodes already decided on. 

