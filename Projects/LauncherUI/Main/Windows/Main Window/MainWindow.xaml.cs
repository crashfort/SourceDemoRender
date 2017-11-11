using System;
using System.Windows;
using System.Windows.Navigation;
using System.Diagnostics;
using System.Collections.Generic;
using System.Linq;

namespace LauncherUI
{
	public partial class MainWindow : Window
	{
		class SaveRestoreData
		{
			public List<AddGameWindow.GameData> Games;
			public int SelectedIndex = 0;
			public string LaunchParameters;
		}

		void AddGamesToList()
		{
			if (!System.IO.File.Exists("LauncherUIData.json"))
			{
				return;
			}

			var saverestore = new SaveRestoreData();

			var json = System.IO.File.ReadAllText("LauncherUIData.json", System.Text.Encoding.UTF8);

			saverestore = Newtonsoft.Json.JsonConvert.DeserializeObject<SaveRestoreData>(json);

			LaunchOptionsTextBox.Text = saverestore.LaunchParameters;

			foreach (var item in saverestore.Games)
			{
				GameComboBox.Items.Add(item);
			}

			GameComboBox.SelectedIndex = saverestore.SelectedIndex;
		}

		public MainWindow()
		{
			InitializeComponent();
			AddGamesToList();

			ErrorText.Text = null;
		}

		void SaveGames()
		{
			var saverestore = new SaveRestoreData();
			saverestore.Games = GameComboBox.Items.Cast<AddGameWindow.GameData>().ToList();
			saverestore.LaunchParameters = LaunchOptionsTextBox.Text;
			saverestore.SelectedIndex = GameComboBox.SelectedIndex;

			var json = Newtonsoft.Json.JsonConvert.SerializeObject(saverestore);

			System.IO.File.WriteAllText("LauncherUIData.json", json, System.Text.Encoding.UTF8);
		}

		void Window_Closing(object sender, System.ComponentModel.CancelEventArgs args)
		{
			SaveGames();
		}

		void Hyperlink_RequestNavigate(object sender, RequestNavigateEventArgs args)
		{
			Process.Start(new ProcessStartInfo(args.Uri.AbsoluteUri));
			args.Handled = true;
		}

		void LaunchButton_Click(object sender, RoutedEventArgs args)
		{
			var options = LaunchOptionsTextBox.Text.Trim();
			var game = (AddGameWindow.GameData)GameComboBox.SelectedItem;
			var sdrpath = game.SDRPath;
			var exepath = game.ExecutablePath;

			if (!System.IO.Directory.Exists(sdrpath))
			{
				ErrorText.Text = "Game SDR path does not exist anymore.";
				return;
			}

			if (!System.IO.File.Exists(exepath))
			{
				ErrorText.Text = "Game executable path does not exist anymore.";
				return;
			}

			if (!System.IO.File.Exists(System.IO.Path.Combine(sdrpath, "SourceDemoRender.dll")))
			{
				ErrorText.Text = "SourceDemoRender.dll does not exist in SDR folder.";
				return;
			}

			var launcher = System.IO.Path.Combine(sdrpath, "LauncherCLI.exe");

			if (!System.IO.File.Exists(launcher))
			{
				ErrorText.Text = "LauncherCLI.exe does not exist in SDR folder.";
				return;
			}

			var startparams = string.Format("\"{0}\" {1}", exepath, options);

			var info = new ProcessStartInfo(launcher, startparams);
			info.WorkingDirectory = sdrpath;

			Process.Start(info);

			ErrorText.Text = null;
		}

		void AddGameButton_Click(object sender, RoutedEventArgs args)
		{
			var dialog = new AddGameWindow();
			dialog.Owner = this;

			dialog.ExistingGames = GameComboBox.Items.Cast<AddGameWindow.GameData>().ToList();
			dialog.OnGameAdded += OnGameAdded;

			dialog.ShowDialog();

			ErrorText.Text = null;
		}

		void RemoveGameButton_Click(object sender, RoutedEventArgs args)
		{
			ErrorText.Text = null;

			if (GameComboBox.SelectedItem == null)
			{
				return;
			}

			var index = GameComboBox.SelectedIndex;

			GameComboBox.Items.RemoveAt(index);
			GameComboBox.SelectedIndex = Math.Max(index - 1, 0);

			SaveGames();
		}

		void OnGameAdded(object sender, AddGameWindow.GameData args)
		{
			var index = GameComboBox.Items.Add(args);
			GameComboBox.SelectedIndex = index;

			SaveGames();
		}

		void GameComboBox_SelectionChanged(object sender, System.Windows.Controls.SelectionChangedEventArgs args)
		{
			ErrorText.Text = null;

			if (GameComboBox.Items.IsEmpty)
			{
				GameComboBox.ToolTip = null;
				return;
			}

			var obj = (AddGameWindow.GameData)GameComboBox.SelectedItem;

			/*
				This event gets called twice on removal, only use the second time.
			*/
			if (obj != null)
			{
				GameComboBox.ToolTip = string.Format("{0}\n\nExecutable\n{1}\n\nSDR\n{2}", obj.DisplayName, obj.ExecutablePath, obj.SDRPath);
			}
		}
	}
}
