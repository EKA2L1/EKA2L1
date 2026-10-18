#!/bin/bash
set -euo pipefail
umask 077

if [[ $# != 2 ]]; then
    echo "Usage: $0 input.dmg output.dmg" >&2
    exit 1
fi
for variable in MACOS_SIGNING_CERTIFICATE MACOS_SIGNING_CERTIFICATE_PASSWORD \
    MACOS_NOTARY_KEY_ID MACOS_NOTARY_ISSUER_ID MACOS_NOTARY_PRIVATE_KEY; do
    if [[ -z ${!variable:-} ]]; then
        echo "::error::Required secret $variable is missing" >&2
        exit 1
    fi
done

script_dir=$(cd "$(dirname "$0")" && pwd)
work_dir=$(mktemp -d "${RUNNER_TEMP:-${TMPDIR:-/tmp}}/eka2l1-notary.XXXXXX")
keychain="$work_dir/signing.keychain-db"
original_keychains=()
while IFS= read -r original_keychain; do
    original_keychains+=("$original_keychain")
done < <(security list-keychains -d user | sed -E 's/^[[:space:]]*"//; s/"[[:space:]]*$//')
mounted=false
cleanup() {
    if [[ $mounted == true ]]; then
        hdiutil detach "$work_dir/mount" || true
    fi
    security list-keychains -d user -s "${original_keychains[@]}" >/dev/null 2>&1 || true
    security delete-keychain "$keychain" >/dev/null 2>&1 || true
    rm -rf "$work_dir"
}
trap cleanup EXIT

python3 - "$work_dir" <<'PY'
import base64
import os
from pathlib import Path
import sys

root = Path(sys.argv[1])
(root / "signing.p12").write_bytes(base64.b64decode(os.environ["MACOS_SIGNING_CERTIFICATE"]))
(root / "notary.p8").write_text(os.environ["MACOS_NOTARY_PRIVATE_KEY"])
PY

keychain_password=$(openssl rand -hex 32)
security create-keychain -p "$keychain_password" "$keychain"
security list-keychains -d user -s "$keychain" "${original_keychains[@]}"
security set-keychain-settings -lut 21600 "$keychain"
security unlock-keychain -p "$keychain_password" "$keychain"
security import "$work_dir/signing.p12" -k "$keychain" \
    -P "$MACOS_SIGNING_CERTIFICATE_PASSWORD" -T /usr/bin/codesign
curl --fail --silent --show-error --location \
    https://www.apple.com/certificateauthority/DeveloperIDG2CA.cer -o "$work_dir/DeveloperIDG2CA.cer"
security import "$work_dir/DeveloperIDG2CA.cer" -k "$keychain"
security set-key-partition-list -S apple-tool:,apple:,codesign: -s \
    -k "$keychain_password" "$keychain" >/dev/null

identity=$(security find-identity -v -p codesigning "$keychain" | \
    awk '/"Developer ID Application:/ { print $2; exit }')
if [[ -z $identity ]]; then
    echo "::error::The P12 must contain a valid Developer ID Application certificate and private key" >&2
    exit 1
fi

mkdir -p "$work_dir/mount" "$work_dir/staging" macos-notarization
hdiutil attach -readonly -nobrowse -mountpoint "$work_dir/mount" "$1"
mounted=true
ditto "$work_dir/mount/EKA2L1.app" "$work_dir/staging/EKA2L1.app"
hdiutil detach "$work_dir/mount"
mounted=false
chmod -R u+w "$work_dir/staging/EKA2L1.app"
# Discard ad-hoc signatures stored as resource xattrs before sealing the bundle.
xattr -cr "$work_dir/staging/EKA2L1.app"
python3 "$script_dir/sign_macos_bundle.py" "$work_dir/staging/EKA2L1.app" \
    "$identity" --keychain "$keychain"
ln -s /Applications "$work_dir/staging/Applications"
hdiutil create -volname EKA2L1 -srcfolder "$work_dir/staging" -format UDZO "$2"
codesign --force --sign "$identity" --keychain "$keychain" --timestamp "$2"

submission_status=0
xcrun notarytool submit "$2" --key "$work_dir/notary.p8" \
    --key-id "$MACOS_NOTARY_KEY_ID" --issuer "$MACOS_NOTARY_ISSUER_ID" \
    --wait --timeout 2h --output-format json > macos-notarization/submission.json || submission_status=$?
cat macos-notarization/submission.json
submission_id=$(python3 - <<'PY'
import json
from pathlib import Path
try:
    print(json.loads(Path("macos-notarization/submission.json").read_text()).get("id", ""))
except ValueError:
    print("")
PY
)
if [[ -n $submission_id ]]; then
    xcrun notarytool log "$submission_id" --key "$work_dir/notary.p8" \
        --key-id "$MACOS_NOTARY_KEY_ID" --issuer "$MACOS_NOTARY_ISSUER_ID" \
        macos-notarization/log.json || true
fi
if [[ $submission_status != 0 ]]; then
    exit "$submission_status"
fi
python3 - <<'PY'
import json
from pathlib import Path
result = json.loads(Path("macos-notarization/submission.json").read_text())
if result.get("status") != "Accepted":
    raise SystemExit("Notarization was not accepted; see macos-notarization/log.json")
PY
xcrun stapler staple "$2"
xcrun stapler validate "$2"
codesign --verify --strict "$2"

hdiutil attach -readonly -nobrowse -mountpoint "$work_dir/mount" "$2"
mounted=true
codesign --verify --deep --strict "$work_dir/mount/EKA2L1.app"
spctl --assess --type execute --verbose=4 "$work_dir/mount/EKA2L1.app"
spctl --assess --type open --context context:primary-signature --verbose=4 "$2"
