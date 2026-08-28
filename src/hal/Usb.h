#pragma once

namespace papyrix::hal {

class Battery;

class Usb {
 public:
  struct Status {
    bool available = false;
    bool connected = false;
  };

  void init(const Battery& battery);
  Status readStatus() const;
  bool isConnected() const { return readStatus().connected; }

 private:
  const Battery* battery_ = nullptr;
};

}  // namespace papyrix::hal
