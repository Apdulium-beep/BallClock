const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path'),vm=require('node:vm');
let now=1795330000000,mode='valid',frameCalls=0,gate;
const state={device:'ballclock',s:{font:1,bmode:1,pal:1,apOnly:1},t:{synced:1,saved:1}};
const frame={device:'ballclock',api_version:1,led_count:128,rgb:'12abef'.repeat(128)};
const element=()=>({dataset:{},style:{},append(){},setAttribute(){},textContent:'',innerHTML:''});
const nodes=new Map();class Clock extends Date{static now(){return now}}
const context=vm.createContext({Date:Clock,AbortController,URLSearchParams,Option:function(){},
 document:{querySelector(key){if(!nodes.has(key))nodes.set(key,element());return nodes.get(key)},querySelectorAll(){return[]},createElement:element,activeElement:{tagName:'BODY'}},
 setTimeout(){return 1},clearTimeout(){},setInterval(){},
 async fetch(url,options={}){
  if(mode==='offline')throw Error('offline');
  if(url==='/api/v1/frame'){
   frameCalls++;assert.equal(options.redirect,'error');
   if(mode==='gate')return new Promise((resolve,reject)=>{gate=resolve;options.signal.addEventListener('abort',()=>reject(Object.assign(Error('aborted'),{name:'AbortError'})),{once:true})});
   if(mode==='404')return {ok:false,status:404};
   return response(mode==='invalid'?{...frame,rgb:'z'.repeat(768)}:frame,mode==='oversize');
  }
  return response(state);
 }});
function response(value,oversize=false){return {ok:true,status:200,json:async()=>JSON.parse(JSON.stringify(value)),text:async()=>oversize?'x'.repeat(5000):JSON.stringify(value)}}
context.window=context;
const script=fs.readFileSync(path.join(__dirname,'../assets/mobile.html'),'utf8').match(/<script>([\s\S]*?)<\/script>/)[1].replace('\n(async()=>{try{adopt','\nglobalThis.boot=(async()=>{try{adopt');
vm.runInContext(script,context);
const tick=sequence=>context.BallClockPreview.tick(sequence),settle=()=>new Promise(setImmediate);
(async()=>{
 await context.boot;tick(0);await settle();let result=tick(0);
 assert.equal(result.kind,'live');assert.equal(result.rgb,frame.rgb);const seq=result.seq;assert.equal(tick(seq).rgb,null);
 now+=1000;const requestsBeforeEdit=frameCalls;vm.runInContext("send('night',1)",context);tick(seq);
 assert.equal(frameCalls,requestsBeforeEdit,'Preview must yield to settings edits');
 await vm.runInContext('flush()',context);tick(seq);await settle();assert.ok(frameCalls>requestsBeforeEdit);
 now+=4000;mode='404';tick(seq);await settle();result=tick(seq);
 assert.equal(result.kind,'preview_error');assert.equal(result.controls,true);assert.equal(result.error,'HTTP 404');
 now+=3000;mode='invalid';tick(seq);await settle();result=tick(seq);assert.equal(result.kind,'preview_error');assert.equal(result.error,'invalid_frame');
 now+=3000;mode='oversize';tick(seq);await settle();assert.equal(tick(seq).error,'invalid_frame');
 now+=13000;mode='offline';tick(seq);await settle();assert.equal(tick(seq).kind,'offline');
 now+=3000;mode='valid';tick(seq);await settle();result=tick(seq);assert.equal(result.kind,'live');assert.ok(result.seq>seq);
 now+=1000;mode='gate';const before=frameCalls;for(let i=0;i<100;i++)tick(result.seq);assert.equal(frameCalls,before+1,'Overlapping requests');
 context.BallClockPreview.stop();await settle();
 mode='valid';now+=1000;tick(result.seq);await settle();assert.equal(tick(result.seq).kind,'live');
 delete context.AbortController;now+=2000;tick(-1);await settle();assert.equal(tick(-1).kind,'live','Older WebView without AbortController');
 console.log('PASS: WebView preview frames, exact RGB, separate control/preview status, malformed/oversize rejection, offline recovery and no overlapping reads.');
})().catch(error=>{console.error(error);process.exitCode=1});
