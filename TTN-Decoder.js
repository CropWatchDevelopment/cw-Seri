function decodeUplink(input) {
  var data = {};
  var warnings = [];
  var errors = [];

  // Check if we have at least 3 bytes
  if (input.bytes.length < 3) {
    errors.push("Payload too short - expected at least 3 bytes");
    return {
      data: data,
      warnings: warnings,
      errors: errors
    };
  }

  try {
    // Temperature: 16-bit signed integer (big-endian)
    // Assuming this is temperature * 100 (i.e., 2345 = 23.45°C)
    var temp_raw = (input.bytes[0] << 8) | input.bytes[1];
    
    // Convert from unsigned to signed 16-bit
    if (temp_raw > 32767) {
      temp_raw = temp_raw - 65536;
    }
    
    // Convert to actual temperature (assuming factor of 100)
    data.temperature_c = temp_raw / 100.0;
    
    // Humidity: 8-bit unsigned integer (0-100%)
    data.humidity = input.bytes[2];
    
    // Validation checks
    if (data.temperature_c < -40 || data.temperature_c > 85) {
      warnings.push("Temperature out of typical sensor range (-40 to 85°C)");
    }
    
    if (data.humidity > 100) {
      warnings.push("Humidity value exceeds 100%");
    }

  } catch (error) {
    errors.push("Error decoding payload: " + error.message);
  }

  return {
    data: data,
    warnings: warnings,
    errors: errors
  };
}

// Legacy decoder function (for older TTN versions)
function Decoder(bytes, port) {
  var result = decodeUplink({bytes: bytes, fPort: port});
  return result.data;
}
