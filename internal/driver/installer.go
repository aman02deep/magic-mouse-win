package driver

import (
	"embed"
	"fmt"
	"io/fs"
	"log"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"sync"
	"syscall"
)

//go:embed assets/drivers/amd64/*
var driverAssets embed.FS

// cachedInstalled caches the driver check result so we don't spawn pnputil on every SSE poll
var (
	cachedInstalled *bool
	cacheMu         sync.Mutex
)

// IsDriverInstalled checks if the BootCamp driver is installed.
// Result is cached after the first check — call InvalidateCache() after installation.
func IsDriverInstalled() bool {
	cacheMu.Lock()
	defer cacheMu.Unlock()

	if cachedInstalled != nil {
		return *cachedInstalled
	}

	result := checkDriverInstalled()
	cachedInstalled = &result
	return result
}

// InvalidateCache forces the next IsDriverInstalled() call to re-check from the OS.
func InvalidateCache() {
	cacheMu.Lock()
	defer cacheMu.Unlock()
	cachedInstalled = nil
}

// checkDriverInstalled does the actual OS-level check (hidden window, no flash).
func checkDriverInstalled() bool {
	// Check if the Apple driver .sys file is installed in System32
	driverPaths := []string{
		`C:\Windows\System32\drivers\AppleWirelessMouse.sys`,
		`C:\Windows\System32\drivers\AppleWirelessMouse64.sys`,
	}
	for _, p := range driverPaths {
		if _, err := os.Stat(p); err == nil {
			return true
		}
	}

	// Fallback: query pnputil but with hidden window
	cmd := exec.Command("pnputil", "/enum-drivers")
	cmd.SysProcAttr = &syscall.SysProcAttr{
		HideWindow:    true,
		CreationFlags: 0x08000000,
	}
	out, err := cmd.Output()
	if err != nil {
		return false
	}
	return strings.Contains(string(out), "AppleWirelessMouse.inf")
}

// InstallDriver extracts the embedded signed driver files to a temporary directory
// and uses pnputil to install them. This requires Administrator privileges.
func InstallDriver() error {
	log.Println("[Installer] Beginning Magic Mouse Driver installation sequence...")

	// 1. Create a secure temporary directory
	tempDir, err := os.MkdirTemp("", "magicmouse-driver-*")
	if err != nil {
		return fmt.Errorf("failed to create temp dir: %w", err)
	}
	defer os.RemoveAll(tempDir)

	// 2. Extract embedded files to the temporary directory
	entries, err := fs.ReadDir(driverAssets, "assets/drivers/amd64")
	if err != nil {
		return fmt.Errorf("failed to read embedded driver assets: %w", err)
	}

	infPath := ""

	for _, entry := range entries {
		if entry.IsDir() {
			continue
		}

		filePath := "assets/drivers/amd64/" + entry.Name()
		data, err := driverAssets.ReadFile(filePath)
		if err != nil {
			return fmt.Errorf("failed to read embedded file %s: %w", entry.Name(), err)
		}

		destPath := filepath.Join(tempDir, entry.Name())
		if err := os.WriteFile(destPath, data, 0644); err != nil {
			return fmt.Errorf("failed to write %s to temp dir: %w", entry.Name(), err)
		}

		if strings.HasSuffix(strings.ToLower(entry.Name()), ".inf") {
			infPath = destPath
		}
	}

	if infPath == "" {
		return fmt.Errorf("could not find .inf file in embedded assets")
	}

	log.Printf("[Installer] Extracted driver files to %s. Running pnputil...", tempDir)

	// 3. Install using PnPUtil (hidden window, requires Admin)
	cmd := exec.Command("pnputil", "/add-driver", infPath, "/install")
	cmd.SysProcAttr = &syscall.SysProcAttr{
		HideWindow:    true,
		CreationFlags: 0x08000000, // CREATE_NO_WINDOW
	}
	out, err := cmd.CombinedOutput()

	log.Printf("[Installer] pnputil output:\n%s", string(out))

	if err != nil {
		return fmt.Errorf("pnputil failed (ensure you are running as Administrator): %w\n%s", err, string(out))
	}

	// Force re-check next time IsDriverInstalled is called
	InvalidateCache()

	log.Println("[Installer] Driver successfully installed!")
	return nil
}
