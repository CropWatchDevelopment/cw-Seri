function decodeUplink(input) {
  var data = {};
  var warnings = [];
  var errors = []; // must remain an array

  // Human-readable error map (used by fPort 10, and referenced for 11)
  var port10Map = {
    0x01: "no sensor detected",
    0x02: "sensor 1 failure",
    0x03: "sensor 2 failure",
    0x04: "sensor validation failed" // sensors disagree
  };

  // helper(s)
  // T2 / legacy temps (no device offset)
  function toTempC(raw16) {
    if (raw16 > 32767) raw16 -= 65536;         // int16
    return raw16 / 100.0;                      // raw is centi-degrees
  }
  // T1 temps (device adds +5500; remove it here)
  function toTempC_T1(raw16) {
    if (raw16 > 32767) raw16 -= 65536;         // int16
    return (raw16 - 5500) / 100.0;             // compensate +5500 then scale
  }

  try {
    // === Port 1: normal packet: T1(2) + H1(1) ===
    if (input.fPort === 1) {
      data.error = null;

      if (input.bytes.length < 3) {
        errors.push("Payload too short on fPort 1 - expected 3+ bytes");
        return { data, warnings, errors };
      }

      var t1_raw = (input.bytes[0] << 8) | input.bytes[1];
      data.temperature_c = toTempC_T1(t1_raw);   // T1 uses offset-compensated path
      data.humidity = input.bytes[2] / 10.0;

      if (data.temperature_c < -40 || data.temperature_c > 85) {
        warnings.push("Temperature out of typical range (-40..85°C)");
      }
      if (data.humidity > 100) {
        warnings.push("Humidity > 100%");
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
    // Layout: T1(2) + H1(1) + T2(2) + H2(1)  => total 6 bytes
    if (input.fPort === 11) {
      if (input.bytes.length < 6) {
        errors.push("Payload too short on fPort 11 - expected 6 bytes");
        return { data, warnings, errors };
      }

      var t1r = (input.bytes[0] << 8) | input.bytes[1];
      var h1  = input.bytes[2];
      var t2r = (input.bytes[3] << 8) | input.bytes[4];
      var h2  = input.bytes[5];

      var t1c = toTempC_T1(t1r); // T1 with +5500 compensation
      var t2c = toTempC(t2r);    // T2 unchanged

      data.error = port10Map[0x04]; // "sensor validation failed"
      data.temperature1_c = t1c;
      data.humidity1 = h1 / 10.0;
      data.temperature2_c = t2c;
      data.humidity2 = h2 / 10.0;
      data.temp_delta_c = +(Math.abs(t1c - t2c).toFixed(2));

      if (data.humidity1 > 100 || data.humidity2 > 100) warnings.push("Humidity value exceeds 100%");
      if (t1c < -40 || t1c > 85) warnings.push("Sensor1 temperature out of range");
      if (t2c < -40 || t2c > 85) warnings.push("Sensor2 temperature out of range");

      return { data, warnings, errors };
    }

    // === Default: treat like port 1 ===
    if (input.bytes.length < 3) {
      errors.push("Payload too short - expected at least 3 bytes");
      return { data, warnings, errors };
    }
    var temp_raw = (input.bytes[0] << 8) | input.bytes[1];
    data.temperature_c = toTempC_T1(temp_raw);  // default path mirrors fPort 1 (T1)
    data.humidity = input.bytes[2] / 10.0;
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
