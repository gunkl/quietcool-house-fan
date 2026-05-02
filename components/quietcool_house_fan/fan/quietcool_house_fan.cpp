#include "quietcool_house_fan.h"
#include "esphome/core/log.h"

namespace esphome {
  namespace quietcool_house_fan {

    static const char *TAG = "quietcool_house_fan.fan";
    static const size_t SYNC_LENGTH = 9;
    static const size_t ID_LENGTH = 7;

    static const uint8_t SYNC_WORD[] = { 0x55, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa }; 

    static const unsigned long REPEAT_SPACING = 130;

    void QuietcoolHouseFan::setup() {
      if (this->radio_ == nullptr) {
        ESP_LOGI(TAG, "Initializing radio");
        ESP_LOGI(TAG, "  PINs");
        ESP_LOGI(TAG, "      CS: %d", this->cs_pin_);
        ESP_LOGI(TAG, "     CLK: %d", this->clk_pin_);
        ESP_LOGI(TAG, "    MISO: %d", this->miso_pin_);
        ESP_LOGI(TAG, "    MOSI: %d", this->mosi_pin_);
        ESP_LOGI(TAG, "    GDO0: %d", this->gdo0_pin_);
        ESP_LOGI(TAG, "    GD02: %d", this->gdo2_pin_);

        this->radio_.reset(
          new CC1101::Radio(
            this->cs_pin_,
            this->clk_pin_,
            this->miso_pin_,
            this->mosi_pin_,
            this->gdo0_pin_,
            this->gdo2_pin_
          )
        );

        ESP_LOGI(TAG, "Created object, trying to boot radio");

        if (this->radio_->begin() == CC1101::STATUS_CHIP_NOT_FOUND) {
          ESP_LOGW(TAG, "Chip not found");
        } else {
          ESP_LOGI(TAG, "Radio loaded, configuring parameters");

          this->radio_->setModulation(CC1101::MOD_2FSK);
          this->radio_->setFrequency(this->center_freq_mhz_);
          this->radio_->setFrequencyDeviation(this->deviation_khz_);
          this->radio_->setDataRate(2.398);
          this->radio_->setOutputPower(10);
          this->radio_->setCrc(false);
          this->radio_->setDataWhitening(false);
          this->radio_->setManchester(false);
          this->radio_->setFEC(false);
        }
      } else {
        ESP_LOGI(TAG, "Radio already loaded for some reason");
      }
    }

    fan::FanTraits QuietcoolHouseFan::get_traits() {
      return fan::FanTraits(
              false,    // No oscillation
              true,     // Supports speed control
              false,    // No direction
              2         // Number of speed settings
      );
    }

    void QuietcoolHouseFan::control(const fan::FanCall &call) {
      float new_speed_out = call.get_speed().value_or(-1.0f);

      ESP_LOGD(TAG, "Control called: state=%s, speed=%s", 
          call.get_state().has_value() ? (call.get_state().value() ? "ON" : "OFF") : "<unchanged>",
          call.get_speed().has_value() ? (std::to_string(new_speed_out)).c_str() : "<unchanged>");

      if (call.get_state().value() != this->state) {
        if (call.get_state().value()) {
          this->send_command(FAN_ON);
        } else {
          this->send_command(FAN_OFF);
        }

        this->state = call.get_state().value();
      }

      if (call.get_speed().has_value() && call.get_speed() != this->speed) {
        if (call.get_speed() == 0) {
          this->send_command(FAN_OFF);

        } else if (call.get_speed() == 1) {
          this->send_command(FAN_LOW);

        } else if (call.get_speed() == 2) {
          this->send_command(FAN_HIGH);
        }

        this->speed = call.get_speed().value();
      }

      this->publish_state();
    }

    void QuietcoolHouseFan::send_command(FanCommand command) {
      const uint8_t full_message_length = SYNC_LENGTH + ID_LENGTH + 2;
      uint8_t message[full_message_length];
    
      memset(message, 0, full_message_length);
      memcpy(message, SYNC_WORD, SYNC_LENGTH);
      memcpy(message + SYNC_LENGTH, this->remote_id_.data(), ID_LENGTH);
      memcpy(message + SYNC_LENGTH + ID_LENGTH, &command, 1);
      memcpy(message + SYNC_LENGTH + ID_LENGTH + 1, &command, 1);
    
      this->send_message(message, full_message_length);
    }


    void QuietcoolHouseFan::send_message(uint8_t *message, uint8_t len) {
      char message_string[len*2 + 1];
      for (size_t i=0; i<len; i++) {
        char buf[2];
        sprintf(buf, "%02x", message[i]);
        memcpy(message_string + (i*2), buf, 2);
      }
      message_string[len*2] = '\0';

      ESP_LOGD(TAG, "Sending message (size %i): %s", len, message_string);

      CC1101::Status status;
      this->radio_->setPacketLengthMode(CC1101::PKT_LEN_MODE_FIXED, len);
      this->radio_->setSyncMode(CC1101::SYNC_MODE_NO_PREAMBLE_CS);

      long now = 0;

      // QC expects each command sent three times
      for (uint8_t i=0; i<3; i++) {
        now = millis();

        status = this->radio_->transmit(message, len);

        // Check to see if there's an error
        if (status == CC1101::STATUS_OK) {
          ESP_LOGD(TAG, "Successful send");
        } else {
          ESP_LOGD(TAG, "Send error: %d", status);
        }

        // Delay until we hit the inter-message timout
        while ((millis()-now) < REPEAT_SPACING) delay(10);
      }
    }

    void QuietcoolHouseFan::dump_config() { LOG_FAN("", "QuietCool House Fan", this); }
  }  // namespace quietcool_house_fan
}  // namespace esphome
