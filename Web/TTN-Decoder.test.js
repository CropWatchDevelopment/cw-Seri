// TTN-Decoder.test.js (Jest, no changes to TTN-Decoder.js required)
const fs = require('fs');
const path = require('path');
const vm = require('vm');

// Load TTN-Decoder.js into a sandbox and grab its globals
function loadDecoder() {
  const file = path.join(__dirname, 'TTN-Decoder.js');
  const code = fs.readFileSync(file, 'utf8');

  // Minimal globals available to the decoder if needed
  const sandbox = {
    console,
    setTimeout,
    clearTimeout,
    // add anything else your decoder might use globally
  };

  vm.createContext(sandbox);
  vm.runInContext(code, sandbox, { filename: 'TTN-Decoder.js' });

  if (typeof sandbox.decodeUplink !== 'function') {
    throw new Error('decodeUplink was not defined as a global in TTN-Decoder.js');
  }
  return {
    decodeUplink: sandbox.decodeUplink,
    Decoder: sandbox.Decoder, // optional legacy function
  };
}

const { decodeUplink } = loadDecoder();

// ---------- Helpers for encoding bytes ----------
function encT1(tempC) {
  // raw = (tempC * 100) + 5500, signed int16 big-endian
  let raw = Math.round(tempC * 100) + 5500;
  if (raw < -32768 || raw > 32767) throw new RangeError('T1 raw out of int16 range: ' + raw);
  if (raw < 0) raw = 65536 + raw; // two's complement
  return [(raw >> 8) & 0xff, raw & 0xff];
}

function encT2(tempC) {
  // raw = tempC * 100, signed int16 big-endian
  let raw = Math.round(tempC * 100);
  if (raw < -32768 || raw > 32767) throw new RangeError('T2 raw out of int16 range: ' + raw);
  if (raw < 0) raw = 65536 + raw;
  return [(raw >> 8) & 0xff, raw & 0xff];
}

function encH(hPercent) {
  // Now 2 bytes: 0..10000 -> 0.0..100.0%
  const raw = Math.max(0, Math.min(10000, Math.round(hPercent * 100)));
  return [(raw >> 8) & 0xff, raw & 0xff];
}

// ---------- Tests ----------
describe('decodeUplink fPort=1 temperature sweep (-110..110°C)', () => {
  test('sweeps temperature and validates decoding + warnings range', () => {
    for (let t = -110; t <= 110; t++) {
      const [b0, b1] = encT1(t);
      const h = encH(5.0); // 5.0%
      const res = decodeUplink({ fPort: 1, bytes: [b0, b1, h[0], h[1]] });

      expect(res.errors).toEqual([]);
      expect(res.data).toHaveProperty('temperature_c');
      expect(res.data.temperature_c).toBeCloseTo(t, 2);

      const outOfRange = t < -40 || t > 85;
      const hasTempWarn = res.warnings.some((w) =>
        /Temperature out of typical range/.test(w)
      );
      expect(hasTempWarn).toBe(outOfRange);
    }
  });
});

describe('decodeUplink fPort=1 humidity sweep (0..100% with 2-byte encoding)', () => {
  test('sweeps humidity and validates decoding + warnings', () => {
    const tOK = encT1(20);
    for (let h = 0; h <= 100; h += 5) {  // Test every 5% to keep it reasonable
      const hBytes = encH(h);
      const res = decodeUplink({ fPort: 1, bytes: [tOK[0], tOK[1], hBytes[0], hBytes[1]] });
      expect(res.data.humidity).toBeCloseTo(h, 2);
      const hasWarn = res.warnings.some((w) => /Humidity > 100%/.test(w));
      expect(hasWarn).toBe(h > 100);  // Should warn if over 100%
    }
  });
});

describe('decodeUplink fPort=10 error code mapping', () => {
  test('known codes map; unknown falls back', () => {
    const c01 = decodeUplink({ fPort: 10, bytes: [0x01] });
    expect(c01.data.error).toBe('no sensor detected');

    const c02 = decodeUplink({ fPort: 10, bytes: [0x02] });
    expect(c02.data.error).toBe('sensor 1 failure');

    const c03 = decodeUplink({ fPort: 10, bytes: [0x03] });
    expect(c03.data.error).toBe('sensor 2 failure');

    const c04 = decodeUplink({ fPort: 10, bytes: [0x04] });
    expect(c04.data.error).toBe('sensor validation failed');

    const unk = decodeUplink({ fPort: 10, bytes: [0xee] });
    expect(unk.data.error).toBe('unknown error code: 238');
  });

  test('payload too short error', () => {
    const res = decodeUplink({ fPort: 10, bytes: [] });
    expect(res.errors[0]).toMatch(/Payload too short on fPort 10/);
  });
});

describe('decodeUplink fPort=11 disagree packet', () => {
  test('decodes both sensors + delta; out-of-range flags', () => {
    const t1 = encT1(10.5);
    const t2 = encT2(50.25);
    const h1 = encH(12.3);
    const h2 = encH(0.4);

    const res = decodeUplink({ fPort: 11, bytes: [t1[0], t1[1], h1[0], h1[1], t2[0], t2[1], h2[0], h2[1]] });

    expect(res.data.error).toBe('sensor validation failed');
    expect(res.data.temperature1_c).toBeCloseTo(10.5, 2);
    expect(res.data.temperature2_c).toBeCloseTo(50.25, 2);
    expect(res.data.humidity1).toBeCloseTo(12.3, 2);
    expect(res.data.humidity2).toBeCloseTo(0.4, 2);
    expect(res.data.temp_delta_c).toBeCloseTo(39.75, 2);

    const t1Bad = encT1(-45);
    const t2Bad = encT2(90);
    const res2 = decodeUplink({ fPort: 11, bytes: [t1Bad[0], t1Bad[1], h1[0], h1[1], t2Bad[0], t2Bad[1], h2[0], h2[1]] });
    expect(res2.warnings).toEqual(
      expect.arrayContaining([
        expect.stringMatching(/Sensor1 temperature out of range/),
        expect.stringMatching(/Sensor2 temperature out of range/),
      ])
    );
  });

  test('payload too short error', () => {
    const res = decodeUplink({ fPort: 11, bytes: [0x00] });
    expect(res.errors[0]).toMatch(/Payload too short on fPort 11 - expected 8 bytes/);
  });
});

describe('decodeUplink default-path parity with fPort≠1/10/11', () => {
  test('same math as fPort 1', () => {
    const [b0, b1] = encT1(23.4);
    const hb = encH(2.5);
    const p1 = decodeUplink({ fPort: 1, bytes: [b0, b1, hb[0], hb[1]] });
    const def = decodeUplink({ fPort: 99, bytes: [b0, b1, hb[0], hb[1]] });
    expect(def.data.temperature_c).toBeCloseTo(p1.data.temperature_c, 4);
    expect(def.data.humidity).toBeCloseTo(p1.data.humidity, 4);
  });

  test('default-path short payload error', () => {
    const res = decodeUplink({ fPort: 99, bytes: [0x00, 0x00, 0x00] });
    expect(res.errors[0]).toMatch(/expected at least 4 bytes/);
  });
});
