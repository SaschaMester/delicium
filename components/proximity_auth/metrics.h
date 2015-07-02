// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROXIMITY_AUTH_METRICS_H
#define COMPONENTS_PROXIMITY_AUTH_METRICS_H

#include "base/time/time.h"

namespace proximity_auth {
namespace metrics {

extern const char kUnknownDeviceModel[];
extern const int kUnknownProximityValue;

// Records the current |rolling_rssi| reading, upon a successful auth attempt.
// |rolling_rssi| should be set to |kUnknownProximityValue| if no RSSI readings
// are available.
void RecordAuthProximityRollingRssi(int rolling_rssi);

// Records the difference between the transmit power and maximum transmit power,
// upon a successful auth attempt. |transmit_power_delta| should be set to
// |kUnknownProximityValue| if no Tx power readings are available.
void RecordAuthProximityTransmitPowerDelta(int transmit_power_delta);

// Records the time elapsed since the last zero RSSI value was read, upon a
// successful auth attempt.
void RecordAuthProximityTimeSinceLastZeroRssi(
    base::TimeDelta time_since_last_zero_rssi);

// Records the phone model used for a successful auth attempt. The model is
// recorded as a 32-bit hash due to the limits of UMA. |device_model| should be
// set to |kUnknownDeviceModel| if the device model could not be read.
void RecordAuthProximityRemoteDeviceModelHash(const std::string& device_model);

}  // namespace metrics
}  // namespace proximity_auth

#endif  // COMPONENTS_PROXIMITY_AUTH_METRICS_H