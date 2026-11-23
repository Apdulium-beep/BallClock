const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm'),assert=require('node:assert/strict');
const script=fs.readFileSync(path.join(__dirname,'../assets/mobile.html'),'utf8').match(/<script>([\s\S]*?)<\/script>/)[1].replace('\n(async()=>{try{adopt','\nglobalThis.boot=(async()=>{try{adopt');
const bank=Array.from({length:8},()=>({name:'',data:'0'.repeat(3712)})),clock={s:{font:1,bmode:1,pal:0,drawSlot:0,drawDigits:0},t:{synced:1,saved:1}};let fail=false;const calls=[];
const clone=v=>JSON.parse(JSON.stringify(v));
function page(){const element=()=>({style:{},dataset:{},append(){},setAttribute(){},querySelectorAll(){return[]},textContent:'',innerHTML:''}),nodes=new Map();
 const c=vm.createContext({URLSearchParams,TextEncoder,Date,Option:function(){},setTimeout(){return 1},clearTimeout(){},setInterval(){},document:{querySelector(k){if(!nodes.has(k))nodes.set(k,element());return nodes.get(k)},querySelectorAll(){return[]},createElement:element,activeElement:{tagName:'BODY'}},
 async fetch(url,opts={}){calls.push(url);const fields=Object.fromEntries(opts.body||[]);if(fail&&url==='/api/v1/art')throw Error('offline');let out;
  if(url==='/api/v1/art'){assert.ok(opts.headers['X-BallClock-Client']);bank[+fields.slot]={name:fields.name,data:fields.data};out={ok:true,saved:true};}
  else if(url==='/api/v1/art/delete'){bank[+fields.slot]={name:'',data:'0'.repeat(3712)};out={ok:true,saved:true};}
  else if(url==='/api/save')out={ok:true,saved:true};else{if(url==='/api/set')Object.assign(clock.s,fields);out={...clock,art:{names:bank.map(p=>p.name),data:bank[+clock.s.drawSlot].data}};}
  return {ok:true,json:async()=>clone(out)};}});c.window=c;vm.runInContext(script,c);return {c,run:s=>vm.runInContext(s,c)};}
(async()=>{const p=page();await p.c.boot;if(process.argv.includes('--geometry')){console.log(p.run('ART_POS.map((p,i)=>i+","+p.r+","+p.x).join("\\n")'));return}
 assert.equal(p.run('ART_POS.length'),128);assert.equal(p.run('new Set(ART_POS.map(p=>p.r+","+p.x)).size'),128);
 p.run("tab=4;drawingDraft={slot:0,name:'Моя картина',pixels:Array.from({length:128},blankPixel),selected:new Set([0,127]),brush:{effect:2,period:20,phase:64,colors:['ff0000','00ff00','0000ff','ffffff']},history:[]}");
 p.run('drawingApply()');assert.equal(p.run('drawingDraft.pixels[0].effect'),2);assert.equal(p.run('drawingDraft.pixels[1].effect'),0);assert.equal(p.run('drawingDraft.pixels[127].phase'),64);
 assert.equal(p.run('encodeDrawing(drawingDraft.pixels).length'),3712);
 assert.equal(p.run('JSON.stringify(decodeDrawing(encodeDrawing(drawingDraft.pixels)))===JSON.stringify(drawingDraft.pixels)'),true);
 assert.equal(p.run("drawingColor({effect:3,period:20,phase:0,colors:['ff0000','00ff00','0000ff','ffffff']},500)"),'00ff00');
 assert.equal(p.run("drawingColor({effect:2,period:20,phase:0,colors:['ff0000','0000ff','000000','ffffff']},250)"),'800080');
 for(const bad of ['bad','0'.repeat(3711),'9'+'0'.repeat(3711),'20000'+'0'.repeat(3707)])assert.throws(()=>p.run('decodeDrawing('+JSON.stringify(bad)+')'));
 await p.run('saveDrawing()');assert.equal(bank[0].name,'Моя картина');assert.equal(+clock.s.bmode,4);assert.equal(p.run('drawingDraft'),null);
 const q=page();await q.c.boot;assert.equal(q.run('A.names[0]'),'Моя картина');assert.equal(q.run('decodeDrawing(A.data)[127].effect'),2);
 q.run("tab=4;drawingDraft={slot:0,name:'Переименована',pixels:decodeDrawing(A.data),selected:new Set(),history:[]}");fail=true;await q.run('saveDrawing()');assert.equal(q.run('drawingDraft!==null'),true);assert.equal(bank[0].name,'Моя картина');fail=false;await q.run('saveDrawing()');assert.equal(bank[0].name,'Переименована');
 q.run("drawingDraft={slot:0,name:'',pixels:[],history:[]}");await q.run('saveDrawing(true)');assert.equal(bank[0].name,'');
 assert.ok(!calls.some(c=>c.includes('/wifi')||c.includes('/scan')));
 console.log('PASS: drawing geometry, codec, individual effects/phase, undo snapshots, UTF-8 save/read across clients, failed save retry, delete; no Wi-Fi reconfiguration.');
})().catch(e=>{console.error(e);process.exitCode=1});
