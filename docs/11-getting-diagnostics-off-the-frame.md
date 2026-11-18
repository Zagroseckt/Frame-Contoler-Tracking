# 11 — Getting diagnostics off the Frame

How to pull logs, captures, configs and tracking data **off** the headset and onto this
workstation. Everything below is intended to be copied as a whole command block where
possible, then run from the project root (`~/workshop/SFControlerTracking`).

Evidence tags are the same as in `README.md`: `[V]` verified on our hardware, `[D]`
documented by Valve, `[I]` inferred, `[U]` untested. Dates and builds matter — record them
with every capture.

---

## 0. Before you start

On the headset (one time, or after an update):

1. **Steam Settings → System → Enable Developer Mode.** This turns on SSH, ADB and RDP `[D]`.
2. **Developer → Set User Password.** There is no default password `[D]`.
3. Make sure the headset is awake and on the same network.

From this workstation, the SSH alias is **`frame`** (user `steamos`) `[V]`. Valve's default
hostname is `frame` and can be changed in **Steam Settings → System → Hostname** `[D]`.
If a bare name does not resolve, add a `Host frame` block to `~/.ssh/config` with the
headset's current address, or use the SteamOS Devkit pairing flow to install a key.

Do not paste an IP address, password, or device serial into any file that gets committed.

---

## 1. Check the link first

```bash
cd ~/workshop/SFControlerTracking
./scripts/frame-check.sh
```

That prints reachability, architecture/OS, SteamVR processes, `/dev/video99`, the binary
listing, and recently modified tracking-ish files. The single-line version is:

```bash
ssh -o BatchMode=yes -o ConnectTimeout=6 frame 'uname -m; . /etc/os-release; echo "$PRETTY_NAME"'
```

If that fails: wake the headset, confirm Developer Mode is still on, check the address in
Quick Settings / Steam Settings → Internet, and re-run. `ssh -v frame` shows where it stalls.

---

## 2. Copy the Steam / SteamVR logs

The live log directory on the device is `/home/steamos/.local/share/Steam/logs/` `[V]`.
It contains `vrserver.txt`, `vrcompositor.txt`, `vrcmd.txt`, `v4l2cam.txt`, and
`XRService-<date>/` subdirectories. The `cdl` helper jumps there in an **interactive**
headset shell; it is not available to a one-shot `ssh`, so use the absolute path.

```bash
cd ~/workshop/SFControlerTracking
mkdir -p "captures/logs/$(date +%F)"
ssh frame 'ls -lt /home/steamos/.local/share/Steam/logs | head -40'
scp -r frame:/home/steamos/.local/share/Steam/logs/. "captures/logs/$(date +%F)/"
```

Useful first pass through the copy:

```bash
grep -iE "tracked|controller|pose|constellation|led|blob" \
  "captures/logs/$(date +%F)/vrserver.txt" | head -100
```

---

## 3. Copy the OpenVR config tree (small, high value)

This tree holds `roy/<serial>.json` controller constellation files, `cv/xrservice/` settings
and calibration history, `steamvr.vrsettings`, and `openvrpaths.vrpath` `[V]`.

```bash
cd ~/workshop/SFControlerTracking
mkdir -p local/frame-export/home/steamos/.config/openvr/config
scp -r frame:/home/steamos/.config/openvr/config/. \
  local/frame-export/home/steamos/.config/openvr/config/
```

The files are named by controller serial, so this directory is private. It lives under
`local/`, which is git-ignored.

---

## 4. Copy the tracking binaries and resources

This is the big one (roughly 340 MB for the whole stack). It mirrors the device's absolute
paths under `local/frame-export/`, which is how the verified export was laid out `[V]`.

```bash
cd ~/workshop/SFControlerTracking
mkdir -p local/frame-export/opt/steamvr/drivers/cv
mkdir -p local/frame-export/opt/steamvr/bin/linuxarm64
scp -r frame:/opt/steamvr/drivers/cv/. local/frame-export/opt/steamvr/drivers/cv/
scp -r frame:/opt/steamvr/bin/linuxarm64/. local/frame-export/opt/steamvr/bin/linuxarm64/
```

Then write and check a manifest, as we did for the first export:

```bash
cd ~/workshop/SFControlerTracking/local/frame-export
find . -type f ! -name MANIFEST.sha256 -print0 | LC_ALL=C sort -z \
  | xargs -0 sha256sum > MANIFEST.sha256
sha256sum -c MANIFEST.sha256
```

Never edit the files on the headset to make a copy easier. The root filesystem is read-only;
if you ever need it writable, `sudo steamos-readonly disable` `[D]`, and restore it after.

---

## 5. Record a tracking dataset with camera video

This part is a **headset UI operation**, not a shell command `[D] [V]`.

1. Wear the headset; open VR dashboard → Steam overlay → VR Settings.
2. Enable **Advanced Settings**.
3. **Developer** tab → **Record Tracking Data** → enable **Allow Camera Recording**
   (plus screen/audio if useful for narration).
4. START, confirm the recording indicator, close the dashboard.
5. Move the controllers through varied poses, including partial occlusion and out-of-frame,
   for roughly 30–60 seconds.
6. STOP from the same menu.

Then find and copy it:

```bash
ssh frame 'find ~ /home/steamos /tmp -newermt "-10 minutes" -type f 2>/dev/null \
  | grep -iE "track|dataset|capture" | head -50'
```

Set `DATASET` to the path that looks right, then:

```bash
cd ~/workshop/SFControlerTracking
mkdir -p captures/datasets
DATASET='/path/printed/by/the/find/above'
scp -r "frame:$DATASET" captures/datasets/
```

Full procedure and open questions: `experiments/EXP-003-tracking-dataset.md`.
Decode the camera streams with `tools/occblob.py` and `run.sh`; see `docs/10`.

---

## 6. Grab the headset view mirror

SteamVR publishes the composited headset view on `/dev/video99` `[V]`. It is **not** a raw
tracking camera, and an unworn headset gives a black frame — grab several.

```bash
cd ~/workshop/SFControlerTracking
mkdir -p captures/cameras
ssh frame 'ffmpeg -f v4l2 -i /dev/video99 -frames:v 1 /tmp/headsetview.png'
scp frame:/tmp/headsetview.png "captures/cameras/headsetview-$(date +%F).png"
```

Note before encoding on-device: the hardware encoder crashes `h264_v4l2m2m`; use `libx264`
if you need a clip. Details in `docs/05` §2.

The true tracking cameras (`/dev/video9`, `13`, `6`, `7`) are held exclusively by XRService
while tracking runs `[V]`. Do not stop tracking casually to read them.

---

## 7. Live device state without copying anything

These run on the Frame and print to the SSH session. Capture them straight into `captures/`:

```bash
cd ~/workshop/SFControlerTracking
mkdir -p captures/logs
ssh frame 'cd /opt/steamvr/bin/linuxarm64 && LD_LIBRARY_PATH=. ./vrcmd --info' \
  | tee "captures/logs/vrcmd-info-$(date +%F).txt"
ssh frame 'cd /opt/steamvr/bin/linuxarm64 && LD_LIBRARY_PATH=. ./vrcmd --dumpsensors' \
  | tee "captures/logs/vrcmd-sensors-$(date +%F).txt"
```

`trackingtest` prints live poses at about 9 Hz; `vrcmd --pollcontrollers` blocks until
controller events arrive. Both are described in `docs/07` §10. Design rationale for using
an OpenVR client instead of a second tracker: `docs/08`.

---

## 8. Lepton / Android logs

`lepton dev logcat` on the Frame covers the Android container, but only while Lepton
Development or an Android title is running `[D]`. The `steamos-devkit` MCP can also collect
logs and run ADB; its ADB target is the **Lepton Android container**, not the native SteamOS
shell. For the native Linux shell, use `adb shell` over USB-C. Keep those two paths distinct.

```bash
ssh frame 'lepton dev logcat' | tee "captures/logs/lepton-$(date +%F).txt"
```

---

## 9. Capture hygiene

- `captures/` and `local/` are git-ignored. Large binaries, room imagery, serials and the
  headset filesystem mirror stay local.
- Never commit credentials, IP addresses, or controller/headset serials. Use `frame` as the
  host name in notes.
- With every capture record: date, headset/SteamVR build, what changed, result, and an
  evidence tag `[V]/[D]/[I]/[U]`.
- If a note contradicts a doc, the note wins — but re-test before it becomes law.

---

## 10. If SSH is unavailable

Alternatives that have been used or documented:

- SteamOS Devkit Client / `steamos-devkit` MCP: discover, pair, and collect logs without a
  shell prompt. Pairing needs **Steam Settings → Developer → Pair new host** on the headset.
- Frame Control's `connect.sh` and `docs/ssh.md` in that project explain key setup and the
  devkit pairing flow in detail.
- `adb shell` over USB-C reaches the native Linux OS; USB power from a plain host port may be
  insufficient.
- RDP with user `steamos` and the Developer Mode password gives a desktop if you need to
  click a dashboard toggle by hand.
