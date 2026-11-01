#!/usr/bin/env python3
"""Submit one root-reviewed step to the running ordinary-download observer."""
from pathlib import Path
import argparse, json, time, sys, os

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--operation', choices=('capture','act','review-result','release'), required=True)
    p.add_argument('--reviewed', action='store_true')
    p.add_argument('--role', choices=('start','native-step','edit-name','save-accept','transfer-cancel','existing-attempt','existing-dismiss','dismiss-note'))
    p.add_argument('--target', choices=('main','fep'), default='main')
    g=p.add_mutually_exclusive_group(); g.add_argument('--point', nargs=2, type=int); g.add_argument('--key'); g.add_argument('--text')
    p.add_argument('--modifier', choices=('Control_L','Shift_L'))
    p.add_argument('--selected-guest-path')
    args=p.parse_args(); state=json.loads((args.output/'state.json').read_text())
    seq=state['command_sequence']+1
    value={'run':state['run'], 'sequence':seq, 'operation':args.operation}
    if args.operation=='act':
        if not args.reviewed or not args.role or not (args.point is not None or args.key or args.text):
            p.error('act needs --reviewed, --role and one actual input')
        action={'target':args.target, 'role':args.role, 'wait':.5}
        if args.point is not None: action['point']=args.point
        elif args.key: action['key']=args.key
        else: action['text']=args.text
        if args.modifier: action['modifier']=args.modifier
        value.update(capture_sha256=state['capture_sha256'], screenshots_reviewed=True, actions=[action])
    elif args.operation=='review-result':
        if not args.reviewed or not args.selected_guest_path:
            p.error('final review needs --reviewed and actual native selected path')
        value.update(native_ui_reviewed=True, selected_guest_path=args.selected_guest_path)
    target=args.output/'commands'/f'{seq:03d}.json'
    temporary=target.with_suffix('.private-tmp')
    created=False
    try:
        with temporary.open('x') as f:
            created=True
            json.dump(value, f)
            f.flush()
            os.fsync(f.fileno())
        # Atomic publication, with no overwrite if a second operator raced.
        os.link(temporary, target)
    finally:
        if created:
            temporary.unlink(missing_ok=True)
    end=time.monotonic()+12
    while time.monotonic()<end:
        current=json.loads((args.output/'state.json').read_text())
        if current['command_sequence']>=seq or current.get('control_error'):
            print(json.dumps(current, ensure_ascii=False, indent=2))
            return 0 if not current.get('control_error') else 1
        time.sleep(.1)
    print('Request written; no acknowledgement in 12 seconds. Do not resend the same input.', file=sys.stderr)
    return 2
if __name__=='__main__': raise SystemExit(main())
