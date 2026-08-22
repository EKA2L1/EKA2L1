#!/usr/bin/env bash
# Build an App Store-signed archive, upload it to TestFlight, and export the
# same archive as the IPA attached to the rolling GitHub release.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${ROOT_DIR}"

required_secrets=(
    APPLE_TEAM_ID
    APPSTORE_ISSUER_ID
    APPSTORE_KEY_ID
    APPSTORE_PRIVATE_KEY
)
for name in "${required_secrets[@]}"; do
    if [ -z "${!name:-}" ]; then
        echo "Required environment variable ${name} is not configured" >&2
        exit 1
    fi
done

temp_root="${RUNNER_TEMP:-${TMPDIR:-/tmp}}"
release_temp="$(mktemp -d "${temp_root}/eka2l1-ios-release.XXXXXX")"
keychain_path="${release_temp}/eka2l1-signing.keychain-db"
key_path="${release_temp}/AuthKey_${APPSTORE_KEY_ID}.p8"
archive_path="${EKA2L1_IOS_ARCHIVE_PATH:-build/ios-device/EKA2L1.xcarchive}"
ipa_path="${EKA2L1_IOS_IPA_PATH:-build/ios-release/EKA2L1-iOS.ipa}"
keychain_created=false

cleanup() {
    rm -f "${release_temp}/dev_cert.p12" "${release_temp}/dist_cert.p12" "${key_path}"
    if [ "${keychain_created}" = true ]; then
        security delete-keychain "${keychain_path}" || true
        security list-keychains -d user -s login.keychain-db
    fi
    rm -rf "${release_temp}"
}
trap cleanup EXIT

printf '%s\n' "${APPSTORE_PRIVATE_KEY}" > "${key_path}"
chmod 600 "${key_path}"

# Persisting the development certificate in repository secrets avoids creating
# one per ephemeral runner and eventually exhausting Apple's certificate cap.
if [ -n "${IOS_DEV_SIGNING_KEY:-}" ] || [ -n "${IOS_DIST_SIGNING_KEY:-}" ]; then
    keychain_password="$(uuidgen)"
    security create-keychain -p "${keychain_password}" "${keychain_path}"
    keychain_created=true
    security set-keychain-settings -lut 21600 "${keychain_path}"
    security unlock-keychain -p "${keychain_password}" "${keychain_path}"

    if [ -n "${IOS_DEV_SIGNING_KEY:-}" ]; then
        printf '%s' "${IOS_DEV_SIGNING_KEY}" | base64 --decode > "${release_temp}/dev_cert.p12"
        security import "${release_temp}/dev_cert.p12" \
            -P "${IOS_DEV_SIGNING_KEY_PASSWORD:-}" \
            -A -t cert -f pkcs12 -k "${keychain_path}"
    fi
    if [ -n "${IOS_DIST_SIGNING_KEY:-}" ]; then
        printf '%s' "${IOS_DIST_SIGNING_KEY}" | base64 --decode > "${release_temp}/dist_cert.p12"
        security import "${release_temp}/dist_cert.p12" \
            -P "${IOS_DIST_SIGNING_KEY_PASSWORD:-}" \
            -A -t cert -f pkcs12 -k "${keychain_path}"
    fi

    security set-key-partition-list -S apple-tool:,apple: \
        -k "${keychain_password}" "${keychain_path}" > /dev/null
    security list-keychains -d user -s "${keychain_path}" login.keychain-db
else
    echo "Warning: no persisted signing certificate is configured; Xcode may create one automatically." >&2
fi

export EKA2L1_IOS_CONFIGURATION="${EKA2L1_IOS_CONFIGURATION:-Release}"
export EKA2L1_IOS_DEVELOPMENT_TEAM="${APPLE_TEAM_ID}"
export EKA2L1_IOS_ARCHIVE_PATH="${archive_path}"
export EKA2L1_ASC_KEY_PATH="${key_path}"
export EKA2L1_ASC_KEY_ID="${APPSTORE_KEY_ID}"
export EKA2L1_ASC_KEY_ISSUER_ID="${APPSTORE_ISSUER_ID}"
./scripts/build_ios.sh archive

upload_options="${release_temp}/UploadOptions.plist"
cat > "${upload_options}" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>method</key>
    <string>app-store-connect</string>
    <key>destination</key>
    <string>upload</string>
    <key>signingStyle</key>
    <string>automatic</string>
    <key>teamID</key>
    <string>${APPLE_TEAM_ID}</string>
    <key>uploadSymbols</key>
    <true/>
    <key>manageAppVersionAndBuildNumber</key>
    <true/>
</dict>
</plist>
EOF

xcodebuild -exportArchive \
    -archivePath "${archive_path}" \
    -exportOptionsPlist "${upload_options}" \
    -exportPath "${release_temp}/upload" \
    -allowProvisioningUpdates \
    -authenticationKeyPath "${key_path}" \
    -authenticationKeyID "${APPSTORE_KEY_ID}" \
    -authenticationKeyIssuerID "${APPSTORE_ISSUER_ID}"

export_options="${release_temp}/ExportOptions.plist"
cat > "${export_options}" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>method</key>
    <string>app-store-connect</string>
    <key>destination</key>
    <string>export</string>
    <key>signingStyle</key>
    <string>automatic</string>
    <key>teamID</key>
    <string>${APPLE_TEAM_ID}</string>
    <key>manageAppVersionAndBuildNumber</key>
    <false/>
</dict>
</plist>
EOF

xcodebuild -exportArchive \
    -archivePath "${archive_path}" \
    -exportOptionsPlist "${export_options}" \
    -exportPath "${release_temp}/export" \
    -allowProvisioningUpdates \
    -authenticationKeyPath "${key_path}" \
    -authenticationKeyID "${APPSTORE_KEY_ID}" \
    -authenticationKeyIssuerID "${APPSTORE_ISSUER_ID}"

exported_ipa="$(find "${release_temp}/export" -maxdepth 2 -type f -name '*.ipa' -print -quit)"
if [ -z "${exported_ipa}" ]; then
    echo "No IPA was produced under ${release_temp}/export" >&2
    exit 1
fi

mkdir -p "$(dirname "${ipa_path}")"
cp "${exported_ipa}" "${ipa_path}"
echo "Exported signed IPA to ${ipa_path}"
