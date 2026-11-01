from pathlib import Path
import os,subprocess,shutil,json,hashlib,sys
out=Path(sys.argv[1]);out.mkdir(mode=0o700,exist_ok=False)
base=Path('/workspace/eka-validation/xdg/data/EKA2L1');cfg=base/'config.yml';profile=base/'bindings/webkit-editor.yml';source=Path('/workspace/symbian-eka-keyboard-work/webkit-editor.yml')
assert subprocess.run(['pgrep','-x','eka2l1_qt'],stdout=subprocess.DEVNULL).returncode!=0
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
record={'config_before_sha256':sha(cfg),'config_before_inode':cfg.stat().st_ino,'profile_preexisted':profile.exists(),'profile_sha256':sha(source),'command':sys.argv[2:]}
saved=out/'config.private.yml';cfg.rename(saved);shutil.copy2(saved,cfg)
profile_saved=out/'profile.private.yml'
if profile.exists():profile.rename(profile_saved)
shutil.copy2(source,profile)
try:
    env=os.environ.copy();env['EKA2L1_KEYBIND_PROFILE']='webkit-editor'
    record['child_exit']=subprocess.run(sys.argv[2:],env=env).returncode
finally:
    active=subprocess.run(['pgrep','-x','eka2l1_qt'],stdout=subprocess.DEVNULL).returncode==0
    record['no_guest_running_at_restore']=not active
    if active:raise RuntimeError('Guest still running: preserving private backup; restore only after normal close')
    cfg.unlink(missing_ok=True);saved.rename(cfg)
    profile.unlink(missing_ok=True)
    if profile_saved.exists():profile_saved.rename(profile)
    record['config_restored']=sha(cfg)==record['config_before_sha256'] and cfg.stat().st_ino==record['config_before_inode']
    record['profile_presence_restored']=profile.exists()==record['profile_preexisted']
    (out/'profile-guard.json').write_text(json.dumps(record,indent=2)+'\n')
assert record['config_restored'] and record['profile_presence_restored']
raise SystemExit(record['child_exit'])
