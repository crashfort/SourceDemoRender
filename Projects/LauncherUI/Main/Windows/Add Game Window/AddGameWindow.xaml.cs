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
			public string GamePath;
			public string ExecutablePath;

			public override string ToString()
			{
				return DisplayName;
			}
		}

		class CurrentGameVerifyData
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

		void OKButton_Click(object sender, RoutedEventArgs args)
		{
			var data = new GameData();
			data.DisplayName = CurrentVerifyGame.DisplayName;
			data.GamePath = GameDirTextBox.Text;
			data.ExecutablePath = GameExeTextBox.Text;

			OnGameAdded(this, data);
			Close();
		}

		void CancelButton_Click(object sender, RoutedEventArgs args)
		{
			Close();
		}

		public List<GameData> ExistingGames;
		public event EventHandler<GameData> OnGameAdded;

		void OnGameDirectorySelected(string gamepath)
		{
			ErrorText.Text = null;
			OKButton.IsEnabled = false;
			GameExeGrid.IsEnabled = false;
			GameExeTextBox.Text = null;

			try
			{
				if (gamepath.Length == 0)
				{
					throw new Exception("Game path text is empty.");
				}

				if (!System.IO.Directory.Exists(gamepath))
				{
					throw new Exception("Specified game path does not exist.");
				}

				foreach (var game in ExistingGames)
				{
					if (game.GamePath == gamepath)
					{
						throw new Exception("Game is already added.");
					}
				}

				var configpath = System.IO.Path.Combine("GameConfig.json");
				var content = System.IO.File.ReadAllText(configpath, new System.Text.UTF8Encoding(false));
				var document = System.Json.JsonValue.Parse(content);
				var dirinfo = new System.IO.DirectoryInfo(gamepath);

				var searcher = string.Format("{0}\\{1}", dirinfo.Parent.Name, dirinfo.Name);

				if (!document.ContainsKey(searcher))
				{
					var format = string.Format("Game \"{0}\" does not exist in game config.", searcher);
					throw new Exception(format);
				}

				var gamejson = document[searcher];

				if (!gamejson.ContainsKey("DisplayName"))
				{
					var format = string.Format("Game config does not contain \"DisplayName\" member for game \"{0}\".", searcher);
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

			GameDirTextBox.Text = gamepath;
			GameExeGrid.IsEnabled = true;
		}

		void GamePathBrowse_Click(object sender, RoutedEventArgs args)
		{
			var dialog = new CommonOpenFileDialog();
			dialog.Title = "Select Game Directory";
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
				OnGameDirectorySelected(dialog.FileName.Trim());
			}
		}

		void OnGameExecutableSelected(string exepath)
		{
			ErrorText.Text = null;
			OKButton.IsEnabled = false;

			try
			{
				if (exepath.Length == 0)
				{
					throw new Exception("Executable path is empty.");
				}

				if (!System.IO.File.Exists(exepath))
				{
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

			GameExeTextBox.Text = exepath;
			OKButton.IsEnabled = true;
		}

		void ExeBrowse_Click(object sender, RoutedEventArgs args)
		{
			var dialog = new OpenFileDialog();
			dialog.Title = "Select Game Executable";
			dialog.Multiselect = false;

			var targetname = System.IO.Path.GetFileName(CurrentVerifyGame.ExpectedExecutableName);
			dialog.Filter = string.Format("{0} ({1})|{1}", CurrentVerifyGame.DisplayName, targetname);

			var res = dialog.ShowDialog();

			if (res.Value)
			{
				OnGameExecutableSelected(dialog.FileName.Trim());
			}
		}
	}
}
