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
		class GameData
		{
			public string DisplayName;
			public AddGameWindow.GameAddData Details;

			public override string ToString()
			{
				return DisplayName;
			}
		}

		class SaveRestoreData
		{
			public List<GameData> Games;
			public int SelectedIndex = 0;
			public string LaunchParameters;
		}

		private void AddGamesToList()
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

			ErrorText.Text = "";
		}

		private void SaveGames()
		{
			var saverestore = new SaveRestoreData();
			saverestore.Games = GameComboBox.Items.Cast<GameData>().ToList();
			saverestore.LaunchParameters = LaunchOptionsTextBox.Text;
			saverestore.SelectedIndex = GameComboBox.SelectedIndex;

			var json = Newtonsoft.Json.JsonConvert.SerializeObject(saverestore);

			System.IO.File.WriteAllText("LauncherUIData.json", json, System.Text.Encoding.UTF8);
		}

		private void Window_Closing(object sender, System.ComponentModel.CancelEventArgs args)
		{
			SaveGames();
		}

		private void Hyperlink_RequestNavigate(object sender, RequestNavigateEventArgs args)
		{
			Process.Start(new ProcessStartInfo(args.Uri.AbsoluteUri));
			args.Handled = true;
		}

		private void LaunchButton_Click(object sender, RoutedEventArgs args)
		{
			var options = LaunchOptionsTextBox.Text.Trim();
			var game = (GameData)GameComboBox.SelectedItem;
			var sdrpath = game.Details.SDRPath;
			var exepath = game.Details.ExecutablePath;

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

			ErrorText.Text = "";
		}

		private void AddGameButton_Click(object sender, RoutedEventArgs args)
		{
			ErrorText.Text = "";

			var dialog = new AddGameWindow();
			dialog.Owner = this;

			dialog.OnGameAdded += OnGameAdded;

			dialog.ShowDialog();
		}

		private void RemoveGameButton_Click(object sender, RoutedEventArgs args)
		{
			ErrorText.Text = "";

			if (GameComboBox.SelectedItem == null)
			{
				return;
			}

			var index = GameComboBox.SelectedIndex;

			GameComboBox.Items.RemoveAt(index);
			GameComboBox.SelectedIndex = Math.Max(index - 1, 0);

			SaveGames();
		}

		private bool IsGameAddedAlready(AddGameWindow.GameAddData args)
		{
			foreach (var item in GameComboBox.Items)
			{
				var game = (GameData)item;

				if (game.Details.SDRPath == args.SDRPath)
				{
					return true;
				}
			}

			return false;
		}

		private void OnGameAdded(object sender, AddGameWindow.GameAddData args)
		{
			var dialog = (AddGameWindow)sender;

			args.SDRPath = args.SDRPath.Trim();
			args.ExecutablePath = args.ExecutablePath.Trim();

			if (args.SDRPath.Length == 0)
			{
				dialog.SDRDirTextBox.Focus();
				throw new Exception("SDR path is empty.");
			}

			if (args.ExecutablePath.Length == 0)
			{
				dialog.GameExeTextBox.Focus();
				throw new Exception("Executable path is empty.");
			}

			if (IsGameAddedAlready(args))
			{
				throw new Exception("Game is already added.");
			}

			if (!System.IO.Directory.Exists(args.SDRPath))
			{
				dialog.SDRDirTextBox.Focus();
				throw new Exception("Specified SDR path does not exist.");
			}

			if (!System.IO.File.Exists(args.ExecutablePath))
			{
				dialog.GameExeTextBox.Focus();
				throw new Exception("Specified executable path does not exist.");
			}

			var sdrpathinfo = new System.IO.DirectoryInfo(args.SDRPath);

			if (sdrpathinfo.Name != "SDR")
			{
				dialog.SDRDirTextBox.Focus();
				throw new Exception("Specified SDR path is not related to SDR.");
			}

			var files = new string[] { "SourceDemoRender.dll", "LauncherCLI.exe", "GameConfig.json" };

			foreach (var name in files)
			{
				if (!System.IO.File.Exists(System.IO.Path.Combine(args.SDRPath, name)))
				{
					dialog.SDRDirTextBox.Focus();

					var format = string.Format("File \"{0}\" does not exist in SDR folder.", name);
					throw new Exception(format);
				}
			}

			var configpath = System.IO.Path.Combine(args.SDRPath, "GameConfig.json");
			var content = System.IO.File.ReadAllText(configpath, System.Text.Encoding.UTF8);
			var document = System.Json.JsonValue.Parse(content);
			var parentdir = System.IO.Directory.GetParent(args.SDRPath);

			if (!document.ContainsKey(parentdir.Name))
			{
				var format = string.Format("Game \"{0}\" does not exist in game config.", parentdir.Name);
				throw new Exception(format);
			}

			var gamejson = document[parentdir.Name];

			if (!gamejson.ContainsKey("DisplayName"))
			{
				var format = string.Format("Game config does not contain \"DisplayName\" member for game \"{0}\".", parentdir.Name);
				throw new Exception(format);
			}

			string displayname = gamejson["DisplayName"];

			if (!gamejson.ContainsKey("ExecutableName"))
			{
				var format = string.Format("Game config does not contain \"ExecutableName\" member for game \"{0}\".", displayname);
				throw new Exception(format);
			}

			string exename = gamejson["ExecutableName"];

			var fileinfo = new System.IO.FileInfo(args.ExecutablePath);

			if (!fileinfo.FullName.EndsWith(exename))
			{
				var format = string.Format("Executable name for \"{0}\" should be \"{1}\".", displayname, exename);
				throw new Exception(format);
			}

			var gamedata = new GameData();
			gamedata.Details = args;
			gamedata.DisplayName = displayname;

			var index = GameComboBox.Items.Add(gamedata);
			GameComboBox.SelectedIndex = index;

			SaveGames();
		}

		private void GameComboBox_SelectionChanged(object sender, System.Windows.Controls.SelectionChangedEventArgs args)
		{
			ErrorText.Text = "";

			if (GameComboBox.Items.IsEmpty)
			{
				GameComboBox.ToolTip = null;
				return;
			}

			var obj = (GameData)GameComboBox.SelectedItem;

			/*
				This event gets called twice on removal, only use the second time.
			*/
			if (obj != null)
			{
				GameComboBox.ToolTip = string.Format("{0}\n\nExecutable\n{1}\n\nSDR\n{2}", obj.DisplayName, obj.Details.ExecutablePath, obj.Details.SDRPath);
			}
		}
	}
}
