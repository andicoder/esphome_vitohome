#pragma once
#include "esphome/core/defines.h"

#ifdef USE_TEXT
#include <string>

#include "esphome/components/text/text.h"
#include "esphome/core/component.h"
#include "vito_entity.h"

namespace esphome::vitohome {

// Writable per-day Schaltzeiten (switching-time) program exposed as an ESPHome
// text input. One entity per weekday: it reads its 8-byte block, decodes it to
// the canonical "06:00-22:00 08:30-12:00" string (decode.h::
// decode_schaltzeiten_day) and publishes that as its state; a value typed in
// Home Assistant is parsed back to 8 bytes (encode_schaltzeiten_day, which
// truncates each switch point to the 10-minute grid the device stores) and
// written. Read and write use the same address, so the hub's read-back re-reads
// exactly the bytes just written. No flash preferences: the device is the
// source of truth, restored by the first poll.
class VitoText final : public text::Text, public Component, public VitoEntityBase {
 public:
  void set_read_back(bool v) {
    this->read_back_ = v;
    this->user_read_back_ = v;
  }
  void set_wpr_day(bool v) { this->wpr_day_ = v; }

  void dump_config() override;
  void handle_response(const ResponseView &response) override;
  void handle_write_response(const ResponseView &response) override;
  void handle_error(optolink::OptolinkResult error) override;
  void handle_write_error(optolink::OptolinkResult error) override;
  const char *entity_kind() const override { return "text"; }

 protected:
  void control(const std::string &value) override;

  std::string pending_value_;

  // WPR day program (format: wpr_day): read as one 24-byte block, written
  // period by period -- only the periods that differ from the last read, one
  // 3-byte write each at base+i, then one read-back of the whole day.
  void control_wpr_day_(const std::string &value);
  void write_next_wpr_period_(int from);
  bool wpr_day_{false};
  bool user_read_back_{true};
  bool have_raw_{false};
  uint8_t raw_[24]{};
  uint8_t target_[24]{};
  int write_index_{-1};
};

}  // namespace esphome::vitohome
#endif  // USE_TEXT
