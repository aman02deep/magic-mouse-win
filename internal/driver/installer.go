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
)

//go:embed assets/drivers/amd64/*
var driverAssets embed.FS

// IsDriverInstalled checks if the Precision Touchpad / BootCamp driver is present in the Windows Driver Store
func IsDriverInstalled() bool {
	// A simple heuristic is to check if the driver file exists in System32 or if pnputil lists it.
	// Since installing via pnputil places it in the driver store, let's query pnputil.
	out, err := exec.Command("pnputil", "/enum-drivers").Output()
	if err != nil {
		return false
	}
	return strings.Contains(string(out), "AmtPtpDevice.inf") || strings.Contains(string(out), "AppleWirelessMouse.inf")
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
	defer os.RemoveAll(tempDir) // Clean up after install

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

	// 3. Install using PnPUtil (Requires Admin)
	cmd := exec.Command("pnputil", "/add-driver", infPath, "/install")
	out, err := cmd.CombinedOutput()
	
	log.Printf("[Installer] pnputil output:\n%s", string(out))

	if err != nil {
		return fmt.Errorf("pnputil failed (Ensure you are running as Administrator): %w\n%s", err, string(out))
	}

	log.Println("[Installer] Driver successfully installed!")
	return nil
}
