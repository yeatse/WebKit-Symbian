#!/usr/bin/env python3
"""Read-only public archive audit; no subprocess, private inputs or raw logs."""
import hashlib,importlib.util,json,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parent
sys.path.insert(0,str(ROOT/'harness'))
from public_review import validate_public_evidence
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
def require(value,message):
 if not value:raise RuntimeError(message)
def main():
 freeze=json.loads((ROOT/'harness/preexecution-freeze.json').read_text())
 for name,want in freeze['files'].items():require(sha(ROOT/'harness'/name)==want,'runtime source changed: '+name)
 summaries=json.loads((ROOT/'run-summaries.json').read_text());identities=json.loads((ROOT/'source-evidence-identities.json').read_text())
 expected={(s,c) for s in ['basic','digest'] for c in ['success','retry','cancel-user','cancel-password','navigate-user','navigate-password']}
 require(len(summaries)==12 and {(x['scheme'],x['case']) for x in summaries}==expected,'ordinary matrix changed')
 heading_count=mask_count=0
 for summary,identity in zip(summaries,identities):
  run=summary['run'];require(identity['run']==run,'identity order differs');d=ROOT/'runs'/run
  for item in identity['archived_public_files']:
   p=Path(item['path']);require(not p.is_absolute() and '..' not in p.parts,'invalid archive path');require(sha(d/p)==item['sha256'],'archived original changed')
  final=json.loads((d/'validation-rootreview.json').read_text());initial=json.loads((d/'validation-initial.json').read_text());strict=json.loads((d/'strict-validation-rootreview.json').read_text());interaction=json.loads((d/'interaction.json').read_text());review=json.loads((d/'root-review.json').read_text())
  require(final['status']=='PASS' and not final['failures'] and not final['incomplete'],'final verdict changed')
  require(initial['status']=='INCOMPLETE' and initial['functional_checks']=='PASS' and not initial['failures'],'initial verdict changed')
  require(strict['status']=='FAIL' and strict['failures']==['interaction must explicitly avoid secret screenshots'],'original strict failure changed')
  require(final['panic_classification']['permitted_background_shutdown']==[],'unexpected background exception')
  public=validate_public_evidence(interaction,d,None);require(public['status']=='PASS','public proof did not pass')
  require(review['run']==run and review['native_query_reviewed'] is True and review['native_query_dismissed_reviewed'] is True and bool(review['native_query_dismissed_observation']),'Root current-run review absent')
  require(summary['input_exit']==summary['host_exit_code']==0 and summary['normal_close'] is True and summary['forced_cleanup'] is False and summary['target_guest_failed'] is False and summary['target_thread_failures']==[],'exit contract changed')
  require(summary['resources_restored'] is True and summary['private_config_profile_restored'] is True and summary['no_guest_running_at_restore'] is True,'restoration contract changed')
  require(summary['curl_trace_disabled'] is True and summary['inputs_unchanged'] is True and summary['tools_unchanged'] is True,'input/privacy pin changed')
  heading_count+=len(interaction['heading_reviews']);mask_count+=len(interaction['password_mask_reviews'])
 require(heading_count==mask_count==12,'public image counts changed')
 print(json.dumps({'status':'PASS','ordinary_runs':12,'public_headings':heading_count,'public_password_probes':mask_count,'original_strict_failures_retained':12,'initial_incomplete_retained':12,'raw_logs_or_private_data_read':False}))
if __name__=='__main__':main()
