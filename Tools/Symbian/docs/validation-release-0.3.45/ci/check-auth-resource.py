from pathlib import Path
import hashlib,json
r=Path(__file__).resolve().parent
source=json.loads((r/'source-auth-limit-verification.json').read_text())
assert source['status']=='PASS_SOURCE_ONLY_ARTIFACT_PENDING'
assert source['source_commit']=='e0e2f321d4e2d4eac98c33c3e51eca0f43ffaac5'
assert len(source['checks'])==11 and all(value is True for value in source['checks'].values())
assert source['sdk_reference']['value']==255
resource=r/'payloads/browser_ui.rsc';data=resource.read_bytes()
expected='fb8e0b7154cb40458860c74bbb5334da613b34339cd47906a1cae0d6d7290ac8'
assert len(data)==242 and hashlib.sha256(data).hexdigest()==expected
report={'status':'PASS','scope':'Actual CI45 SIS-extracted auth/popup resource exactly matches the independently compiled fixed255 resource; source contract is recorded separately','resource':{'path':str(resource),'bytes':len(data),'sha256':expected},'source_contract_report':{'path':str(r/'source-auth-limit-verification.json'),'sha256':hashlib.sha256((r/'source-auth-limit-verification.json').read_bytes()).hexdigest()},'guest_or_phone_execution':False}
(r/'auth-resource-verification.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
