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

		string UpdateBranch = "master";

		public StartWindow()
		{
			if (ShouldUpdateSkip())
			{
				ProceedToMainWindow();
				return;
			}

			InitializeComponent();

			CheckBranch();

			MainProcedure();
		}

		bool ShouldUpdateSkip()
		{
			if (System.IO.File.Exists("LauncherUI_UpdateSkip"))
			{
				return true;
			}

			return false;
		}

		void CheckBranch()
		{
			if (System.IO.File.Exists("LauncherUI_UpdateBranch"))
			{
				UpdateBranch = System.IO.File.ReadAllText("LauncherUI_UpdateBranch", new System.Text.UTF8Encoding(false));
			}
		}

		string BuildGitHubPath(string file)
		{
			return string.Format("https://raw.githubusercontent.com/crashfort/SourceDemoRender/{0}/{1}", UpdateBranch, file);
		}

		async Task<int> GetGitHubLibraryVersion()
		{
			var path = BuildGitHubPath("Version/Latest.json");

			var content = await NetClient.GetStringAsync(path);
			var document = System.Json.JsonValue.Parse(content);

			return document["LibraryVersion"];
		}

		async Task<string> GetGitHubGameConfig()
		{
			return await NetClient.GetStringAsync(BuildGitHubPath("Output/SDR/GameConfig.json"));
		}

		async Task<string> GetGitHubExtensionConfig()
		{
			return await NetClient.GetStringAsync(BuildGitHubPath("Output/SDR/ExtensionConfig.json"));
		}

		bool ShouldFileBeUpdated(string filename)
		{
			if (System.IO.File.Exists(filename))
			{
				var fileinfo = new System.IO.FileInfo(filename);

				if (fileinfo.IsReadOnly)
				{
					return false;
				}
			}

			return true;
		}

		void AddLogText(string format, params object[] args)
		{
			StatusText.Text += string.Format(format + "\n", args);
		}

		async void MainProcedure()
		{
			AddLogText("Looking for SDR updates.");

			bool autoskip = false;

			try
			{
				var webver = await GetGitHubLibraryVersion();

				if (LocalVersion == webver)
				{
					AddLogText("Using the latest library version.");
					autoskip = true;
				}

				else if (webver > LocalVersion)
				{
					AddLogText("Library update is available from {0} to {1}. Press Upgrade to view release.", LocalVersion, webver);
					UpgradeButton.IsEnabled = true;
				}

				if (ShouldFileBeUpdated("GameConfig.json"))
				{
					var webconfig = await GetGitHubGameConfig();
					System.IO.File.WriteAllText("GameConfig.json", webconfig, new System.Text.UTF8Encoding(false));

					AddLogText("Latest game config was downloaded.");
				}

				else
				{
					AddLogText("Game config is set to read only so it will not be updated.");
					autoskip = false;
				}

				if (ShouldFileBeUpdated("ExtensionConfig.json"))
				{
					var webconfig = await GetGitHubExtensionConfig();
					System.IO.File.WriteAllText("ExtensionConfig.json", webconfig, new System.Text.UTF8Encoding(false));

					AddLogText("Latest extension config was downloaded.");
				}

				else
				{
					AddLogText("Extension config is set to read only so it will not be updated.");
					autoskip = false;
				}
			}

			catch (Exception error)
			{
				AddLogText(error.Message);
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
