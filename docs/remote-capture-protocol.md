# QuietCool Remote — Button Capture Protocol

Purpose: discover the RF command byte(s) the physical wall remote sends for each button —
especially the five **timer** durations (1h/2h/4h/8h/12h), which were never captured by the
original project (the fan's own transmit side only needed on/off/low/high). This also
characterizes real remote behavior (bursts, keepalives, mid-timer off) that the decode logic
in `component.yaml` depends on.

Run this once, live, against your paired remote and installed ESP32. Flash the build with the
Step A/C/D changes first (enhanced `on_packet` logging + the `event.quietcool_remote` entity —
the timer bytes are placeholders at this point and simply won't decode to a token yet, which is
expected and harmless).

## Before you start

- Confirm `remote_id` in `component.yaml` matches your paired remote (or `autoset_remote` has
  learned it) — you should see `id_match=yes` on every log line from this remote.
- Watch logs live: `esphome logs component.yaml` (or the ESPHome dashboard's log viewer), filtered
  for `quietcool-fan`.
- Each log line now looks like:
  ```
  [D][quietcool-fan]: Remote command received: 0x3f [KNOWN(high)] id_match=yes since_last_rx_ms=1523
  [D][quietcool-fan]:          from remote ID: [0xcb, 0x00, 0xd5, 0x12]
  ```
  `since_last_rx_ms` is the gap since the previous *distinct* (post-debounce) received command —
  useful for telling a single button's burst apart from two separate presses.

## Step 1 — Sanity pass (validates the pipeline before testing unknowns)

Press each of these once, ~5s apart, and confirm the log tags it `KNOWN` with the expected name:

| Button | Expected byte | Expected tag |
|---|---|---|
| On | `0xbf` | `KNOWN(on/high)` |
| Off | `0x80` | `KNOWN(off)` |
| Low | `0x1f` | `KNOWN(low)` |
| High | `0x3f` | `KNOWN(high)` |

**If any of these come back `UNKNOWN` or with an unexpected byte, stop here** — the remote
pairing/ID is likely wrong. Fix that before continuing; the timer results below are meaningless
until the sanity pass is clean.

## Step 2 — Timer buttons, isolated

For each of **1h, 2h, 4h, 8h, 12h** (in that order), with the fan off beforehand:

1. Press the button once.
2. Wait 10 seconds.
3. Record every distinct `UNKNOWN 0x??` byte logged for this press (there may be more than one
   distinct byte — see Step 3).
4. Turn the fan off (press Off) to reset state before the next button.
5. Press the **same** timer button again to confirm the byte(s) repeat identically.

Fill in as you go:

| Button | Byte(s) seen (1st press) | Byte(s) seen (2nd press, repeat check) | Notes |
|---|---|---|---|
| 1h  | | | |
| 2h  | | | |
| 4h  | | | |
| 8h  | | | |
| 12h | | | |

## Step 3 — Sequence check

While doing Step 2, check whether each timer press produced:
- **(a) One distinct command byte** (repeated 2-3x in a tight burst, `since_last_rx_ms` small
  between repeats) — the simple case, decode is a flat byte → token map.
- **(b) Two different distinct bytes in sequence** (e.g. an implicit "on" byte followed by a
  separate "duration" byte, a beat apart) — some remotes signal mode+confirm as two packets.
  If so, note the exact pair and the typical gap between them (`since_last_rx_ms` on the second
  line) — decode will need to combine the pair within that window into one `timer_Nh` token
  instead of firing on the first byte alone.

Record your finding: ☐ (a) single byte per timer button   ☐ (b) two-byte sequence — pair: ______

## Step 4 — Idle / keepalive check

1. Press an 8h timer button to start it.
2. Do **not** press anything else. Let the log run for 10–15 minutes.
3. Record whether any further `quietcool-fan` log lines appear during that idle window (from
   the remote — not from any HA-side on/off calls you might trigger separately).

Result: ☐ No packets during idle countdown   ☐ Periodic keepalive packets seen — byte(s): ______, roughly every ______ 

If keepalives are seen and their byte matches a timer token, the existing 250ms debounce window
is too short to suppress them (they're minutes apart) — a different mechanism (state-based: "a
timer token was already accepted and hasn't changed value") would be needed; flag this back
before uncommenting the timer branches.

## Step 5 — Physical-off-during-timer check

1. Start any timer duration.
2. Before it expires, press **Off** on the remote.
3. Confirm the log shows the already-known `0x80` (`KNOWN(off)`) byte, same as a normal off.

Result: ☐ Confirmed off byte is 0x80 as expected   ☐ Different — byte seen: ______

This validates an assumption the Climate Advisor integration (issue
[gunkl/ClimateAdvisor#486](https://github.com/gunkl/ClimateAdvisor/issues/486)) relies on: a
mid-timer manual off is detectable the same way as any other off.

## Step 6 — Re-press / override check

1. Start one timer duration (e.g. 4h).
2. Before it expires, press a **different** duration button (e.g. 8h).
3. Record what's logged: does the remote send a distinct "cancel" signal first, or does it just
   emit the new duration's byte directly?

Result: ______________________________________________

## After completing this protocol

1. Fill in the 5 discovered timer bytes in `component.yaml`'s `on_packet` handler — uncomment
   and complete the two placeholder blocks (`name` tagging block and `token` decode block).
2. If Step 3 found a two-byte sequence, extend the decode logic to combine the pair (a small
   follow-up — flag it in the issue rather than guessing the shape here).
3. If Step 4 found keepalives that would defeat the debounce window, flag it in the issue before
   uncommenting the timer branches — a state-based guard is needed instead of (or in addition
   to) the time-based debounce.
4. Reflash and re-verify each of the 9 buttons fires the correct `event_type` on
   `event.quietcool_remote` in Home Assistant → Developer Tools → Events.
5. Update the byte table in `README.md` with the confirmed timer codes.
