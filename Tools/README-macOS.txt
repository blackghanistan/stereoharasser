StereoHarasser - macOS (VST3 + Audio Unit, Apple Silicon and Intel)

INSTALL
1. Double-click "Install.command". The first time macOS may refuse because the file was
   downloaded: right-click it, choose Open, then Open again.
2. Restart Ableton Live. In Live: Settings > Plug-ins, make sure
   "Use VST3 Plug-In System Folders" and/or "Use Audio Units" is on, then click Rescan.
3. Find StereoHarasser in the browser under Plug-Ins > Audio Effects, and drag it onto a track.

DO IT BY HAND INSTEAD
- Copy StereoHarasser.vst3       to  ~/Library/Audio/Plug-Ins/VST3
- Copy StereoHarasser.component  to  ~/Library/Audio/Plug-Ins/Components
- In Terminal, clear the download flag:
    xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/StereoHarasser.vst3
    xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/StereoHarasser.component

NOTE
The plug-in reports a fixed latency of 1023 samples (about 21 ms at 48 kHz); Live compensates
for it automatically.
