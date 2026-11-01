int main(){int cases=0;try{
for(int rawSecret : {0, 0x100}) { for(int maximum : {64, 1024*1024}) {
 for(auto original : {std::u16string{},std::u16string(u"A|B"),std::u16string(u"known fixture \u4f60\u597d \U0001f600\n")}){
  for(bool backwards:{false,true}){
   Editor editor;editor.maximum=maximum;editor.text=original;editor.anchor=backwards?2:1;editor.cursor=backwards?1:2;
   if(original.empty())editor.anchor=editor.cursor=0;
   auto anchor=editor.anchor,cursor=editor.cursor;CHostBridgedImeFEP f;f.iInputCapabilities.editor=&editor;f.iInputCapabilities.rawSecret=rawSecret;f.iImeDialog.iHost=&f;f.OpenDialogInputL();CheckRequest(f,editor);f.iDialogPending=true;
   require(editor.text==original&&editor.anchor==anchor&&editor.cursor==cursor,"open changed field/selection");require(!editor.starts&&!editor.sets,"open began edit");require(f.iImeDialog.draft==original,"host draft mismatch");
   f.iImeDialog.iStatus.value=KErrCancel;f.iImeDialog.RunL();require(!f.iDialogPending,"cancel retained pending");require(editor.text==original&&editor.anchor==anchor&&editor.cursor==cursor,"cancel changed field/selection");require(!editor.starts&&!editor.sets&&!editor.commits,"cancel edited field");++cases;
  }
 }
 for(auto replacement : {std::u16string{},std::u16string(u"accepted \u4f60\u597d \U0001f600\n")}){
  Editor editor;editor.maximum=maximum;editor.text=u"old known fixture";editor.anchor=2;editor.cursor=4;CHostBridgedImeFEP f;f.iInputCapabilities.editor=&editor;f.iInputCapabilities.rawSecret=rawSecret;f.iImeDialog.iHost=&f;f.OpenDialogInputL();CheckRequest(f,editor);f.iDialogPending=true;accepted=replacement;f.iImeDialog.iStatus.value=KErrNone;f.iImeDialog.RunL();require(editor.text==replacement,"accepted text not replaced");require(editor.starts==1&&editor.commits==1&&!f.iDialogPending,"accepted completion mismatch");require(editor.anchor==(int)replacement.size()&&editor.cursor==(int)replacement.size(),"accepted caret mismatch");f.iImeDialog.RunL();require(editor.commits==1,"duplicate completion committed twice");++cases;
 }
 {Editor editor;editor.maximum=maximum;editor.text=u"unchanged";editor.anchor=2;editor.cursor=3;CHostBridgedImeFEP f;f.iInputCapabilities.editor=&editor;f.iInputCapabilities.rawSecret=rawSecret;f.OpenDialogInputL();CheckRequest(f,editor);f.iDialogPending=true;f.CancelDialogL();require(editor.text==u"unchanged"&&editor.anchor==2&&editor.cursor==3,"guest close edited field");require(!f.iDialogPending&&f.iImeDialog.cancels==1,"guest close mismatch");++cases;}
 {Editor editor;editor.maximum=maximum;editor.text=u"open failure";editor.anchor=3;editor.cursor=2;CHostBridgedImeFEP f;f.iInputCapabilities.editor=&editor;f.iInputCapabilities.rawSecret=rawSecret;f.iImeDialog.error=-14;bool failed=false;try{f.OpenDialogInputL();CheckRequest(f,editor);}catch(int e){failed=e==-14;}require(failed&&!f.iInFEPWork,"open failure state");require(editor.text==u"open failure"&&editor.anchor==3&&editor.cursor==2&&!editor.starts,"open failure edited field");++cases;}
 } }
 require(oldA0Calls==0,"legacy A0 was used");
 require(CleanupStack::stack.empty(),"cleanup stack leaked");std::cout<<cases<<" actual-method state cases PASS (secret 0/256, maxima 64/1048576; mock editor; no guest)\n";
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}return 0;}
