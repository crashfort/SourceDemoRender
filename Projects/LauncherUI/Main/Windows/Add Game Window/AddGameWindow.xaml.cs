using System;
using System.Windows;
using Microsoft.Win32;
using Microsoft.WindowsAPICodePack.Dialogs;

namespace LauncherUI
{
	public partial class AddGameWindow : Window
	{
		public struct GameAddData
		{
			public string ExecutablePath;
			public string SDRPath;
		}

		public AddGameWindow()
		{
			InitializeComponent();
			ErrorText.Text = "";
		}

		private void OKButton_Click(object sender, RoutedEventArgs args)
		{
			var data = new GameAddData();
			data.ExecutablePath = GameExeTextBox.Text.Trim();
			data.SDRPath = SDRDirTextBox.Text.Trim();

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

		public event EventHandler<GameAddData> OnGameAdded;

		private void ExeBrowse_Click(object sender, RoutedEventArgs args)
		{
			ErrorText.Text = "";

			var dialog = new OpenFileDialog();
			dialog.Title = "Select Game Executable";
			dialog.Multiselect = false;
			dialog.Filter = "Executables (*.exe)|*.exe";

			var res = dialog.ShowDialog();

			if (res.Value)
			{
				GameExeTextBox.Text = dialog.FileName;
			}
		}

		private void SDRBrowse_Click(object sender, RoutedEventArgs args)
		{
			ErrorText.Text = "";

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
				SDRDirTextBox.Text = dialog.FileName;
			}
		}
	}
}
