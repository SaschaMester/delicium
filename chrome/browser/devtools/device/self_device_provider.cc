// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/device/self_device_provider.h"

#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/thread_task_runner_handle.h"
#include "net/socket/tcp_client_socket.h"

namespace {

const char kDeviceModel[] = "Local Chrome";
const char kBrowserName[] = "Chrome";
const char kLocalhost[] = "127.0.0.1";
const char kSerial[] = "local";

static void RunSocketCallback(
    const AndroidDeviceManager::SocketCallback& callback,
    scoped_ptr<net::StreamSocket> socket,
    int result) {
  callback.Run(result, socket.Pass());
}

}  // namespace

SelfAsDeviceProvider::SelfAsDeviceProvider(int port) : port_(port) {
}

void SelfAsDeviceProvider::QueryDevices(const SerialsCallback& callback) {
  std::vector<std::string> result;
  result.push_back(kSerial);
  callback.Run(result);
}

void SelfAsDeviceProvider::QueryDeviceInfo(const std::string& serial,
                                           const DeviceInfoCallback& callback) {
  AndroidDeviceManager::DeviceInfo device_info;
  device_info.model = kDeviceModel;
  device_info.connected = true;

  AndroidDeviceManager::BrowserInfo browser_info;
  browser_info.socket_name = base::IntToString(port_);
  browser_info.display_name = kBrowserName;
  browser_info.type = AndroidDeviceManager::BrowserInfo::kTypeChrome;

  device_info.browser_info.push_back(browser_info);

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(callback, device_info));
}

void SelfAsDeviceProvider::OpenSocket(const std::string& serial,
                                      const std::string& socket_name,
                                      const SocketCallback& callback) {
  // Use plain socket for remote debugging and port forwarding on Desktop
  // (debugging purposes).
  net::IPAddressNumber ip_number;
  net::ParseIPLiteralToNumber(kLocalhost, &ip_number);
  int port;
  base::StringToInt(socket_name, &port);
  net::AddressList address_list =
      net::AddressList::CreateFromIPAddress(ip_number, port);
  scoped_ptr<net::StreamSocket> socket(new net::TCPClientSocket(
      address_list, NULL, net::NetLog::Source()));
  socket->Connect(
      base::Bind(&RunSocketCallback, callback, base::Passed(&socket)));
}

void SelfAsDeviceProvider::ReleaseDevice(const std::string& serial) {
  DCHECK(serial == kSerial);
  if (!release_callback_.is_null())
    release_callback_.Run();
}

void SelfAsDeviceProvider::set_release_callback_for_test(
    const base::Closure& callback) {
  release_callback_ = callback;
}

SelfAsDeviceProvider::~SelfAsDeviceProvider() {
}
