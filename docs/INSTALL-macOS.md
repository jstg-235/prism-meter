# Installing New Constellation Prism Meter on macOS

This is a test build. It runs on Apple silicon and Intel Macs (macOS 10.13 or
later) and includes the **AU** (Logic Pro, GarageBand, Ableton Live, and other
AU hosts) and **VST3** (Ableton Live, Cubase, Reaper, Studio One, and others)
versions.

The build is not yet signed with an Apple Developer ID or notarized, so macOS
will ask you to approve it once. That is expected for test builds.

## Option A: the installer (.pkg), recommended

1. Download `PrismMeter-<version>-macOS.pkg`.
2. Double-click it. macOS will say it "cannot verify" the installer. Click
   **Done** (not "Move to Bin").
3. Open **System Settings > Privacy & Security**, scroll down to the Security
   section, and click **Open Anyway** next to the Prism Meter message. Enter
   your password, then click **Open Anyway** again.
   - On macOS 14 or earlier you can instead Control-click the .pkg, choose
     **Open**, then **Open**.
4. Follow the installer. It puts the plugins in:
   - `/Library/Audio/Plug-Ins/Components/Prism Meter.component` (AU)
   - `/Library/Audio/Plug-Ins/VST3/Prism Meter.vst3` (VST3)

If you prefer Terminal, this does steps 2 and 3 in one go:

```sh
xattr -d com.apple.quarantine ~/Downloads/PrismMeter-*-macOS.pkg
open ~/Downloads/PrismMeter-*-macOS.pkg
```

## Option B: copy the plugins yourself (.zip)

1. Download and unzip `PrismMeter-<version>-macOS.zip`.
2. In Finder press **Shift-Command-G** and go to
   `~/Library/Audio/Plug-Ins/Components`. Drag `Prism Meter.component` there.
3. Do the same with `Prism Meter.vst3` into `~/Library/Audio/Plug-Ins/VST3`.
4. Remove the download quarantine so hosts will load them. In Terminal:

```sh
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/"Prism Meter.component"
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/"Prism Meter.vst3"
```

The zip also has `Prism Meter.app`, a standalone version that meters your
audio input with no DAW. To open it the first time, Control-click it and choose
**Open** (or use **Open Anyway** as above).

## Finding it in your DAW

- **Logic Pro**: restart Logic. It scans new Audio Units at launch. The plugin
  is under **Audio Units > New Constellation > Prism Meter**. If Logic marks it
  as failed, open **Logic Pro > Settings > Plug-in Manager**, select it and
  click **Reset & Rescan Selection**.
- **Ableton Live**: open **Settings > Plug-Ins**, turn on **Use Audio Units**
  and/or **Use VST3 Plug-In System Folders**, then click **Rescan**. It shows up
  under **Plug-Ins > New Constellation**.
- Other hosts: rescan plugins, then look under the maker **New Constellation**.

Put Prism Meter on a track or the master bus. It does not change the audio.

## Uninstalling

Delete these, whichever exist:

- `/Library/Audio/Plug-Ins/Components/Prism Meter.component`
- `/Library/Audio/Plug-Ins/VST3/Prism Meter.vst3`
- `~/Library/Audio/Plug-Ins/Components/Prism Meter.component`
- `~/Library/Audio/Plug-Ins/VST3/Prism Meter.vst3`

## If something goes wrong

- **The DAW doesn't list it**: rescan, or restart the DAW. For AU, Terminal
  `auval -v aufx PrMt NwCn` should end with "AU VALIDATION SUCCEEDED".
- **"Prism Meter is damaged and can't be opened"**: the quarantine flag is
  still set. Run the `xattr` commands above.
- Please send a note of your macOS version, DAW and version, and what happened.
