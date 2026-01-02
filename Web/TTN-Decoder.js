function decodeUplink(input) {
  let data = {};
  let warnings = [];
  let errors = [];

  // Calibration constants (kept from your original)
  const CAL_EC_SCALE = 1413 / 1706.4; // ≈ 0.828303

  // Reset reason mapping based on YOUR firmware output
  const resetReasonMap = {
    0x00: "Unknown/None",
    0x01: "PIN Reset",
    0x02: "POR/PDR",
    0x03: "Software Reset",
    0x04: "Independent Watchdog",
    0x05: "Window Watchdog",
    0x06: "Low Power Reset",
    0xFF: "Not set / invalid"
  };

  const port10Map = {
    0x01: "no sensor detected",
    0x02: "sensor 1 failure",
    0x03: "sensor 2 failure",
    0x04: "sensor validation failed",
    0x05: "humidity validation failed"
  };

  function toInt16(u16) {
    return u16 > 32767 ? u16 - 65536 : u16;
  }
  function toTempC(raw16) {
    return toInt16(raw16) / 100.0;
  }
  function toTempC_T1(raw16) {
    return (toInt16(raw16) - 5500) / 100.0;
  }

  function readU16BE(bytes, i) {
    return ((bytes[i] & 0xff) << 8) | (bytes[i + 1] & 0xff);
  }
  function readI16BE(bytes, i) {
    const u = readU16BE(bytes, i);
    return (u & 0x8000) ? u - 0x10000 : u;
  }
  function readU32BE(bytes, i) {
    return (
      ((bytes[i] & 0xff) << 24) |
      ((bytes[i + 1] & 0xff) << 16) |
      ((bytes[i + 2] & 0xff) << 8) |
      (bytes[i + 3] & 0xff)
    ) >>> 0;
  }
  function u32ToHex(u32) {
    return "0x" + (u32 >>> 0).toString(16).toUpperCase().padStart(8, "0");
  }

  try {
    // fPort 2: soil sensor
    if (input.fPort === 2) {
      if (input.bytes.length < 8) {
        errors.push("Payload too short on fPort 2 - expected 8 bytes");
        return { data, warnings, errors };
      }

      const ec_raw  = readU16BE(input.bytes, 2);
      const t_raw   = readU16BE(input.bytes, 4);
      const vwc_raw = readU16BE(input.bytes, 6);

      const temperature = +toTempC(t_raw).toFixed(2);
      const ec_mS_cm = +((ec_raw * CAL_EC_SCALE) / 1000.0).toFixed(2);

      let moisture = +(vwc_raw / 10.0).toFixed(1);
      if (moisture > 100.0) moisture = 100.0;
      if (moisture < 0) moisture = 0.0;

      data = { temperature, moisture, ec: ec_mS_cm, ph: null };
      return { data, warnings, errors };
    }

    // fPort 1: ambient
    if (input.fPort === 1) {
      if (input.bytes.length < 4) {
        errors.push("Payload too short on fPort 1 - expected 4 bytes");
        return { data, warnings, errors };
      }
      const t1_raw = readU16BE(input.bytes, 0);
      const h1_raw = readU16BE(input.bytes, 2);
      data.temperature_c = +toTempC_T1(t1_raw).toFixed(2);
      data.humidity = +(h1_raw / 100.0).toFixed(2);
      return { data, warnings, errors };
    }

    // fPort 9: serial numbers + reset reason
    if (input.fPort === 9) {
      if (input.bytes.length !== 9) {
        errors.push(`Payload length mismatch on fPort 9 - expected 9 bytes, got ${input.bytes.length}`);
        return { data, warnings, errors };
      }

      const serial1 = readU32BE(input.bytes, 0);
      const serial2 = readU32BE(input.bytes, 4);
      const resetReason = input.bytes[8] & 0xff;

      data = {
        serial_1_u32: serial1,
        serial_1_hex: u32ToHex(serial1),
        serial_2_u32: serial2,
        serial_2_hex: u32ToHex(serial2),
        reset_reason: resetReason,
        reset_reason_text: resetReasonMap[resetReason] || `Unknown code 0x${resetReason.toString(16).toUpperCase().padStart(2, "0")}`
      };

      return { data, warnings, errors };
    }

    // fPort 10: error code
    if (input.fPort === 10) {
      if (input.bytes.length < 1) {
        errors.push("Payload too short on fPort 10");
        return { data, warnings, errors };
      }
      const code = input.bytes[0] & 0xff;
      data.error = port10Map[code] || ("unknown error code " + code);
      return { data, warnings, errors };
    }

    // fPort 11: sensors disagree / error detail
    const len = input.bytes.length;
    if (input.fPort === 11) {
      if (len !== 4 && len !== 6 && len !== 8) {
        errors.push(`Unexpected payload length on fPort 11 - got ${len} bytes (expected 4, 6, or 8)`);
        return { data, warnings, errors };
      }

      if (len >= 4) {
        const t1r = readI16BE(input.bytes, 0);
        const t2r = readI16BE(input.bytes, 2);
        data.temperature1_c = +toTempC_T1(t1r).toFixed(2);
        data.temperature2_c = +toTempC(t2r).toFixed(2);
        data.error = "I2C_READ_ERROR_TEMP_MISMATCH";
      }

      if (len === 6) {
        const extra = readU16BE(input.bytes, 4);
        data.extra_raw = extra;
        warnings.push(`fPort 11 included extra 2 bytes: 0x${extra.toString(16).toUpperCase().padStart(4, "0")}`);
      }

      if (len === 8) {
        const t1r = readI16BE(input.bytes, 0);
        const h1r = readU16BE(input.bytes, 2);
        const t2r = readI16BE(input.bytes, 4);
        const h2r = readU16BE(input.bytes, 6);

        data.temperature1_c = +toTempC_T1(t1r).toFixed(2);
        data.humidity1 = +(h1r / 100.0).toFixed(2);
        data.temperature2_c = +toTempC(t2r).toFixed(2);
        data.humidity2 = +(h2r / 100.0).toFixed(2);
        data.error = "SENSOR_DISAGREE_EXTENDED";
      }

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
