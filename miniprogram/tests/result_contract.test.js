// Cross-device wire result names must survive the ESP32 queue and cloud whitelist.
const {test} = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
test('ESP32 result queue preserves every remote result accepted by the cloud', () => {
  const root = path.resolve(__dirname, '../..');
  const header = fs.readFileSync(path.join(root, 'ESP32/src/cloud_command.h'), 'utf8');
  const source = fs.readFileSync(path.join(root, 'ESP32/src/remote_link.cpp'), 'utf8');
  const cloud = fs.readFileSync(path.join(root, 'miniprogram/cloudfunctions/deviceCommand/index.js'), 'utf8');
  const capacity = Number(header.match(/#define CLOUD_CMD_RESULT_LEN\s+(\d+)/)[1]);
  const names = [...source.matchAll(/return "([a-z_]+)";/g)].map(m => m[1]);
  const allowed = JSON.parse(cloud.match(/const knownResults = (\[[^;]+\]);/)[1]);
  assert(names.includes('capture_timeout'));
  for (const name of names) {
    assert(Buffer.byteLength(name) + 1 <= capacity, `${name} gets truncated`);
    assert(allowed.includes(name), `${name} is rejected by the cloud`);
  }
});
