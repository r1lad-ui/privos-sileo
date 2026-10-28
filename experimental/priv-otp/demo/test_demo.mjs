/* Run with: node experimental/priv-otp/demo/test_demo.mjs */
import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { createHash, createHmac, webcrypto } from 'node:crypto';

const html = await readFile(new URL('./index.html', import.meta.url), 'utf8');
const matches = [...html.matchAll(/<script type="module" id="otp-demo-code">([\s\S]*?)<\/script>/g)];
assert.equal(matches.length, 1, 'One self-contained module');
assert.equal((html.match(/<script\b/g) || []).length, 1, 'No external scripts');
const source = matches[0][1];
const digest = createHash('sha256').update(source).digest('base64');
assert.ok(html.includes(`script-src 'sha256-${digest}'`), 'Inline script matches CSP hash');
assert.ok(html.includes("default-src 'none'"), 'Resources denied by default');
assert.ok(html.includes("connect-src 'none'"), 'Network disabled by CSP');
assert.equal(/\b(fetch|XMLHttpRequest|WebSocket|localStorage|sessionStorage|indexedDB)\b/.test(source), false, 'No network or persistent storage API');

// Import only the one module in this repository's own static fixture.
const demo = await import(`data:text/javascript;base64,${Buffer.from(source).toString('base64')}`);
const { publicTestCode, phaseAt, startDemo, PUBLIC_TEST_KEY, PROFILE } = demo;
assert.equal(PUBLIC_TEST_KEY, '12345678901234567890123456789012');
assert.deepEqual(PROFILE, { period: 60, digits: 6, algorithm: 'SHA-256' });

let checks = 0;
const rfc = [
  [59, '46119246'], [1111111109, '68084774'], [1111111111, '67062674'],
  [1234567890, '91819424'], [2000000000, '90698825'], [20000000000, '77737706']
];
for (const [time, expected] of rfc) {
  assert.equal(await publicTestCode(time, 30, 8, webcrypto.subtle), expected, `RFC 6238 SHA-256 at ${time}`);
  checks++;
}
const profile = [
  [0, '920136'], [59, '920136'], [60, '119246'], [119, '119246'],
  [120, '882438'], [1560, '021280'], [1234567890, '450756'], [257698037820, '447045']
];
for (const [time, expected] of profile) {
  assert.equal(await publicTestCode(time, 60, 6, webcrypto.subtle), expected, `PRIV profile at ${time}`);
  // Independently build a big-endian counter and HMAC with Node/OpenSSL.
  const counter = Buffer.alloc(8);
  counter.writeBigUInt64BE(BigInt(Math.floor(time / 60)));
  const mac = createHmac('sha256', PUBLIC_TEST_KEY).update(counter).digest();
  const offset = mac.at(-1) & 15;
  const independent = String((mac.readUInt32BE(offset) & 0x7fffffff) % 1000000).padStart(6, '0');
  assert.equal(independent, expected, `Independent public fixture at ${time}`);
  checks++;
}
for (const args of [[-1], [1.5], [NaN], [Infinity], [Number.MAX_SAFE_INTEGER + 1], [0, 0], [0, -1], [0, 1.5], [0, 60, 7]]) {
  await assert.rejects(publicTestCode(...args), RangeError);
  checks++;
}
await assert.rejects(publicTestCode(0, 60, 6, null), /WEB_CRYPTO_UNAVAILABLE/);
checks++;

assert.deepEqual(phaseAt(0), { counter: 0, seconds: 60, progress: 60 });
assert.deepEqual(phaseAt(59000), { counter: 0, seconds: 1, progress: 1 });
assert.deepEqual(phaseAt(59999), { counter: 0, seconds: 1, progress: 0.001 });
assert.deepEqual(phaseAt(60000), { counter: 1, seconds: 60, progress: 60 });
assert.throws(() => phaseAt(-1), RangeError);
assert.throws(() => phaseAt(0.1), RangeError);
checks += 6;

function fakeDocument() {
  const elements = Object.fromEntries(['code', 'seconds', 'remaining', 'status'].map(id => [id, {
    textContent: '', value: 0, attributes: {}, classes: new Set(),
    setAttribute(name, value) { this.attributes[name] = value; },
    classList: { add(name) { elements[id].classes.add(name); } }
  }]));
  return { elements, hidden: false, getElementById: id => elements[id], addEventListener() {} };
}
let now = 59999;
const doc = fakeDocument();
const app = startDemo(doc, () => now, () => 1);
await app.ready;
assert.equal(doc.elements.code.textContent, '920 136');
assert.equal(doc.elements.seconds.textContent, '1 s');
now = 60000;
await app.tick();
assert.equal(doc.elements.code.textContent, '119 246');
assert.equal(doc.elements.seconds.textContent, '60 s');
now = 1560000;
await app.tick();
assert.equal(doc.elements.code.textContent, '021 280', 'Leading zero is retained in display');
now = 0;
await app.tick();
assert.equal(doc.elements.code.textContent, '920 136', 'Display follows clock rollback (this is a generator, not a verifier)');
checks += 6;

const originalCrypto = Object.getOwnPropertyDescriptor(globalThis, 'crypto');
try {
  let releaseFirst;
  let firstSignStarted;
  const started = new Promise(resolve => { firstSignStarted = resolve; });
  let signCount = 0;
  Object.defineProperty(globalThis, 'crypto', { configurable: true, value: { subtle: {
    importKey: (...args) => webcrypto.subtle.importKey(...args),
    async sign(...args) {
      if (++signCount === 1) {
        const pause = new Promise(resolve => { releaseFirst = resolve; });
        firstSignStarted();
        await pause;
      }
      return webcrypto.subtle.sign(...args);
    }
  } } });
  let raceNow = 59999;
  const raced = fakeDocument();
  const raceApp = startDemo(raced, () => raceNow, () => 1);
  await started;
  raceNow = 60000;
  await raceApp.tick();
  assert.equal(raced.elements.code.textContent, '119 246');
  releaseFirst();
  await raceApp.ready;
  assert.equal(raced.elements.code.textContent, '119 246', 'A late result from the old minute never replaces the new code');
  checks += 2;

  Object.defineProperty(globalThis, 'crypto', { value: undefined, configurable: true });
  const unsupported = fakeDocument();
  const stopped = startDemo(unsupported, () => 0, () => 1);
  await stopped.ready;
  assert.equal(unsupported.elements.code.textContent, '— — —');
  assert.ok(unsupported.elements.status.classes.has('error'));
  assert.match(unsupported.elements.status.textContent, /Web Crypto/);
  checks += 3;
} finally {
  Object.defineProperty(globalThis, 'crypto', originalCrypto);
}

console.log(`PASS: ${checks} TOTP, time, input and UI checks; CSP and offline resource checks passed.`);
