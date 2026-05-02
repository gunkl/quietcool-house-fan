#pragma once

#include "esphome/components/fan/fan.h"
#include "esphome/core/component.h"

#include <cc1101.h>

namespace esphome {
  namespace quietcool_house_fan {
    enum FanCommand {
      FAN_WAKE = 0x66,
      FAN_ON   = 0xbf,
      FAN_OFF  = 0x80,
      FAN_LOW  = 0x1f,
      FAN_HIGH = 0x3f
    };

    class QuietcoolHouseFan :
      public Component,
      public fan::Fan
    {
      public:

        void dump_config() override;
        fan::FanTraits get_traits() override;
        void setup() override;

        float get_setup_priority() const override { return setup_priority::BUS; }

        void set_pins(
            uint8_t cs,
            uint8_t clk,
            uint8_t mosi,
            uint8_t miso,
            uint8_t gdo0,
            uint8_t gdo2) {
          this->cs_pin_ = cs;
          this->clk_pin_ = clk;
          this->mosi_pin_ = mosi;
          this->miso_pin_ = miso;
          this->gdo0_pin_ = gdo0;
          this->gdo2_pin_ = gdo2;
        }
        void set_frequencies(
            float center_freq_mhz,
            float deviation_khz) {
          this->center_freq_mhz_ = center_freq_mhz;
          this->deviation_khz_ = deviation_khz;
        }

      protected:
        void control(const fan::FanCall &call) override;

        void send_command(FanCommand command);
        void send_message(uint8_t *message, uint8_t len);

      private:
        std::unique_ptr<CC1101::Radio> radio_;

        uint8_t cs_pin_{};
        uint8_t clk_pin_{};
        uint8_t miso_pin_{};
        uint8_t mosi_pin_{};
        uint8_t gdo0_pin_{};
        uint8_t gdo2_pin_{};
        float center_freq_mhz_{};
        float deviation_khz_{};
        float speed_{0.0f};
        std::array<uint8_t, 7> remote_id_{};

      public:
        void set_remote_id(const std::vector<uint8_t> &remote_id) {
          for (size_t i = 0; i < 7 && i < remote_id.size(); ++i) remote_id_[i] = remote_id[i];
        }
    };
  }  // namespace quietcool_house_fan
}  // namespace esphome
