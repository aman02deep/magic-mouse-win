using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Controls.Primitives;
using System;
using System.IO;
using System.Text.Json;
using System.Text.Json.Nodes;

namespace MagicMouse.Settings.Pages;

public sealed partial class GesturesPage : Page
{
    private static readonly string ConfigPath = Path.Combine(
        AppContext.BaseDirectory, "..", "..", "..", "..", "..", "configs", "default.json");

    private JsonObject _config = new();
    private bool _loading = false;

    public GesturesPage()
    {
        this.InitializeComponent();
        LoadConfig();
    }

    // ── Config I/O ────────────────────────────────────────────────────────────

    private void LoadConfig()
    {
        _loading = true;
        try
        {
            if (File.Exists(ConfigPath))
            {
                var text = File.ReadAllText(ConfigPath);
                _config = JsonNode.Parse(text)?.AsObject() ?? new JsonObject();
            }

            var g = _config["gestures"]?.AsObject() ?? new JsonObject();

            SwipeEnabled.IsOn           = g["swipe_enabled"]?.GetValue<bool>()   ?? true;
            SwipeSensitivity.Value      = g["swipe_sensitivity"]?.GetValue<double>() ?? 5;
            ThreeFingerEnabled.IsOn     = g["three_finger_enabled"]?.GetValue<bool>() ?? true;
            ThreeFingerSensitivity.Value = g["three_finger_sensitivity"]?.GetValue<double>() ?? 5;
            TapClickEnabled.IsOn        = g["tap_click_enabled"]?.GetValue<bool>() ?? false;
            SmartZoomEnabled.IsOn       = g["smart_zoom_enabled"]?.GetValue<bool>() ?? true;
        }
        catch { /* silently use defaults */ }
        finally { _loading = false; }
    }

    private void SaveConfig()
    {
        if (_loading) return;
        try
        {
            _config["gestures"] = new JsonObject
            {
                ["swipe_enabled"]            = SwipeEnabled.IsOn,
                ["swipe_sensitivity"]         = SwipeSensitivity.Value,
                ["three_finger_enabled"]      = ThreeFingerEnabled.IsOn,
                ["three_finger_sensitivity"]  = ThreeFingerSensitivity.Value,
                ["tap_click_enabled"]         = TapClickEnabled.IsOn,
                ["smart_zoom_enabled"]        = SmartZoomEnabled.IsOn
            };

            var dir = Path.GetDirectoryName(ConfigPath)!;
            Directory.CreateDirectory(dir);
            File.WriteAllText(ConfigPath,
                JsonSerializer.Serialize(_config, new JsonSerializerOptions { WriteIndented = true }));
        }
        catch { /* log via real logger in a future refactor */ }
    }

    // ── Event handlers ────────────────────────────────────────────────────────

    private void Gesture_Toggled(object sender, RoutedEventArgs e)   => SaveConfig();
    private void Sensitivity_Changed(object sender, RangeBaseValueChangedEventArgs e) => SaveConfig();

    private void SaveButton_Click(object sender, RoutedEventArgs e)
    {
        SaveConfig();
        var btn = (Button)sender;
        btn.Content = "Saved ✓";
        DispatcherQueue.TryEnqueue(async () =>
        {
            await System.Threading.Tasks.Task.Delay(1500);
            btn.Content = "Save changes";
        });
    }
}
