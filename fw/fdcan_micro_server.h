// Copyright 2019-2020 Josh Pieper, jjp@pobox.com.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include "mjlib/multiplex/micro_datagram_server.h"

#include "fw/fdcan.h"

namespace fw {

class FDCanMicroServer : public mjlib::multiplex::MicroDatagramServer {
 public:
  FDCanMicroServer(FDCan* can) : fdcan_(can) {}

  void AsyncRead(Header* header,
                 const mjlib::base::string_span& data,
                 const mjlib::micro::SizeCallback& callback) override {
    MJ_ASSERT(!current_read_callback_);
    current_read_callback_ = callback;
    current_read_data_ = data;
    current_read_header_ = header;
  }

  void AsyncWrite(const Header& header,
                  const std::string_view& data,
                  const mjlib::micro::SizeCallback& callback) override {
    const auto actual_dlc = RoundUpDlc(data.size());
    const uint32_t id =
        ((header.source & 0xff) << 8) | (header.destination & 0xff);

    if (actual_dlc == data.size()) {
      fdcan_->Send(id, data, {});
    } else {
      std::memcpy(buf_, data.data(), data.size());
      for (size_t i = data.size(); i < actual_dlc; i++) {
        buf_[i] = 0x50;
      }
      fdcan_->Send(id, std::string_view(buf_, actual_dlc), {});
    }

    callback(mjlib::micro::error_code(), data.size());
  }

  Properties properties() const override {
    Properties properties;
    properties.max_size = 64;
    return properties;
  }

  bool Poll(FDCAN_RxHeaderTypeDef* header, mjlib::base::string_span data) {
    const bool got_data = fdcan_->Poll(header, data);
    if (!got_data) { return false; }

    const auto size_in_bytes = FDCan::ParseDlc(header->DataLength);

    if (header->Identifier & ~0xffff) {
      // This is definitely not a multiplex packet.
      return true;
    }

    // Weird, no one is waiting for us, just return.
    if (!current_read_header_) { return false; }

    std::memcpy(&current_read_data_[0], &data[0], size_in_bytes);

    current_read_header_->destination = header->Identifier & 0xff;
    current_read_header_->source = (header->Identifier >> 8) & 0xff;
    current_read_header_->size = size_in_bytes;

    auto copy = current_read_callback_;
    auto bytes = current_read_header_->size;

    current_read_callback_ = {};
    current_read_header_ = {};
    current_read_data_ = {};

    copy(mjlib::micro::error_code(), bytes);
    return false;
  }

  static size_t RoundUpDlc(size_t value) {
    if (value == 0) { return 0; }
    if (value == 1) { return 1; }
    if (value == 2) { return 2; }
    if (value == 3) { return 3; }
    if (value == 4) { return 4; }
    if (value == 5) { return 5; }
    if (value == 6) { return 6; }
    if (value == 7) { return 7; }
    if (value == 8) { return 8; }
    if (value <= 12) { return 12; }
    if (value <= 16) { return 16; }
    if (value <= 20) { return 20; }
    if (value <= 24) { return 24; }
    if (value <= 32) { return 32; }
    if (value <= 48) { return 48; }
    if (value <= 64) { return 64; }
    return 0;
  }

 private:
  FDCan* const fdcan_;

  mjlib::micro::SizeCallback current_read_callback_;
  Header* current_read_header_ = nullptr;
  mjlib::base::string_span current_read_data_;

  char buf_[64] = {};
};

}
