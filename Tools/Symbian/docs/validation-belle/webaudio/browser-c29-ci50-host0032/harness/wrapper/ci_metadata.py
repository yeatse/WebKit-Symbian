"""CI50 package/source provenance only. Import is inert; no network or runtime."""
import hashlib,json,os,re,subprocess,zipfile
from pathlib import Path

CANDIDATE=Path('/workspace/symbian-belle-work/candidate29')
REPO=Path('/workspace/WebKit-Symbian-graphics')
SOURCE_COMMIT='886d227f1780298cc17cf99695a3c78c0b7d9ff6'
RUN_ID=41299027063
JOB_ID=124825909684
RUN_NUMBER=50
VERSION='0.3.50'
REPOSITORY='yeatse/WebKit-Symbian'
BRANCH='2.52.6-symbian'
OPTIONS='Source/cmake/OptionsSymbian.cmake'
MINIMUM_REQUIRED={
 'Source/WebCore/platform/audio/symbian/'+name for name in ['AudioDestinationSymbian.cpp','AudioDestinationSymbianNative.cpp','AudioFileReaderSymbian.cpp','FFTFrameSymbian.cpp']
}|{
 'Source/WebCore/Modules/webaudio/'+name for name in ['AudioContext.cpp','AudioNodeOutput.cpp','MediaElementAudioSourceNode.cpp','BaseAudioContext.idl','AudioWorklet.idl','AudioWorkletNode.idl']
}|{'Source/WebCore/page/Settings.yaml','Source/WebKitLegacy/symbian/WebViewImpl.cpp'}
REPORT_ROLES={'run','artifacts','verification','archive','package','sis_payload','auth_resource','e32_imports'}
PACKAGES={
 'symbian-webkit-icu.sis':(0xe1a5c013,['c:/resource/icu/icudt70l.dat']),
 'jsc.sis':(0xe1a5c011,['!:/sys/bin/jsc.exe','c:/data/es2020.js','c:/data/intl.js','c:/data/basic.js','c:/data/resizable-arraybuffer.js']),
 'browser.sis':(0xe1a5c012,['!:/sys/bin/browser.exe','!:/private/10003a3f/import/apps/browser_reg.rsc','!:/resource/apps/browser_loc.rsc','!:/resource/apps/browser_ui.rsc','c:/data/webkit/test.html','c:/resource/webkit/cacert.pem','c:/resource/fontconfig/fonts.conf']),
}
ASSETS={
 'browser':('browser.sis','!:/sys/bin/browser.exe','browser.exe'),
 'registration':('browser.sis','!:/private/10003a3f/import/apps/browser_reg.rsc','browser_reg.rsc'),
 'localization':('browser.sis','!:/resource/apps/browser_loc.rsc','browser_loc.rsc'),
 'ui':('browser.sis','!:/resource/apps/browser_ui.rsc','browser_ui.rsc'),
 'icu':('symbian-webkit-icu.sis','c:/resource/icu/icudt70l.dat','icudt70l.dat'),
 'fontconfig':('browser.sis','c:/resource/fontconfig/fonts.conf','fonts.conf'),
}

def require(ok,message):
 if not ok:raise RuntimeError(message)
def digest(path):
 h=hashlib.sha256()
 with Path(path).open('rb')as stream:
  for block in iter(lambda:stream.read(1024*1024),b''):h.update(block)
 return h.hexdigest()
def sha_bytes(data):return hashlib.sha256(data).hexdigest()
def valid_sha(value):return type(value)is str and re.fullmatch('[0-9a-f]{64}',value)is not None
def integer(value):return type(value)is int and value>0
def file(path):
 require(type(path)in(str,Path)or isinstance(path,Path),'file path required');p=Path(path)
 require(not p.is_relative_to(Path('/workspace/eka-validation/xdg')),'private guest/config paths cannot be CI evidence')
 require(p.is_absolute()and p==p.resolve()and p.is_file()and not p.is_symlink(),'canonical regular file required: '+str(p))
 for parent in p.parents:require(not parent.is_symlink(),'file symlink ancestor refused')
 return p
def strict_json(data):
 def pairs(rows):
  value={}
  for key,item in rows:require(key not in value,'duplicate JSON field');value[key]=item
  return value
 def constant(_):raise RuntimeError('nonfinite JSON value')
 try:return json.loads(data,object_pairs_hook=pairs,parse_constant=constant)
 except(ValueError,UnicodeError)as error:raise RuntimeError('invalid JSON')from error
def document(path):
 p=file(path);require(p.stat().st_size<=4*1024*1024,'bounded report required');value=strict_json(p.read_bytes());require(type(value)is dict,'report object required');return value
def register(pins,key,path,expected,size=None):
 require(valid_sha(expected),'expected SHA-256 required: '+key);p=file(path)
 if size is not None:require(integer(size)and p.stat().st_size==size,'actual size differs: '+key)
 require(digest(p)==expected,'actual SHA-256 differs: '+key);pins[key]=(p,expected);return p
def reference(value,pins,key):
 require(type(value)is dict and set(value)=={'path','sha256'},'report reference differs: '+key)
 p=register(pins,key,value['path'],value['sha256']);return p,document(p)
def target(value):
 require(type(value)is str,'target required');return re.sub('/+','/',value.replace('\\','/')).lower()
def uid(value):
 try:return int(value,0)if type(value)is str else value
 except ValueError:return None
def _git(*args):
 result=subprocess.run(['git',*args],cwd=REPO,env={**os.environ,'GIT_OPTIONAL_LOCKS':'0'},stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=15)
 require(result.returncode==0,'required CI git object unavailable');return result.stdout

def source_snapshots(commit,required,materialize=False):
 require(commit==SOURCE_COMMIT and type(commit)is str and re.fullmatch('[0-9a-f]{40}',commit),'exact CI50 source commit required')
 require(set(required)==MINIMUM_REQUIRED,'all twelve required CI source paths must be retained')
 require(_git('cat-file','-t',commit).strip()==b'commit','source identity is not a commit')
 data={name:_git('show',commit+':'+name)for name in sorted(set(required)|{OPTIONS})}
 require(re.findall(rb'^WEBKIT_OPTION_DEFAULT_PORT_VALUE\(ENABLE_WEB_AUDIO PRIVATE (ON|OFF)\)\s*$',data[OPTIONS],re.M)==[b'ON'],'CI source default WebAudio is not ON')
 view=data['Source/WebKitLegacy/symbian/WebViewImpl.cpp'].decode();settings=data['Source/WebCore/page/Settings.yaml'].decode()
 require(all(token in view for token in ['settings.setWebAudioEnabled(true);','settings.setAudioWorkletEnabled(false);','settings.setRequiresUserGestureForAudioPlayback(true);']),'CI WebView Audio policy missing')
 require(re.search(r'AudioWorkletEnabled:\s+type: bool\s+condition: ENABLE\(WEB_AUDIO\)\s+defaultValue:\s+WebCore:\s+OS\(SYMBIAN\): false',settings),'CI Symbian worklet default missing')
 rows=[]
 for name,value in data.items():
  p=CANDIDATE/'source'/name
  for parent in [p,*p.parents]:require(not parent.is_symlink(),'source output symlink refused')
  if p.exists():require(file(p).read_bytes()==value,'materialized source differs from actual CI git object')
  elif not materialize:raise RuntimeError('materialized CI source missing: '+name)
  blob=_git('rev-parse',commit+':'+name).decode().strip();require(re.fullmatch('[0-9a-f]{40}',blob),'source blob identity missing')
  rows.append({'relative':name,'path':str(p),'bytes':len(value),'sha256':sha_bytes(value),'git_blob':blob,'source_commit':commit})
 if materialize:
  for row in rows:
   p=Path(row['path']);p.parent.mkdir(parents=True,exist_ok=True)
   if not p.exists():p.write_bytes(data[row['relative']])
 return rows

def _build_metadata(contract_path,contract_sha,commit,required,materialize_sources=False):
 pins={};p=register(pins,'audit_contract',contract_path,contract_sha);contract=document(p)
 require(set(contract)=={'schema','source_commit','run_id','run_number','package_version','artifact_zip','reports'}and contract['schema']=='classic-webaudio-ci-audit-v1','audit contract fields/schema differ')
 require(contract['source_commit']==commit==SOURCE_COMMIT and contract['run_id']==RUN_ID and type(contract['run_id'])is int and contract['run_number']==RUN_NUMBER and type(contract['run_number'])is int and contract['package_version']==VERSION,'CI50 identity/version differs')
 require(type(contract['reports'])is dict and set(contract['reports'])==REPORT_ROLES,'all eight actual reports required')
 reports={};refs={}
 for role in sorted(REPORT_ROLES):
  rp,value=reference(contract['reports'][role],pins,'report_'+role);reports[role]=value;refs[role]={'path':str(rp),'sha256':contract['reports'][role]['sha256']}
 run=reports['run'];archive=reports['archive'];verification=reports['verification']
 require(run.get('id')==RUN_ID and run.get('run_number')==RUN_NUMBER and run.get('head_sha')==commit and run.get('head_branch')==BRANCH and run.get('event')=='push'and run.get('status')=='completed'and run.get('conclusion')=='success','actual CI run not complete/success/exact source')
 if'repository'in run:require(type(run['repository'])is dict and run['repository'].get('full_name')==REPOSITORY,'actual CI repository differs')
 require(verification.get('status')=='PASS'and verification.get('repo')==REPOSITORY and verification.get('run_id')==RUN_ID and verification.get('job_id')==JOB_ID and verification.get('head_sha')==commit and verification.get('package_version')==VERSION and verification.get('payload_count')==13 and verification.get('workflow_status')=='success','actual whole-package audit differs')
 for key in ['all_three_sis_sha256_match_artifact_manifest','all_package_headers_sections_crc_valid','all_decompressed_payload_sha1_valid','all_payload_sizes_valid']:require(verification.get(key)is True,'whole-package verification gate missing: '+key)
 require(verification.get('release_default_WEB_AUDIO')=='ON','whole audit must identify the CI default WebAudio ON source')
 az=contract['artifact_zip'];require(type(az)is dict and set(az)=={'path','bytes','sha256','artifact_id'}and integer(az['artifact_id']),'artifact ZIP identity required');zip_path=register(pins,'artifact_zip',az['path'],az['sha256'],az['bytes'])
 raw_artifacts=reports['artifacts'].get('artifacts');require(type(raw_artifacts)is list,'actual artifact list required');selected=[item for item in raw_artifacts if type(item)is dict and item.get('id')==az['artifact_id']];require(len(selected)==1,'unique actual artifact required');artifact=selected[0]
 require(artifact.get('name')=='sis'and artifact.get('expired')is False and artifact.get('size_in_bytes')==az['bytes']and artifact.get('digest')=='sha256:'+az['sha256'],'actual artifact digest differs')
 workflow=artifact.get('workflow_run',{});require(workflow.get('id')==RUN_ID and workflow.get('head_sha')==commit and workflow.get('head_branch')==BRANCH,'artifact workflow source differs')
 require(archive.get('status')=='PASS'and archive.get('artifact_id')==az['artifact_id']and archive.get('bytes')==az['bytes']and archive.get('sha256')==az['sha256']and archive.get('matches_github_digest')is True and archive.get('zip_member_crc_valid')is True and archive.get('workflow_head_sha')==commit and archive.get('workflow_conclusion')=='success'and archive.get('job_id')==JOB_ID and archive.get('job_conclusion')=='success','actual ZIP audit differs')
 require(set(archive.get('members',[]))=={'SHA256SUMS',*PACKAGES}and len(archive.get('members',[]))==4,'exact ZIP audit members required')
 summary=verification.get('sis_artifact',{});require(summary.get('id')==az['artifact_id']and summary.get('bytes')==az['bytes']and summary.get('sha256')==az['sha256']and summary.get('matches_github_digest')is True,'whole audit artifact identity differs')
 package=reports['package'];payload=reports['sis_payload'];require(package.get('status')==payload.get('status')=='PASS'and package.get('run_id')==payload.get('run_id')==RUN_ID and package.get('head_sha')==payload.get('head_sha')==commit and payload.get('sis_version')==VERSION and payload.get('payload_count')==13,'package/payload audit identity differs')
 packs=package.get('packages');spacks=payload.get('packages');require(type(packs)is list and type(spacks)is list and len(packs)==len(spacks)==3,'three package records required')
 pmap={item.get('package'):item for item in packs if type(item)is dict};smap={item.get('name'):item for item in spacks if type(item)is dict};require(set(pmap)==set(smap)==set(PACKAGES),'exact package set required')
 verified_payloads={};package_rows=[]
 with zipfile.ZipFile(zip_path)as z:
  infos=z.infolist();require(len(infos)==4 and {i.filename for i in infos}=={'SHA256SUMS',*PACKAGES},'exact four actual ZIP members required')
  require(all(not i.is_dir()and i.file_size<=128*1024*1024 for i in infos),'bounded ZIP members required');require(z.getinfo('SHA256SUMS').file_size<=4096,'bounded artifact manifest required')
  sums={}
  for line in z.read('SHA256SUMS').decode('ascii').splitlines():
   fields=line.split();require(len(fields)==2 and valid_sha(fields[0]),'invalid artifact SHA256SUMS');name=fields[1].lstrip('*');require(name not in sums,'duplicate artifact manifest member');sums[name]=fields[0]
  require(set(sums)==set(PACKAGES),'artifact manifest package set differs')
  for name,(expected_uid,targets)in PACKAGES.items():
   row=pmap[name];other=smap[name];require(uid(row.get('uid'))==uid(other.get('uid3'))==expected_uid and row.get('version')==other.get('version')==VERSION and row.get('header_and_section_crc')=='PASS'and row.get('all_payload_hashes_valid')is True and other.get('header_and_section_crc_valid')is True and type(other.get('signature_chains'))is int and other['signature_chains']==0,'package UID/version/CRC/hash audit differs: '+name)
   pack_path=register(pins,'package_'+name,row['path'],row['sha256'],row['bytes']);require(row['sha256']==sums[name]and z.getinfo(name).file_size==row['bytes'],'ZIP package manifest differs')
   h=hashlib.sha256()
   with z.open(name)as member:
    for block in iter(lambda:member.read(1024*1024),b''):h.update(block)
   require(h.hexdigest()==row['sha256'],'actual ZIP/SIS bytes differ')
   files=row.get('files');pf=other.get('payloads');require(type(files)is list and type(pf)is list and len(files)==len(pf)==len(targets),'package payload count differs')
   fm={target(item['target']):item for item in files};pm={target(item['target']):item for item in pf};require(set(fm)==set(pm)==set(targets),'exact payload target set required')
   for index,t in enumerate(targets):
    item=fm[t];pi=pm[t];require(item.get('file_hash_valid')is True and pi.get('file_hash_valid')is True and all(item.get(k)==pi.get(k)for k in ['bytes','sha256','extracted_path']),'payload audit records differ')
    register(pins,'payload_'+name+'_'+str(index),item['extracted_path'],item['sha256'],item['bytes']);verified_payloads[(name,t)]=item
    if t.endswith('.exe'):
     expected_exe_uid=0xe1a5c002 if name=='browser.sis'else 0xe1a5c001;expected_caps=0x2000 if name=='browser.sis'else 0
     require(all(uid(record.get('e32_uid3'))==expected_exe_uid and uid(record.get('capabilities'))==expected_caps and record.get('hardware_float_type')==1 and type(record.get('hardware_float_type'))is int and((uid(record.get('e32_flags'))or 0)>>20)&15==1 for record in [item,pi]),'audited E32 UID/capability/VFP identity differs')
   package_rows.append({'name':name,'path':str(pack_path),'bytes':row['bytes'],'sha256':row['sha256'],'uid':hex(expected_uid),'version':VERSION})
  require(z.testzip()is None,'actual ZIP member CRC differs')
 require(len(verified_payloads)==13,'all thirteen payloads required')
 top_packs=verification.get('packages');require(type(top_packs)is list and len(top_packs)==3,'whole audit package summaries missing')
 top={item.get('name'):item for item in top_packs};require(set(top)==set(PACKAGES)and all(top[row['name']].get('bytes')==row['bytes']and top[row['name']].get('sha256')==row['sha256']for row in package_rows),'whole audit package identity differs')
 assets={}
 for role,(pack,t,basename)in ASSETS.items():
  row=verified_payloads[(pack,t)];p=register(pins,role,CANDIDATE/basename,row['sha256'],row['bytes']);assets[role]={'path':str(p),'bytes':row['bytes'],'sha256':row['sha256'],'package':pack,'target':t}
 require(assets['ui']['bytes']==242,'CI Auth255 UI must be the actual 242-byte resource')
 auth=reports['auth_resource'];require(auth.get('status')=='PASS','actual Auth resource audit missing');ar=auth.get('resource',{});require(ar.get('bytes')==assets['ui']['bytes']and ar.get('sha256')==assets['ui']['sha256'],'Auth report UI identity differs');register(pins,'auth_resource_payload',ar['path'],ar['sha256'],ar['bytes'])
 _,auth_source=reference(auth.get('source_contract_report'),pins,'auth_source_report');checks=auth_source.get('checks');status=auth_source.get('status');require(type(status)is str and(status=='PASS'or status.startswith('PASS_'))and auth_source.get('source_commit')==commit and type(checks)is dict and checks and all(item is True for item in checks.values())and checks.get('password_API_uses_official_SDK_enum')is True and checks.get('password_resource_maximum_255')is True,'actual CI Auth255 source contract differs')
 require(auth_source.get('sdk_reference',{}).get('value')==255 and auth_source.get('expected_runtime_UI_resource',{}).get('bytes')==242 and auth_source.get('expected_runtime_UI_resource',{}).get('sha256')==assets['ui']['sha256'],'Auth255 SDK/resource binding differs')
 require(verification.get('auth_password_source_API_and_RSS_limit')==255 and verification.get('auth_resource',{}).get('bytes')==242 and verification.get('auth_resource',{}).get('sha256')==assets['ui']['sha256'],'whole audit Auth255 record differs')
 e32=reports['e32_imports'];require(e32.get('schema')=='ci-e32-import-audit-v1'and e32.get('status')=='PASS'and e32.get('source_commit')==commit and e32.get('run_id')==RUN_ID and e32.get('artifact_id')==az['artifact_id']and e32.get('method')=='parsed-e32-import-table','actual parsed E32 import audit required');image=e32.get('image',{});require(image.get('bytes')==assets['browser']['bytes']and image.get('sha256')==assets['browser']['sha256'],'E32 audit is for another browser');register(pins,'e32_audited_image',image['path'],image['sha256'],image['bytes']);dlls=e32.get('canonical_imported_dlls');require(type(dlls)is list and len(dlls)==len(set(dlls))and all(type(name)is str for name in dlls)and'mediaclientaudiostream.dll'in dlls,'actual ostream import missing')
 sources=source_snapshots(commit,required,materialize_sources)
 for index,row in enumerate(sources):register(pins,'source_'+str(index),row['path'],row['sha256'],row['bytes'])
 metadata={'schema':'classic-webaudio-c29-ci-v1','candidate_id':'C29','audit_contract':{'path':str(file(contract_path)),'sha256':contract_sha},'ci':{'repository':REPOSITORY,'source_commit':commit,'run_id':RUN_ID,'run_number':RUN_NUMBER,'job_id':JOB_ID,'package_version':VERSION,'artifact_id':az['artifact_id']},'artifact_zip':dict(az),'reports':refs,'packages':package_rows,'assets':assets,'source_files':sources,'release_default_web_audio':'ON','runtime_audio_worklet_enabled':False,'claims':{'scope':'Actual CI browser payload provenance; controlled runtime acceptance is separate','sis_installer_tested':False,'physical_phone_tested':False,'runtime_verified_by_metadata':False}}
 return metadata,pins

def build_metadata(contract_path,contract_sha,commit,required,materialize_sources=False):
 try:return _build_metadata(contract_path,contract_sha,commit,required,materialize_sources)
 except RuntimeError:raise
 except(OSError,ValueError,TypeError,KeyError,OverflowError,zipfile.BadZipFile,subprocess.SubprocessError)as error:raise RuntimeError('missing or invalid actual CI package audit material')from error

def verify_metadata(path,sha,commit,required):
 pins={};p=register(pins,'candidate_metadata',path,sha);actual=document(p);require(actual.get('schema')=='classic-webaudio-c29-ci-v1'and actual.get('candidate_id')=='C29','CI metadata schema differs');ref=actual.get('audit_contract');require(type(ref)is dict and set(ref)=={'path','sha256'},'audit contract binding required');expected,inputs=build_metadata(ref['path'],ref['sha256'],commit,required,False);require(actual==expected,'metadata differs from actual reports/materialized CI sources');inputs.update(pins);return actual,inputs
