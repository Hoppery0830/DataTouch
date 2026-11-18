'use strict';

const test = require('node:test');
const assert = require('node:assert/strict');
const { once } = require('node:events');
const { WebSocket } = require('ws');
const { createService } = require('./server');

test('only the matching device receives an authenticated wake; offline devices fall back to poll', async t => {
  const deviceKey = 'a'.repeat(64);
  const notifyKey = 'b'.repeat(64);
  const deviceId = 'esp32-dev01';
  const server = createService({ deviceId, deviceKey, notifyKey });
  server.listen(0, '127.0.0.1');
  await once(server, 'listening');
  t.after(() => server.close());
  const host = '127.0.0.1:' + server.address().port;
  const headers = (key, id) => ({ authorization: 'Bearer ' + key, 'x-device-id': id });
  const notify = async (key = notifyKey, id = deviceId) => (await fetch('http://' + host + '/notify', {
    method: 'POST', headers: headers(key, id)
  })).status;
  const connect = key => new WebSocket('ws://' + host + '/device', { headers: headers(key, deviceId) });
  assert.equal(await notify(), 404);
  const [err] = await once(connect(notifyKey), 'error');
  assert.match(err.message, /401/);
  const device = connect(deviceKey);
  t.after(() => device.terminate());
  await once(device, 'open');
  assert.equal(await notify(deviceKey), 401);
  assert.equal(await notify(notifyKey, 'other'), 404);
  const message = once(device, 'message');
  assert.equal(await notify(), 204);
  assert.equal((await message)[0].toString(), 'wake');
  device.close();
  await once(device, 'close');
  assert.equal(await notify(), 404);
});
