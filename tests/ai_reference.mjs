// Regenerate the fingerprints in ai.zig from the pinned, unmodified JS classes.
// node tests/ai_reference.mjs ../oneslime-vball/OneSlime.min.js
import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import { readFileSync } from 'node:fs';
import vm from 'node:vm';

const source = readFileSync(process.argv[2], 'utf8');
assert.equal(createHash('sha256').update(source).digest('hex'),
  '097b88e7b5009fdb454370af4d16fd3a096403cea2afcec9836434c25e17d872');
const classes = source.slice(source.indexOf('class nt'), source.indexOf('class ct'));
const context = vm.createContext({ Math: Object.create(Math) });
new vm.Script(`
const ki = value => value | 0;
let u, h, y;
${classes}
globalThis.create = stage => {
  const controller = new [_t, ot, pt][stage]();
  controller.init(2);
  return controller;
};
globalThis.step = (controller, values, random) => {
  [u, , y] = values; h = values[8];
  Math.random = random;
  controller.saveVars(values, false, false);
  const actions = [];
  controller.move = action => actions.push(action);
  controller.moveSlime();
  return actions;
};`).runInContext(context);

class Random {
  constructor(seed) { this.seed = seed >>> 0; this.calls = 0; }
  integer() { return this.seed = (Math.imul(this.seed, 1664525) + 1013904223) >>> 0; }
  next() { this.calls++; return this.integer() / 4294967296; }
  pick(values) { return values[this.integer() % values.length]; }
}
function snapshot(random, frame) {
  const v = [
    random.pick([200,499,500,539,540,579,580,600,619,620,699,700,729,730,770,800,840,860,900,990]),
    random.pick([35,50,51,100,124,125,129,130,131,200,249,250,299,300,400]),
    random.pick([-15,-8,-1,0,1,8,15]),
    random.pick([-22,-13,-12,-3,-2,-1,0,12,13,22]),
    random.pick([150,199,200,249,250,251,299,300,301,400]),
    random.pick([0,29,100]), 0, 0,
    random.pick([555,574,575,580,585,666,700,770,799,800,801,840,845,860,900,901,950]),
    random.pick([0,29,100]),
    random.pick([-8,0,8]), random.pick([-9,0,9]),
  ];
  if(frame % 3 === 0) { v[0] += .25; v[1] += .5; v[3] += .5; }
  return v;
}
const fingerprints = [];
for(let stage = 0; stage < 3; stage++) {
  const row = [];
  for(let persistent = 0; persistent < 2; persistent++) {
    const inputs = new Random(0x51a1beef);
    const controller = context.create(stage);
    let hash = 2166136261;
    for(let frame = 0; frame < 4096; frame++) {
      const values = snapshot(inputs, frame);
      const random = new Random(inputs.integer());
      if(!persistent) controller.i_serveType = inputs.integer() % 6 - 1;
      const actions = context.step(controller, values, () => random.next());
      assert.ok(actions.length <= 4 && random.calls < 16);
      let code = controller.i_serveType + 1 | actions.length << 3 | random.calls << 16;
      actions.forEach((action, i) => code |= action << (6 + i * 2));
      hash = Math.imul(hash ^ code, 16777619) >>> 0;
      if(process.argv.includes('--dump')) console.log(stage, persistent, frame, code);
    }
    row.push(hash);
  }
  fingerprints.push(row);
}
console.log('Reference fingerprints:', JSON.stringify(fingerprints));
console.log('PASS 24,576 original-controller decisions captured (actions, serve state, random-call counts)');
