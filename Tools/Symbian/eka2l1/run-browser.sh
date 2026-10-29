#!/usr/bin/env bash
# Run a browser image with isolated logs and a bounded lifetime.
# EKA2L1_EXPECT_PASS optionally requires a fresh browser console/log marker.
. "$(dirname "$0")/../scripts/common.sh"
: "${EKA2L1:?set EKA2L1 to the eka2l1_qt executable}"
export XDG_DATA_HOME=${XDG_DATA_HOME:-$WORK/eka2l1/xdg/data}
export XDG_CONFIG_HOME=${XDG_CONFIG_HOME:-$WORK/eka2l1/xdg/config}
export XDG_CACHE_HOME=${XDG_CACHE_HOME:-$WORK/eka2l1/xdg/cache}
export XDG_RUNTIME_DIR=${XDG_RUNTIME_DIR:-$WORK/eka2l1/xdg/runtime}
mkdir -p "$XDG_RUNTIME_DIR"
chmod 700 "$XDG_RUNTIME_DIR"
export EKA2L1_DATA=${EKA2L1_DATA:-$XDG_DATA_HOME/EKA2L1/data}
export SYMBIAN_BROWSER_WORK=$WORK SYMBIAN_BROWSER_TESTS=$REPO/tests
export SYMBIAN_BROWSER_PAGE=$REPO/browser/test.html
export SYMBIAN_BROWSER_ICU=$ICU_DIR/install/share/icu/icudt70l.dat
export SYMBIAN_BROWSER_CLOSE_WINDOW=$REPO/eka2l1/close-emulator-window.py
python3 - "$@" <<'PY'
import datetime, fcntl, os
from pathlib import Path
import re, shutil, signal, subprocess, sys, tempfile, time

if len(sys.argv) < 2:
    sys.exit('usage: run-browser.sh BROWSER_EXE [URL]')
exe=Path(sys.argv[1]).resolve()
url=sys.argv[2] if len(sys.argv)>2 else 'file:///C:/Data/WebKit/test.html'
data=Path(os.environ['EKA2L1_DATA']).resolve(); data.mkdir(parents=True,exist_ok=True)
lockpath=data/'.browser-run.lock'
if inherited_fd:=os.environ.get('EKA2L1_BROWSER_LOCK_FD'):
    lock=os.fdopen(os.dup(int(inherited_fd)),'a')
    held=os.fstat(lock.fileno());expected_lock=lockpath.stat()
    if (held.st_dev,held.st_ino)!=(expected_lock.st_dev,expected_lock.st_ino):
        sys.exit('Inherited browser lock does not match this emulator data directory')
else:
    lock=lockpath.open('a')
fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
work=Path(os.environ['SYMBIAN_BROWSER_WORK'])
runs=work/'runs';runs.mkdir(parents=True,exist_ok=True)
run=Path(tempfile.mkdtemp(prefix='browser-'+datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%SZ-'),dir=runs))
c=data/'drives/c'
for directory in ['sys/bin','Data/WebKit','private/10003a3f/import/apps','resource/apps','resource/icu']:
    (c/directory).mkdir(parents=True,exist_ok=True)
for src,dst in [(exe,'sys/bin/browser.exe'),(exe.parent/'browser_reg.rsc','private/10003a3f/import/apps/browser_reg.rsc'),(exe.parent/'browser_loc.rsc','resource/apps/browser_loc.rsc'),(Path(os.environ['SYMBIAN_BROWSER_ICU']),'resource/icu/icudt70l.dat'),(Path(os.environ['SYMBIAN_BROWSER_PAGE']),'Data/WebKit/test.html')]:
    shutil.copy2(src,c/dst)
# Older archived browser images predate the native prompt resource. New
# candidates carry it beside the executable, on the same guest drive.
ui_resource=exe.parent/'browser_ui.rsc'
if ui_resource.is_file():
    shutil.copy2(ui_resource,c/'resource/apps/browser_ui.rsc')
for page in Path(os.environ['SYMBIAN_BROWSER_TESTS']).glob('*.html'):
    shutil.copy2(page,c/'Data/WebKit'/page.name)
for name in ['log.txt','log.old.txt','curl.txt']:
    old=c/'Data/WebKit'/name
    if old.exists():old.rename(run/('previous-'+name))
emulator=str(Path(os.environ['EKA2L1']).resolve())
command=[emulator,'--device',os.environ.get('EKA2L1_DEVICE','RM-707'),'--run','C:\\sys\\bin\\browser.exe',' '+url]
if not os.environ.get('DISPLAY'):
    sys.exit('Set DISPLAY to a running Xvfb (-nolisten tcp) or X display for screenshots')
expected=os.environ.get('EKA2L1_EXPECT_PASS','')
deadline=time.monotonic()+float(os.environ.get('WAIT','60'))
process=None
failure=None
hostlog=run/'emulator.log'
def read(path):return path.read_text(errors='replace') if path.exists() else ''
def interrupted(signum,frame):
    signal.signal(signal.SIGINT,signal.SIG_IGN);signal.signal(signal.SIGTERM,signal.SIG_IGN)
    raise KeyboardInterrupt
signal.signal(signal.SIGINT,interrupted);signal.signal(signal.SIGTERM,interrupted)
try:
    with hostlog.open('w') as output:
        process=subprocess.Popen(command,cwd=str(Path(emulator).parent),stdout=output,stderr=subprocess.STDOUT,start_new_session=True)
        while time.monotonic()<deadline:
            if process.poll() is not None:
                failure='browser/emulator exited before observation completed';break
            if re.search(r'access violation|panicked|KERN-EXEC|USER-EXEC',read(hostlog),re.I):
                failure='guest panic or access violation';break
            time.sleep(.25)
        shot=Path(os.environ.get('SCREENSHOT',str(run/'browser.png')))
        subprocess.run(['import','-window','root',str(shot)],check=True,timeout=15)
except KeyboardInterrupt:
    failure='run interrupted'
finally:
    # A second cancellation must not interrupt bounded process-group cleanup.
    signal.signal(signal.SIGINT,signal.SIG_IGN);signal.signal(signal.SIGTERM,signal.SIG_IGN)
    if process:
        if process.poll() is None:
            try:
                # EKA's VFS buffers host FILE writes. Closing the Qt window lets
                # it flush those files; SIGTERM can otherwise lose the last log.
                subprocess.run([sys.executable,os.environ['SYMBIAN_BROWSER_CLOSE_WINDOW'],str(process.pid)],check=True,timeout=5)
                process.wait(timeout=10)
            except (subprocess.SubprocessError,OSError) as error:
                failure=failure or 'emulator graceful shutdown failed; forced cleanup may lose buffered guest logs: '+str(error)
        else:
            failure=failure or 'emulator exited before requested shutdown'
        if process.poll() is not None and process.returncode:
            failure=failure or f'emulator exited with status {process.returncode}'
        try:os.killpg(process.pid,signal.SIGTERM)
        except ProcessLookupError:pass
        try:process.wait(timeout=3)
        except subprocess.TimeoutExpired:pass
        try:os.killpg(process.pid,signal.SIGKILL)
        except ProcessLookupError:pass
        process.wait()
    for name in ['log.txt','curl.txt']:
        src=c/'Data/WebKit'/name
        if src.exists():shutil.copy2(src,run/name)
content=read(run/'log.txt')
if expected and expected not in content:failure=failure or 'missing fresh expected browser marker: '+expected
if re.search(r'access violation|panicked|KERN-EXEC|USER-EXEC',read(hostlog),re.I):failure=failure or 'guest panic or access violation'
print('logs: '+str(run));print(content)
if failure:sys.exit(failure)
print('Browser observation completed; inspect screenshot and test outcomes: '+str(shot))
PY
