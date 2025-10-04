function decodeUplink(input) {
  var data = {};
  var warnings = [];
  var errors = []; // must remain an array

  // Human-readable error map (used by fPort 10, and referenced for 11)
  var port10Map = {
    0x01: "no sensor detected",
    0x02: "sensor 1 failure",
    0x03: "sensor 2 failure",
    0x04: "sensor validation failed", // sensors disagree
    0x05: "humidity validation failed" // humidity sensors disagree
  };

  // helper(s)
  function toInt16(raw16) {
    return raw16 > 32767 ? raw16 - 65536 : raw16;
  }
  // T2 / legacy temps (no device offset)
  function toTempC(raw16) {
    raw16 = toInt16(raw16);
    return raw16 / 100.0;                      // raw is centi-degrees
  }
  // T1 temps (device adds +5500; remove it here)
  function toTempC_T1(raw16) {
    raw16 = toInt16(raw16);                    // int16
    return (raw16 - 5500) / 100.0;             // compensate +5500 then scale
  }

  try {
    // === Port 1: normal packet: T1(2) + H1(2) ===
    if (input.fPort === 1) {
      data.error = null;

      if (input.bytes.length < 4) {
        errors.push("Payload too short on fPort 1 - expected 4+ bytes");
        return { data, warnings, errors };
      }

      var t1_raw = (input.bytes[0] << 8) | input.bytes[1];
      var h1_raw = (input.bytes[2] << 8) | input.bytes[3];
      data.temperature_c = toTempC_T1(t1_raw);   // T1 uses offset-compensated path
      data.humidity = h1_raw / 100.0;

      if (data.temperature_c < -40 || data.temperature_c > 85) {
        warnings.push("Temperature out of typical range (-40..85°C)");
      }
      if (data.humidity > 100) {
        warnings.push("Humidity > 100%");
      }
      return { data, warnings, errors };
    }

    // === Port 2: soil sensor packet: ε25(2) + EC(2) + Temp(2) + VWC(2) ===
    if (input.fPort === 2) {
      data.error = null;
      data.sensor = "soil";

      if (input.bytes.length < 8) {
        errors.push("Payload too short on fPort 2 - expected 8 bytes");
        return { data, warnings, errors };
      }

      var e25_raw = (input.bytes[0] << 8) | input.bytes[1];
      var ec_raw = (input.bytes[2] << 8) | input.bytes[3];
      var soil_temp_raw = (input.bytes[4] << 8) | input.bytes[5];
      var vwc_raw = (input.bytes[6] << 8) | input.bytes[7];

      var epsilon25 = toInt16(e25_raw) / 100.0;
      var ec = toInt16(ec_raw) / 10.0;          // scaled by 10 -> mS/cm
      var soil_temp_c = toTempC(soil_temp_raw); // scaled by 100 -> °C
      var vwc = toInt16(vwc_raw) / 10.0;        // scaled by 10 -> %

      data.soil_epsilon25 = epsilon25;
      data.soil_ec_mScm = ec;
      data.soil_temperature_c = soil_temp_c;
      data.soil_vwc_percent = vwc;

      if (vwc < 0 || vwc > 100) {
        warnings.push("Soil VWC outside 0-100% range");
      }
      if (soil_temp_c < -40 || soil_temp_c > 85) {
        warnings.push("Soil temperature out of typical range (-40..85°C)");
      }

      return { data, warnings, errors };
    }

    // === Port 10: error-only (first byte is code) ===
    if (input.fPort === 10) {
      if (input.bytes.length < 1) {
        errors.push("Payload too short on fPort 10 - expected ≥1 byte");
        return { data, warnings, errors };
      }
      var code = input.bytes[0] & 0xFF;
      data.error = port10Map[code] || ("unknown error code: " + code);
      return { data, warnings, errors };
    }

    // === Port 11: sensors disagree — carry both sensors ===
    // Layout: T1(2) + H1(2) + T2(2) + H2(2)  => total 8 bytes
    if (input.fPort === 11) {
      if (input.bytes.length < 8) {
        errors.push("Payload too short on fPort 11 - expected 8 bytes");
        return { data, warnings, errors };
      }

      var t1r = (input.bytes[0] << 8) | input.bytes[1];
      var h1r = (input.bytes[2] << 8) | input.bytes[3];
      var t2r = (input.bytes[4] << 8) | input.bytes[5];
      var h2r = (input.bytes[6] << 8) | input.bytes[7];

      var t1c = toTempC_T1(t1r); // T1 with +5500 compensation
      var t2c = toTempC(t2r);    // T2 unchanged

      data.error = port10Map[0x04]; // "sensor validation failed"
      data.temperature1_c = t1c;
      data.humidity1 = h1r / 100.0;
      data.temperature2_c = t2c;
      data.humidity2 = h2r / 100.0;
      data.temp_delta_c = +(Math.abs(t1c - t2c).toFixed(2));

      if (data.humidity1 > 100 || data.humidity2 > 100) warnings.push("Humidity value exceeds 100%");
      if (t1c < -40 || t1c > 85) warnings.push("Sensor1 temperature out of range");
      if (t2c < -40 || t2c > 85) warnings.push("Sensor2 temperature out of range");

      return { data, warnings, errors };
    }

    // === Default: treat like port 1 ===
    if (input.bytes.length < 4) {
      errors.push("Payload too short - expected at least 4 bytes");
      return { data, warnings, errors };
    }
    var temp_raw = (input.bytes[0] << 8) | input.bytes[1];
    var hum_raw = (input.bytes[2] << 8) | input.bytes[3];
    data.temperature_c = toTempC_T1(temp_raw);  // default path mirrors fPort 1 (T1)
    data.humidity = hum_raw / 100.0;
    if (data.temperature_c < -40 || data.temperature_c > 85) {
      warnings.push("Temperature out of typical range (-40..85°C)");
    }
    if (data.humidity > 100) {
      warnings.push("Humidity > 100%");
    }
  } catch (e) {
    errors.push("Error decoding payload: " + e.message);
  }

  return { data, warnings, errors };
}

// Legacy TTN decoder
function Decoder(bytes, port) {
  var result = decodeUplink({ bytes: bytes, fPort: port });
  return result.data;
}
