from pathlib import Path
import os,json,hashlib,subprocess,sys,uuid
base=Path('/workspace/symbian-fep-ui-runtime-work');out=base/'suite-c15-01';out.mkdir(exist_ok=False)
image=Path('/workspace/symbian-belle-work/candidate15/browser.exe');host=Path('/workspace/EKA2L1-validation/build/bin/eka2l1_qt');fep=host.parent/'patch/avkonfep_general.dll';sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
assert sha(image)=='473679d5c12d7dfeb057955ebd2ec0291cdd5db4654b6a3a2c2dba7c31fb995e';assert sha(host)=='277db2b3e44c69d4b860b2f76cf194178e820556671ad32f4891fcae2cb4c2ff';assert sha(fep)=='aad1a4c6a4a52c70d3ac430d45dce671a8ff664071c921146bc65f754a612f6b'
result={'status':'RUNNING','browser_sha256':sha(image),'host_sha256':sha(host),'fep_sha256':sha(fep),'fixture_sha256':sha(base/'index.html'),'driver_sha256':sha(base/'drive.py'),'cases':[],'native_ui_review_pending':True}
for case in ['cancel','submit','programmatic','readonly']:
 token=uuid.uuid4().hex;directory=out/case;directory.mkdir();env=os.environ.copy();env.update(EKA2L1=str(host),ICU_DIR='/workspace/symbian-idna-work/icu',SYMBIAN_WEBKIT_WORK=str(directory),WAIT='90',EKA2L1_EXPECT_PASS='FEP-UI '+token+' '+case+' PAGE-PASS',EKA2L1_COMPLETION_GRACE='5');row={'case':case,'run':token}
 with (directory/'runner.log').open('w') as log:
  runner=subprocess.Popen(['bash','/workspace/WebKit-Symbian-graphics/Tools/Symbian/eka2l1/run-browser.sh',str(image),'http://localhost:8812/?run='+token+'&case='+case],env=env,stdout=log,stderr=subprocess.STDOUT)
  with (directory/'input-driver.log').open('w') as output:
   action=subprocess.run([sys.executable,str(base/'drive.py'),'--run',token,'--case',case,'--output',str(directory/'input')],env=env,stdout=output,stderr=subprocess.STDOUT,timeout=45)
  row['input_exit']=action.returncode;row['launcher_exit']=runner.wait(timeout=100)
 run=next((directory/'runs').glob('browser-*'));text=(run/'log.txt').read_text();assert 'FEP-UI '+token+' '+case+' FAIL ' not in text
 row['status']='PASS_PENDING_OPERATOR_REVIEW' if row['input_exit']==row['launcher_exit']==0 else 'FAIL';result['cases'].append(row);(out/'result.json').write_text(json.dumps(result,indent=2)+'\n');print(case,row['status'],flush=True)
 if row['status']=='FAIL':raise SystemExit(1)
result['status']='INPUT_PAGE_AND_NORMAL_CLOSE_PASS_PENDING_OPERATOR_REVIEW';(out/'result.json').write_text(json.dumps(result,indent=2)+'\n')
