// Native unit tests for protocol.h/cpp and registers.h
// Compile: g++ -std=c++17 -I../components/waterfurnace -o test_protocol test_protocol.cpp ../components/waterfurnace/protocol.cpp
// Run: ./test_protocol

#include "protocol.h"
#include "registers.h"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>

using namespace esphome::waterfurnace;

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) static void test_##name()
#define RUN(name) do { \
    printf("  %-50s", #name); \
    try { test_##name(); tests_passed++; printf("PASS\n"); } \
    catch (...) { tests_failed++; printf("FAIL\n"); } \
  } while(0)

#define ASSERT_EQ(a, b) do { \
    auto _a = (a); auto _b = (b); \
    if (_a != _b) { \
      printf("FAIL: %s == %s (%d != %d) at line %d\n", #a, #b, (int)_a, (int)_b, __LINE__); \
      throw 1; \
    } \
  } while(0)

#define ASSERT_FLOAT_EQ(a, b, eps) do { \
    float _a = (a); float _b = (b); \
    if (_a - _b > eps || _b - _a > eps) { \
      printf("FAIL: %s == %s (%.4f != %.4f) at line %d\n", #a, #b, _a, _b, __LINE__); \
      throw 1; \
    } \
  } while(0)

#define ASSERT_TRUE(a) do { if (!(a)) { printf("FAIL: %s at line %d\n", #a, __LINE__); throw 1; } } while(0)
#define ASSERT_FALSE(a) do { if (a) { printf("FAIL: !%s at line %d\n", #a, __LINE__); throw 1; } } while(0)

// ====== CRC16 Tests ======

TEST(crc16_known_vector) {
  // Standard ModBus test: slave 1, func 3, addr 0, qty 1 → CRC 0x840A
  uint8_t data[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x01};
  uint16_t result = crc16(data, sizeof(data));
  ASSERT_EQ(result, 0x0A84);  // Note: our crc16 returns native, wire is lo-hi
}

TEST(crc16_func65_request) {
  // Func 65 request: slave 1, func 65, addr 88 (0x0058), qty 4 (0x0004)
  uint8_t data[] = {0x01, 0x41, 0x00, 0x58, 0x00, 0x04};
  uint16_t result = crc16(data, sizeof(data));
  ASSERT_EQ(result, 0xD5BD);
}

TEST(crc16_func66_request) {
  // Func 66 request: slave 1, func 66, regs 745 (0x02E9), 746 (0x02EA)
  uint8_t data[] = {0x01, 0x42, 0x02, 0xE9, 0x02, 0xEA};
  uint16_t result = crc16(data, sizeof(data));
  ASSERT_EQ(result, 0x6629);
}

TEST(crc16_empty) {
  uint8_t data[] = {0x01, 0x41};
  uint16_t result = crc16(data, sizeof(data));
  ASSERT_EQ(result, 0x10C0);
}

// ====== Frame Building Tests ======

TEST(build_read_ranges_basic) {
  // Read register 88, quantity 4
  auto frame = build_read_ranges_request({{88, 4}});
  // Expected: [01][41][00 58][00 04][CRC_lo][CRC_hi]
  ASSERT_EQ(frame.size(), 8u);
  ASSERT_EQ(frame[0], SLAVE_ADDRESS);
  ASSERT_EQ(frame[1], FUNC_READ_RANGES);
  ASSERT_EQ(frame[2], 0x00);  // addr high
  ASSERT_EQ(frame[3], 0x58);  // addr low (88)
  ASSERT_EQ(frame[4], 0x00);  // qty high
  ASSERT_EQ(frame[5], 0x04);  // qty low (4)
  ASSERT_EQ(frame[6], 0xBD);  // CRC lo
  ASSERT_EQ(frame[7], 0xD5);  // CRC hi
}

TEST(build_read_ranges_multiple) {
  auto frame = build_read_ranges_request({{19, 2}, {30, 2}});
  ASSERT_EQ(frame.size(), 12u);  // 2 + 4 + 4 + 2(CRC)
  ASSERT_EQ(frame[0], 0x01);
  ASSERT_EQ(frame[1], FUNC_READ_RANGES);
  // Range 1: addr=19 (0x0013), qty=2
  ASSERT_EQ(frame[2], 0x00);
  ASSERT_EQ(frame[3], 0x13);
  ASSERT_EQ(frame[4], 0x00);
  ASSERT_EQ(frame[5], 0x02);
  // Range 2: addr=30 (0x001E), qty=2
  ASSERT_EQ(frame[6], 0x00);
  ASSERT_EQ(frame[7], 0x1E);
  ASSERT_EQ(frame[8], 0x00);
  ASSERT_EQ(frame[9], 0x02);
  // CRC should be valid
  ASSERT_TRUE(validate_frame_crc(frame.data(), frame.size()));
}

TEST(build_read_registers_request) {
  auto frame = build_read_registers_request({745, 746});
  ASSERT_EQ(frame.size(), 8u);  // 2 + 2*2 + 2(CRC)
  ASSERT_EQ(frame[0], SLAVE_ADDRESS);
  ASSERT_EQ(frame[1], FUNC_READ_REGISTERS);
  ASSERT_EQ(frame[2], 0x02);  // 745 high
  ASSERT_EQ(frame[3], 0xE9);  // 745 low
  ASSERT_EQ(frame[4], 0x02);  // 746 high
  ASSERT_EQ(frame[5], 0xEA);  // 746 low
  ASSERT_TRUE(validate_frame_crc(frame.data(), frame.size()));
}

TEST(build_write_registers_request) {
  // Write register 12619=700, register 12620=730
  auto frame = build_write_registers_request({{12619, 700}, {12620, 730}});
  ASSERT_EQ(frame.size(), 12u);  // 2 + 2*(2+2) + 2(CRC)
  ASSERT_EQ(frame[0], SLAVE_ADDRESS);
  ASSERT_EQ(frame[1], FUNC_WRITE_REGISTERS);
  // Addr 12619 = 0x314B
  ASSERT_EQ(frame[2], 0x31);
  ASSERT_EQ(frame[3], 0x4B);
  // Value 700 = 0x02BC
  ASSERT_EQ(frame[4], 0x02);
  ASSERT_EQ(frame[5], 0xBC);
  ASSERT_TRUE(validate_frame_crc(frame.data(), frame.size()));
}

TEST(build_write_single_request) {
  auto frame = build_write_single_request(400, 1);
  ASSERT_EQ(frame.size(), 8u);
  ASSERT_EQ(frame[0], SLAVE_ADDRESS);
  ASSERT_EQ(frame[1], FUNC_WRITE_SINGLE);
  ASSERT_EQ(frame[2], 0x01);  // 400 high
  ASSERT_EQ(frame[3], 0x90);  // 400 low
  ASSERT_EQ(frame[4], 0x00);  // value high
  ASSERT_EQ(frame[5], 0x01);  // value low
  ASSERT_TRUE(validate_frame_crc(frame.data(), frame.size()));
}

// ====== Frame Validation Tests ======

TEST(validate_frame_crc_valid) {
  auto frame = build_read_ranges_request({{88, 4}});
  ASSERT_TRUE(validate_frame_crc(frame.data(), frame.size()));
}

TEST(validate_frame_crc_invalid) {
  auto frame = build_read_ranges_request({{88, 4}});
  frame[3] ^= 0xFF;  // Corrupt a byte
  ASSERT_FALSE(validate_frame_crc(frame.data(), frame.size()));
}

TEST(validate_frame_crc_too_short) {
  uint8_t data[] = {0x01, 0x03};
  ASSERT_FALSE(validate_frame_crc(data, 2));
}

// ====== Response Parsing Tests ======

TEST(parse_register_values_basic) {
  // Two registers: 0x02BC (700) and 0x02DA (730)
  uint8_t data[] = {0x02, 0xBC, 0x02, 0xDA};
  auto values = parse_register_values(data, 4);
  ASSERT_EQ(values.size(), 2u);
  ASSERT_EQ(values[0], 700u);
  ASSERT_EQ(values[1], 730u);
}

TEST(parse_register_values_empty) {
  auto values = parse_register_values(nullptr, 0);
  ASSERT_EQ(values.size(), 0u);
}

TEST(parse_register_values_odd_bytes) {
  // 3 bytes = 1 register (last byte ignored)
  uint8_t data[] = {0x02, 0xBC, 0xFF};
  auto values = parse_register_values(data, 3);
  ASSERT_EQ(values.size(), 1u);
  ASSERT_EQ(values[0], 700u);
}

TEST(is_error_response_true) {
  ASSERT_TRUE(is_error_response(0xC1));   // 0x41 | 0x80
  ASSERT_TRUE(is_error_response(0xC2));   // 0x42 | 0x80
  ASSERT_TRUE(is_error_response(0x83));   // 0x03 | 0x80
}

TEST(is_error_response_false) {
  ASSERT_FALSE(is_error_response(0x41));  // func 65
  ASSERT_FALSE(is_error_response(0x42));  // func 66
  ASSERT_FALSE(is_error_response(0x03));  // func 3
}

// ====== Register Type Conversion Tests ======

TEST(convert_unsigned) {
  ASSERT_FLOAT_EQ(convert_register(240, RegisterType::UNSIGNED), 240.0f, 0.001f);
  ASSERT_FLOAT_EQ(convert_register(65535, RegisterType::UNSIGNED), 65535.0f, 1.0f);
}

TEST(convert_signed) {
  ASSERT_FLOAT_EQ(convert_register(0xFFFF, RegisterType::SIGNED), -1.0f, 0.001f);
  ASSERT_FLOAT_EQ(convert_register(0xFF9C, RegisterType::SIGNED), -100.0f, 0.001f);
  ASSERT_FLOAT_EQ(convert_register(100, RegisterType::SIGNED), 100.0f, 0.001f);
}

TEST(convert_tenths) {
  ASSERT_FLOAT_EQ(convert_register(700, RegisterType::TENTHS), 70.0f, 0.001f);
  ASSERT_FLOAT_EQ(convert_register(735, RegisterType::TENTHS), 73.5f, 0.001f);
}

TEST(convert_signed_tenths) {
  // 70.0°F = 700
  ASSERT_FLOAT_EQ(convert_register(700, RegisterType::SIGNED_TENTHS), 70.0f, 0.001f);
  // -10.5°F = 0xFF97 = 65431 unsigned, -105 signed → -10.5
  uint16_t neg = static_cast<uint16_t>(static_cast<int16_t>(-105));
  ASSERT_FLOAT_EQ(convert_register(neg, RegisterType::SIGNED_TENTHS), -10.5f, 0.001f);
}

TEST(convert_hundredths) {
  ASSERT_FLOAT_EQ(convert_register(705, RegisterType::HUNDREDTHS), 7.05f, 0.001f);
  ASSERT_FLOAT_EQ(convert_register(200, RegisterType::HUNDREDTHS), 2.0f, 0.001f);
}

TEST(convert_boolean) {
  ASSERT_FLOAT_EQ(convert_register(0, RegisterType::BOOLEAN), 0.0f, 0.001f);
  ASSERT_FLOAT_EQ(convert_register(1, RegisterType::BOOLEAN), 1.0f, 0.001f);
  ASSERT_FLOAT_EQ(convert_register(42, RegisterType::BOOLEAN), 1.0f, 0.001f);
}

// ====== 32-bit Register Tests ======

TEST(to_uint32_basic) {
  ASSERT_EQ(to_uint32(0x0001, 0x0000), 0x00010000u);
  ASSERT_EQ(to_uint32(0, 500), 500u);
  ASSERT_EQ(to_uint32(1, 500), 65536u + 500u);
}

TEST(to_int32_negative) {
  // -1 as int32 = 0xFFFFFFFF
  ASSERT_EQ(to_int32(0xFFFF, 0xFFFF), -1);
  // -1000 as int32
  int32_t val = -1000;
  uint32_t uval = static_cast<uint32_t>(val);
  ASSERT_EQ(to_int32(uval >> 16, uval & 0xFFFF), -1000);
}

// ====== IZ2 Zone Bit Extraction Tests ======

TEST(iz2_extract_fan_mode_auto) {
  // Neither bit 7 nor bit 8 set
  ASSERT_EQ(iz2_extract_fan_mode(0x0000), FAN_AUTO);
  ASSERT_EQ(iz2_extract_fan_mode(0x0001), FAN_AUTO);  // Only carry bit
}

TEST(iz2_extract_fan_mode_continuous) {
  // Bit 7 (0x80) set
  ASSERT_EQ(iz2_extract_fan_mode(0x0080), FAN_CONTINUOUS);
  ASSERT_EQ(iz2_extract_fan_mode(0x00FF), FAN_CONTINUOUS);  // Bit 7 has priority
}

TEST(iz2_extract_fan_mode_intermittent) {
  // Bit 8 (0x100) set, bit 7 not set
  ASSERT_EQ(iz2_extract_fan_mode(0x0100), FAN_INTERMITTENT);
  ASSERT_EQ(iz2_extract_fan_mode(0x0101), FAN_INTERMITTENT);
}

TEST(iz2_extract_cooling_setpoint) {
  // Bits 1-6: ((value & 0x7E) >> 1) + 36
  // Cooling SP = 75°F: 75 - 36 = 39, shifted left: 39 << 1 = 78 = 0x4E
  uint16_t config1 = 0x4E;  // bits 1-6 = 39
  ASSERT_EQ(iz2_extract_cooling_setpoint(config1), 75u);

  // Cooling SP = 36°F (minimum): 36 - 36 = 0
  ASSERT_EQ(iz2_extract_cooling_setpoint(0x0000), 36u);

  // Cooling SP = 99°F: 99 - 36 = 63, 63 << 1 = 126 = 0x7E
  ASSERT_EQ(iz2_extract_cooling_setpoint(0x007E), 99u);
}

TEST(iz2_extract_heating_setpoint) {
  // Heating SP = 68°F: 68 - 36 = 32
  // 32 = 0b100000: carry (bit 0 of config1) = 1, upper (bits 11-15 of config2) = 0b00000
  // Actually: carry=1, upper=0 → (1 << 5) | 0 = 32 → 32 + 36 = 68
  uint16_t config1 = 0x0001;  // carry bit = 1
  uint16_t config2 = 0x0000;  // upper bits = 0
  ASSERT_EQ(iz2_extract_heating_setpoint(config1, config2), 68u);

  // Heating SP = 72°F: 72 - 36 = 36 = 0b100100
  // carry = 0, upper = 0b10010 = 18, (0 << 5) | 18 = 18... that's wrong
  // Wait: 36 = (carry << 5) | upper + 36? No: result = ((carry << 5) | upper) + 36
  // 36 - 36 = 0 ... no.  72 - 36 = 36
  // 36 = 0b100100: carry = bit 0 = 0, bits 11-15 from config2 = 0b10010 = 18
  // (0 << 5) | 18 = 18 ≠ 36.
  // Actually the carry is the MSB: carry << 5 = bit 5
  // 36 = 0b100100: bit 5 = 1 (carry=1), bits 4-0 = 00100 = 4
  // (1 << 5) | 4 = 36 → 36 + 36 = 72 ✓
  // config1 bit 0 = 1, config2 bits 15-11 = 00100 = 4 → shifted: 4 << 11 = 0x2000
  config1 = 0x0001;
  config2 = 0x2000;  // bits 15-11 = 00100 = 4
  ASSERT_EQ(iz2_extract_heating_setpoint(config1, config2), 72u);
}

TEST(iz2_extract_mode) {
  ASSERT_EQ(iz2_extract_mode(0x0000), MODE_OFF);
  ASSERT_EQ(iz2_extract_mode(0x0100), MODE_AUTO);
  ASSERT_EQ(iz2_extract_mode(0x0200), MODE_COOL);
  ASSERT_EQ(iz2_extract_mode(0x0300), MODE_HEAT);
}

TEST(iz2_damper_open) {
  ASSERT_TRUE(iz2_damper_open(0x0010));
  ASSERT_FALSE(iz2_damper_open(0x0000));
  ASSERT_FALSE(iz2_damper_open(0x0020));
}

// ====== Fault Code Tests ======

TEST(fault_code_to_string_known) {
  ASSERT_TRUE(strcmp(fault_code_to_string(1), "Input Error") == 0);
  ASSERT_TRUE(strcmp(fault_code_to_string(2), "High Pressure") == 0);
  ASSERT_TRUE(strcmp(fault_code_to_string(99), "System Reset") == 0);
}

TEST(fault_code_to_string_unknown) {
  ASSERT_TRUE(strcmp(fault_code_to_string(50), "Unknown Fault") == 0);
  ASSERT_TRUE(strcmp(fault_code_to_string(0), "Unknown Fault") == 0);
}

// ====== Register Group Tests ======

TEST(system_id_ranges_count) {
  auto ranges = get_system_id_ranges();
  size_t total = 0;
  for (const auto &r : ranges) total += r.second;
  // 1 + 4 + 12 + 5 + 2 + 1 + 2 = 27 registers
  ASSERT_EQ(total, 27u);
}

TEST(component_detect_ranges_count) {
  auto ranges = get_component_detect_ranges();
  size_t total = 0;
  for (const auto &r : ranges) total += r.second;
  // 3+3+3+3+3+3+3+1 = 22
  ASSERT_EQ(total, 22u);
}

TEST(iz2_ranges_for_zones) {
  auto ranges = get_iz2_ranges(3);
  ASSERT_EQ(ranges.size(), 2u);
  ASSERT_EQ(ranges[0].first, 31007u);
  ASSERT_EQ(ranges[0].second, 9u);   // 3 zones * 3 regs
  ASSERT_EQ(ranges[1].first, 31200u);
  ASSERT_EQ(ranges[1].second, 9u);
}

TEST(iz2_ranges_zero_zones) {
  auto ranges = get_iz2_ranges(0);
  ASSERT_EQ(ranges.size(), 0u);
}

// ====== Main ======

int main() {
  printf("Protocol and Register Unit Tests\n");
  printf("================================\n\n");

  printf("CRC16:\n");
  RUN(crc16_known_vector);
  RUN(crc16_func65_request);
  RUN(crc16_func66_request);
  RUN(crc16_empty);

  printf("\nFrame Building:\n");
  RUN(build_read_ranges_basic);
  RUN(build_read_ranges_multiple);
  RUN(build_read_registers_request);
  RUN(build_write_registers_request);
  RUN(build_write_single_request);

  printf("\nFrame Validation:\n");
  RUN(validate_frame_crc_valid);
  RUN(validate_frame_crc_invalid);
  RUN(validate_frame_crc_too_short);

  printf("\nResponse Parsing:\n");
  RUN(parse_register_values_basic);
  RUN(parse_register_values_empty);
  RUN(parse_register_values_odd_bytes);
  RUN(is_error_response_true);
  RUN(is_error_response_false);

  printf("\nRegister Type Conversions:\n");
  RUN(convert_unsigned);
  RUN(convert_signed);
  RUN(convert_tenths);
  RUN(convert_signed_tenths);
  RUN(convert_hundredths);
  RUN(convert_boolean);

  printf("\n32-bit Registers:\n");
  RUN(to_uint32_basic);
  RUN(to_int32_negative);

  printf("\nIZ2 Zone Extraction:\n");
  RUN(iz2_extract_fan_mode_auto);
  RUN(iz2_extract_fan_mode_continuous);
  RUN(iz2_extract_fan_mode_intermittent);
  RUN(iz2_extract_cooling_setpoint);
  RUN(iz2_extract_heating_setpoint);
  RUN(iz2_extract_mode);
  RUN(iz2_damper_open);

  printf("\nFault Codes:\n");
  RUN(fault_code_to_string_known);
  RUN(fault_code_to_string_unknown);

  printf("\nRegister Groups:\n");
  RUN(system_id_ranges_count);
  RUN(component_detect_ranges_count);
  RUN(iz2_ranges_for_zones);
  RUN(iz2_ranges_zero_zones);

  printf("\n================================\n");
  printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
  return tests_failed > 0 ? 1 : 0;
}
