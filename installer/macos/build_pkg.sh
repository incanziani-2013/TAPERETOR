#!/usr/bin/env bash
# Builds the TRAPERETOR macOS installer (.pkg) with one selectable choice per plugin format.
# Usage: build_pkg.sh <artefacts Release dir> <version> <output dir>
# The VST2 choice is included only when <artefacts>/VST/TRAPERETOR.vst exists.
set -euo pipefail

ART="$1"
VERSION="$2"
OUT="$3"
NAME=TRAPERETOR
ID=com.traperetor
HERE="$(cd "$(dirname "$0")" && pwd)"
WORK="$(mktemp -d)"
mkdir -p "$OUT" "$WORK/pkgs" "$WORK/resources"
cp "$HERE/../../LICENSE" "$WORK/resources/LICENSE.txt"

CHOICES=""
OUTLINE=""

# component <id suffix> <bundle> <install location> <title>
component() {
  local kind="$1" bundle="$2" location="$3" title="$4"
  local root="$WORK/root-$kind" plist="$WORK/$kind.plist" pkg="$NAME-$kind.pkg"
  mkdir -p "$root"
  cp -R "$bundle" "$root/"
  # Keep the installer from "relocating" the bundle to another copy it finds on disk.
  pkgbuild --analyze --root "$root" "$plist"
  # pkgbuild may find no bundle (empty array) or leave out BundleIsRelocatable, so add the key to each entry it found.
  local i=0
  while /usr/libexec/PlistBuddy -c "Print :$i" "$plist" >/dev/null 2>&1; do
    /usr/libexec/PlistBuddy -c "Delete :$i:BundleIsRelocatable" "$plist" >/dev/null 2>&1 || true
    /usr/libexec/PlistBuddy -c "Add :$i:BundleIsRelocatable bool false" "$plist"
    i=$((i + 1))
  done
  plutil -p "$plist"
  pkgbuild --root "$root" --component-plist "$plist" --identifier "$ID.$kind" \
    --version "$VERSION" --install-location "$location" "$WORK/pkgs/$pkg"
  OUTLINE+="    <line choice=\"$kind\"/>"$'\n'
  CHOICES+="  <choice id=\"$kind\" title=\"$title\" description=\"Installs $(basename "$bundle") in $location\">
    <pkg-ref id=\"$ID.$kind\"/>
  </choice>
  <pkg-ref id=\"$ID.$kind\" version=\"$VERSION\" onConclusion=\"none\">$pkg</pkg-ref>"$'\n'
}

component vst3 "$ART/VST3/$NAME.vst3" /Library/Audio/Plug-Ins/VST3 "VST3 plugin"
if [ -d "$ART/VST/$NAME.vst" ]; then
  component vst2 "$ART/VST/$NAME.vst" /Library/Audio/Plug-Ins/VST "VST2 plugin"
fi

cat > "$WORK/distribution.xml" <<EOF
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
  <title>$NAME $VERSION</title>
  <license file="LICENSE.txt"/>
  <options customize="allow" require-scripts="false" hostArchitectures="arm64,x86_64"/>
  <domains enable_localSystem="true"/>
  <choices-outline>
$OUTLINE  </choices-outline>
$CHOICES</installer-gui-script>
EOF

productbuild --distribution "$WORK/distribution.xml" --package-path "$WORK/pkgs" \
  --resources "$WORK/resources" "$OUT/$NAME-$VERSION-macOS-Setup.pkg"
rm -rf "$WORK"
