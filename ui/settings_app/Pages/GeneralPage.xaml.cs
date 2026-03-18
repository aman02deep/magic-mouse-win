using System;
using System.Collections.Generic;
using System.IO;
using System.IO.Pipes;
using System.Text;
using System.Text.Json;
using System.Text.Json.Nodes;
using System.Threading;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;
using System.Windows.Shapes;

namespace MagicMouse.Settings.Pages;

public partial class GeneralPage : UserControl
{
    private static readonly string ConfigPath = System.IO.Path.Combine(
        AppContext.BaseDirectory, "..", "..", "..", "..", "..", "configs", "default.json");

    private JsonObject _config = new();
    private bool _isLoaded = false;

    // Touch visualization
    private readonly Dictionary<int, Ellipse> _touchDots = new();
    private const double CanvasTouchLeft = 5;
    private const double CanvasTouchTop = 58;
    private const double CanvasTouchWidth = 100;
    private const double CanvasTouchHeight = 97;

    // IPC
    private CancellationTokenSource? _ipcCts;
    private Task? _ipcTask;

    public GeneralPage()
    {
        InitializeComponent();
        LoadConfig();
        VersionText.Text = System.Reflection.Assembly.GetExecutingAssembly()
            .GetName().Version?.ToString(3) ?? "0.1.5";
        Loaded += OnPageLoaded;
        Unloaded += OnPageUnloaded;
    }

    private void OnPageLoaded(object sender, RoutedEventArgs e)
    {
        _ipcCts = new CancellationTokenSource();
        _ipcTask = Task.Run(() => ListenIpcAsync(_ipcCts.Token));
    }

    private void OnPageUnloaded(object sender, RoutedEventArgs e)
    {
        _ipcCts?.Cancel();
    }

    // ──────────────────────────────────────────────────────
    // IPC listener — reads events pushed by the Rust service
    // ──────────────────────────────────────────────────────
    private async Task ListenIpcAsync(CancellationToken ct)
    {
        while (!ct.IsCancellationRequested)
        {
            try
            {
                using var pipe = new NamedPipeClientStream(
                    ".", "MagicMouseService",
                    PipeDirection.InOut, PipeOptions.Asynchronous);

                await pipe.ConnectAsync(3000, ct);

                Dispatcher.Invoke(() =>
                {
                    DeviceNameText.Text = "Aman's Magic Mouse";
                    DeviceNameText.Foreground = new SolidColorBrush(Color.FromRgb(0xAA, 0xAA, 0xFF));
                    BatteryText.Text = "Checking...";
                });

                using var reader = new StreamReader(pipe, Encoding.UTF8, leaveOpen: true);
                string? line;
                while (!ct.IsCancellationRequested &&
                       (line = await reader.ReadLineAsync(ct)) != null)
                {
                    HandleEvent(line);
                }
            }
            catch (OperationCanceledException) { break; }
            catch
            {
                // Service not running or disconnected — retry after 2s
                await Task.Delay(2000, ct).ContinueWith(_ => { });
                Dispatcher.Invoke(() =>
                {
                    DeviceNameText.Text = "No device connected";
                    DeviceNameText.Foreground = new SolidColorBrush(Color.FromRgb(0x88, 0x88, 0xAA));
                    BatteryText.Text = "--";
                    ClearAllDots();
                });
            }
        }
    }

    private void HandleEvent(string json)
    {
        try
        {
            var node = JsonNode.Parse(json);
            string? type = node?["type"]?.GetValue<string>();

            switch (type)
            {
                case "touch":
                    var fingerNodes = node?["fingers"]?.AsArray();
                    var fingers = new List<(double x, double y)>();
                    if (fingerNodes != null)
                    {
                        foreach (var fn in fingerNodes)
                        {
                            var fx = fn?["x"]?.GetValue<double>() ?? 0;
                            var fy = fn?["y"]?.GetValue<double>() ?? 0;
                            fingers.Add((fx, fy));
                        }
                    }
                    Dispatcher.Invoke(() => UpdateTouchDots(fingers));
                    break;

                case "battery":
                    var level = node?["level"]?.GetValue<int>() ?? -1;
                    var charging = node?["charging"]?.GetValue<bool>() ?? false;
                    Dispatcher.Invoke(() => UpdateBattery(level, charging));
                    break;
            }
        }
        catch { /* Ignore malformed events */ }
    }

    // ──────────────────────────────────────────────────────
    // Touch dot rendering
    // ──────────────────────────────────────────────────────
    private void UpdateTouchDots(List<(double x, double y)> fingers)
    {
        // Remove dots for fingers no longer present
        var toRemove = new List<int>();
        foreach (var id in _touchDots.Keys)
        {
            if (id >= fingers.Count)
                toRemove.Add(id);
        }
        foreach (var id in toRemove)
        {
            MouseCanvas.Children.Remove(_touchDots[id]);
            _touchDots.Remove(id);
        }

        // Add / update dots
        for (int i = 0; i < fingers.Count; i++)
        {
            var (nx, ny) = fingers[i];

            // Map 0..1 to the touch surface area of the canvas
            double cx = CanvasTouchLeft + nx * CanvasTouchWidth;
            double cy = CanvasTouchTop + ny * CanvasTouchHeight;

            if (!_touchDots.TryGetValue(i, out var dot))
            {
                dot = new Ellipse
                {
                    Width = 22, Height = 22,
                    Fill = new SolidColorBrush(Color.FromArgb(180, 100, 140, 255)),
                    Stroke = new SolidColorBrush(Color.FromRgb(150, 180, 255)),
                    StrokeThickness = 1.5
                };
                MouseCanvas.Children.Add(dot);
                _touchDots[i] = dot;
            }

            Canvas.SetLeft(dot, cx - dot.Width / 2);
            Canvas.SetTop(dot, cy - dot.Height / 2);
        }
    }

    private void ClearAllDots()
    {
        foreach (var dot in _touchDots.Values)
            MouseCanvas.Children.Remove(dot);
        _touchDots.Clear();
    }

    // ──────────────────────────────────────────────────────
    // Battery display
    // ──────────────────────────────────────────────────────
    private void UpdateBattery(int level, bool charging)
    {
        if (level < 0) { BatteryText.Text = "--"; return; }

        BatteryText.Text = $"{level}%";
        BatteryIcon.Text = level switch
        {
            >= 90 => "🔋",
            >= 60 => "🔋",
            >= 30 => "🪫",
            _ =>     "🪫"
        };
        ChargingText.Text = charging ? "⚡ Charging" : "";

        BatteryText.Foreground = level switch
        {
            >= 50 => new SolidColorBrush(Color.FromRgb(0x66, 0xDD, 0x88)),
            >= 20 => new SolidColorBrush(Color.FromRgb(0xFF, 0xCC, 0x44)),
            _ =>     new SolidColorBrush(Color.FromRgb(0xFF, 0x55, 0x55))
        };
    }

    // ──────────────────────────────────────────────────────
    // Config
    // ──────────────────────────────────────────────────────
    private void LoadConfig()
    {
        try
        {
            if (File.Exists(ConfigPath))
            {
                var text = File.ReadAllText(ConfigPath);
                _config = JsonNode.Parse(text)?.AsObject() ?? new JsonObject();
            }
            EnableToggle.IsChecked = _config["enabled"]?.GetValue<bool>() ?? true;
            StartupToggle.IsChecked = _config["run_at_startup"]?.GetValue<bool>() ?? true;
            _isLoaded = true;
        }
        catch (Exception ex)
        {
            ShowStatus(false, $"Could not load config: {ex.Message}");
        }
    }

    private void SaveConfig()
    {
        if (!_isLoaded) return;
        try
        {
            _config["enabled"] = EnableToggle.IsChecked == true;
            _config["run_at_startup"] = StartupToggle.IsChecked == true;

            var dir = System.IO.Path.GetDirectoryName(ConfigPath)!;
            Directory.CreateDirectory(dir);
            File.WriteAllText(ConfigPath,
                JsonSerializer.Serialize(_config, new JsonSerializerOptions { WriteIndented = true }));

            ShowStatus(true, "Settings saved.");
        }
        catch (Exception ex)
        {
            ShowStatus(false, $"Could not save config: {ex.Message}");
        }
    }

    private void ShowStatus(bool success, string message)
    {
        StatusBar.Foreground = new SolidColorBrush(success ? Colors.LightGreen : Colors.Salmon);
        StatusBar.Text = message;
        StatusBar.Visibility = Visibility.Visible;
    }

    private static void SetStartup(bool enable)
    {
        const string keyPath = @"SOFTWARE\Microsoft\Windows\CurrentVersion\Run";
        const string valueName = "MagicMouseTrayHost";
        using var key = Microsoft.Win32.Registry.CurrentUser.OpenSubKey(keyPath, writable: true);
        if (key is null) return;
        if (enable)
        {
            var exe = System.IO.Path.Combine(AppContext.BaseDirectory, "tray_host.exe");
            key.SetValue(valueName, $"\"{exe}\"");
        }
        else
        {
            key.DeleteValue(valueName, throwOnMissingValue: false);
        }
    }

    private void Toggle_Changed(object sender, RoutedEventArgs e) => SaveConfig();
    private void StartupToggle_Changed(object sender, RoutedEventArgs e)
    {
        SetStartup(StartupToggle.IsChecked == true);
        SaveConfig();
    }
}
