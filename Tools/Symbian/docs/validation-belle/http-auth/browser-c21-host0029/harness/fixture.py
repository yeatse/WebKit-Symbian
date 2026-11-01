#!/usr/bin/env python3
"""Unchanged HTTP auth fixture plus a console-only trusted top-parent button."""
import json
from support import ROOT,load_module
base=load_module('auth_fixture_unchanged',ROOT/'upstream/fixture.py')
INDEX=base.Handler.index;RESULT=base.Handler.result_page

def probe(run):
 return '''<script>(function(){if(window!==window.top)return;var run=RUN;
var b=document.createElement('button');b.id='parent-trusted-probe';b.textContent='Check parent response';b.style.cssText='position:fixed;left:12px;right:12px;bottom:8px;width:calc(100% - 24px);height:42px;z-index:20;font:16px sans-serif';document.body.appendChild(b);
function payload(){var r=b.getBoundingClientRect();return {run:run,source:'top-parent',page_path:location.pathname,x:r.left+r.width/2,y:r.top+r.height/2,width:innerWidth,height:innerHeight};}
function report(){console.log('AUTH-PARENT-PROBE-READY '+JSON.stringify(payload()));}
b.onclick=function(e){var p=payload();p.trusted=e.isTrusted===true;console.log('AUTH-PARENT-PROBE-CLICK '+JSON.stringify(p));};
requestAnimationFrame(function(){requestAnimationFrame(report);});window.addEventListener('resize',report);
})();</script>'''.replace('RUN',json.dumps(run))
def index(self):return INDEX(self)+probe(self.state.run)
def result(self,event,title):return RESULT(self,event,title)+probe(self.state.run)
base.Handler.index=index;base.Handler.result_page=result
if __name__=='__main__':base.main()
