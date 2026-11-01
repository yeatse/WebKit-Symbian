from pathlib import Path
import importlib.util,os,subprocess,json,shutil,hashlib,fcntl
out=Path('/workspace/symbian-belle-work/p1-validation/options-calibration-c17');out.mkdir(exist_ok=False)
spec=importlib.util.spec_from_file_location('support','/workspace/symbian-p1-system-runtime-work/runtime_support.py');s=importlib.util.module_from_spec(spec);spec.loader.exec_module(s)
data=s.DATA;sha=s.sha256;report={'status':'CALIBRATION_ONLY','inputs':{}}
image=Path('/workspace/symbian-belle-work/candidate17/browser.exe');host=Path('/workspace/EKA2L1-validation/build/bin/eka2l1_qt')
assert sha(image)=='0d91d91ea1e016b7725097bac0768a9aa2fdf9710b765795769d2da07608a1c8';assert sha(host)=='277db2b3e44c69d4b860b2f76cf194178e820556671ad32f4891fcae2cb4c2ff';assert subprocess.run(['pgrep','-x','eka2l1_qt'],stdout=subprocess.DEVNULL).returncode!=0
locks=[]
for n in ['.jsc-run.lock','.browser-run.lock']:
 f=(data/n).open('a');fcntl.flock(f,fcntl.LOCK_EX|fcntl.LOCK_NB);locks.append(f)
saved=s.SavedFiles(data,'options-c17',report.setdefault('restoration',{}));cfg=data.parent/'config.yml';private=out/'config.private.yml';inode=cfg.stat().st_ino;before=sha(cfg)
try:
 saved.reserve(data/'drives/c/Data/WebKit',directory=True)
 for n in ['sys/bin/browser.exe','private/10003a3f/import/apps/browser_reg.rsc','resource/apps/browser_loc.rsc','resource/apps/browser_ui.rsc','resource/icu/icudt70l.dat','resource/fontconfig/fonts.conf']:saved.reserve(data/'drives/c'/n)
 cfg.rename(private);shutil.copy2(private,cfg)
 for p in [image,host,host.parent/'patch/avkonfep_general.dll',image.parent/'browser_ui.rsc']:report['inputs'][p.name]=sha(p)
 env=os.environ.copy();env.update(EKA2L1=str(host),ICU_DIR='/workspace/symbian-idna-work/icu',SYMBIAN_WEBKIT_WORK=str(out),WAIT='180',SCREENSHOT='null:',EKA2L1_BROWSER_LOCK_FD=str(locks[1].fileno()));env.pop('EKA2L1_EXPECT_PASS',None);env.pop('EKA2L1_COMPLETION_GRACE',None)
 with (out/'runner.log').open('w') as log:
  child=subprocess.Popen(['bash','/workspace/WebKit-Symbian-graphics/Tools/Symbian/eka2l1/run-browser.sh',str(image),'about:blank'],env=env,pass_fds=tuple(f.fileno() for f in locks),stdout=log,stderr=subprocess.STDOUT)
  (out/'runner-pid').write_text(str(child.pid));report['launcher_exit']=child.wait()
finally:
 assert subprocess.run(['pgrep','-x','eka2l1_qt'],stdout=subprocess.DEVNULL).returncode!=0,'live guest: preserve backups'
 if private.exists():cfg.unlink(missing_ok=True);private.rename(cfg)
 report['config_restored']=cfg.stat().st_ino==inode and sha(cfg)==before
 saved.restore();(out/'result.json').write_text(json.dumps(report,indent=2)+'\n')
