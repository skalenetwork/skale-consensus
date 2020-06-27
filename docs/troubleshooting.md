<!-- SPDX-License-Identifier: (AGPL-3.0-only OR CC-BY-4.0) -->

# Troubleshooting

## Stack Size

It is recommended to run skale-consensus with unlimited stack size. This can be done by:

```bash
export NO_ULIMIT_CHECK=1
```

## File descriptor error

```
ConsensusEngine:Engine init failed:File descriptor limit (ulimit -n) is less than 65535.
See https://bugs.launchpad.net/ubuntu/+source/lightdm/+bug/1627769
```

See [Stack Size](#stack-size).
