<!-- SPDX-License-Identifier: (AGPL-3.0-only OR CC-BY-4.0) -->

# Running tests

There are many testing configurations available in the `test` directory.

Consensus tests

* `consensus-basic` - runs basic consensus test
* `consensus-two-engines` - runs two consensus engines
* `change-schain-index` - test corrupt config (see onenode_corrupt_config)
* `consensus-finalization-download` - test finalization download
* `consensus-stuck` -
* `corrupt-proposal` - 

Serialization Tests

* `tx-serialize` - serialize/deserialize transaction test
* `tx-list-serialize` - serialize/deserialize transaction list test
* `committed-block-list-serialize` - serialize/deserialize block list
* `committed-block-defragment` - fragment/defragment committed block test

SGX Tests

* `sgx` -

## Example Onenode basic test

```bash
cp -f ./build/consensust /tmp/consensust
cd test/onenode
/tmp/consensust [consensus-basic]
```

Should output:

```
Filters: [consensus-basic]
[2020-06-27 11:17:53.963] [config] [info] Monitoring agent started monitoring
[2020-06-27 11:17:55.211] [1:main] [info] 0:Starting node
[2020-06-27 11:17:55.215] [1:main] [info] 0:Waiting to connect to peers
[2020-06-27 11:17:55.215] [1:main] [info] 0:Consensus engine version:1.51.1
[2020-06-27 11:17:55.215] [1:main] [info] 0:Last committed block in consensus:0
[2020-06-27 11:17:55.215] [1:main] [info] 0:Jump starting the system with block:0
[2020-06-27 11:17:55.391] [1:main] [info] 0:BIN_CONSENSUS_START: PROPOSING: 1
[2020-06-27 11:17:55.391] [1:consensus] [info] 0:Started all nodes
[2020-06-27 11:17:55.392] [1:consensus] [info] 0:BLOCK_DECIDE: PRPSR:1:BID:1:STATS:|1|D1R1P0L0|| Now signing block ...
[2020-06-27 11:17:55.392] [1:main] [info] 0:BLOCK_SIGNED: Now finalizing block ... BID:1
[2020-06-27 11:17:55.425] [1:main] [info] 0:BLOCK_COMMIT: PRPSR:1:BID: 1:ROOT:2:HASH:62dc2bf5:BLOCK_TXS:10000:DMSG:0:MPRPS:1:RPRPS:0:TXS:10000:TXLS:1:KNWN:10000:MGS:5:INSTS:2:BPS:0:HDRS:4:SOCK:0:CONS:0:DSDS:0
[2020-06-27 11:17:55.585] [1:main] [info] 1:BIN_CONSENSUS_START: PROPOSING: 1
[2020-06-27 11:17:55.679] [1:consensus] [info] 1:BLOCK_DECIDE: PRPSR:1:BID:2:STATS:|1|D1R2P0L0|| Now signing block ...
[2020-06-27 11:17:55.680] [1:main] [info] 1:BLOCK_SIGNED: Now finalizing block ... BID:2
[2020-06-27 11:17:55.712] [1:main] [info] 1:BLOCK_COMMIT: PRPSR:1:BID: 2:ROOT:3:HASH:07d9dd94:BLOCK_TXS:10000:DMSG:0:MPRPS:2:RPRPS:0:TXS:20000:TXLS:2:KNWN:20000:MGS:7:INSTS:3:BPS:0:HDRS:5:SOCK:0:CONS:0:DSDS:0
[2020-06-27 11:17:55.878] [1:main] [info] 2:BIN_CONSENSUS_START: PROPOSING: 1
[2020-06-27 11:17:55.974] [1:consensus] [info] 2:BLOCK_DECIDE: PRPSR:1:BID:3:STATS:|1|D1R1P0L0|| Now signing block ...
[2020-06-27 11:17:55.974] [1:main] [info] 2:BLOCK_SIGNED: Now finalizing block ... BID:3
[2020-06-27 11:17:56.007] [1:main] [info] 2:BLOCK_COMMIT: PRPSR:1:BID: 3:ROOT:4:HASH:47497424:BLOCK_TXS:10000:DMSG:0:MPRPS:3:RPRPS:0:TXS:30000:TXLS:3:KNWN:20000:MGS:7:INSTS:4:BPS:0:HDRS:4:SOCK:0:CONS:0:DSDS:0
[2020-06-27 11:17:56.171] [1:main] [info] 3:BIN_CONSENSUS_START: PROPOSING: 1
[2020-06-27 11:17:56.265] [1:consensus] [info] 3:BLOCK_DECIDE: PRPSR:1:BID:4:STATS:|1|D1R1P0L0|| Now signing block ...
[2020-06-27 11:17:56.265] [1:main] [info] 3:BLOCK_SIGNED: Now finalizing block ... BID:4
[2020-06-27 11:17:56.297] [1:main] [info] 3:BLOCK_COMMIT: PRPSR:1:BID: 4:ROOT:5:HASH:bced89b1:BLOCK_TXS:10000:DMSG:0:MPRPS:3:RPRPS:0:TXS:38489:TXLS:3:KNWN:20000:MGS:8:INSTS:5:BPS:0:HDRS:4:SOCK:0:CONS:0:DSDS:0
...
[2020-06-27 11:18:25.215] [1:main] [info] 94:BIN_CONSENSUS_START: PROPOSING: 1
[2020-06-27 11:18:25.316] [1:consensus] [info] 94:BLOCK_DECIDE: PRPSR:1:BID:95:STATS:|1|D1R0P0L0|| Now signing block ...
[2020-06-27 11:18:25.317] [1:main] [info] 94:BLOCK_SIGNED: Now finalizing block ... BID:95
[2020-06-27 11:18:25.352] [1:main] [info] 94:BLOCK_COMMIT: PRPSR:1:BID: 95:ROOT:96:HASH:247e3e11:BLOCK_TXS:10000:DMSG:0:MPRPS:3:RPRPS:0:TXS:49417:TXLS:3:KNWN:20000:MGS:13:INSTS:11:BPS:0:HDRS:3:SOCK:0:CONS:0:DSDS:0
[2020-06-27 11:18:25.392] [config] [info] Exit requested
[2020-06-27 11:18:25.392] [1:main] [info] 95:Consensus exiting: Node:Exit requested
[2020-06-27 11:18:25.392] [1:main] [info] 95:Consensus exiting: Node:Exit requested
===============================================================================
All tests passed (4 assertions in 1 test case)
```

If running consecutive tests using `consensusd`, be sure to clean the database between each run:

```bash
rm -rf /tmp/*.db*
```