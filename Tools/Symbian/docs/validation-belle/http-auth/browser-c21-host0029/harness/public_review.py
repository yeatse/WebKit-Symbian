"""One public empty heading and each public mask FEP; all other images rejected."""
from pathlib import Path
from heading_review import validate_heading
from mask_review import validate_probe_evidence
from parent_probe import validate_closure

def validate_public_evidence(interaction,root,guest=None):
 failed,incomplete,images=validate_heading(interaction,root)
 result=validate_probe_evidence(interaction,root,allowed_public_images=images)
 result['failures']+=failed+validate_closure(interaction,guest);result['incomplete']+=incomplete
 result['status']='FAIL' if result['failures'] else 'INCOMPLETE' if result['incomplete'] else 'PASS'
 return result
