import json,re
SIZES=[1024,0,24]
HASHES=['785b0751fc2c53dc14a4ce3d800e69ef9ce1009eb327ccf458afe09c242c26c9','e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855','8a4a90e74c3d9c754d0f3ab6b391646f7de56516d70a5ad20c05e49030a481da']
def validate(raw,token,*unused):
 try:
  def require(v,m):
   if not v:raise ValueError(m)
  lines=raw.decode('utf-8').splitlines()if isinstance(raw,bytes)else raw.splitlines();require(len(lines)==13,'exact BEGIN/ABI/9 CASE/RESULT/exiting required')
  begin=json.loads(lines[0].removeprefix('BEGIN '));require(lines[0].startswith('BEGIN ')and begin=={'suite':'file-io-contract','version':1,'run':token},'fresh BEGIN differs')
  abi=json.loads(lines[1].removeprefix('ABI '));require(lines[1].startswith('ABI ')and abi=={'wchar':2,'wchar_align':2,'TText':2,'stat':96,'off_t':8,'st_size':8,'unicode_units':[23383,55357,56832]},'measured ABI differs')
  rows=[]
  for i,line in enumerate(lines[2:11]):
   require(line.startswith('CASE '),'CASE missing');r=json.loads(line[5:]);index=i//3;api=['native','narrow','wide'][i%3];require(r.get('file')==index and r.get('api')==api,'case identity/order differs')
   keys={'api','file','pass','metadata_rc','metadata_errno','open_rc','open_errno','read_rc','read_errno','close_rc','size','bytes','regular','exact','hash_ok','sha256'};require(set(r)==keys,'CASE shape differs')
   require(all(type(r[k])is int for k in ['file','metadata_rc','metadata_errno','open_rc','open_errno','read_rc','read_errno','close_rc','size','bytes']),'integer results required');require(all(type(r[k])is bool for k in ['pass','regular','exact','hash_ok']),'boolean results required')
   require(isinstance(r['sha256'],str)and(r['sha256']==''or re.fullmatch('[0-9a-f]{64}',r['sha256'])),'hash encoding differs')
   expected=all(r[k]==0 for k in ['metadata_rc','open_rc','read_rc','close_rc'])and r['regular']and r['size']==SIZES[index]and r['bytes']==SIZES[index]and r['exact']and r['hash_ok']and r['sha256']==HASHES[index];require(r['pass']==expected,'pass disagrees with actual results');rows.append(r)
  passed=sum(r['pass']for r in rows);failed=9-passed;result=json.loads(lines[11].removeprefix('RESULT '));require(lines[11].startswith('RESULT ')and result=={'suite':'file-io-contract','version':1,'run':token,'passed':passed,'failed':failed,'total':9},'RESULT differs')
  code=int(bool(failed));require(lines[12]=='file-io-contract exiting '+str(code),'explicit exit disagrees')
  return{'status':'LOG_CONTRACT_PASS_PENDING_TARGET_EXIT'if not failed else'API_ASSERTIONS_FAIL_COMPLETE_PENDING_TARGET_EXIT','run':token,'complete':True,'cases':9,'passed':passed,'failed':failed,'worker_exit':code,'rows':rows,'abi':abi}
 except(ValueError,TypeError,KeyError,UnicodeError)as e:return{'status':'FAIL','run':token,'complete':False,'failures':[str(e)]}
