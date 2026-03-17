using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using MagicMouse.Settings.Pages;

namespace MagicMouse.Settings;

public sealed partial class MainWindow : Window
{
    public MainWindow()
    {
        this.InitializeComponent();
        this.Title = "Magic Mouse Settings";

        // Navigate to General page by default.
        ContentFrame.Navigate(typeof(GeneralPage));
        NavView.SelectedItem = NavView.MenuItems[0];
    }

    private void NavView_SelectionChanged(NavigationView sender,
                                          NavigationViewSelectionChangedEventArgs args)
    {
        if (args.SelectedItem is NavigationViewItem item)
        {
            Type? pageType = item.Tag?.ToString() switch
            {
                "General"  => typeof(GeneralPage),
                "Gestures" => typeof(GesturesPage),
                "Scroll"   => typeof(ScrollPage),
                _          => null
            };

            if (pageType is not null)
                ContentFrame.Navigate(pageType);
        }
    }
}
