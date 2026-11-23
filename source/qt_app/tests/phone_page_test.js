const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');

const html = fs.readFileSync(process.argv[2] ? path.resolve(process.argv[2]) : path.join(__dirname, '../../android/assets/mobile.html'), 'utf8');
const script = html.match(/<script>([\s\S]*?)<\/script>/)[1]
    .replace('\n(async()=>{try{adopt', '\nglobalThis.boot=(async()=>{try{adopt');

function harness(synced) {
    const nodes = new Map();
    const element = () => ({dataset: {}, style: {}, append() {}, add() {}, setAttribute() {}, textContent: '', innerHTML: ''});
    const calls = [];
    const state = {s: {font: 1, bmode: 1, bright: 100, ssid: ''}, t: {synced, saved: 1, time: '12:00:00', ap: 1, ip: '192.168.4.1'}};
    const controls = {fail: false};
    const context = vm.createContext({
        document: {
            querySelector(key) { if (!nodes.has(key)) nodes.set(key, element()); return nodes.get(key); },
            querySelectorAll() { return []; }, createElement: element, activeElement: {tagName: 'BODY'}
        },
        Option: function() {}, URLSearchParams, Date,
        setTimeout() { return 1; }, clearTimeout() {}, setInterval() {},
        async fetch(url, options) {
            const body = Object.fromEntries(options?.body || []);
            calls.push({url, body});
            if (controls.fail) throw Error('offline');
            if (url === '/api/set') { Object.assign(state.s, body); state.t.saved = 0; }
            if (url === '/api/settime') { state.t.synced = 1; state.t.saved = 1; }
            if (url === '/api/save') {
                state.t.saved = 1;
                return {ok: true, json: async () => ({ok: true, saved: true})};
            }
            return {ok: true, json: async () => JSON.parse(JSON.stringify(state))};
        }
    });
    context.window = context;
    vm.runInContext(script, context);
    return {context, calls, nodes, controls, run: code => vm.runInContext(code, context)};
}

(async () => {
    const h = harness(0);
    await h.context.boot;
    assert.equal(h.calls.filter(c => c.url === '/api/settime').length, 1);
    const time = h.calls.find(c => c.url === '/api/settime').body;
    assert.ok(Number(time.epoch) > 1600000000);
    assert.equal(Number(time.tz), -new Date().getTimezoneOffset());
    h.run("send('bright',120);send('speed',33)");
    await h.run('flush()');
    assert.deepEqual(h.calls.find(c => c.url === '/api/set').body, {bright: '120', speed: '33'});
    const saved = await h.run('saveAll()');
    if (process.argv[2]) assert.equal(saved, true, 'Explicit close must receive a positive save confirmation');
    assert.match(h.nodes.get('#saveStatus').textContent, /Настройки сохранены/);
    h.controls.fail = true;
    h.run("send('bright',121)");
    await assert.rejects(h.run('flush()'));
    assert.equal(h.run('pend.bright'), 121);
    const failedSave = await h.run('saveAll()');
    if (process.argv[2]) assert.equal(failedSave, false, 'Failed save must not allow an acknowledged close');
    assert.match(h.nodes.get('#saveStatus').textContent, /Нет подтверждения/);
    h.controls.fail = false;
    await h.run('saveAll()');
    assert.equal(h.run('Object.keys(pend).length'), 0);
    assert.match(h.nodes.get('#saveStatus').textContent, /Можно отключаться/);
    const alreadySynced = harness(1);
    await alreadySynced.context.boot;
    assert.equal(alreadySynced.calls.filter(c => c.url === '/api/settime').length, 0);
    console.log('PASS: phone time initialization, batched settings, explicit flash-save confirmation and offline retry.');
})().catch(error => { console.error(error); process.exitCode = 1; });
