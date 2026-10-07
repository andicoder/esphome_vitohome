// P300 Remote_Procedure_Call (function code 0x07): how the WPR heat-pump
// controllers serve their fault history ('WPRError' at 0xA801, 30 entries of
// 8 bytes). A plain read of 0xA801 is refused; each entry is one RPC whose
// one-byte parameter is the entry index, and the answer carries the entry.
//
// Wire format, as gismo2004/optov (custom_components/optov/optolink.py,
// read_rpc) sends it to a Vitotronic 200 WO1A:
//   41 | len | 00 | 07 | addrHi addrLo | DataLength = len(param) | param.. | cs
// Unlike a READ, the length byte counts the OUTGOING parameter; the device
// decides how much it returns.

#include <cstdio>
#include <cstring>
#include <numeric>
#include <vector>

#include "fake_optolink.h"
#include "protocol_select.h"

using namespace esphome::vitohome;
using Engine = optolink::OptolinkEngine<SelectedProtocol>;  // P300: no protocol flag

namespace {
int g_responses = 0;
int g_errors = 0;
uint8_t g_last_payload[16] = {0};
uint8_t g_last_len = 0;
uint16_t g_last_addr = 0;
}  // namespace

static void pump(Engine &a, int n = 8) {
  for (int i = 0; i < n; ++i)
    a.loop();
}

static std::vector<uint8_t> frame(std::vector<uint8_t> body) {
  uint8_t cs = std::accumulate(body.begin(), body.end(), static_cast<uint8_t>(0));
  std::vector<uint8_t> out = {0x41};
  out.insert(out.end(), body.begin(), body.end());
  out.push_back(cs);
  return out;
}

static void handshake(Engine &a, FakeOptolink &u) {
  pump(a);
  u.feed({0x05});
  pump(a);
  u.feed({0x06});
  pump(a);
  u.clear_written();
}

int main() {
  optolink::optolink_test_clock_freeze();

  FakeOptolink uart;
  Engine adapter(&uart);
  adapter.onResponse([](const uint8_t *data, uint8_t length, uint16_t address) {
    g_responses++;
    g_last_len = length;
    g_last_addr = address;
    if (data != nullptr && length <= sizeof(g_last_payload))
      std::memcpy(g_last_payload, data, length);
  });
  adapter.onError([](optolink::OptolinkResult, uint16_t) { g_errors++; });
  adapter.begin();
  handshake(adapter, uart);

  int failures = 0;
  auto check = [&failures](bool ok, const char *what) {
    std::printf("  %-56s %s\n", what, ok ? "ok" : "FAIL");
    if (!ok)
      failures++;
  };

  // --- A. the request frame -------------------------------------------------
  const uint8_t index = 0x03;
  check(adapter.rpc(0xA801, &index, 1), "A: rpc accepted");
  pump(adapter);
  const std::vector<uint8_t> want = frame({0x06, 0x00, 0x07, 0xA8, 0x01, 0x01, 0x03});
  check(uart.written() == want, "A: 41 06 00 07 A8 01 01 03 cs on the wire");
  check(!adapter.rpc(0xA801, &index, 1), "A: a second call while busy is refused");

  // --- B. the 8-byte answer to a 1-byte parameter is delivered -------------
  uart.feed({0x06});
  pump(adapter);
  // Entry 3: index 03, Unix time 0x68B4_6A10 LE, fault code A9, two flags.
  uart.feed(frame({0x0D, 0x01, 0x07, 0xA8, 0x01, 0x08, 0x03, 0x10, 0x6A, 0xB4, 0x68, 0xA9, 0x00, 0x00}));
  pump(adapter);
  const uint8_t entry[8] = {0x03, 0x10, 0x6A, 0xB4, 0x68, 0xA9, 0x00, 0x00};
  check(g_responses == 1 && g_errors == 0, "B: one response, no error");
  check(g_last_len == 8 && std::memcmp(g_last_payload, entry, 8) == 0, "B: all 8 entry bytes delivered");
  check(g_last_addr == 0xA801, "B: response echoes 0xA801");
  check(!adapter.isBusy(), "B: engine free afterwards");

  // --- C. guards ------------------------------------------------------------
  uart.clear_written();
  check(!adapter.rpc(0xA801, nullptr, 1), "C: null parameter refused");
  check(!adapter.rpc(0xA801, &index, 0), "C: empty parameter refused");
  check(uart.written().empty(), "C: nothing sent for a refused call");

  std::printf("vs2 rpc: %s\n", failures == 0 ? "all checks passed" : "FAILURES");
  return failures == 0 ? 0 : 1;
}
