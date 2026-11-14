#!/usr/bin/env bash
# Packages a Release build of Prism Meter for testers on macOS.
#
# Produces, in the output folder:
#   PrismMeter-<version>-macOS.pkg   installer: AU + VST3 into /Library/Audio/Plug-Ins
#   PrismMeter-<version>-macOS.zip   the bundles + Standalone app + install notes
#
# The bundles are ad-hoc signed ("codesign -s -"), not Developer ID signed or
# notarized, so macOS Gatekeeper will ask the tester to approve them once.
# See docs/INSTALL-macOS.md.
#
# Usage: tools/package_macos.sh <build-dir> <version> <out-dir>
set -euo pipefail

BUILD_DIR=${1:?build dir}
VERSION=${2:?version}
OUT_DIR=${3:?output dir}
ROOT=$(cd "$(dirname "$0")/.." && pwd)

ART="$BUILD_DIR/plugin/PrismMeter_artefacts/Release"
AU="$ART/AU/Prism Meter.component"
VST3="$ART/VST3/Prism Meter.vst3"
APP="$ART/Standalone/Prism Meter.app"
IDENT="com.newconstellation.prismmeter"
NAME="PrismMeter-$VERSION-macOS"

for b in "$AU" "$VST3" "$APP"; do
    [[ -d "$b" ]] || { echo "missing $b" >&2; exit 1; }
done

# Ad-hoc sign every bundle so Apple silicon hosts will load them, then check.
for b in "$AU" "$VST3" "$APP"; do
    codesign --force --deep --sign - --timestamp=none "$b"
    codesign --verify --deep --strict "$b"
    lipo -info "$(find "$b/Contents/MacOS" -type f | head -1)"
done

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT
mkdir -p "$OUT_DIR"

# ---- .pkg installer -------------------------------------------------------
STAGE="$WORK/pkgroot/Library/Audio/Plug-Ins"
mkdir -p "$STAGE/Components" "$STAGE/VST3"
ditto "$AU" "$STAGE/Components/Prism Meter.component"
ditto "$VST3" "$STAGE/VST3/Prism Meter.vst3"

# Install exactly where we say: never "relocate" onto a copy found elsewhere.
pkgbuild --analyze --root "$WORK/pkgroot" "$WORK/components.plist"
i=0
while plutil -extract "$i" xml1 -o /dev/null "$WORK/components.plist" 2>/dev/null; do
    plutil -replace "$i.BundleIsRelocatable" -bool NO "$WORK/components.plist"
    i=$((i + 1))
done
[[ $i -gt 0 ]] || { echo "pkgbuild --analyze found no bundles" >&2; exit 1; }

mkdir -p "$WORK/scripts"
cat > "$WORK/scripts/postinstall" <<'EOF'
#!/bin/sh
# Make hosts rescan Audio Units right away.
killall -9 AudioComponentRegistrar 2>/dev/null || true
exit 0
EOF
chmod +x "$WORK/scripts/postinstall"

pkgbuild \
    --root "$WORK/pkgroot" \
    --component-plist "$WORK/components.plist" \
    --scripts "$WORK/scripts" \
    --identifier "$IDENT.pkg" \
    --version "$VERSION" \
    --install-location / \
    "$WORK/PrismMeter-component.pkg"

cat > "$WORK/distribution.xml" <<EOF
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
    <title>New Constellation Prism Meter $VERSION</title>
    <options customize="never" require-scripts="false" hostArchitectures="arm64,x86_64"/>
    <domains enable_localSystem="true"/>
    <choices-outline><line choice="default"/></choices-outline>
    <choice id="default" title="Prism Meter (AU + VST3)">
        <pkg-ref id="$IDENT.pkg"/>
    </choice>
    <pkg-ref id="$IDENT.pkg" version="$VERSION">PrismMeter-component.pkg</pkg-ref>
</installer-gui-script>
EOF

productbuild \
    --distribution "$WORK/distribution.xml" \
    --package-path "$WORK" \
    "$OUT_DIR/$NAME.pkg"

# ---- .zip (manual install) ------------------------------------------------
Z="$WORK/zip/$NAME"
mkdir -p "$Z"
ditto "$AU" "$Z/Prism Meter.component"
ditto "$VST3" "$Z/Prism Meter.vst3"
ditto "$APP" "$Z/Prism Meter.app"
cp "$ROOT/docs/INSTALL-macOS.md" "$Z/INSTALL.md"
(cd "$WORK/zip" && ditto -c -k --keepParent "$NAME" "$OUT_DIR/$NAME.zip")

echo "Packaged:"
ls -lh "$OUT_DIR"
