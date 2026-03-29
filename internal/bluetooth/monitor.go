package bluetooth

import (
	"fmt"
	"log"
	"strings"
	"sync"
	"time"

	"magicmouse-util/internal/device"
)

const (
	appleCompanyID = 0x004C // Apple Inc. Bluetooth SIG company identifier
)

// Monitor polls for Magic Mouse info using BLE scanning + PowerShell fallback
type Monitor struct {
	mu       sync.RWMutex
	mouse    *device.MagicMouse
	interval time.Duration
	stopCh   chan struct{}
}

// NewMonitor creates a new Bluetooth monitor
func NewMonitor() *Monitor {
	return &Monitor{
		interval: 10 * time.Second,
		stopCh:   make(chan struct{}),
	}
}

// Start begins polling for device info
func (m *Monitor) Start() {
	log.Println("Bluetooth monitor started (PowerShell mode)")

	// Do an initial PowerShell poll
	m.enrichWithPowerShell()

	ticker := time.NewTicker(m.interval)
	defer ticker.Stop()
	for {
		select {
		case <-ticker.C:
			m.enrichWithPowerShell()
		case <-m.stopCh:
			log.Println("Bluetooth monitor stopped")
			return
		}
	}
}

// enrichWithPowerShell adds battery/instanceID info via Windows WMI/PnP APIs
func (m *Monitor) enrichWithPowerShell() {
	dev := queryPnpDevice()
	if dev == nil {
		m.mu.Lock()
		if m.mouse != nil {
			m.mouse.Connected = false
		}
		m.mu.Unlock()
		return
	}

	m.mu.Lock()
	if m.mouse == nil {
		btAddr := extractBluetoothAddr(dev.InstanceID)
		m.mouse = &device.MagicMouse{
			Name:          dev.FriendlyName,
			BluetoothAddr: btAddr,
			DeviceID:      device.GenerateDeviceID(btAddr),
			Model:         device.ModelMagicMouse2,
			LastSeen:      time.Now(),
		}
	}
	m.mouse.InstanceID = dev.InstanceID
	m.mouse.Connected = dev.Status == "OK" || dev.Present
	if m.mouse.Name == "" {
		m.mouse.Name = dev.FriendlyName
	}
	if m.mouse.BluetoothAddr == "" {
		btAddr := extractBluetoothAddr(dev.InstanceID)
		m.mouse.BluetoothAddr = btAddr
		m.mouse.DeviceID = device.GenerateDeviceID(btAddr)
	}
	m.mu.Unlock()

	// This operation is extremely slow (takes up to 18 seconds on Windows)
	// so we did the fast device status update above, and now do battery.
	battery, charging := ReadBattery(dev.InstanceID)

	m.mu.Lock()
	defer m.mu.Unlock()
	if battery >= 0 && battery <= 100 {
		m.mouse.BatteryLevel = battery
		m.mouse.BatteryAvailable = true
		m.mouse.ChargingState = charging
	} else {
		m.mouse.BatteryLevel = 0
		m.mouse.BatteryAvailable = false
		m.mouse.ChargingState = device.StateUnknown
	}
}

// Stop halts the monitor
func (m *Monitor) Stop() {
	close(m.stopCh)
}

// GetMouse returns the current mouse state (thread-safe)
func (m *Monitor) GetMouse() *device.MagicMouse {
	m.mu.RLock()
	defer m.mu.RUnlock()
	if m.mouse == nil {
		return nil
	}
	copy := *m.mouse
	return &copy
}

// isMagicMouse checks if a BLE device name matches known Magic Mouse names
func isMagicMouse(name string) bool {
	if name == "" {
		return false
	}
	lower := strings.ToLower(name)
	return strings.Contains(lower, "magic mouse") ||
		strings.Contains(lower, "apple mouse") ||
		(strings.Contains(lower, "mouse") && strings.Contains(lower, "aman"))
}

// extractBluetoothAddr pulls the MAC address from a Windows PnP InstanceID
func extractBluetoothAddr(instanceID string) string {
	upper := strings.ToUpper(instanceID)

	if idx := strings.Index(upper, "DEV_"); idx != -1 {
		raw := upper[idx+4:]
		addr := ""
		for _, c := range raw {
			if (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') {
				addr += string(c)
			} else {
				break
			}
		}
		if len(addr) == 12 {
			return formatMAC(addr)
		}
	}

	parts := strings.Split(upper, "&")
	for i := len(parts) - 1; i >= 0; i-- {
		seg := parts[i]
		if idx := strings.Index(seg, "_"); idx != -1 {
			seg = seg[:idx]
		}
		seg = strings.TrimSpace(seg)
		if len(seg) == 12 && isHex(seg) {
			return formatMAC(seg)
		}
	}
	return ""
}

func formatMAC(s string) string {
	s = strings.ToUpper(s)
	mac := ""
	for i := 0; i < 12; i += 2 {
		if i > 0 {
			mac += ":"
		}
		mac += s[i : i+2]
	}
	return mac
}

func isHex(s string) bool {
	for _, c := range s {
		if !((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f')) {
			return false
		}
	}
	return true
}

func parseIntFromString(s string) (int, error) {
	s = strings.TrimSpace(s)
	val := 0
	_, err := fmt.Sscanf(s, "%d", &val)
	return val, err
}
