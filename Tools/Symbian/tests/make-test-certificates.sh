#!/usr/bin/env bash
# Creates short-lived local test credentials; never include them in a SIS.
set -euo pipefail
out=${1:?usage: make-test-certificates.sh NEW_OUTPUT_DIRECTORY}
if [ -e "$out" ]; then
    echo "Refusing to overwrite existing test credentials: $out" >&2
    exit 1
fi
umask 077
mkdir -p "$out"
openssl req -x509 -newkey rsa:2048 -nodes -days 7 -sha256 \
    -keyout "$out/ca.key" -out "$out/ca.crt" -subj '/CN=WebKit Symbian local test CA' \
    -addext 'basicConstraints=critical,CA:TRUE' -addext 'keyUsage=critical,keyCertSign,cRLSign' >/dev/null 2>&1
for name in good wrong; do
    host=localhost
    [ "$name" != wrong ] || host=wrong.invalid
    openssl req -new -newkey rsa:2048 -nodes -sha256 -keyout "$out/$name.key" \
        -out "$out/$name.csr" -subj "/CN=$host" >/dev/null 2>&1
    cat > "$out/$name.ext" <<EOF
basicConstraints=critical,CA:FALSE
keyUsage=critical,digitalSignature,keyEncipherment
extendedKeyUsage=serverAuth
subjectAltName=DNS:$host
EOF
    openssl x509 -req -in "$out/$name.csr" -CA "$out/ca.crt" -CAkey "$out/ca.key" \
        -CAcreateserial -days 2 -sha256 -extfile "$out/$name.ext" -out "$out/$name.crt" >/dev/null 2>&1
done
openssl req -x509 -newkey rsa:2048 -nodes -days 2 -sha256 \
    -keyout "$out/untrusted.key" -out "$out/untrusted.crt" -subj '/CN=localhost' \
    -addext 'basicConstraints=critical,CA:FALSE' -addext 'subjectAltName=DNS:localhost' >/dev/null 2>&1
openssl x509 -in "$out/good.crt" -noout -dates
printf 'Test CA: %s/ca.crt\n' "$out"
