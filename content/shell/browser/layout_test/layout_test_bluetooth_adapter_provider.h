// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_LAYOUT_TEST_LAYOUT_TEST_BLUETOOTH_ADAPTER_PROVIDER_H_
#define CONTENT_SHELL_BROWSER_LAYOUT_TEST_LAYOUT_TEST_BLUETOOTH_ADAPTER_PROVIDER_H_

#include "base/callback.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "device/bluetooth/test/mock_bluetooth_discovery_session.h"
#include "device/bluetooth/test/mock_bluetooth_gatt_characteristic.h"
#include "device/bluetooth/test/mock_bluetooth_gatt_service.h"

namespace content {

// Implements fake adapters with named mock data set for use in tests as a
// result of layout tests calling testRunner.setBluetoothMockDataSet.
class LayoutTestBluetoothAdapterProvider {
 public:
  // Returns a BluetoothAdapter. Its behavior depends on |fake_adapter_name|.
  static scoped_refptr<device::BluetoothAdapter> GetBluetoothAdapter(
      const std::string& fake_adapter_name);

 private:
  // Returns "EmptyAdapter" fake BluetoothAdapter with the following
  // characteristics:
  //  - |StartDiscoverySessionWithFilter| runs the success callback with
  //  |DiscoverySession|
  //    as argument.
  //  - |GetDevices| returns an empty list of devices.
  static scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>>
  GetEmptyAdapter();

  // Returns a fake BluetoothAdapter that asserts that its
  // StartDiscoverySessionWithFilter() method is called with a filter consisting
  // of the standard battery, heart rate, and glucose services.
  //  - |StartDiscoverySessionWithFilter(correct arguments)| runs the success
  //    callback with |DiscoverySession| as the argument. With incorrect
  //    arguments, it runs the failure callback.
  //  - |GetDevices| returns a device with a Battery service.
  static scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>>
  GetScanFilterCheckingAdapter();

  // Returns "SingleEmptyDeviceAdapter" fake BluetoothAdapter with the following
  // characteristics:
  //  - |StartDiscoverySessionWithFilter| runs the success callback with
  //  |DiscoverySession|
  //    as argument.
  //  - |GetDevices| returns a list with an |EmptyDevice|.
  static scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>>
  GetSingleEmptyDeviceAdapter();

  // Returns "MultiDeviceAdapter", a fake BluetoothAdapter with the following
  // characteristics:
  //  - |StartDiscoverySessionWithFilter| runs the success callback with
  //  |DiscoverySession|
  //    as argument.
  //  - |GetDevices| returns a list with 2 devices:
  //    - GetUUIDs() returns a Heart Rate Service,
  //      and GetName() returns "Heart Rate Device".
  //    - GetUUIDs() returns a Glucose Service,
  //      and GetName() returns "Glucose Device".
  static scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>>
  GetMultiDeviceAdapter();

  // Returns "ConnectableDeviceAdapter" fake BluetoothAdapter with the
  // following characteristics:
  //  - |StartDiscoverySessionWithFilter| runs the success callback with
  //  |DiscoverySession|
  //    as argument.
  //  - |GetDevices| returns a list with a |ConnectableDevice|.
  static scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>>
  GetConnectableDeviceAdapter();

  // Returns "UnconnectableDeviceAdapter" fake BluetoothAdapter with the
  // following characteristics:
  //  - |StartDiscoverySessionWithFilter| runs the success callback with
  //  |DiscoverySession|
  //    as argument.
  //  - |GetDevices| returns a list with an |UnconnectableDevice|.
  static scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>>
  GetUnconnectableDeviceAdapter();

  // Returns a fake |DiscoverySession| with the following characteristics:
  //  - |Stop| runs the success callback.
  static scoped_ptr<testing::NiceMock<device::MockBluetoothDiscoverySession>>
  GetDiscoverySession();

  // Returns an |EmptyDevice| with the following characeteristics:
  //  - |GetAddress| returns "Empty Mock Device instanceID".
  //  - |GetName| returns "Empty Mock Device name".
  //  - |GetBluetoothClass| returns 0x1F00.  "Unspecified Device Class": see
  //    bluetooth.org/en-us/specification/assigned-numbers/baseband
  //  - |GetVendorIDSource| returns |BluetoothDevice::VENDOR_ID_BLUETOOTH|.
  //  - |GetVendorID| returns 0xFFFF.
  //  - |GetProductID| returns 1.
  //  - |GetDeviceID| returns 2.
  //  - |IsPaired| returns true.
  //  - |GetUUIDs| returns a list with two UUIDs: "1800" and "1801".
  //  - |GetGattServices| returns a list with one service "Generic Access".
  //    "Generic Access" has a "Device Name" characteristic, with a value of
  //    "Empty Mock Device Name", and a "Reconnection Address" characteristic
  //    which can't be read.
  static scoped_ptr<testing::NiceMock<device::MockBluetoothDevice>>
  GetEmptyDevice(device::MockBluetoothAdapter* adapter,
                 const std::string& device_name = "Empty Mock Device");

  // Returns a fake |ConnectableDevice| with the same characteristics as
  // |EmptyDevice| except:
  //  - |CreateGattConnection| runs success callback with a
  //    fake BluetoothGattConnection as argument.
  static scoped_ptr<testing::NiceMock<device::MockBluetoothDevice>>
  GetConnectableDevice(device::MockBluetoothAdapter* adapter);

  // Returns a fake |UnconnectableDevice| with the same characteristics as
  // |EmptyDevice| except:
  //  - |CreateGattConnection| runs error callback with
  //    |BluetoothDevice::ERROR_FAILED| as argument.
  static scoped_ptr<testing::NiceMock<device::MockBluetoothDevice>>
  GetUnconnectableDevice(device::MockBluetoothAdapter* adapter);

  // Returns a fake BluetoothGattService with the following characteristics:
  // - |GetIdentifier| returns |uuid|.
  // - |GetUUID| returns BluetoothUUID(|uuid|).
  // - |IsLocal| returns false.
  // - |IsPrimary| returns true.
  // - |GetDevice| returns |device|.
  static scoped_ptr<testing::NiceMock<device::MockBluetoothGattService>>
  GetGattService(device::MockBluetoothDevice* device, const std::string& uuid);

  // Returns a fake BluetoothGattCharacteristic with the following
  // characteristics:
  // - |GetIdentifier| returns |uuid|.
  // - |GetUUID| returns BluetoothUUID(|uuid|).
  // - |IsLocal| returns false.
  // - |GetService| returns |service|.
  // - |IsNotifying| returns false.
  static scoped_ptr<testing::NiceMock<device::MockBluetoothGattCharacteristic>>
  GetGattCharacteristic(device::MockBluetoothGattService* service,
                        const std::string& uuid);
};

}  // namespace content

#endif  // CONTENT_SHELL_BROWSER_LAYOUT_TEST_LAYOUT_TEST_BLUETOOTH_ADAPTER_PROVIDER_H_
