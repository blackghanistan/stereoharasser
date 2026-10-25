#!/bin/bash
# StereoHarasser installer for macOS (Ableton Live, Logic, GarageBand, ...).
# Double-click this file (first time: right-click > Open). It copies the plug-ins into your
# user plug-in folders, clears the "downloaded from the internet" flag and signs them for this Mac.

set -e
cd "$(dirname "$0")"

NAME="StereoHarasser"
VST3_DIR="$HOME/Library/Audio/Plug-Ins/VST3"
AU_DIR="$HOME/Library/Audio/Plug-Ins/Components"

install_bundle () {
    local bundle="$1" dest_dir="$2"

    if [ ! -d "$bundle" ]; then
        echo "  skipping $bundle (not found next to this installer)"
        return
    fi

    mkdir -p "$dest_dir"
    rm -rf "$dest_dir/$bundle"
    cp -R "$bundle" "$dest_dir/"
    xattr -dr com.apple.quarantine "$dest_dir/$bundle" 2>/dev/null || true
    codesign --force --deep --sign - "$dest_dir/$bundle"
    echo "  installed $dest_dir/$bundle"
}

echo "Installing $NAME ..."
install_bundle "$NAME.vst3"      "$VST3_DIR"
install_bundle "$NAME.component" "$AU_DIR"

# make macOS notice the new Audio Unit straight away
killall -9 AudioComponentRegistrar 2>/dev/null || true

echo
echo "Done. In Ableton Live: Settings > Plug-ins, switch on 'Use VST3 Plug-In System Folders'"
echo "and/or 'Use Audio Units', click Rescan, then find $NAME under Plug-Ins > Audio Effects."
echo
read -n 1 -s -r -p "Press any key to close this window."
echo
