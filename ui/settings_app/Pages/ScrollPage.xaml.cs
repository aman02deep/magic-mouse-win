using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Controls.Primitives;
using System;
using System.IO;
using System.Text.Json;
using System.Text.Json.Nodes;

namespace MagicMouse.Settings.Pages;

public sealed partial class ScrollPage : Page
{
    private static readonly string ConfigPath = Path.Combine(
        AppContext.BaseDirectory, "..", "..", "..", "..", "..", "configs", "default.json");

    private JsonObject _config = new();
    private bool _loading = false;

    public ScrollPage()
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

            var s = _config["scroll"]?.AsObject() ?? new JsonObject();

            NaturalScrollToggle.IsOn = s["natural_scroll"]?.GetValue<bool>()    ?? true;
            SpeedSlider.Value        = s["speed"]?.GetValue<double>()            ?? 5;
            InertiaToggle.IsOn       = s["inertia_enabled"]?.GetValue<bool>()   ?? true;
            FrictionSlider.Value     = s["inertia_friction"]?.GetValue<double>() ?? 4;
            CurveCombo.SelectedIndex = s["curve"]?.GetValue<int>()              ?? 1;

            InertiaPanel.Visibility  = InertiaToggle.IsOn ? Visibility.Visible : Visibility.Collapsed;
        }
        catch { /* silently use defaults */ }
        finally { _loading = false; }
    }

    private void SaveConfig()
    {
        if (_loading) return;
        try
        {
            _config["scroll"] = new JsonObject
            {
                ["natural_scroll"]   = NaturalScrollToggle.IsOn,
                ["speed"]            = SpeedSlider.Value,
                ["inertia_enabled"]  = InertiaToggle.IsOn,
                ["inertia_friction"] = FrictionSlider.Value,
                ["curve"]            = CurveCombo.SelectedIndex
            };

            var dir = Path.GetDirectoryName(ConfigPath)!;
            Directory.CreateDirectory(dir);
            File.WriteAllText(ConfigPath,
                JsonSerializer.Serialize(_config, new JsonSerializerOptions { WriteIndented = true }));
        }
        catch { }
    }

    // ── Event handlers ────────────────────────────────────────────────────────

    private void NaturalScroll_Toggled(object sender, RoutedEventArgs e) => SaveConfig();

    private void Inertia_Toggled(object sender, RoutedEventArgs e)
    {
        InertiaPanel.Visibility = InertiaToggle.IsOn ? Visibility.Visible : Visibility.Collapsed;
        SaveConfig();
    }

    private void Speed_Changed(object sender, RangeBaseValueChangedEventArgs e)   => SaveConfig();
    private void Friction_Changed(object sender, RangeBaseValueChangedEventArgs e) => SaveConfig();
    private void Curve_Changed(object sender, SelectionChangedEventArgs e)          => SaveConfig();

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
