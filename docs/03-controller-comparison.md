# 03 — Controller comparison

Side-by-side of every controller family relevant to this project. Verified facts are tagged;
press claims are tagged as such. Fill in `[V]` rows as we measure on our own hardware.

## Table

| | **Steam Frame Controller** | **Quest 3 — Touch Plus** | **Valve Index Controller** | **Quest Pro — Touch Pro** |
|---|---|---|---|---|
| Tracking principle | Optical IR constellation + IMU, tracked by headset cameras `[D]` | Optical IR constellation + IMU, tracked by Quest cameras `[D]` | Lighthouse: photodiodes on controller receive laser sweeps from base stations `[D]` | **Self-tracking**: onboard cameras + chipset; no headset line-of-sight needed `[D]` |
| IR emitters | **18 LEDs** (face, handle, bottom) `[P]` | **8 LEDs** (7 face + 1 handle) `[D]` | None (receivers, not emitters) `[D]` | None used for tracking `[D]` |
| Base stations needed | No `[D]` | No `[D]` | **Yes**, 2 recommended `[D]` | No `[D]` |
| Ring | Ringless `[P]` | Ringless `[D]` | Ring + grip plate `[D]` | Ringless `[D]` |
| IMU | Yes `[D]` | Yes `[D]` | Yes `[D]` | Yes `[D]` |
| Radio | Proprietary 2.4 GHz `[I]` | 2.4 GHz, FCC-confirmed `[D]` | Proprietary 2.4 GHz dongle/headset link `[I]` | 2.4 GHz to headset `[I]` |
| Pairing flow | Auto-pair on first power-on; re-pair prompts in headset `[D]` | Phone app: Meta+B / Menu+Y, confirm in app `[D]` | SteamVR pairing, base-station-bound `[D]` | Same family flow as Quest `[I]` |
| Finger tracking | Capacitive on every input + handle, skeletal input API `[D]` | Capacitive thumb rest, grip, trigger, buttons `[D]` | Capacitive grip/trigger/thumb, 87-sensor hand model `[D]` | Capacitive, similar to Touch `[I]` |
| Triggers | Analog, capacitive `[D]` | Two-stage analog index trigger `[D]` | Analog + force sensor `[D]` | Analog `[I]` |
| Sticks | TMR (magnetoresistive), drift-resistant `[P]` | Standard, drift reports exist `[P]` | Standard `[D]` | Standard `[D]` |
| Notable | Gamepad-parity layout: A/B/X/Y right, D-pad left `[D]` | Same as Quest 2 layout | Trackpad + squeeze force | Cameras on controller |

## Why the LED count matters more than anything else here

The Frame's vision system is built around **18 emitters per controller** `[P]`. Eight emitters
is a dramatically thinner constellation, and Touch Plus puts seven of them on a plane facing
the headset with a single one on the handle `[D]`.

Two independent problems follow:

1. **Identification.** A fixed 8-dot pattern is much easier to mistake for an unrelated
   cluster than an 18-dot pattern. Meta compensates with AI and by fusing continuous hand
   tracking `[D]`; the Frame has no announced hand tracking `[P]`.
2. **Occlusion.** Meta's own CTO noted the ringless design means LEDs get hidden by hands and
   orientation more often `[D]`. Frame controllers mitigate with spread-out LEDs; Touch Plus
   mitigates with software.

## The Index problem, stated plainly

Index controllers do not emit anything. They *receive* laser sweeps and report sensor timings
`[D]`. The Frame does not emit laser sweeps and, per press reporting at launch, does not
support outside-in/lighthouse tracking `[P]` — re-verify on the current build `[U]`.

Therefore "pair an Index controller to the Frame" cannot mean what it means for a Quest 3
controller. It can only mean one of:

- **Rig it.** Add our own IR emitters to the Index controller and track that rig optically
  (`ROADMAP.md` P4). The Index controller's native sensing goes unused.
- **Bridge it.** Keep a lighthouse setup alive on a PC and relay poses. That's a different
  project and drags base stations back into the room.

The far goal in this repo assumes **rig it**.

## Buying/borrowing notes

- Quest 3 controllers are sold individually, left or right `[P]`. A single spare is enough for
  first optical experiments.
- Index controllers require base stations to do anything native — do not buy them for this
  project expecting them to work standalone.
- An IR-sensitive camera (many phone cameras without IR-cut filters, or a cheap security cam
  with the filter removed) is the single most useful cheap instrument here. It sees the LEDs
  without any Valve software.
