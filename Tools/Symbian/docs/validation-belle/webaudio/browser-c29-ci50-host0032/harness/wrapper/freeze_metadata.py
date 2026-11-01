#!/usr/bin/env python3
"""Root freezes actual CI reports and git-object source snapshots; no build or runtime."""
import argparse,json
from pathlib import Path
import run as wrapper
def main():
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--audit-contract',type=Path);parser.add_argument('--audit-contract-sha256');parser.add_argument('--source-commit',default=wrapper.CI_SOURCE_COMMIT);parser.add_argument('--sources-only',action='store_true');parser.add_argument('--output',type=Path);args=parser.parse_args()
 try:
  helper=wrapper.provenance_module();wrapper.require(args.source_commit==wrapper.CI_SOURCE_COMMIT,'explicit actual CI50 commit required')
  if args.sources_only:
   rows=helper.source_snapshots(args.source_commit,wrapper.REQUIRED_SOURCES,materialize=True);value={'status':'CI_COMMIT_SOURCES_ONLY_ARTIFACT_PENDING','source_commit':args.source_commit,'source_files':rows,'runtime_started':False};output=args.output or wrapper.CANDIDATE_ROOT/'source-identities-ci.json'
  else:
   wrapper.require(args.audit_contract is not None and args.audit_contract_sha256,'actual audited CI package contract and SHA required');value,pins=helper.build_metadata(args.audit_contract,args.audit_contract_sha256,args.source_commit,wrapper.REQUIRED_SOURCES,materialize_sources=True);output=args.output or wrapper.CANDIDATE_ROOT/'runtime-metadata.json'
   for path,expected in pins.values():wrapper.require(wrapper.sha(wrapper.safe_file(path))==expected,'actual audited CI input changed while freezing')
  wrapper.require(output.is_absolute()and output==output.resolve()and not output.exists(),'fresh canonical metadata output required')
  for parent in output.parents:wrapper.require(not parent.is_symlink(),'metadata output symlink ancestor refused')
  output.parent.mkdir(parents=True,exist_ok=True);output.write_text(json.dumps(value,indent=2)+'\n');print(json.dumps({'status':'FROZEN_CI_SOURCES_ONLY'if args.sources_only else'FROZEN_CI_METADATA_ONLY','path':str(output),'sha256':wrapper.sha(output),'runtime_started':False,'sis_installer_verified':False,'physical_phone_verified':False},indent=2));return 0
 except(OSError,ValueError,TypeError,RuntimeError)as error:print('FAIL: '+str(error));return 1
if __name__=='__main__':raise SystemExit(main())
