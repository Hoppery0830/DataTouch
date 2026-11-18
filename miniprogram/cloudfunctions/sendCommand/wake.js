'use strict';

const https = require('https');

// Delivery is optional: commands already exist in the durable poll queue.
async function notifyDevice(deviceId) {
  const url = process.env.WAKE_NOTIFY_URL;
  const key = process.env.WAKE_NOTIFY_KEY;
  if (!url || !key) return;
  try {
    await new Promise((resolve, reject) => {
      const request = https.request(url, {
        method: 'POST',
        headers: { Authorization: 'Bearer ' + key, 'X-Device-Id': deviceId }
      }, res => {
        res.resume();
        if (res.statusCode === 204) resolve();
        else reject(new Error('HTTP ' + res.statusCode));
      });
      const timeout = setTimeout(() => request.destroy(new Error('timeout')), 1000);
      request.on('close', () => clearTimeout(timeout));
      request.on('error', reject);
      request.end();
    });
  } catch (err) {
    console.warn('[sendCommand] Wake unavailable; polling will deliver:', err.code || err.message);
  }
}

module.exports = { notifyDevice };
