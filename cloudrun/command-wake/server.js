'use strict';

const http = require('node:http');
const { timingSafeEqual } = require('node:crypto');
const { WebSocket, WebSocketServer } = require('ws');

function createService({ deviceId, deviceKey, notifyKey }) {
  if (!/^[\w-]{1,64}$/.test(deviceId || '') ||
      ![deviceKey, notifyKey].every(key => /^[a-f0-9]{64}$/i.test(key || '')) ||
      deviceKey === notifyKey) {
    throw new Error('Configure DEVICE_ID and two distinct 64-hex-character keys');
  }

  function authorized(req, key) {
    const expected = Buffer.from('Bearer ' + key);
    const actual = Buffer.from(req.headers.authorization || '');
    return actual.length === expected.length && timingSafeEqual(actual, expected);
  }

  let device;
  const ws = new WebSocketServer({ noServer: true, maxPayload: 128 });
  const server = http.createServer((req, res) => {
    req.resume();
    if (req.method === 'GET' && req.url === '/health') return res.writeHead(200).end('ok');
    if (req.method !== 'POST' || req.url !== '/notify') return res.writeHead(404).end();
    if (!authorized(req, notifyKey)) return res.writeHead(401).end();
    if (req.headers['x-device-id'] !== deviceId || device?.readyState !== WebSocket.OPEN) {
      return res.writeHead(404).end();
    }
    device.send('wake', err => res.writeHead(err ? 503 : 204).end());
  });

  server.on('upgrade', (req, socket, head) => {
    socket.on('error', () => socket.destroy());
    if (req.url !== '/device' || req.headers['x-device-id'] !== deviceId ||
        !authorized(req, deviceKey)) {
      socket.end('HTTP/1.1 401 Unauthorized\r\nConnection: close\r\n\r\n');
      return;
    }
    ws.handleUpgrade(req, socket, head, client => {
      device?.terminate();
      device = client;
      client.on('error', () => client.terminate());
      client.on('close', () => { if (device === client) device = undefined; });
      // Only the authenticated cloud function can generate wake notifications.
      client.on('message', () => client.close(1008));
    });
  });
  return server;
}

if (require.main === module) {
  createService({
    deviceId: process.env.DEVICE_ID,
    deviceKey: process.env.DEVICE_WS_KEY,
    notifyKey: process.env.WAKE_NOTIFY_KEY
  }).listen(Number(process.env.PORT || 8080), '0.0.0.0');
}

module.exports = { createService };
