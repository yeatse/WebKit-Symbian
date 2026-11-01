from pathlib import Path
import datetime,hashlib,json,shutil,zipfile
r=Path(__file__).resolve().parent;archive=r/'ci49-sis.zip';sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
assert archive.stat().st_size==33763135 and sha(archive)=='fa7910e07e06a9d47de50799f2cdf78abfbd2cc45b4661b56f0c8e6eb692d9be'
records=[]
with zipfile.ZipFile(archive) as z:
 assert z.testzip() is None and set(z.namelist())=={'SHA256SUMS','browser.sis','jsc.sis','symbian-webkit-icu.sis'}
 for name in z.namelist():
  p=r/'sis'/name;assert p.read_bytes()==z.read(name);records.append({'path':str(p),'bytes':p.stat().st_size,'sha256':sha(p)})
report=json.loads((r/'sis-payload-verification.json').read_text());assert report['status']=='PASS' and report['payload_count']==13
expected=set()
for package in report['packages']:
 for item in package['payloads']:
  p=Path(item['extracted_path']);assert p.is_relative_to(r/'payloads') and p.is_file() and not p.is_symlink();assert p.stat().st_size==item['bytes'] and sha(p)==item['sha256'];expected.add(p);records.append({'path':str(p),'bytes':p.stat().st_size,'sha256':item['sha256']})
assert set((r/'payloads').iterdir())==expected
manifest={'status':'VERIFIED_CANONICAL_ZIP_RECONSTRUCTIBLE_DERIVED_CLEANUP','utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'reason':'Shared runtime preservation reserve fell below 300MiB during concurrent runtime deployment; remove only this task owned derived extraction after exact checks. Canonical original artifact and all audit evidence stay.','canonical_ZIP':{'path':str(archive),'bytes':archive.stat().st_size,'sha256':sha(archive),'ZIP_CRC_valid':True},'removed_derived_files':records,'bytes_removed':sum(x['bytes'] for x in records),'all_reports_logs_sources_and_audit_scripts_preserved':True,'other_artifacts_SDK_or_runtime_untouched':True,'restore':['Unzip the four canonical ZIP members into sis/','python3 verify-payloads.py /workspace/symbian-ci49-validation --tools-root /workspace/symbian-ci49-validation/verification-tools/Tools/Symbian'],'historical_verification_root_manifest_sha256':'c2de79ba08679aef5c07f0b7446c3ea003332bbcfeb4d10ce62df2b6f3ff49be'}
(r/'derived-extraction-cleanup.json').write_text(json.dumps(manifest,indent=2)+'\n')
shutil.rmtree(r/'sis');shutil.rmtree(r/'payloads');print(json.dumps({'bytes_removed':manifest['bytes_removed'],'canonical_ZIP_retained':True,'raw_audit_evidence_retained':True}))
