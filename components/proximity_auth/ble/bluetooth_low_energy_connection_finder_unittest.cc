// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proximity_auth/ble/bluetooth_low_energy_connection_finder.h"

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "components/proximity_auth/connection.h"
#include "components/proximity_auth/remote_device.h"
#include "components/proximity_auth/wire_message.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_uuid.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "device/bluetooth/test/mock_bluetooth_discovery_session.h"
#include "device/bluetooth/test/mock_bluetooth_gatt_connection.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::AtLeast;
using testing::NiceMock;
using testing::Return;
using testing::StrictMock;
using testing::SaveArg;

namespace proximity_auth {
namespace {

const char kDeviceName[] = "Device name";
const char kBluetoothAddress[] = "11:22:33:44:55:66";
const RemoteDevice kRemoteDevice = {kDeviceName, kBluetoothAddress};

const char kServiceUUID[] = "DEADBEEF-CAFE-FEED-FOOD-D15EA5EBEEEF";
const char kToPeripheralCharUUID[] = "FBAE09F2-0482-11E5-8418-1697F925EC7B";
const char kFromPeripheralCharUUID[] = "5539ED10-0483-11E5-8418-1697F925EC7B";

const char kOtherUUID[] = "AAAAAAAA-AAAA-AAAA-AAAA-D15EA5EBEEEF";
const char kOtherBluetoothAddress[] = "00:00:00:00:00:00";

const int kMaxNumberOfAttempts = 2;

class MockConnection : public Connection {
 public:
  MockConnection() : Connection(kRemoteDevice) {}
  ~MockConnection() override {}

  MOCK_METHOD0(Connect, void());

  using Connection::SetStatus;

 private:
  void Disconnect() override {}
  void SendMessageImpl(scoped_ptr<WireMessage> message) override {}

  DISALLOW_COPY_AND_ASSIGN(MockConnection);
};

class MockBluetoothLowEnergyConnectionFinder
    : public BluetoothLowEnergyConnectionFinder {
 public:
  MockBluetoothLowEnergyConnectionFinder()
      : BluetoothLowEnergyConnectionFinder(kServiceUUID,
                                           kToPeripheralCharUUID,
                                           kFromPeripheralCharUUID,
                                           kMaxNumberOfAttempts) {
    SetDelayForTesting(base::TimeDelta());
  }

  ~MockBluetoothLowEnergyConnectionFinder() override {}

  // Mock methods don't support return type scoped_ptr<>. This is a possible
  // workaround: mock a proxy method to be called by the target overrided method
  // (CreateConnection).
  MOCK_METHOD0(CreateConnectionProxy, Connection*());

  // Creates a mock connection and sets an expectation that the mock connection
  // finder's CreateConnection() method will be called and will return the
  // created connection. Returns a reference to the created connection.
  // NOTE: The returned connection's lifetime is managed by the connection
  // finder.
  MockConnection* ExpectCreateConnection() {
    scoped_ptr<MockConnection> connection(new NiceMock<MockConnection>());
    MockConnection* connection_alias = connection.get();
    EXPECT_CALL(*this, CreateConnectionProxy())
        .WillOnce(Return(connection.release()));
    return connection_alias;
  }

 protected:
  scoped_ptr<Connection> CreateConnection(
      scoped_ptr<device::BluetoothGattConnection> gatt_connection) override {
    return make_scoped_ptr(CreateConnectionProxy());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockBluetoothLowEnergyConnectionFinder);
};

}  // namespace

class ProximityAuthBluetoothLowEnergyConnectionFinderTest
    : public testing::Test {
 protected:
  ProximityAuthBluetoothLowEnergyConnectionFinderTest()
      : adapter_(new NiceMock<device::MockBluetoothAdapter>),
        connection_callback_(
            base::Bind(&ProximityAuthBluetoothLowEnergyConnectionFinderTest::
                           OnConnectionFound,
                       base::Unretained(this))),
        device_(new NiceMock<device::MockBluetoothDevice>(adapter_.get(),
                                                          0,
                                                          kDeviceName,
                                                          kBluetoothAddress,
                                                          false,
                                                          false)),
        last_discovery_session_alias_(nullptr) {
    device::BluetoothAdapterFactory::SetAdapterForTesting(adapter_);

    std::vector<const device::BluetoothDevice*> devices;
    ON_CALL(*adapter_, GetDevices()).WillByDefault(Return(devices));

    ON_CALL(*adapter_, IsPresent()).WillByDefault(Return(true));
    ON_CALL(*adapter_, IsPowered()).WillByDefault(Return(true));
  }

  void OnConnectionFound(scoped_ptr<Connection> connection) {
    last_found_connection_ = connection.Pass();
  }

  void FindAndExpectStartDiscovery(
      BluetoothLowEnergyConnectionFinder& connection_finder) {
    device::BluetoothAdapter::DiscoverySessionCallback discovery_callback;
    scoped_ptr<device::MockBluetoothDiscoverySession> discovery_session(
        new NiceMock<device::MockBluetoothDiscoverySession>());
    last_discovery_session_alias_ = discovery_session.get();

    // Starting a discovery session. StartDiscoveryWithFilterRaw is a proxy for
    // StartDiscoveryWithFilter.
    EXPECT_CALL(*adapter_, StartDiscoverySessionWithFilterRaw(_, _, _))
        .WillOnce(SaveArg<1>(&discovery_callback));
    EXPECT_CALL(*adapter_, AddObserver(_));
    ON_CALL(*last_discovery_session_alias_, IsActive())
        .WillByDefault(Return(true));
    connection_finder.Find(connection_callback_);
    ASSERT_FALSE(discovery_callback.is_null());
    discovery_callback.Run(discovery_session.Pass());
  }

  void ExpectStopDiscoveryAndRemoveObserver() {
    EXPECT_CALL(*last_discovery_session_alias_, Stop(_, _)).Times(AtLeast(1));
    EXPECT_CALL(*adapter_, RemoveObserver(_)).Times(AtLeast(1));
  }

  // Prepare |device_| with |uuid|.
  void PrepareDevice(const std::string& uuid) {
    std::vector<device::BluetoothUUID> uuids;
    uuids.push_back(device::BluetoothUUID(uuid));
    ON_CALL(*device_, GetUUIDs()).WillByDefault(Return(uuids));
  }

  // Prepare expectations to add/change a right device.
  void PrepareForNewRightDevice(
      const std::string& uuid,
      device::BluetoothDevice::GattConnectionCallback& callback) {
    PrepareDevice(uuid);
    ON_CALL(*device_, IsPaired()).WillByDefault(Return(true));
    EXPECT_CALL(*device_, CreateGattConnection(_, _))
        .WillOnce(SaveArg<0>(&callback));
  }

  // Prepare expectations to add/change a wrong device.
  void PrepareForNewWrongDevice(const std::string& uuid) {
    PrepareDevice(uuid);
    ON_CALL(*device_, IsPaired()).WillByDefault(Return(true));
    EXPECT_CALL(*device_, CreateGattConnection(_, _)).Times(0);
  }

  scoped_refptr<device::MockBluetoothAdapter> adapter_;
  ConnectionFinder::ConnectionCallback connection_callback_;
  scoped_ptr<device::MockBluetoothDevice> device_;
  scoped_ptr<Connection> last_found_connection_;
  device::MockBluetoothDiscoverySession* last_discovery_session_alias_;

 private:
  base::MessageLoop message_loop_;
};

TEST_F(ProximityAuthBluetoothLowEnergyConnectionFinderTest,
       ConstructAndDestroyDoesntCrash) {
  // Destroying a BluetoothConnectionFinder for which Find() has not been called
  // should not crash.
  BluetoothLowEnergyConnectionFinder connection_finder(
      kServiceUUID, kToPeripheralCharUUID, kFromPeripheralCharUUID,
      kMaxNumberOfAttempts);
}

TEST_F(ProximityAuthBluetoothLowEnergyConnectionFinderTest,
       Find_StartsDiscoverySession) {
  BluetoothLowEnergyConnectionFinder connection_finder(
      kServiceUUID, kToPeripheralCharUUID, kFromPeripheralCharUUID,
      kMaxNumberOfAttempts);

  EXPECT_CALL(*adapter_, StartDiscoverySessionWithFilterRaw(_, _, _));
  EXPECT_CALL(*adapter_, AddObserver(_));
  connection_finder.Find(connection_callback_);
}

TEST_F(ProximityAuthBluetoothLowEnergyConnectionFinderTest,
       Find_StopsDiscoverySessionBeforeDestroying) {
  BluetoothLowEnergyConnectionFinder connection_finder(
      kServiceUUID, kToPeripheralCharUUID, kFromPeripheralCharUUID,
      kMaxNumberOfAttempts);

  device::BluetoothAdapter::DiscoverySessionCallback discovery_callback;
  scoped_ptr<device::MockBluetoothDiscoverySession> discovery_session(
      new NiceMock<device::MockBluetoothDiscoverySession>());
  device::MockBluetoothDiscoverySession* discovery_session_alias =
      discovery_session.get();

  EXPECT_CALL(*adapter_, StartDiscoverySessionWithFilterRaw(_, _, _))
      .WillOnce(SaveArg<1>(&discovery_callback));
  ON_CALL(*discovery_session_alias, IsActive()).WillByDefault(Return(true));
  EXPECT_CALL(*adapter_, AddObserver(_));
  connection_finder.Find(connection_callback_);

  EXPECT_CALL(*discovery_session_alias, Stop(_, _));
  ASSERT_FALSE(discovery_callback.is_null());
  discovery_callback.Run(discovery_session.Pass());

  EXPECT_CALL(*adapter_, RemoveObserver(_));
}

TEST_F(ProximityAuthBluetoothLowEnergyConnectionFinderTest,
       Find_CreatesGattConnectionWhenRightDeviceIsAdded) {
  BluetoothLowEnergyConnectionFinder connection_finder(
      kServiceUUID, kToPeripheralCharUUID, kFromPeripheralCharUUID,
      kMaxNumberOfAttempts);
  device::BluetoothDevice::GattConnectionCallback gatt_connection_callback;
  FindAndExpectStartDiscovery(connection_finder);
  ExpectStopDiscoveryAndRemoveObserver();

  PrepareForNewRightDevice(kServiceUUID, gatt_connection_callback);
  connection_finder.DeviceAdded(adapter_.get(), device_.get());
  ASSERT_FALSE(gatt_connection_callback.is_null());
}

TEST_F(ProximityAuthBluetoothLowEnergyConnectionFinderTest,
       Find_DoesntCreateGattConnectionWhenWrongDeviceIsAdded) {
  BluetoothLowEnergyConnectionFinder connection_finder(
      kServiceUUID, kToPeripheralCharUUID, kFromPeripheralCharUUID,
      kMaxNumberOfAttempts);
  FindAndExpectStartDiscovery(connection_finder);
  ExpectStopDiscoveryAndRemoveObserver();

  PrepareForNewWrongDevice(kOtherUUID);
  connection_finder.DeviceAdded(adapter_.get(), device_.get());
}

TEST_F(ProximityAuthBluetoothLowEnergyConnectionFinderTest,
       Find_CreatesGattConnectionWhenRightDeviceIsChanged) {
  BluetoothLowEnergyConnectionFinder connection_finder(
      kServiceUUID, kToPeripheralCharUUID, kFromPeripheralCharUUID,
      kMaxNumberOfAttempts);
  device::BluetoothDevice::GattConnectionCallback gatt_connection_callback;
  FindAndExpectStartDiscovery(connection_finder);
  ExpectStopDiscoveryAndRemoveObserver();

  PrepareForNewRightDevice(kServiceUUID, gatt_connection_callback);
  connection_finder.DeviceChanged(adapter_.get(), device_.get());
  ASSERT_FALSE(gatt_connection_callback.is_null());
}

TEST_F(ProximityAuthBluetoothLowEnergyConnectionFinderTest,
       Find_DoesntCreateGattConnectionWhenWrongDeviceIsChanged) {
  BluetoothLowEnergyConnectionFinder connection_finder(
      kServiceUUID, kToPeripheralCharUUID, kFromPeripheralCharUUID,
      kMaxNumberOfAttempts);
  FindAndExpectStartDiscovery(connection_finder);
  ExpectStopDiscoveryAndRemoveObserver();

  PrepareForNewWrongDevice(kOtherUUID);
  connection_finder.DeviceChanged(adapter_.get(), device_.get());
}

TEST_F(ProximityAuthBluetoothLowEnergyConnectionFinderTest,
       Find_CreatesTwoGattConnections) {
  StrictMock<MockBluetoothLowEnergyConnectionFinder> connection_finder;
  FindAndExpectStartDiscovery(connection_finder);
  ExpectStopDiscoveryAndRemoveObserver();
  connection_finder.ExpectCreateConnection();

  // Prepare to add |device_|.
  device::BluetoothDevice::GattConnectionCallback gatt_connection_callback;
  PrepareForNewRightDevice(kServiceUUID, gatt_connection_callback);

  // Prepare to add |other_device|.
  device::BluetoothDevice::GattConnectionCallback
      other_gatt_connection_callback;
  NiceMock<device::MockBluetoothDevice> other_device(
      adapter_.get(), 0, kDeviceName, kOtherBluetoothAddress, false, false);
  std::vector<device::BluetoothUUID> uuids;
  uuids.push_back(device::BluetoothUUID(kServiceUUID));
  ON_CALL(other_device, IsPaired()).WillByDefault(Return(true));
  ON_CALL(other_device, GetUUIDs()).WillByDefault((Return(uuids)));
  EXPECT_CALL(other_device, CreateGattConnection(_, _))
      .WillOnce(SaveArg<0>(&other_gatt_connection_callback));

  // Add the devices.
  connection_finder.DeviceAdded(adapter_.get(), device_.get());
  connection_finder.DeviceAdded(adapter_.get(), &other_device);

  ASSERT_FALSE(gatt_connection_callback.is_null());
  ASSERT_FALSE(other_gatt_connection_callback.is_null());

  base::RunLoop run_loop;
  gatt_connection_callback.Run(make_scoped_ptr(
      new NiceMock<device::MockBluetoothGattConnection>(kBluetoothAddress)));
  run_loop.RunUntilIdle();

  // The second device should be forgetten.
  EXPECT_CALL(*adapter_, GetDevice(std::string(kOtherBluetoothAddress)))
      .WillOnce(Return(&other_device));
  EXPECT_CALL(other_device, Disconnect(_, _));
  other_gatt_connection_callback.Run(
      make_scoped_ptr(new NiceMock<device::MockBluetoothGattConnection>(
          kOtherBluetoothAddress)));
}

TEST_F(ProximityAuthBluetoothLowEnergyConnectionFinderTest,
       Find_ConnectionSucceeds) {
  StrictMock<MockBluetoothLowEnergyConnectionFinder> connection_finder;

  // Starting discovery.
  FindAndExpectStartDiscovery(connection_finder);
  ExpectStopDiscoveryAndRemoveObserver();

  // Finding and creating a GATT connection to the right device.
  device::BluetoothDevice::GattConnectionCallback gatt_connection_callback;
  PrepareForNewRightDevice(kServiceUUID, gatt_connection_callback);
  connection_finder.DeviceAdded(adapter_.get(), device_.get());

  // Creating a connection.
  MockConnection* connection = connection_finder.ExpectCreateConnection();
  ASSERT_FALSE(gatt_connection_callback.is_null());
  base::RunLoop run_loop;
  gatt_connection_callback.Run(make_scoped_ptr(
      new NiceMock<device::MockBluetoothGattConnection>(kBluetoothAddress)));
  run_loop.RunUntilIdle();
  EXPECT_FALSE(last_found_connection_);
  connection->SetStatus(Connection::IN_PROGRESS);
  connection->SetStatus(Connection::CONNECTED);
  EXPECT_TRUE(last_found_connection_);
}

TEST_F(ProximityAuthBluetoothLowEnergyConnectionFinderTest,
       Find_ConnectionFails_RestartDiscoveryAndConnectionSucceeds) {
  StrictMock<MockBluetoothLowEnergyConnectionFinder> connection_finder;

  // Starting discovery.
  FindAndExpectStartDiscovery(connection_finder);
  base::Closure stop_discovery_session_callback;
  EXPECT_CALL(*last_discovery_session_alias_, Stop(_, _))
      .WillOnce(SaveArg<0>(&stop_discovery_session_callback));

  // Preparing to create a GATT connection to the right device.
  device::BluetoothDevice::GattConnectionCallback gatt_connection_callback;
  PrepareForNewRightDevice(kServiceUUID, gatt_connection_callback);
  MockConnection* connection = connection_finder.ExpectCreateConnection();

  // Trying to create a connection.
  connection_finder.DeviceAdded(adapter_.get(), device_.get());
  ASSERT_FALSE(gatt_connection_callback.is_null());
  base::RunLoop run_loop;
  gatt_connection_callback.Run(make_scoped_ptr(
      new NiceMock<device::MockBluetoothGattConnection>(kBluetoothAddress)));
  run_loop.RunUntilIdle();
  ASSERT_FALSE(last_found_connection_);
  connection->SetStatus(Connection::IN_PROGRESS);

  // Stopping the discovery session.
  ASSERT_FALSE(stop_discovery_session_callback.is_null());
  stop_discovery_session_callback.Run();

  // Preparing to restart the discovery session.
  device::BluetoothAdapter::DiscoverySessionCallback discovery_callback;
  std::vector<const device::BluetoothDevice*> devices;
  ON_CALL(*adapter_, GetDevices()).WillByDefault(Return(devices));
  EXPECT_CALL(*adapter_, StartDiscoverySessionWithFilterRaw(_, _, _))
      .WillOnce(SaveArg<1>(&discovery_callback));

  // Connection fails.
  connection->SetStatus(Connection::DISCONNECTED);

  // Restarting the discovery session.
  scoped_ptr<device::MockBluetoothDiscoverySession> discovery_session(
      new NiceMock<device::MockBluetoothDiscoverySession>());
  last_discovery_session_alias_ = discovery_session.get();
  ON_CALL(*last_discovery_session_alias_, IsActive())
      .WillByDefault(Return(true));
  ASSERT_FALSE(discovery_callback.is_null());
  discovery_callback.Run(discovery_session.Pass());

  // Preparing to create a GATT connection to the right device.
  PrepareForNewRightDevice(kServiceUUID, gatt_connection_callback);
  connection = connection_finder.ExpectCreateConnection();

  // Trying to create a connection.
  connection_finder.DeviceAdded(adapter_.get(), device_.get());
  EXPECT_CALL(*last_discovery_session_alias_, Stop(_, _)).Times(AtLeast(1));
  ASSERT_FALSE(gatt_connection_callback.is_null());
  base::RunLoop other_run_loop;
  gatt_connection_callback.Run(make_scoped_ptr(
      new NiceMock<device::MockBluetoothGattConnection>(kBluetoothAddress)));
  other_run_loop.RunUntilIdle();

  // Completing the connection.
  EXPECT_FALSE(last_found_connection_);
  connection->SetStatus(Connection::IN_PROGRESS);
  connection->SetStatus(Connection::CONNECTED);
  EXPECT_TRUE(last_found_connection_);
}

TEST_F(ProximityAuthBluetoothLowEnergyConnectionFinderTest,
       Find_AdapterRemoved_RestartDiscoveryAndConnectionSucceeds) {
  StrictMock<MockBluetoothLowEnergyConnectionFinder> connection_finder;

  // Starting discovery.
  FindAndExpectStartDiscovery(connection_finder);

  // Removing the adapter.
  ON_CALL(*adapter_, IsPresent()).WillByDefault(Return(false));
  ON_CALL(*adapter_, IsPowered()).WillByDefault(Return(false));
  ON_CALL(*last_discovery_session_alias_, IsActive())
      .WillByDefault(Return(false));
  connection_finder.AdapterPoweredChanged(adapter_.get(), false);
  connection_finder.AdapterPresentChanged(adapter_.get(), false);

  // Adding the adapter.
  ON_CALL(*adapter_, IsPresent()).WillByDefault(Return(true));
  ON_CALL(*adapter_, IsPowered()).WillByDefault(Return(true));

  device::BluetoothAdapter::DiscoverySessionCallback discovery_callback;
  scoped_ptr<device::MockBluetoothDiscoverySession> discovery_session(
      new NiceMock<device::MockBluetoothDiscoverySession>());
  last_discovery_session_alias_ = discovery_session.get();

  // Restarting the discovery session.
  EXPECT_CALL(*adapter_, StartDiscoverySessionWithFilterRaw(_, _, _))
      .WillOnce(SaveArg<1>(&discovery_callback));
  connection_finder.AdapterPresentChanged(adapter_.get(), true);
  connection_finder.AdapterPoweredChanged(adapter_.get(), true);
  ON_CALL(*last_discovery_session_alias_, IsActive())
      .WillByDefault(Return(true));

  ASSERT_FALSE(discovery_callback.is_null());
  discovery_callback.Run(discovery_session.Pass());

  // Preparing to create a GATT connection to the right device.
  device::BluetoothDevice::GattConnectionCallback gatt_connection_callback;
  PrepareForNewRightDevice(kServiceUUID, gatt_connection_callback);
  MockConnection* connection = connection_finder.ExpectCreateConnection();

  // Trying to create a connection.
  connection_finder.DeviceAdded(adapter_.get(), device_.get());
  EXPECT_CALL(*last_discovery_session_alias_, Stop(_, _)).Times(AtLeast(1));
  ASSERT_FALSE(gatt_connection_callback.is_null());
  base::RunLoop run_loop;
  gatt_connection_callback.Run(make_scoped_ptr(
      new NiceMock<device::MockBluetoothGattConnection>(kBluetoothAddress)));
  run_loop.RunUntilIdle();

  // Completing the connection.
  ASSERT_FALSE(last_found_connection_);
  connection->SetStatus(Connection::IN_PROGRESS);
  connection->SetStatus(Connection::CONNECTED);
  EXPECT_TRUE(last_found_connection_);
}

}  // namespace proximity_auth
