# BLE protocol v4

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

The companion writes compact UTF-8 JSON immediately after connecting and every
15 seconds. Payloads are limited to 250 bytes for the negotiated 256-byte MTU.

```json
{"v":4,"s":42,"t":1792426620,"o":480,"r":93,"d":27233,"f":65,"z":4,"y":938,"l":1200,"m":"gpt-5.6-sol","h":200,"n":9684,"b":0,"x":[22,614,26356,5532,65728,3535,41974,714,"trae/gpt-5.6-sol",571]}
```

The short scalar keys carry time, quota, today's request metrics, and the
latest route. The `x` array carries 30-day session count, calls, input/output/
cache/reasoning tokens, estimated cost in millionths of a dollar, cache-hit
rate in tenths of a percent, top model, and top-model calls.

The ESP32 marks retained data stale after 60 seconds without a snapshot.

## Commands

Allowed actions are `focus_magpie` and `refresh`. Arbitrary prompts and shell
commands are rejected.

```json
{"v":4,"sid":319028314,"id":7,"a":"focus_magpie"}
{"v":4,"id":7,"ok":1,"m":"FOCUSED"}
```
