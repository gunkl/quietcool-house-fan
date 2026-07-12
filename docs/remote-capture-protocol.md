# QuietCool Remote — Button Capture Protocol & Findings

This document originally specified the test procedure for discovering the physical remote's
command bytes (especially the five **timer** durations, never captured by the original
project). That capture has now been run — this document records the procedure **and** the
confirmed results, for anyone re-verifying on different hardware or extending the decode.

## Confirmed codes vs. known states

| Field | State | Code | Confidence |
|---|---|---|---|
| Power | On | `0xBF` | **Confirmed** — a dedicated "off → wake with no button pressed (default On/High/no-timer)" capture showed 5 clean `0xBF` readings across two separate bursts (proper ~1s heartbeat gap between them), correct ID throughout, with no competing code nearby. An earlier theory that `0xBF` might be RF corruption of `0x3F` (they differ by exactly one bit) is retracted — this dedicated test shows `0xBF` behaving exactly like every other genuine code (burst + heartbeat cadence, robust repetition), which a corruption theory can't explain since there was no `0x3F` present in that window for it to be a misread of. |
| Power | Off | `0x80` (action) / `0xB0` (status) | **Confirmed, both** — `0x80` fires as the immediate action when Off is pressed; `0xB0` is a separate, equally real re-announcement of the same fact (seen after the action, and independently on a later plain wake-up with no fresh press). Normalize both to one canonical "off" value in decode logic so `0xB0` following `0x80` doesn't double-fire. |
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

**De-prioritized — remote status chatter, not user commands:** `0x50`, `0x8c`, `0xde`, and
`0x43` were observed but don't correspond to any of the three user-controllable fields
(power/speed/timer). They're intentionally left undecoded in `component.yaml` — they don't
affect what Climate Advisor needs from this integration.

**Resolved — `0xB0` is Off's status re-announcement, distinct from `0x80`'s action byte.**
Unlike Speed/Timer (where one byte serves as both the action and the heartbeat value), Off
gets two: `0x80` fires once, right when the button is pressed; `0xB0` is what gets re-announced
afterward (on wake, on heartbeat), confirmed by it appearing on a later plain wake-up with no
fresh press at all.

**Theory for why Off (and only Off) needs a dedicated status byte:** `0x66` (Wake) appears to
function as a handshake — "I'm transmitting, listen for my status" — followed by a 3× burst of
whatever the current status actually is. When the fan is On, that status slot is filled by the
current **Speed** value (which already implies "on" — there's nothing else to report). When the
fan is **Off**, there's no speed to report, so Off gets its own dedicated status byte (`0xB0`)
to fill that slot. This explains why there's no equivalent second byte for "On" — On's status
*is* whatever speed is currently held.

**Noise heuristic (confirmed across multiple sessions):** a genuine code always appears as a
3×-per-value transmission (matching the documented protocol behavior), though real-world
corruption often leaves fewer than 3 clean copies. When a value looks anomalous, check whether
it's a **corrupted sibling of an adjacent clean value** (wrong ID, or a 1-bit-different byte)
rather than assuming it's a new code. Confirmed examples: `0xFF`/wrong-ID next to a clean `0xBF`
pair; `0x81`/wrong-ID next to a clean `0x80` pair; a wrong-ID `0xB0` and a corrupted `0xB4`
bracketing a clean `0xB0`. **Pattern noticed across all of these: the corrupted copy is
consistently the *first* transmission attempt right after a silence gap** — plausibly a
receiver settling effect (AGC/carrier-sense lock-on) rather than random noise scattered evenly
across all three copies. When judging a burst, weight the 2nd/3rd readings over the 1st.

This heuristic was tested against `0xBF` itself (it differs from `0x3F` by exactly one bit) and
initially mis-classified as likely corruption — the dedicated On/default-High capture above
overturned that: `0xBF` repeated cleanly 5× with no competing `0x3F` nearby, which a corruption
theory can't explain. Lesson: a clean bit-distance coincidence is not sufficient evidence of
corruption on its own — only a dedicated, isolated capture (or the burst/ID-corruption pattern
above) reliably distinguishes a real code from noise.

## The key protocol discovery: repeats happen, but are not a guarantee to design around

The original assumption (matching the upstream README) was one press → one tight burst of ~3
identical packets ~70ms apart, then silence. A capture of a single LOW-speed press instead
showed `0x1F` repeating **6 times over ~6 seconds**, with **two `0x9F` (timer=none) readings
interspersed partway through** — confirmed to be from a *single* button press, not repeated
presses. Later captures reconfirmed this happening with other fields too (a lone `0xBF` amid a
`0x3F` run; continued `0x9F` repeats during a steady On/Low state).

**Important correction (from later, more careful review): the remote only transmits while it's
actively "on" from an interaction, and repeats beyond the guaranteed 3×-per-command are an
*observed* behavior, not something to rely on architecturally.** Sometimes multiple fields'
commands are seen together (speed + timer, 3× each), and sometimes the whole sequence repeats a
few times — but this is not indefinite, not continuous, and must not be assumed to always
happen. **The only real guarantee is: a genuine command is sent 3× in a tight burst.** Do not
design logic that depends on "if this attempt fails, a later repeat will save it."

This is still why `component.yaml` does **not** use a time-window debounce against the *held*
value to filter repeats — no fixed window can distinguish "re-announcing an old, already-known
value" from "a new press of the same value" purely by elapsed time; comparing against the held
per-field value is what gets that right, regardless of whether/how many repeats occur. See the
ontology comment at the top of the `on_packet` handler in `component.yaml` for the full
reasoning — **do not reintroduce a time-based debounce against the held value for this
purpose.** But since repeats of a *genuinely new* value are bounded (not guaranteed beyond 3×),
a *new* value needs a different, tighter mechanism to be trusted — see the next section.

## Multi-reception confirmation: a single reading is not enough

A generic problem surfaced across several capture sessions: the receiver occasionally decodes a
packet with the **correct remote ID but a corrupted command byte**, and that corrupted byte can
coincidentally land on a byte that's a real, valid value for a field — purely because of which
bit flipped. This is exactly what made `0xBF` briefly look like a possible corruption of `0x3F`
in an earlier draft of this document (see above) — a single ambiguous reading, with no way to
tell real from corrupted without a dedicated follow-up test.

**The fix, generalized:** since a real command is guaranteed to be sent 3× (per the protocol's
own convention) but *not* guaranteed to repeat beyond that, `component.yaml`'s `on_packet` now
requires a differing value to be seen **twice, matching**, before it updates the held state or
fires an HA event — on top of the existing edge-trigger (a value equal to what's already held is
always just a heartbeat, handled as before). A `pending_<field>_value/count/ms` global per field
tracks a candidate in progress, with **two timing tiers**:

- First differing reading of a field → becomes the pending candidate (count=1).
- A second reading of the *same* value, within `CONFIRM_WINDOW_MS` (1500ms) → confirmed: held
  state updates, event fires, pending resets.
- A **mismatching** reading for the same field, arriving within the tighter
  `SAME_BURST_TOLERANCE_MS` (400ms) of the candidate's first sighting, is treated as a possibly
  corrupted copy of the *same* burst and is **ignored outright** — it does not overwrite the
  candidate. This is what correctly handles a sequence like `3F, 2F, 3F` (a bit-flip corruption
  landing on a different value *within the same field*): the `2F` is discarded, and the two
  clean `3F` readings still confirm normally.
- A mismatching reading arriving *after* `SAME_BURST_TOLERANCE_MS` (but still within
  `CONFIRM_WINDOW_MS`) is trusted as a genuinely new candidate and **does** overwrite — by then
  it's very unlikely to be a corrupted copy of the original burst, and treating it conservatively
  would only delay recognizing a real second press.
- A reading for a **different field** never interferes with a field's own candidate at all
  (fields are routed independently to disjoint byte ranges) — so an interleaved `0xBF` amid a
  `0x3F` run, in any order, is always handled correctly (see the ordering check below).
- A candidate that never gets a second matching reading within `CONFIRM_WINDOW_MS` is silently
  abandoned — the same window check that confirms a match also naturally lets a stale candidate
  expire, no separate expiry step needed.

**Explicitly verified, order-independent, for a corrupted byte from a *different* field** (e.g.
real signal = 3× `0x3F`, one corrupted to `0xBF`): `BF,3F,3F` / `3F,BF,3F` / `3F,3F,BF` are all
accepted correctly, since `0xBF` routes to the POWER block regardless of position and never
touches the SPEED field's pending state.

**Timing tiers chosen from observed data**: genuine same-burst repeats land 70–260ms apart
(`SAME_BURST_TOLERANCE_MS=400` gives comfortable margin over that); `CONFIRM_WINDOW_MS=1500` is
a generous *upper bound* that also happens to cover the ~950ms–1s heartbeat gap observed between
separate re-announcement bursts when one occurs — but per the correction above, that extra
repeat is a bonus, not something relied upon; the design must (and does) work correctly using
only the guaranteed 3×-per-command burst.

This resolves the `0xBF`/`0x3F` ambiguity, the same-field `3F`/`2F` case, and similar noise seen
in captures (the isolated `0xB4`, the `0x74`/`0x8d`/`0x14` interference cluster) generically
going forward — no more one-off dedicated tests needed to settle whether a single reading was
real. Applies uniformly to all three fields, including `timer=none` (`0x9F`), which has no HA
token of its own but still needs its held state confirmed correctly so a later real timer
duration is detected as a genuine change.

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

## Status: all three fields (power, speed, timer) fully confirmed

The previously-suggested follow-up — an isolated "fan off → press ON only → watch" capture to
upgrade Power/On from "very likely" to "confirmed" — has been superseded by a dedicated
"off → wake with no button pressed" capture that confirmed `0xBF` (On) robustly, resolving the
last open item. No further capture is required for the feature's core fields.
