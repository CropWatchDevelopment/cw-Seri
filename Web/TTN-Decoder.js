function decodeUplink(input) {
  var data = {
    error: null // keep explicit for consistency
  };
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

  // Reset reason map for fPort 9 byte 8
  var resetReasonMap = {
    0x01: "PIN Reset (NRST pin)",
    0x02: "POR/PDR (Power On Reset)",
    0x03: "Software Reset",
    0x04: "Independent Watchdog (IWDG)",
    0x05: "Window Watchdog (WWDG)",
    0x06: "Low Power Reset",
    0x00: "Unknown/None"
  };

  // helper(s)
  // T2 / legacy temps (no device offset)
  function toTempC(raw16) {
    if (raw16 > 32767) raw16 -= 65536; // int16
    return raw16 / 100.0;              // raw is centi-degrees
  }
  // T1 temps (device adds +5500; remove it here)
  function toTempC_T1(raw16) {
    if (raw16 > 32767) raw16 -= 65536; // int16
    return (raw16 - 5500) / 100.0;     // compensate +5500 then scale
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
      data.temperature_c = toTempC_T1(t1_raw); // T1 uses offset-compensated path
      data.humidity = h1_raw / 100.0;

      if (data.temperature_c < -40 || data.temperature_c > 85) {
        warnings.push("Temperature out of typical range (-40..85°C)");
      }
      if (data.humidity > 100) {
        warnings.push("Humidity > 100%");
      }
      return { data, warnings, errors };
    }

    // === Port 9: Sensor Serials + Reset Reason ===
    // Layout (9 bytes total):
    // 0-3: Sensor 1 Serial
    // 4-7: Sensor 2 Serial
    // 8:   Reset Reason Code
    if (input.fPort === 9) {
      data.error = null;

      if (input.bytes.length < 9) {
        errors.push("Payload too short on fPort 9 - expected 9 bytes");
        return { data, warnings, errors };
      }

      // Manual reconstruction to avoid 32-bit signed overflow from bitwise ops
      var s1 = 0;
      s1 += input.bytes[0] * 16777216; // 2^24
      s1 += input.bytes[1] * 65536;    // 2^16
      s1 += input.bytes[2] * 256;      // 2^8
      s1 += input.bytes[3];

      var s2 = 0;
      s2 += input.bytes[4] * 16777216;
      s2 += input.bytes[5] * 65536;
      s2 += input.bytes[6] * 256;
      s2 += input.bytes[7];

      var resetCode = input.bytes[8] & 0xFF;

      data.sensor1_serial = s1;
      data.sensor2_serial = s2;
      data.reset_reason_code = resetCode;
      data.reset_reason =
        resetReasonMap.hasOwnProperty(resetCode)
          ? resetReasonMap[resetCode]
          : "Unknown reset reason";

      return { data, warnings, errors };
    }

    // === Port 10: error-only (first byte is code) ===
    if (input.fPort === 10) {
      if (input.bytes.length < 1) {
        errors.push("Payload too short on fPort 10 - expected ≥1 byte");
        return { data, warnings, errors };
      }
      var code10 = input.bytes[0] & 0xFF;
      data.error = port10Map[code10] || ("unknown error code: " + code10);
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

      if (data.humidity1 > 100 || data.humidity2 > 100) {
        warnings.push("Humidity value exceeds 100%");
      }
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
    data.temperature_c = toTempC_T1(temp_raw); // default path mirrors fPort 1 (T1)
    data.humidity = hum_raw / 100.0;

    if (data.temperature_c < -40 || data.temperature_c > 85) {
      warnings.push("Temperature out of typical range (-40..85°C)");
    }
    if (data.humidity > 100) {
      warnings.push("Humidity > 100%");
    }
  } catch (e) {
    errors.push("Error decoding payload: " + (e && e.message ? e.message : e));
  }

  return { data, warnings, errors };
}

// Legacy TTN decoder
function Decoder(bytes, port) {
  var result = decodeUplink({ bytes: bytes, fPort: port });
  return result.data;
}
