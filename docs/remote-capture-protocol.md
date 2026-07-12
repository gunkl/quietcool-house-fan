# QuietCool Remote — Button Capture Protocol & Findings

This document originally specified the test procedure for discovering the physical remote's
command bytes (especially the five **timer** durations, never captured by the original
project). That capture has now been run — this document records the procedure **and** the
confirmed results, for anyone re-verifying on different hardware or extending the decode.

## Confirmed codes vs. known states

| Field | State | Code | Confidence |
|---|---|---|---|
| Power | On | `0xBF` | Very likely — matches the known transmit-side byte, confirmed with correct remote ID in a live capture (the low→high speed-change sequence), but not yet from a dedicated *isolated* press test the way Off was. |
| Power | Off | `0x80` | **Confirmed** — dedicated isolated test, correct remote ID. |
| Speed | Low | `0x1F` | **Confirmed** — reconfirmed in the high→low transition. |
| Speed | Medium | `0x2F` | **Untested/assumed.** Falls directly between Low and High in the speed family (low nibble fixed at `F`, high nibble `1`→low/`2`→medium/`3`→high). This fan is 2-speed and can't exercise it — documented as if it functioned, per project decision, for forward compatibility. |
| Speed | High | `0x3F` | **Confirmed** — reconfirmed in the low→high transition. |
| Timer | 1h | `0x91` | **Confirmed** |
| Timer | 2h | `0x92` | **Confirmed** |
| Timer | 4h | `0x94` | **Confirmed** |
| Timer | 8h | `0x98` | **Confirmed** |
| Timer | 12h | `0x9C` | **Confirmed** |
| Timer | None | `0x9F` | **Confirmed** |

The timer family fixes the high nibble at `0x9` and uses the low nibble as the literal hour
count, with `0xF` as the "no timer" sentinel. Also confirmed: `0x66` = Wake (pairing-related
chatter, not a user-actionable field).

**De-prioritized — remote status chatter, not user commands:** `0x50`, `0x8c`, `0xb0`, `0xde`,
and `0x43` were observed but don't correspond to any of the three user-controllable fields
(power/speed/timer). They're intentionally left undecoded in `component.yaml` — they don't
affect what Climate Advisor needs from this integration. (One unconfirmed, non-blocking
observation: `0xb0` followed `0x80` in the Off sequence the same way `0xbf` followed `0x3f` in
a speed change — possibly a general "power-field re-announcement" pattern, not verified.)

## The key protocol discovery: this is a heartbeat, not a burst-per-press

The original assumption (matching the upstream README) was one press → one tight burst of ~3
identical packets ~70ms apart, then silence. A capture of a single LOW-speed press instead
showed `0x1F` repeating **6 times over ~6 seconds**, with **two `0x9F` (timer=none) readings
interspersed partway through** — confirmed to be from a *single* button press, not repeated
presses.

**Conclusion: the remote periodically re-broadcasts its full current status** (power, speed,
timer, in rotation) for several seconds after any interaction. This is why `component.yaml`
does **not** use a time-window debounce to filter repeats — no fixed window can distinguish
"still re-announcing an old press" from "a new press of the same value several seconds later."
Instead, `on_packet` tracks the last-*held* value per field and fires `event.quietcool_remote`
only when a field's newly-decoded value differs from what's currently held (edge-triggered).
See the ontology comment at the top of the `on_packet` handler in `component.yaml` for the
full reasoning — **do not reintroduce a time-based debounce for this purpose.**

## Original capture procedure (for reference / re-verification on other hardware)

1. **Sanity pass**: press On, Off, Low, High once each, ~5s apart — confirms the logging
   pipeline and remote pairing (`id_match=yes`) before testing anything else.
2. **Timer buttons, isolated**: press each of 1h/2h/4h/8h/12h once, wait ~10s, record the
   distinct byte(s); press again to confirm repeatability.
3. **Sequence check**: confirmed a *single* byte per field (not a two-packet mode+confirm
   sequence) — the flat per-field byte→token decode in `component.yaml` is correct as-is.
4. **Idle/heartbeat check**: confirmed via the LOW-speed capture above — the remote
   re-announces status for several seconds after any interaction; this is what drove the
   edge-triggered redesign.
5. **Physical-off-during-timer check**: confirmed — the already-known `0x80` byte appears when
   Off is pressed mid-timer, same as a normal off. Validates the assumption the Climate Advisor
   integration ([gunkl/ClimateAdvisor#486](https://github.com/gunkl/ClimateAdvisor/issues/486))
   relies on.
6. **Re-press/override check**: pressing a different timer duration while one is running simply
   emits the new duration's byte directly (no distinct "cancel" signal observed) — the
   edge-triggered decode handles this correctly already, since any value change fires.

## Remaining optional follow-up (not blocking)

An isolated "fan off → press ON only → watch" capture, mirroring the rigor of the Off test,
would upgrade Power/On from "very likely" to "confirmed." Not required — the feature works
correctly either way.
