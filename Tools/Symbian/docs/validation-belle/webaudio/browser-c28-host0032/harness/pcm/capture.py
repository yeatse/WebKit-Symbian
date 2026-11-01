#!/usr/bin/env python3
"""Explicit, finite capture of the fixed private test sink; default preparation only."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import selectors
import signal
import subprocess
import time

PACAT = Path('/workspace/symbian-media-work/sysroot/usr/bin/pacat')
SERVER = 'unix:/workspace/symbian-media-work/runtime/native'
SOURCE = 'symbian_media.monitor'
RATE, CHANNELS, FRAME_BYTES = 48000, 2, 4


def save(path, value):
    temp = path.with_suffix(path.suffix + '.tmp')
    temp.write_text(json.dumps(value, indent=2, allow_nan=False) + '\n')
    temp.replace(path)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output', required=True, type=Path)
    p.add_argument('--max-seconds', type=float, default=120)
    p.add_argument('--execute', action='store_true')
    args = p.parse_args()
    if not 2 <= args.max_seconds <= 180:
        p.error('max-seconds must be 2..180')
    args.output.mkdir(mode=0o700, parents=False, exist_ok=False)
    meta = {'suite': 'classic-webaudio-pcm', 'version': 1,
            'status': 'PREPARED_NOT_CAPTURED', 'execute_requested': args.execute,
            'whole_run_verified': False, 'source': SOURCE, 'server': SERVER,
            'format': 's16le', 'sample_rate': RATE, 'channels': CHANNELS,
            'max_seconds': args.max_seconds,
            'capture_script_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
            'pacat': {'path': str(PACAT), 'bytes': PACAT.stat().st_size,
                      'sha256': hashlib.sha256(PACAT.read_bytes()).hexdigest()}}
    save(args.output / 'capture.json', meta)
    if not args.execute:
        print(json.dumps({'status': meta['status'], 'output': str(args.output)}))
        return
    # Do not rely on a default device or touch other Pulse servers/sources.
    if os.environ.get('PULSE_SERVER') != SERVER or os.environ.get('PULSE_SOURCE') != SOURCE:
        raise SystemExit('source the fixed private media env before explicit execution')
    command = [str(PACAT), '--record', '--raw', '--format=s16le', '--rate=48000',
               '--channels=2', '--device=' + SOURCE, '--client-name=classic-webaudio-test',
               '--latency-msec=20', '--process-time-msec=10']
    stop = {'signal': None}

    def request_stop(signum, frame):
        stop['signal'] = signum

    signal.signal(signal.SIGINT, request_stop)
    signal.signal(signal.SIGTERM, request_stop)
    child = None
    total, chunks, anchors = 0, 0, []
    origins = []
    first = last = None
    started = time.monotonic()
    meta.update(status='CAPTURING', started_monotonic=started, pid=os.getpid(),
                command=command, forced_cleanup=False, deadline_expired=False,
                stdout_eof=False, bytes=0, capture_ready=False)
    save(args.output / 'capture.json', meta)
    completed_main = False
    try:
        with (args.output / 'audio.raw').open('xb') as raw, (args.output / 'pacat.stderr').open('xb') as err:
            child = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=err, stdin=subprocess.DEVNULL)
            meta['owned_pacat_pid'] = child.pid
            save(args.output / 'capture.json', meta)
            sel = selectors.DefaultSelector()
            sel.register(child.stdout, selectors.EVENT_READ)
            while True:
                now = time.monotonic()
                if stop['signal'] is not None:
                    meta['stop_reason'] = 'root_signal'
                    break
                if now - started >= args.max_seconds:
                    meta.update(deadline_expired=True, stop_reason='capture_deadline')
                    break
                available = sel.select(timeout=min(.1, args.max_seconds - (now - started)))
                if not available:
                    if child.poll() is not None:
                        meta['stop_reason'] = 'unexpected_child_exit'
                        break
                    continue
                data = os.read(child.stdout.fileno(), 65536)
                observed = time.monotonic()
                if not data:
                    meta.update(stdout_eof=True, stop_reason='unexpected_stdout_eof')
                    break
                raw.write(data)
                raw.flush()
                total += len(data)
                chunks += 1
                anchor = {'observed_monotonic': observed, 'bytes': total}
                origins.append(observed - total / (RATE * FRAME_BYTES))
                if first is None:
                    first = anchor.copy()
                    meta.update(capture_ready=True, first_pcm=first)
                    save(args.output / 'ready.json', {'status': 'PCM_BYTES_RECEIVED', **anchor})
                last = anchor
                if chunks == 1 or chunks % 10 == 0:
                    anchors.append(anchor.copy())
                    meta.update(bytes=total, last_pcm=last)
                    save(args.output / 'capture.json', meta)
            sel.close()
            if child.poll() is None:
                meta['pacat_stop_signal'] = signal.SIGINT
                meta['pacat_stop_monotonic'] = time.monotonic()
                child.send_signal(signal.SIGINT)
                try:
                    child.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    meta['forced_cleanup'] = True
                    child.kill()
                    child.wait(timeout=3)
            # Tail after requested stop belongs to the capture but has no delivery
            # clock anchor. Do not use it to narrow the timing uncertainty.
            tail = child.stdout.read()
            if tail:
                raw.write(tail)
                total += len(tail)
            child.stdout.close()
            meta['pacat_returncode'] = child.returncode
            meta['tail_bytes_after_stop'] = len(tail)
            completed_main = True
    except BaseException as error:
        meta['capture_error_type'] = type(error).__name__
        raise
    finally:
        if child is not None and child.poll() is None:
            meta['forced_cleanup'] = True
            child.kill()
            child.wait(timeout=3)
        meta.update(stopped_monotonic=time.monotonic(), received_stop_signal=stop['signal'],
                    bytes=total, first_pcm=first, last_pcm=last, timing_anchors=anchors,
                    chunks=chunks, frame_aligned=total % FRAME_BYTES == 0)
        if origins:
            meta['estimated_origin_range_monotonic'] = [min(origins), max(origins)]
            meta['origin_spread_seconds'] = max(origins) - min(origins)
        pcm = args.output / 'audio.raw'
        if pcm.exists():
            meta['sha256'] = hashlib.sha256(pcm.read_bytes()).hexdigest()
            meta['duration_seconds'] = total / (RATE * FRAME_BYTES)
        meta['status'] = 'CAPTURE_STOPPED_REQUIRES_ANALYSIS'
        if (not completed_main or not meta.get('capture_ready') or meta.get('forced_cleanup')
                or meta.get('deadline_expired') or meta.get('stop_reason') != 'root_signal'
                or meta.get('pacat_returncode') not in (0, -2) or total % FRAME_BYTES):
            meta['status'] = 'CAPTURE_INCOMPLETE'
        meta['timing_limit'] = 'PCM alignment is estimated from host delivery observations, not hardware timestamps. Spread measures delivery variation, not unknown constant queue delay. No exact guest latency is claimed.'
        save(args.output / 'capture.json', meta)
    print(json.dumps({'status': meta['status'], 'bytes': total, 'output': str(args.output)}))


if __name__ == '__main__':
    main()
