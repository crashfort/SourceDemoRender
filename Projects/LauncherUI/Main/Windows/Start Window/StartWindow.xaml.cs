using System;
using System.Runtime.InteropServices;
using System.Threading.Tasks;
using System.Windows;
using System.Net.Http;
using System.Diagnostics;

namespace LauncherUI
{
	public partial class StartWindow : Window
	{
		[DllImport("SourceDemoRender.dll", CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
		static extern int SDR_LibraryVersion();

		int LocalVersion = SDR_LibraryVersion();
		HttpClient NetClient = new HttpClient();

		public StartWindow()
		{
			InitializeComponent();
			MainProcedure();
		}

		async Task<int> GetGitHubLibraryVersion()
		{
			var webstr = await NetClient.GetStringAsync("https://raw.githubusercontent.com/crashfort/SourceDemoRender/master/Version/Latest");
			return int.Parse(webstr);
		}

		async Task<string> GetGitHubGameConfig()
		{
			var webstr = await NetClient.GetStringAsync("https://raw.githubusercontent.com/crashfort/SourceDemoRender/master/Output/SDR/GameConfig.json");
			return webstr;
		}

		void HideProgress()
		{
			Progress.IsIndeterminate = false;
			Progress.Visibility = Visibility.Hidden;
		}

		async void MainProcedure()
		{
			bool autoskip = false;

			try
			{
				var webver = await GetGitHubLibraryVersion();

				string finalstr = "";

				if (LocalVersion == webver)
				{
					finalstr = "Using the latest library version.";
					autoskip = true;
				}

				else if (webver > LocalVersion)
				{
					finalstr = string.Format("Library update is available from {0} to {1}. Press Upgrade to view release.", LocalVersion, webver);
					UpgradeButton.IsEnabled = true;
				}

				bool downloadcfg = true;

				if (System.IO.File.Exists("GameConfig.json"))
				{
					var fileinfo = new System.IO.FileInfo("GameConfig.json");

					if (fileinfo.IsReadOnly)
					{
						finalstr += " Game config is set to read only so it will not be updated.";
						autoskip = false;

						downloadcfg = false;
					}
				}

				if (downloadcfg)
				{
					var webconfig = await GetGitHubGameConfig();
					System.IO.File.WriteAllText("GameConfig.json", webconfig, new System.Text.UTF8Encoding(false));

					finalstr += " Latest game config was downloaded.";
				}

				StatusText.Text = finalstr;
				HideProgress();
			}

			catch (Exception error)
			{
				StatusText.Text = error.Message;
				HideProgress();

				autoskip = false;
			}

			StartButton.IsEnabled = true;

			if (autoskip)
			{
				ProceedToMainWindow();
			}
		}

		void ProceedToMainWindow()
		{
			var dialog = new MainWindow();
			dialog.Show();

			Close();
		}

		void UpgradeButton_Click(object sender, RoutedEventArgs args)
		{
			Process.Start(new ProcessStartInfo("https://github.com/crashfort/SourceDemoRender/releases"));
			Close();
		}

		void StartButton_Click(object sender, RoutedEventArgs args)
		{
			ProceedToMainWindow();
		}
	}
}
