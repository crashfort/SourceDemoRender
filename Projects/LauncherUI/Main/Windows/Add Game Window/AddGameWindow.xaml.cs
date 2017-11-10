using System;
using System.Collections.Generic;
using System.Windows;
using Microsoft.Win32;
using Microsoft.WindowsAPICodePack.Dialogs;

namespace LauncherUI
{
	public partial class AddGameWindow : Window
	{
		public class GameData
		{
			public string DisplayName;
			public string SDRPath;
			public string ExecutablePath;

			public override string ToString()
			{
				return DisplayName;
			}
		}

		private struct CurrentGameVerifyData
		{
			public string DisplayName;
			public string ExpectedExecutableName;
		};

		CurrentGameVerifyData CurrentVerifyGame = new CurrentGameVerifyData();

		public AddGameWindow()
		{
			InitializeComponent();
			ErrorText.Text = null;
		}

		private void OKButton_Click(object sender, RoutedEventArgs args)
		{
			var data = new GameData();
			data.DisplayName = CurrentVerifyGame.DisplayName;
			data.SDRPath = SDRDirTextBox.Text.Trim();
			data.ExecutablePath = GameExeTextBox.Text.Trim();

			try
			{
				OnGameAdded(this, data);
			}

			catch (Exception error)
			{
				ErrorText.Text = error.Message;
				return;
			}

			Close();
		}

		private void CancelButton_Click(object sender, RoutedEventArgs args)
		{
			Close();
		}

		public List<GameData> ExistingGames;
		public event EventHandler<GameData> OnGameAdded;

		private void SDRBrowse_Click(object sender, RoutedEventArgs args)
		{
			var dialog = new CommonOpenFileDialog();
			dialog.Title = "Select SDR Folder";
			dialog.IsFolderPicker = true;
			dialog.AddToMostRecentlyUsedList = false;
			dialog.AllowNonFileSystemItems = false;
			dialog.EnsureFileExists = true;
			dialog.EnsurePathExists = true;
			dialog.EnsureReadOnly = false;
			dialog.EnsureValidNames = true;
			dialog.Multiselect = false;
			dialog.ShowPlacesList = true;

			if (dialog.ShowDialog() == CommonFileDialogResult.Ok)
			{
				ErrorText.Text = null;
				OKButton.IsEnabled = false;
				GameExeGrid.IsEnabled = false;
				GameExeTextBox.Text = null;

				SDRDirTextBox.Text = dialog.FileName;

				string sdrpath = SDRDirTextBox.Text.Trim();

				try
				{
					if (sdrpath.Length == 0)
					{
						SDRDirTextBox.Focus();
						throw new Exception("SDR path is empty.");
					}

					if (!System.IO.Directory.Exists(sdrpath))
					{
						SDRDirTextBox.Focus();
						throw new Exception("Specified SDR path does not exist.");
					}

					var sdrpathinfo = new System.IO.DirectoryInfo(sdrpath);

					if (sdrpathinfo.Name != "SDR")
					{
						SDRDirTextBox.Focus();
						throw new Exception("Specified SDR path is not related to SDR.");
					}

					var files = new string[] { "SourceDemoRender.dll", "LauncherCLI.exe", "GameConfig.json" };

					foreach (var name in files)
					{
						if (!System.IO.File.Exists(System.IO.Path.Combine(sdrpath, name)))
						{
							SDRDirTextBox.Focus();

							var format = string.Format("File \"{0}\" does not exist in SDR folder.", name);
							throw new Exception(format);
						}
					}

					foreach (var game in ExistingGames)
					{
						if (game.SDRPath == sdrpath)
						{
							throw new Exception("Game is already added.");
						}
					}

					var configpath = System.IO.Path.Combine(sdrpath, "GameConfig.json");
					var content = System.IO.File.ReadAllText(configpath, System.Text.Encoding.UTF8);
					var document = System.Json.JsonValue.Parse(content);
					var parentdir = System.IO.Directory.GetParent(sdrpath);

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

					CurrentVerifyGame.DisplayName = gamejson["DisplayName"];

					if (!gamejson.ContainsKey("ExecutableName"))
					{
						var format = string.Format("Game config does not contain \"ExecutableName\" member for game \"{0}\".", CurrentVerifyGame.DisplayName);
						throw new Exception(format);
					}

					CurrentVerifyGame.ExpectedExecutableName = gamejson["ExecutableName"];
				}

				catch (Exception error)
				{
					ErrorText.Text = error.Message;
					return;
				}

				GameExeGrid.IsEnabled = true;
			}
		}

		private void ExeBrowse_Click(object sender, RoutedEventArgs args)
		{
			var dialog = new OpenFileDialog();
			dialog.Title = "Select Game Executable";
			dialog.Multiselect = false;
			dialog.Filter = "Executables (*.exe)|*.exe";

			var res = dialog.ShowDialog();

			if (res.Value)
			{
				ErrorText.Text = null;
				OKButton.IsEnabled = false;

				GameExeTextBox.Text = dialog.FileName;

				var exepath = GameExeTextBox.Text.Trim();

				try
				{
					if (exepath.Length == 0)
					{
						GameExeTextBox.Focus();
						throw new Exception("Executable path is empty.");
					}

					if (!System.IO.File.Exists(exepath))
					{
						GameExeTextBox.Focus();
						throw new Exception("Specified executable path does not exist.");
					}

					var fileinfo = new System.IO.FileInfo(exepath);

					if (!fileinfo.FullName.EndsWith(CurrentVerifyGame.ExpectedExecutableName))
					{
						var format = string.Format("Executable name for \"{0}\" should be \"{1}\".", CurrentVerifyGame.DisplayName, CurrentVerifyGame.ExpectedExecutableName);
						throw new Exception(format);
					}
				}

				catch (Exception error)
				{
					ErrorText.Text = error.Message;
					return;
				}

				OKButton.IsEnabled = true;
			}
		}
	}
}
