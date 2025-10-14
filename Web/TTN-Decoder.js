function decodeUplink(input) {
  var data = {};
  var warnings = [];
  var errors = [];

  // Calibration: 1413 µS/cm reference / your mean 1706.4
  const CAL_EC_SCALE = 1413 / 1706.4; // ≈ 0.828303

  const port10Map = {
    0x01: "no sensor detected",
    0x02: "sensor 1 failure",
    0x03: "sensor 2 failure",
    0x04: "sensor validation failed",
    0x05: "humidity validation failed"
  };

  function toInt16(u16) { return u16 > 32767 ? u16 - 65536 : u16; }
  function toTempC(raw16) { return toInt16(raw16) / 100.0; }
  function toTempC_T1(raw16) { return (toInt16(raw16) - 5500) / 100.0; }

  try {
    // === fPort 2: soil sensor ===
    if (input.fPort === 2) {
      if (input.bytes.length < 8) {
        errors.push("Payload too short on fPort 2 - expected 8 bytes");
        return { data, warnings, errors };
      }

      const e25_raw = (input.bytes[0] << 8) | input.bytes[1]; // ε25 * 100 (unused)
      const ec_raw  = (input.bytes[2] << 8) | input.bytes[3]; // µS/cm (already)
      const t_raw   = (input.bytes[4] << 8) | input.bytes[5]; // °C * 100
      const vwc_raw = (input.bytes[6] << 8) | input.bytes[7]; // % * 10

      const temperature = +toTempC(t_raw).toFixed(2);

      // EC: µS/cm -> apply calibration -> mS/cm
      const ec_mS_cm = +((ec_raw * CAL_EC_SCALE) / 1000.0).toFixed(2);

      // Moisture: % with 1 decimal, clamp to 100.0
      let moisture = +(vwc_raw / 10.0).toFixed(1);
      if (moisture > 100.0) moisture = 100.0;
      if (moisture < 0) moisture = 0.0;

      data = {
        temperature: temperature,
        moisture: moisture,
        ec: ec_mS_cm, // mS/cm
        ph: null
      };

      return { data, warnings, errors };
    }

    // === fPort 1: ambient ===
    if (input.fPort === 1) {
      if (input.bytes.length < 4) {
        errors.push("Payload too short on fPort 1 - expected 4 bytes");
        return { data, warnings, errors };
      }
      const t1_raw = (input.bytes[0] << 8) | input.bytes[1];
      const h1_raw = (input.bytes[2] << 8) | input.bytes[3];
      data.temperature_c = toTempC_T1(t1_raw);
      data.humidity = h1_raw / 100.0;
      return { data, warnings, errors };
    }

    // === fPort 10: error code ===
    if (input.fPort === 10) {
      if (input.bytes.length < 1) {
        errors.push("Payload too short on fPort 10");
        return { data, warnings, errors };
      }
      const code = input.bytes[0] & 0xff;
      data.error = port10Map[code] || ("unknown error code " + code);
      return { data, warnings, errors };
    }

    // === fPort 11: sensors disagree (for completeness) ===
    if (input.fPort === 11) {
      if (input.bytes.length < 8) {
        errors.push("Payload too short on fPort 11 - expected 8 bytes");
        return { data, warnings, errors };
      }
      const t1r = (input.bytes[0] << 8) | input.bytes[1];
      const h1r = (input.bytes[2] << 8) | input.bytes[3];
      const t2r = (input.bytes[4] << 8) | input.bytes[5];
      const h2r = (input.bytes[6] << 8) | input.bytes[7];
      data.error = port10Map[0x04];
      data.temperature1_c = toTempC_T1(t1r);
      data.humidity1 = h1r / 100.0;
      data.temperature2_c = toTempC(t2r);
      data.humidity2 = h2r / 100.0;
      return { data, warnings, errors };
    }

  } catch (e) {
    errors.push("Error decoding payload: " + e.message);
  }

  return { data, warnings, errors };
}

// TTN legacy wrapper
function Decoder(bytes, port) {
  const result = decodeUplink({ bytes, fPort: port });
  return result.data;
}

