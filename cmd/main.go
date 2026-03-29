package main

import (
	"flag"
	"fmt"
	"log"
	"os"
	"os/exec"
	"os/signal"
	"path/filepath"
	"runtime"
	"syscall"

	"magicmouse-util/internal/bluetooth"
	"magicmouse-util/internal/server"
	"magicmouse-util/internal/tray"
)

var version = "0.2.0"

func main() {
	showVersion := flag.Bool("version", false, "Print version and exit")
	noTray := flag.Bool("no-tray", false, "Run without system tray (headless/server mode)")
	port := flag.String("port", ":7878", "Web dashboard port")
	noBrowser := flag.Bool("no-browser", false, "Don't auto-open browser on start")
	flag.Parse()

	if *showVersion {
		fmt.Printf("MagicMouseUtil v%s\n", version)
		os.Exit(0)
	}

	// Setup logging — write to %TEMP% for portability (exe dir might be read-only)
	logPath := filepath.Join(os.TempDir(), "magicmouse.log")
	logFile, err := os.OpenFile(logPath, os.O_CREATE|os.O_WRONLY|os.O_APPEND, 0644)
	if err == nil {
		log.SetOutput(logFile)
		defer logFile.Close()
	}
	log.SetFlags(log.LstdFlags | log.Lshortfile)
	log.Printf("MagicMouse Utility v%s starting...", version)
	log.Printf("Log file: %s", logPath)

	// Start Bluetooth monitor
	btMonitor := bluetooth.NewMonitor()
	go btMonitor.Start()

	// Start web server
	webServer := server.New(btMonitor)
	go func() {
		if err := webServer.Start(*port); err != nil {
			log.Fatalf("Web server error: %v", err)
		}
	}()

	// Auto-open browser
	if !*noBrowser {
		go func() {
			// Small delay to let server start
			sleepMs(500)
			openBrowser(fmt.Sprintf("http://localhost%s", *port))
		}()
	}

	if *noTray {
		// Headless mode — wait for OS signal
		log.Println("Running in headless mode (no tray)")
		quit := make(chan os.Signal, 1)
		signal.Notify(quit, syscall.SIGINT, syscall.SIGTERM)
		<-quit
	} else {
		// System tray mode (blocks on message pump)
		log.Println("Starting system tray")
		trayApp := tray.New(btMonitor)
		trayApp.Start()
	}

	btMonitor.Stop()
	log.Println("MagicMouse Utility shut down.")
}

func openBrowser(url string) {
	var cmd *exec.Cmd
	switch runtime.GOOS {
	case "windows":
		cmd = exec.Command("rundll32", "url.dll,FileProtocolHandler", url)
	default:
		cmd = exec.Command("xdg-open", url)
	}
	if err := cmd.Start(); err != nil {
		log.Printf("Failed to open browser: %v", err)
	}
}

func sleepMs(ms int) {
	k32 := syscall.NewLazyDLL("kernel32.dll")
	k32.NewProc("Sleep").Call(uintptr(ms))
}
