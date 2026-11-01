#!/usr/bin/env python3
"""Compile actual EKA descriptor and file IPC bodies against fault-injected host seams."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--eka', type=Path, required=True)
parser.add_argument('--work', type=Path)
parser.add_argument('--sanitize', action='store_true')
a = parser.parse_args()
work = a.work or Path(tempfile.mkdtemp(prefix='eka-file-io-'))
work.mkdir(parents=True, exist_ok=True)
paths = ['src/emu/utils/include/utils/des.h', 'src/emu/utils/src/des.cpp', 'src/emu/services/src/fs/files.cpp']
source = [(a.eka / p).read_text() for p in paths]
h, d, f = source
header = h[h.index('    enum des_type'):h.index('    // TODO, IMPORTANT')]
helper = h[h.index('    inline void set_descriptor_length_variable'):h.rindex('\n}')]
impl = d[d.index('    std::uint32_t desc_base::get_max_length'):d.index('    rw_des_stream::rw_des_stream')]
start = f.index('    // The translation of one guest address') if '    // The translation of one guest address' in f else f.index('    void fs_server_client::file_write')
handlers = f[start:f.index('    struct fs_file_mapping', start)]
(work/'actual-descriptor.inc').write_text(header + helper)
(work/'actual-descriptor-methods.inc').write_text(impl)
(work/'actual-file-io.inc').write_text(handlers)
cmd = ['c++', '-std=c++17', '-O1', '-g', '-Wall', '-Wextra', '-Wno-unused-parameter', '-I'+str(work), str(Path(__file__).with_name('host.cpp')), '-o', str(work/'host-test')]
if a.sanitize:
    cmd[1:1] = ['-fsanitize=address,undefined', '-fno-omit-frame-pointer']
result = {'harness_sha256':hashlib.sha256(Path(__file__).with_name('host.cpp').read_bytes()).hexdigest(), 'runner_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(), 'scope':'Actual descriptor definitions/methods and file_read/file_write bodies; substituted guest mapping, IPC and VFS. Not guest IPC acceptance.', 'sources':{p:hashlib.sha256((a.eka/p).read_bytes()).hexdigest() for p in paths}, 'command':cmd}
c = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
(work/'compile.log').write_text(c.stdout)
result['compile_exit'] = c.returncode
if c.returncode:
    print(c.stdout)
else:
    c = subprocess.run([str(work/'host-test')], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    (work/'run.log').write_text(c.stdout)
    result['run_exit'] = c.returncode
    print(c.stdout, end='')
(work/'result.json').write_text(json.dumps(result,indent=2)+'\n')
raise SystemExit(c.returncode)
