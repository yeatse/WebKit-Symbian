from pathlib import Path
import importlib.util, struct, binascii, hashlib, json, argparse
spec = importlib.util.spec_from_file_location('sis', str(Path(__file__).resolve().parents[3] / 'docs/validation-idna/package/verify-icu-sis.py'))
s = importlib.util.module_from_spec(spec)
spec.loader.exec_module(s)
parser = argparse.ArgumentParser(description='Verify CI40 SIS payloads in ARTIFACT_DIR/sis and extract to ARTIFACT_DIR/payloads.')
parser.add_argument('artifact_dir', type=Path)
base = parser.parse_args().artifact_dir.resolve()
expect = {
    'symbian-webkit-icu.sis': (0xE1A5C013, ['c:\\resource\\icu\\icudt70l.dat']),
    'jsc.sis': (0xE1A5C011, ['!:\\sys\\bin\\jsc.exe', 'c:\\data\\es2020.js', 'c:\\data\\intl.js', 'c:\\data\\basic.js', 'c:\\data\\resizable-arraybuffer.js']),
    'browser.sis': (0xE1A5C012, ['!:\\sys\\bin\\browser.exe', '!:\\private\\10003a3f\\import\\apps\\browser_reg.rsc', '!:\\resource\\apps\\browser_loc.rsc', 'c:\\data\\webkit\\test.html', 'c:\\resource\\webkit\\cacert.pem']),
}
reports = []
(base / 'payloads').mkdir(exist_ok=True)
expected_sums = {}
for line in (base / 'sis' / 'SHA256SUMS').read_text().splitlines():
    digest, package = line.split()
    expected_sums[package.lstrip('*')] = digest
assert set(expected_sums) == set(expect)
for name, (uid, targets) in expect.items():
    image = (base / 'sis' / name).read_bytes()
    assert image and hashlib.sha256(image).hexdigest() == expected_sums[name]
    checksum = binascii.crc_hqx(image[:12:2], 0) | (binascii.crc_hqx(image[1:12:2], 0) << 16)
    assert struct.unpack_from('<IIII', image) == (0x10201A7A, 0, uid, checksum)
    kind, contents, end = s.field(image, 16)
    assert kind == 12 and end == len(image)
    parts = {kind: (payload, raw) for kind, payload, raw in s.fields(contents)}
    for checksum_kind, payload_kind in [(34, 3), (35, 30)]:
        assert struct.unpack('<H', parts[checksum_kind][0])[0] == binascii.crc_hqx(parts[payload_kind][1], 0)
    kind, controller, end = s.field(s.decompress(parts[3][0]))
    assert kind == 13
    children = s.fields(controller)
    sections = {kind: payload for kind, payload, _ in children}
    info = {kind: payload for kind, payload, _ in s.fields(sections[14][:-2])}
    assert struct.unpack('<I', info[9])[0] == uid
    version = '.'.join(map(str, struct.unpack('<III', info[4])))
    assert version == '0.3.40'
    descriptions = s.array(sections[28], 24)
    units = s.array(parts[30][0], 31)
    assert len(units) == 1
    items = s.array(units[0], 32)
    assert len(items) == len(descriptions) == len(targets)
    payloads = []
    for index, (desc, target) in enumerate(zip(descriptions, targets)):
        kind, value, pos = s.field(desc)
        assert kind == 1 and value.decode('utf-16le') == target
        kind, _, pos = s.field(desc, pos)
        assert kind == 1
        kind, value, pos = s.field(desc, pos)
        capabilities = 0
        if kind == 41:
            capabilities = struct.unpack('<I', value)[0]
            kind, value, pos = s.field(desc, pos)
        assert kind == 25 and struct.unpack_from('<I', value)[0] == 1
        kind, expected, _ = s.field(value, 4)
        assert kind == 37
        op, options, stored, size, fileindex = struct.unpack('<IIQQI', desc[pos:])
        assert op == 1 and options == 0 and fileindex == index
        kind, compressed, end = s.field(items[index])
        assert kind == 3 and end == len(items[index])
        payload = s.decompress(compressed)
        assert len(payload) == size and len(compressed) - 12 == stored
        assert hashlib.sha1(payload).digest() == expected
        item = {'target': target, 'bytes': size, 'sha256': hashlib.sha256(payload).hexdigest(), 'file_hash_valid': True}
        if target.endswith('.exe'):
            assert payload[16:20] == b'EPOC'
            flags = struct.unpack_from('<I', payload, 0x2c)[0]
            item.update(e32_flags=hex(flags), hardware_float_type=(flags >> 20) & 15, capabilities=hex(struct.unpack_from('<I', payload, 0x88)[0]))
            assert capabilities == struct.unpack_from('<I', payload, 0x88)[0]
            exe_uid3 = struct.unpack_from('<I', payload, 8)[0]
            assert exe_uid3 == {'jsc.sis': 0xE1A5C001, 'browser.sis': 0xE1A5C002}[name]
            item['e32_uid3'] = hex(exe_uid3)
            assert item['hardware_float_type'] == 1
        extracted = base / 'payloads' / target.rsplit('\\', 1)[-1]
        assert not extracted.exists() or extracted.read_bytes() == payload
        extracted.write_bytes(payload)
        item['extracted_path'] = str(extracted)
        payloads.append(item)
    reports.append({'name': name, 'uid3': hex(uid), 'version': version, 'signature_chains': sum(kind == 39 for kind, _, _ in children), 'header_and_section_crc_valid': True, 'payloads': payloads})
report = {'status': 'PASS', 'run_id': 40925052880, 'head_sha': 'd46c6c5a84b9b96c12827be8214fbb35a93207b7', 'sis_version': '0.3.40', 'payload_count': sum(len(r['payloads']) for r in reports), 'packages': reports}
(base / 'sis-payload-verification.json').write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps(report, indent=2))

summary = dict(status='PASS', run_id=report['run_id'], head_sha=report['head_sha'], branch='2.52.6-symbian', packages=[])
for package in reports:
    path = base / 'sis' / package['name']
    summary['packages'].append(dict(package=package['name'], uid=package['uid3'], version=package['version'], bytes=path.stat().st_size, sha256=expected_sums[path.name], path=str(path), header_and_section_crc='PASS', all_payload_hashes_valid=True, files=package['payloads']))
(base / 'package-verification.json').write_text(json.dumps(summary, indent=2) + '\n')
