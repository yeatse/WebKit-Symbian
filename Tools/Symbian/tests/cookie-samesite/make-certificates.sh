#!/usr/bin/env bash
set -eu
out=${1:?usage: make-certificates.sh OUTPUT_DIRECTORY}
test ! -e "$out" || { echo 'Refusing to replace existing credentials' >&2; exit 1; }
umask 077
mkdir -p "$out"
openssl req -x509 -newkey rsa:2048 -nodes -days 7 -sha256 \
  -keyout "$out/ca.key" -out "$out/ca.crt" -subj '/CN=Symbian SameSite fixture CA' \
  -addext 'basicConstraints=critical,CA:TRUE' -addext 'keyUsage=critical,keyCertSign,cRLSign' >/dev/null 2>&1
openssl req -new -newkey rsa:2048 -nodes -sha256 \
  -keyout "$out/server.key" -out "$out/server.csr" -subj '/CN=localhost' >/dev/null 2>&1
cat > "$out/server.ext" <<'EOF'
basicConstraints=critical,CA:FALSE
keyUsage=critical,digitalSignature,keyEncipherment
extendedKeyUsage=serverAuth
subjectAltName=DNS:localhost,IP:127.0.0.1
EOF
openssl x509 -req -in "$out/server.csr" -CA "$out/ca.crt" -CAkey "$out/ca.key" \
  -CAcreateserial -days 2 -sha256 -extfile "$out/server.ext" -out "$out/server.crt" >/dev/null 2>&1
openssl x509 -in "$out/server.crt" -noout -dates -ext subjectAltName
