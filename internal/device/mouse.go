package device

import "time"

// MouseModel represents the Magic Mouse model year
type MouseModel string

const (
	ModelMagicMouse1 MouseModel = "Magic Mouse (1st gen)"
	ModelMagicMouse2 MouseModel = "Magic Mouse 2 (2015)"
	ModelMagicMouse3 MouseModel = "Magic Mouse 3 (2021)"
	ModelUnknown     MouseModel = "Unknown"
)

// ChargingState represents battery charging status
type ChargingState string

const (
	StateCharging    ChargingState = "Charging"
	StateDischarging ChargingState = "Discharging"
	StateFull        ChargingState = "Full"
	StateUnknown     ChargingState = "Unknown"
)

// MagicMouse holds all data about the connected mouse
type MagicMouse struct {
	Name             string        `json:"name"`
	Connected        bool          `json:"connected"`
	BluetoothAddr    string        `json:"bluetooth_address"`
	DeviceID         string        `json:"device_id"`
	InstanceID       string        `json:"instance_id"`
	Model            MouseModel    `json:"model"`
	BatteryLevel     int           `json:"battery_level"`
	BatteryAvailable bool          `json:"battery_available"`
	DriverInstalled  bool          `json:"driver_installed"`
	ChargingState    ChargingState `json:"charging_state"`
	LastSeen         time.Time     `json:"last_seen"`
	RSSI             int16         `json:"rssi"`
	LastError        string        `json:"last_error,omitempty"`
}

// GenerateDeviceID creates a unique stable ID from the Bluetooth address.
// Format: MMU-<sanitized BT address>
func GenerateDeviceID(btAddr string) string {
	if btAddr == "" {
		return "MMU-UNKNOWN"
	}
	clean := ""
	for _, c := range btAddr {
		if c != ':' {
			clean += string(c)
		}
	}
	return "MMU-" + clean
}

// InferModel tries to infer the Magic Mouse model from product ID or name
func InferModel(productID, name string) MouseModel {
	switch productID {
	case "030d", "030D":
		return ModelMagicMouse1
	case "0269":
		return ModelMagicMouse2
	case "0374":
		return ModelMagicMouse3
	}
	return ModelMagicMouse2
}
