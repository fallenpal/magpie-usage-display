## BLE protocol v5

Device name: `Magpie Display`

Service UUID: `7d8b6c20-8f6d-4b44-a0f8-1b6570c0de01`

| Characteristic | UUID suffix | Direction | Properties |
|---|---:|---|---|
| Status | `02` | Mac -> ESP32 | encrypted Write |
| Command | `03` | ESP32 -> Mac | encrypted Read + Notify |
| Result | `04` | Mac -> ESP32 | encrypted Write |
| Device info | `05` | ESP32 -> Mac | Read |

Pairing uses BLE Secure Connections, bonding, MITM protection, and a random
six-digit display passkey. Bonded devices reconnect without showing a passkey.

## Status

The companion writes a complete batch immediately after connecting and every
15 seconds, or sooner when Magpie data changes. Each message is compact UTF-8
JSON and is limited to 250 bytes. All messages in a batch share the snapshot
sequence `s`.

| `k` | Message | Count per batch |
|---|---|---|
| `s` | Summary and today's metrics | exactly 1 |
| `x` | Cumulative statistics | exactly 1 |
| `p` | Provider metadata | one per provider |
| `w` | Quota window | one per window |
| `e` | Batch end and commit | exactly 1 |

```json
{"v":5,"s":42,"k":"s","t":1800000000,"o":480,"d":162,"f":7,"z":2,"y":714,"l":1250,"m":"gpt-5.6-sol","h":200,"n":9876,"b":0,"c":3,"w":6,"r":5}
{"v":5,"s":42,"k":"x","a":[40,500,123456,23456,987654,34567,6110000,889,"gpt-5.6-sol",450]}
{"v":5,"s":42,"k":"p","i":0,"c":3,"id":"codex-fc9469","m":"Codex","ta":1792638605}
{"v":5,"s":42,"k":"w","p":0,"i":0,"w":2,"id":"7-days","m":"7 days","ok":1,"u":95,"r":5,"ra":1793086420}
{"v":5,"s":42,"k":"e","c":3,"w":6}
```

The summary carries `generated_at` (`t`), `utc_offset_minutes` (`o`), today's
tokens/calls/errors/success rate/average TTFT (`d`/`f`/`z`/`y`/`l`), latest
request (`m`/`h`/`n`/`b`), provider and window counts (`c`/`w`), the lowest
finite remaining percentage (`r`), and optional truncation flags (`tr`/`to`).
The `x` message carries 30-day session count, calls, input/output/cache/
reasoning tokens, estimated cost in millionths of a dollar, cache-hit rate in
permille, top model, and top-model calls.

Percentages and reset times are omitted when unknown. A window with `ok=0` is
present but its quota cannot be determined. A window with `ul=1` is unlimited
and has no percentages. Missing values must never be rendered as 0 or 100.

The device keeps a staging batch and commits it atomically only after a valid
`e` whose counts match. A new `s` discards unfinished staging, a batch older
than 5 seconds is discarded, and the previous snapshot remains visible. The
ESP32 marks retained data stale after 60 seconds without a committed snapshot.

Limits are 8 providers, 4 windows per provider, and 16 windows in total.
Oversized data is truncated in sorted order and reported with `tr=1` and `to`.
A batch contains at most 32 messages.

The full field contract and shared fixtures are in
[[round-display/INTERFACE-CONTRACT]].

## Commands

Allowed actions are `focus_magpie` and `refresh`. Arbitrary prompts and shell
commands are rejected.

```json
{"v":5,"sid":319028314,"id":7,"a":"focus_magpie"}
{"v":5,"id":7,"ok":1,"m":"FOCUSED"}
```

The device info characteristic returns
`{"v":5,"fw":"0.5.0","screen":"466x466"}`.
