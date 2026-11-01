from pathlib import Path
import subprocess,json
root=Path(__file__).parent
src=(root/'overlay/src/patch/avkonfep/src/HostBridgedImeFEP.cpp').read_text()
def method(name,return_type='void'):
 start=src.index(return_type+' '+name)
 body=src.index('{',start);depth=1;i=body+1
 while depth:
  if src[i]=='{':depth+=1
  elif src[i]=='}':depth-=1
  i+=1
 return src[start:i]+'\n'
text=(root/'state-header.cpp').read_text()+method('CHostDialogIme::Open','TInt')
for m in ('CHostDialogIme::RunL','CHostBridgedImeFEP::OpenDialogInputL','CHostBridgedImeFEP::CommitChangedTextL','CHostBridgedImeFEP::CancelChangedTextL','CHostBridgedImeFEP::CancelDialogL'):text+=method(m)
text+=(root/'state-main.cpp').read_text();(root/'state-test.cpp').write_text(text)
records=[]
for name,code,expected in [('state-test',text,0),('bad-secret-comparison',text.replace('SupportsSecretText() != EFalse','SupportsSecretText() == ETrue'),1)]:
 (root/(name+'.cpp')).write_text(code)
 cmd=['g++','-std=c++20','-fsanitize=address,undefined','-fno-omit-frame-pointer','-O1','-g',str(root/(name+'.cpp')),'-o',str(root/name)]
 r=subprocess.run(cmd,capture_output=True,text=True);(root/(name+'-build.log')).write_text(r.stdout+r.stderr)
 if r.returncode:print(r.stderr);raise SystemExit(r.returncode)
 r=subprocess.run([str(root/name)],capture_output=True,text=True);(root/(name+'.log')).write_text(r.stdout+r.stderr)
 records.append({'name':name,'expected_exit':expected,'actual_exit':r.returncode,'output':r.stdout+r.stderr})
 assert r.returncode==expected,records[-1]
(root/'state-results.json').write_text(json.dumps({'status':'PASS','actual_extracted_method_cases':40,'negative_control_rejected':True,'records':records},indent=2)+'\n')
print('40 actual-method cases PASS; incorrect ETrue comparison rejected')
