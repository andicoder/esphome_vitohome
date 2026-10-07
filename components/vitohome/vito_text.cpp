#include "vito_text.h"
#ifdef USE_TEXT

#include "decode.h"
#include "esphome/core/log.h"
#include "vitohome.h"

namespace esphome::vitohome {

static const char *const TAG = "vitohome.text";

// A per-day program is 8 bytes; the canonical string is at most
// "HH:MM-HH:MM" x4 + 3 spaces = 47 chars, +NUL.
static constexpr uint8_t SCHALTZEITEN_LEN = 8;

void VitoText::dump_config() {
  LOG_TEXT("  ", "Text", this);
  ESP_LOGCONFIG(TAG, "    Type: %s  Address: 0x%04X  Length: %u  read_back: %s",
                this->wpr_day_ ? "wpr_day" : "schaltzeiten", this->datapoint_.address(), this->datapoint_.length(),
                this->read_back_ ? "yes" : "no");
}

void VitoText::control(const std::string &value) {
  if (this->wpr_day_) {
    this->control_wpr_day_(value);
    return;
  }
  uint8_t buf[SCHALTZEITEN_LEN];
  if (!encode_schaltzeiten_day(value.c_str(), buf)) {
    // Unparseable program (bad time, too many pairs, ...). Refuse to transmit
    // rather than write a partial/wrong schedule; the device keeps its value.
    ESP_LOGE(TAG, "%s: '%s' is not a valid switching-time program - not written", this->datapoint_.name(),
             value.c_str());
    return;
  }
  if (!this->set_write_payload_(buf, SCHALTZEITEN_LEN)) {
    ESP_LOGE(TAG, "%s: failed to stage write payload", this->datapoint_.name());
    return;
  }
  this->pending_value_ = value;
  if (this->vh_parent_ == nullptr || !this->vh_parent_->request_write(this)) {
    ESP_LOGE(TAG, "%s: write could not be queued", this->datapoint_.name());
    return;
  }
  ESP_LOGD(TAG, "%s: queued write '%s'", this->datapoint_.name(), value.c_str());
}

void VitoText::control_wpr_day_(const std::string &value) {
  if (this->write_index_ >= 0) {
    ESP_LOGW(TAG, "%s: a write is still running - '%s' not written", this->datapoint_.name(), value.c_str());
    return;
  }
  if (!this->have_raw_) {
    // Only periods that differ are written, so the current day must be known.
    ESP_LOGW(TAG, "%s: day not read yet - '%s' not written", this->datapoint_.name(), value.c_str());
    return;
  }
  if (!encode_wpr_day(value.c_str(), this->target_)) {
    ESP_LOGE(TAG, "%s: '%s' is not a valid day program - not written", this->datapoint_.name(), value.c_str());
    return;
  }
  this->pending_value_ = value;
  this->write_next_wpr_period_(0);
}

static bool wpr_period_equal(const uint8_t *a, const uint8_t *b) {
  // Unused periods (start == end) are equal whatever their mode byte holds:
  // the controller leaves 00 00 02 in some, and rewriting that as 00 00 00
  // would spend a write on nothing.
  const bool a_unused = a[0] == a[1], b_unused = b[0] == b[1];
  if (a_unused || b_unused)
    return a_unused && b_unused;
  return std::memcmp(a, b, 3) == 0;
}

void VitoText::write_next_wpr_period_(int from) {
  int i = from;
  while (i < static_cast<int>(WPR_DAY_PERIODS) && wpr_period_equal(this->raw_ + 3 * i, this->target_ + 3 * i))
    i++;
  if (i >= static_cast<int>(WPR_DAY_PERIODS)) {
    const bool wrote = this->write_index_ >= 0;
    this->write_index_ = -1;
    this->read_back_ = this->user_read_back_;
    if (!wrote) {
      ESP_LOGI(TAG, "%s: unchanged, nothing written", this->datapoint_.name());
      return;
    }
    if (this->user_read_back_) {
      if (this->vh_parent_ != nullptr)
        this->vh_parent_->request_priority_read(this);
    } else {
      std::memcpy(this->raw_, this->target_, sizeof(this->raw_));
      this->publish_state(this->pending_value_);
    }
    return;
  }
  this->write_index_ = i;
  // The hub's own read-back would re-read the whole day after every period;
  // one read after the last period is enough.
  this->read_back_ = false;
  this->set_write_datapoint(optolink::Datapoint(
      this->datapoint_.name(), static_cast<uint16_t>(this->datapoint_.address() + i), 3, optolink::noconv));
  if (!this->set_write_payload_(this->target_ + 3 * i, 3) || this->vh_parent_ == nullptr ||
      !this->vh_parent_->request_write(this)) {
    ESP_LOGE(TAG, "%s: write of period %d could not be queued - stopped", this->datapoint_.name(), i + 1);
    this->write_index_ = -1;
    this->read_back_ = this->user_read_back_;
    return;
  }
  ESP_LOGD(TAG, "%s: writing period %d at 0x%04X", this->datapoint_.name(), i + 1, this->datapoint_.address() + i);
}

void VitoText::handle_response(const ResponseView &response) {
  if (this->wpr_day_) {
    char wpr_out[128];
    if (decode_wpr_day(response.data, response.data_length, wpr_out, sizeof(wpr_out)) < 0) {
      ESP_LOGW(TAG, "%s: response too short (have %u bytes, need %u)", this->datapoint_.name(), response.data_length,
               this->datapoint_.length());
      return;
    }
    std::memcpy(this->raw_, response.data, sizeof(this->raw_));
    this->have_raw_ = true;
    ESP_LOGD(TAG, "%s = '%s'", this->datapoint_.name(), wpr_out);
    this->publish_state(wpr_out);
    return;
  }
  char out[64];
  if (decode_schaltzeiten_day(response.data, response.data_length, out, sizeof(out)) < 0) {
    ESP_LOGW(TAG, "%s: response too short (have %u bytes, need %u)", this->datapoint_.name(), response.data_length,
             this->datapoint_.length());
    return;
  }
  ESP_LOGD(TAG, "%s = '%s'", this->datapoint_.name(), out);
  // publish_state(const char *) assigns into the text sensor's reused state
  // string; wrapping `out` in std::string(out) would force a fresh per-response
  // heap allocation instead.
  this->publish_state(out);
}

void VitoText::handle_write_response(const ResponseView & /*response*/) {
  if (this->wpr_day_ && this->write_index_ >= 0) {
    // This period is now on the device; record it so a stop half-way leaves
    // raw_ describing what is actually there.
    std::memcpy(this->raw_ + 3 * this->write_index_, this->target_ + 3 * this->write_index_, 3);
    this->write_next_wpr_period_(this->write_index_ + 1);
    return;
  }
  if (!this->read_back_) {
    // No read-back requested: publish optimistically on the device ACK. With
    // read_back (default) the hub immediately re-reads this address and
    // handle_response() publishes the device's own canonical view (which may
    // differ from the input after 10-minute truncation).
    this->publish_state(this->pending_value_);
  }
}

void VitoText::handle_error(optolink::OptolinkResult /*error*/) {
  // Keep the last published value; a transient read/write error should not
  // blank the entity. The hub already logs the protocol-level error.
}

void VitoText::handle_write_error(optolink::OptolinkResult /*error*/) {
  if (!this->wpr_day_ || this->write_index_ < 0)
    return;
  ESP_LOGE(TAG, "%s: write of period %d failed - stopped, reading the day back", this->datapoint_.name(),
           this->write_index_ + 1);
  this->write_index_ = -1;
  this->read_back_ = this->user_read_back_;
  if (this->vh_parent_ != nullptr)
    this->vh_parent_->request_priority_read(this);
}

}  // namespace esphome::vitohome
#endif  // USE_TEXT
