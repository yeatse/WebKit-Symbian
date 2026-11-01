#!/usr/bin/env python3
"""Verify archived public proof without accessing original workspace paths or UI."""
from pathlib import Path
import gzip,hashlib,json,re
ROOT=Path(__file__).resolve().parent
sha=lambda b:hashlib.sha256(b).hexdigest()
def load(path):return json.loads(path.read_text())
def require(ok,why):
 if not ok:raise AssertionError(why)
def verify():
 manifest=load(ROOT/'archive-sources.json');archived=set()
 for item in manifest['files']:
  p=ROOT/item['destination'];require(p.is_file() and not p.is_symlink(),'missing/unsafe '+str(p));raw=p.read_bytes()
  require(sha(raw)==item['archived_sha256'] and len(raw)==item['archived_bytes'],'archive hash/length '+item['destination'])
  if item['gzip']:raw=gzip.decompress(raw)
  require(sha(raw)==item['source_sha256'] and len(raw)==item['source_bytes'],'original hash/length '+item['destination']);archived.add(item['destination'])
 pins=load(ROOT/'fixture/pins.c19-host0029.json');expected_pins={'browser':'6b6e8e0a430ff9c1ce0be935161d4b7e61804444e2947cb0218e74cb2e5420ae','host':'07af513045bf26e200f7d711c682e219ee4c7cecc9e30f2fa3b08732fc1012e3','fep':'fd6092f2ad61d39ccb47475229c367f5494fe68a8bd2bd8d0e04dc1bafcdc7ae'}
 require({k:v['sha256'] for k,v in pins.items()}==expected_pins,'C19/host0029/FEP pins')
 require(sha((ROOT/'harness/native-interact.py').read_bytes())=='3b31229833fc4544cdd17b1a6357e766bb1de40c165e2efa4f7452e41947b2f0','native helper identity')
 summary={'status':'PASS','scope':'public fixed-string ordinary password FEP on C19 browser/host0029; root sampled-mask review plus exact UI result, normal exit and restoration','cases':[]}
 total_immediate=total_reviewed=total_images=0
 for case,expected_length,expected_selection,immediate_count,review_count in [('cancel',17,[1,4],17,21),('submit',18,[18,18],18,22),('empty',0,[0,0],0,4)]:
  p=ROOT/'cases'/case;accept=load(p/'acceptance.json');result=load(p/'result.json');opened=load(p/'input/open-result.json');interaction=load(p/'input/interaction.json');session=load(p/'input/session.json')
  token='secret-c19-h29-'+case+'-01'
  require(accept['status']=='PASS' and accept['case']==case and accept['driver_exit']==accept['runner_exit']==0 and accept['original_result_retained'] is True,'acceptance '+case)
  require(result['status']=='RUNNER_AND_RESTORATION_PASS_PENDING_DRIVER_MASK_REVIEW','retain raw runner pending '+case)
  require(interaction['status']=='NATIVE_INPUT_AND_PAGE_PASS_PENDING_RUNNER_AND_MASK_REVIEW','retain raw driver pending '+case)
  require(opened['status']=='OPENED_PENDING_ACTUAL_MASK_REVIEW_AND_CALIBRATION','retain raw open pending '+case)
  require(result['run']==token and result['case']==case and result['launcher_exit']==0 and result['config_restored'] is True,'normal exit/config '+case)
  restoration=result['restoration'];require(restoration['restored'] is True and len(restoration['files'])==7,'restoration count '+case)
  for item in restoration['files']:require(item['restored'] is True and item['before']==item['after'],'restoration identity '+case)
  for item in [opened,interaction,session]:
   require(item['run']==token and item['case']==case and item['pins']==pins,'run/pins '+case)
   for name,identity in item['identities'].items():require(sha((ROOT/'fixture'/name).read_bytes())==identity,'fixture/driver/server '+case)
   for image in item['screenshots']:require(sha((p/'input'/image['path']).read_bytes())==image['sha256'],'driver image '+case)
  require(opened['pid']==session['pid']==interaction['pid'] and opened['fep_geometry']==session['fep_geometry']==interaction['fep_geometry'],'continuous session '+case)
  require(result['inputs']['browser.exe']==expected_pins['browser'] and result['inputs']['eka2l1_qt']==expected_pins['host'] and result['inputs']['avkonfep_general.dll']==expected_pins['fep'],'wrapper binary identities '+case)
  require(result['inputs']['browser_ui.rsc']=='251cfbede521cc5d0a3e36400cf108b12b6bb8eb7956e33a6781d8ef4bf5bc3d','C19 UI resource '+case)
  for name in ['index.html','drive.py']:require(result['inputs'][name]==sha((ROOT/'fixture'/name).read_bytes()),'wrapper fixture/driver '+case)
  logs=list((p/'runs').glob('browser-*/log.txt'));require(len(logs)==1,'one raw browser log '+case)
  marker='SECRET-FEP '+token+' '+case+' ';events=[]
  for line in logs[0].read_text().splitlines():
   if marker in line:
    kind,data=line.split(marker,1)[1].split(' ',1);events.append((kind,json.loads(data)))
  require([kind for kind,_ in events]==['READY','ARMED','FIELD-TAP','PAGE-PASS'],'exact raw event sequence '+case)
  page=events[-1][1];require(page==interaction['page_pass'] and page['type']=='password' and page['trustedTap'] is True and page['utf16Length']==expected_length and page['selection']==expected_selection and page['maskReviewRequired'] is True,'exact page assertion '+case)
  require('Browser observation completed;' in (p/'runner.log').read_text(),'normal runner completion '+case)
  require((p/'finish-driver.log').read_text().strip()=='NATIVE_INPUT_AND_PAGE_PASS_PENDING_RUNNER_AND_MASK_REVIEW','driver finish '+case)
  images=list((p/'input').glob('*.png'));immediate=list((p/'input').glob('typed-*-immediate-fep.png'))
  require(len(immediate)==immediate_count,'immediate frame count '+case)
  reviewed=accept['sampled_frames'];require(len(reviewed)==review_count and len({i['name'] for i in reviewed})==review_count,'root reviewed frame count '+case)
  for image in reviewed:require(sha((p/'input'/image['name']).read_bytes())==image['sha256'],'root reviewed image hash '+case)
  require(set(i['name'] for i in reviewed)==set(i.name for i in images)-{'before-start.png','after-page-check.png'},'complete sampled mask review '+case)
  total_immediate+=len(immediate);total_reviewed+=len(reviewed);total_images+=len(images)
  summary['cases'].append({'case':case,'run':token,'status':'PASS','exact_utf16_length':expected_length,'exact_selection':expected_selection,'driver_exit':0,'runner_exit':0,'config_restored':True,'restored_items':7,'all_before_after_equal':True,'root_reviewed_mask_images':len(reviewed),'immediate_key_images':len(immediate),'all_input_images':len(images),'raw_pending_statuses_retained':True})
 require(len(archived)==103 and total_reviewed==47 and total_immediate==35 and total_images==53,'archive totals')
 all_paths=[p.relative_to(ROOT) for p in ROOT.rglob('*') if p.is_file()]
 for p in all_paths:require(not any(x.lower().startswith(('private','previous','.p1-originals')) or x.lower()=='originals' for x in p.parts),'excluded private/previous/originals path')
 summary.update(original_files=103,total_archived_screenshots=54,root_reviewed_mask_screenshots=47,immediate_key_screenshots=35,mask_review_attribution='root acceptance.json; verifier confirms identity/completeness, not pixels or uninterrupted video')
 return summary
if __name__=='__main__':print(json.dumps(verify(),indent=2))
