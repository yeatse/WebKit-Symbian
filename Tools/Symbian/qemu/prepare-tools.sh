#!/usr/bin/env bash
# Download and unpack Debian tools under WORK; never install system packages.
. "$(dirname "$0")/../scripts/common.sh"
qemu_work=$WORK/qemu
apt_state=$qemu_work/apt
tools_root=${ARM11_TOOLS_ROOT:-$qemu_work/sysroot}
mkdir -p "$apt_state/lists/partial" "$apt_state/archives/partial" "$apt_state/empty" "$tools_root"
touch "$apt_state/status"
cat > "$apt_state/sources.list" <<'EOF'
deb [signed-by=/usr/share/keyrings/debian-archive-keyring.gpg] https://deb.debian.org/debian trixie main
deb [signed-by=/usr/share/keyrings/debian-archive-keyring.gpg] https://deb.debian.org/debian trixie-updates main
deb [signed-by=/usr/share/keyrings/debian-archive-keyring.gpg] https://security.debian.org/debian-security trixie-security main
EOF
cat > "$apt_state/apt.conf" <<EOF
Dir::Etc::parts "$apt_state/empty";
Dir::Etc::main "/dev/null";
Dir::Etc::sourcelist "$apt_state/sources.list";
Dir::Etc::sourceparts "$apt_state/empty";
Dir::State::lists "$apt_state/lists";
Dir::State::status "$apt_state/status";
Dir::State::extended_states "$apt_state/extended_states";
Dir::Cache::archives "$apt_state/archives";
Dir::Cache::pkgcache "$apt_state/pkgcache.bin";
Dir::Cache::srcpkgcache "$apt_state/srcpkgcache.bin";
APT::Install-Recommends "false";
APT::Sandbox::User "$(id -un)";
Acquire::Retries "3";
EOF
APT_CONFIG="$apt_state/apt.conf" apt-get update > "$apt_state/update.log" 2>&1
APT_CONFIG="$apt_state/apt.conf" apt-get --yes --download-only install \
    qemu-user gcc-arm-linux-gnueabi g++-arm-linux-gnueabi libc6-dev-armel-cross \
    > "$apt_state/download.log" 2>&1
for package in "$apt_state/archives"/*.deb; do
    dpkg-deb --extract "$package" "$tools_root"
done
sha256sum "$apt_state/archives"/*.deb > "$apt_state/packages.sha256"
cat > "$qemu_work/env.sh" <<EOF
export ARM11_TOOLS_ROOT='$tools_root'
export ARM11_LINUX_SYSROOT='$tools_root/usr/arm-linux-gnueabi'
export QEMU_ARM='$tools_root/usr/bin/qemu-arm'
export PATH='$tools_root/usr/bin':\$PATH
export LD_LIBRARY_PATH='$tools_root/usr/lib/x86_64-linux-gnu'\${LD_LIBRARY_PATH:+:\$LD_LIBRARY_PATH}
EOF
printf 'Tools unpacked in %s\nSource %s/env.sh before building or running.\n' "$tools_root" "$qemu_work"
