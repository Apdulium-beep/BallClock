const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path'),vm=require('node:vm');
const script=fs.readFileSync(path.join(__dirname,'../assets/mobile.html'),'utf8').match(/<script>([\s\S]*?)<\/script>/)[1].replace('\n(async()=>{try{adopt','\nglobalThis.boot=(async()=>{try{adopt');
const clone=value=>JSON.parse(JSON.stringify(value));
const clock={s:{font:1,color:'ffffff',bmode:1,pal:0,night:0},t:{synced:1,saved:1}};
for(let i=0;i<8;i++)clock.s['grad'+i]='';
let saved=clone(clock),failSave=false;const calls=[];
function page(){const nodes=new Map(),element=()=>({style:{},dataset:{},append(){},setAttribute(){},querySelectorAll(){return[]},textContent:'',innerHTML:''});
 const context=vm.createContext({URLSearchParams,TextEncoder,Date,Option:function(){},setTimeout(){return 1},clearTimeout(){},setInterval(){},
 document:{querySelector(key){if(!nodes.has(key))nodes.set(key,element());return nodes.get(key)},querySelectorAll(){return[]},createElement:element,activeElement:{tagName:'BODY'}},
 async fetch(url,options={}){const body=Object.fromEntries(options.body||[]);calls.push({url,body});
  if(url==='/api/set'){Object.assign(clock.s,body);clock.t.saved=0;}
  if(url==='/api/save'){if(failSave)throw Error('offline');clock.t.saved=1;saved=clone(clock);return {ok:true,json:async()=>({ok:true,saved:true})};}
  return {ok:true,json:async()=>clone(clock)};}});
 context.window=context;vm.runInContext(script,context);return {context,run:code=>vm.runInContext(code,context),nodes};}
(async()=>{
 const a=page();await a.context.boot;
 for(const [hex,h,s,v] of [['ff0000',0,1,1],['00ff00',120,1,1],['0000ff',240,1,1],['000000',0,0,0],['ffffff',0,0,1]]){
  assert.equal(a.run(`hsvToHex(${h},${s},${v})`),hex);
  assert.equal(a.run(`(()=>{const p=hexToHsv('${hex}');return hsvToHex(p.h,p.s,p.v)})()`),hex);
 }
 for(let i=0;i<1024;i++){const hex=((i*1657967)%0x1000000).toString(16).padStart(6,'0');assert.equal(a.run(`(()=>{const p=hexToHsv('${hex}');return hsvToHex(p.h,p.s,p.v)})()`),hex);}
 a.run("openGradient();gradientDraft.name='Северное сияние';gradientDraft.colors=['123456','38c3ff','91e0c2','ffffff']");await a.run('saveGradient()');
 assert.equal(saved.s.grad0,'Северное сияние|123456,38c3ff,91e0c2,ffffff');assert.equal(+saved.s.pal,9);
 const b=page();await b.context.boot;assert.equal(b.run('savedGradients()[0].name'),'Северное сияние','Second device did not load clock library');
 const before=JSON.stringify(clock.s);b.run("openGradient(0);gradientDraft.name='Несохранённое';gradientDraft=null;layout(true)");assert.equal(JSON.stringify(clock.s),before);
 b.run("openGradient();gradientDraft.name='Северное сияние'");const count=calls.length;await b.run('saveGradient()');assert.equal(calls.length,count,'Duplicate name was sent');
 for(const bad of ['', 'Bad|name','Bad\nname','я'.repeat(33)]){b.run('gradientDraft.name='+JSON.stringify(bad));await b.run('saveGradient()');assert.equal(calls.length,count);}
 b.run("gradientDraft.name='Мой градиент'");failSave=true;await b.run('saveGradient()');assert.equal(saved.s.grad1,'','Failed save claimed persistence');assert.ok(b.run('gradientDraft!==null'));failSave=false;await b.run('saveGradient()');assert.match(saved.s.grad1,/^Мой градиент\|/);
 clock.s=clone(saved.s);const c=page();await c.context.boot;assert.equal(c.run('savedGradients().length'),2,'Library lost after simulated reboot');
 c.run('openGradient(0)');await c.run('saveGradient(true)');assert.equal(saved.s.grad0,'');
 c.run("openGradient(1);gradientDraft.name='Переименован'");await c.run('saveGradient()');assert.match(saved.s.grad1,/^Переименован\|/);
 for(let i=0;i<8;i++)clock.s['grad'+i]='Слот '+i+'|ff0000,00ff00,0000ff,ffffff';const full=page();await full.context.boot;full.run('openGradient()');assert.equal(full.run('gradientDraft'),null);
 assert.ok(!calls.some(c=>c.url.includes('/wifi')||c.url.includes('/scan')));
 console.log('PASS: visual color conversions, named gradient save/read on second device, cancel, validation, retry, reboot, rename/delete, capacity, and no Wi-Fi changes.');
})().catch(error=>{console.error(error);process.exitCode=1});
