from pathlib import Path
import importlib.util,os,subprocess,json,shutil,fcntl,sys,argparse
p=argparse.ArgumentParser();p.add_argument('--case',choices=['cancel','submit','empty'],required=True);p.add_argument('--run',required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
assert a.run.replace('-','').replace('_','').isalnum() and len(a.run)<=64
out=a.output;out.mkdir(exist_ok=False,mode=0o700)
spec=importlib.util.spec_from_file_location('support','/workspace/symbian-p1-system-runtime-work/runtime_support.py');s=importlib.util.module_from_spec(spec);spec.loader.exec_module(s)
data=s.DATA;sha=s.sha256;report={'status':'RUNNING','run':a.run,'case':a.case,'inputs':{}}
image=Path('/workspace/symbian-belle-work/candidate19/browser.exe');host=Path('/workspace/EKA2L1-validation/build/bin/eka2l1_qt');fep=host.parent/'patch/avkonfep_general.dll'
assert sha(image)=='6b6e8e0a430ff9c1ce0be935161d4b7e61804444e2947cb0218e74cb2e5420ae';assert sha(host)=='07af513045bf26e200f7d711c682e219ee4c7cecc9e30f2fa3b08732fc1012e3';assert sha(fep)==sha(data.parent/'patch/avkonfep_general.dll')=='fd6092f2ad61d39ccb47475229c367f5494fe68a8bd2bd8d0e04dc1bafcdc7ae';assert subprocess.run(['pgrep','-x','eka2l1_qt'],stdout=subprocess.DEVNULL).returncode!=0
locks=[]
for n in ['.jsc-run.lock','.browser-run.lock']:
 f=(data/n).open('a');fcntl.flock(f,fcntl.LOCK_EX|fcntl.LOCK_NB);locks.append(f)
saved=s.SavedFiles(data,a.run,report.setdefault('restoration',{}));cfg=data.parent/'config.yml';private=out/'config.private.yml';inode=cfg.stat().st_ino;before=sha(cfg)
try:
 saved.reserve(data/'drives/c/Data/WebKit',directory=True)
 for n in ['sys/bin/browser.exe','private/10003a3f/import/apps/browser_reg.rsc','resource/apps/browser_loc.rsc','resource/apps/browser_ui.rsc','resource/icu/icudt70l.dat','resource/fontconfig/fonts.conf']:saved.reserve(data/'drives/c'/n)
 cfg.rename(private);shutil.copy2(private,cfg)
 for q in [image,host,fep,image.parent/'browser_ui.rsc',Path(__file__).parent/'index.html',Path(__file__).parent/'drive.py']:report['inputs'][q.name]=sha(q)
 shutil.copy2(Path(__file__).parent/'calibration.template.json',out/'calibration.json')
 env=os.environ.copy();env.update(EKA2L1=str(host),ICU_DIR='/workspace/symbian-idna-work/icu',SYMBIAN_WEBKIT_WORK=str(out),WAIT='300',SCREENSHOT='null:',EKA2L1_BROWSER_LOCK_FD=str(locks[1].fileno()),EKA2L1_EXPECT_PASS='SECRET-FEP '+a.run+' '+a.case+' PAGE-PASS',EKA2L1_COMPLETION_GRACE='5')
 with (out/'runner.log').open('w') as log:
  child=subprocess.Popen(['bash','/workspace/WebKit-Symbian-graphics/Tools/Symbian/eka2l1/run-browser.sh',str(image),'http://localhost:8813/?run='+a.run+'&case='+a.case],env=env,pass_fds=tuple(f.fileno() for f in locks),stdout=log,stderr=subprocess.STDOUT)
  (out/'runner-pid').write_text(str(child.pid));report['launcher_exit']=child.wait()
finally:
 assert subprocess.run(['pgrep','-x','eka2l1_qt'],stdout=subprocess.DEVNULL).returncode!=0,'live guest: preserve backups'
 if private.exists():cfg.unlink(missing_ok=True);private.rename(cfg)
 report['config_restored']=cfg.stat().st_ino==inode and sha(cfg)==before
 saved.restore();report['status']='RUNNER_AND_RESTORATION_PASS_PENDING_DRIVER_MASK_REVIEW' if report.get('launcher_exit')==0 and report['config_restored'] else 'FAIL';(out/'result.json').write_text(json.dumps(report,indent=2)+'\n')
print(report['status']);raise SystemExit(report.get('launcher_exit',1))
