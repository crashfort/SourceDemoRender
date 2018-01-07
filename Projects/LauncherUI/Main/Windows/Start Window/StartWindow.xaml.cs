using System;
using System.Threading.Tasks;
using System.Windows;
using System.Net.Http;
using System.Diagnostics;

namespace LauncherUI
{
	public partial class StartWindow : Window
	{
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

		async Task<string> GetGitHubVersionsDocument()
		{
			return await NetClient.GetStringAsync(BuildGitHubPath("Version/Latest.json"));
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

		class SequenceData
		{
			public bool HasMessage = false;
		}

		SequenceData ProgramSequence(string document)
		{
			var ret = new SequenceData();

			var json = System.Json.JsonValue.Parse(document);
			int libraryweb = json["LibraryVersion"];

			int librarylocal = SDR.LibraryAPI.GetLibraryVersion();

			if (libraryweb == librarylocal)
			{
				AddLogText("Using the latest library version.");
			}

			else if (libraryweb > librarylocal)
			{
				AddLogText("Library update is available from version {0} to {1}. Press Upgrade to view release.", librarylocal, libraryweb);
				UpgradeButton.IsEnabled = true;
			}

			return ret;
		}

		string GetFileHashString(string filename)
		{
			byte[] hash;

			using (var hasher = System.Security.Cryptography.SHA1.Create())
			{
				using (var stream = System.IO.File.OpenRead(filename))
				{
					hash = hasher.ComputeHash(stream);
				}
			}

			string ret = "";

			foreach (var item in hash)
			{
				ret += item.ToString("X2");
			}

			return ret;
		}

		SequenceData ExtensionSequence(string document)
		{
			var ret = new SequenceData();

			var exts = SDR.ExtensionLoader.LoadAll("Extensions\\Enabled\\");
			var disabled = SDR.ExtensionLoader.LoadAll("Extensions\\Disabled\\");

			exts.AddRange(disabled);

			var json = System.Json.JsonValue.Parse(document);

			foreach (var item in exts)
			{
				if (item.Query.Namespace == null)
				{
					continue;
				}

				if (json.ContainsKey(item.Query.Namespace))
				{
					var root = json[item.Query.Namespace];

					int localver = item.Query.Version;
					int webver = root["Version"];

					if (webver > localver)
					{
						ret.HasMessage = true;

						var name = item.Query.Name;

						var format = string.Format("Update is available for extension \"{0}\" from version {1} to {2}.", name, localver, webver);
						AddLogText(format);
					}

					var localhash = GetFileHashString(item.RelativePath);
					string webhash = root["Hash"];

					if (localhash == webhash)
					{
						SDR.VerifiedExtensions.Add(item.FileName);
					}
				}
			}

			return ret;
		}

		async Task<SequenceData> ConfigSequence()
		{
			var ret = new SequenceData();

			if (ShouldFileBeUpdated("GameConfig.json"))
			{
				var webconfig = await GetGitHubGameConfig();
				System.IO.File.WriteAllText("GameConfig.json", webconfig, new System.Text.UTF8Encoding(false));

				AddLogText("Latest game config was downloaded.");
			}

			else
			{
				AddLogText("Game config is set to read only so it will not be updated.");
				ret.HasMessage = true;
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
				ret.HasMessage = true;
			}

			return ret;
		}

		async void MainProcedure()
		{
			AddLogText("Looking for updates.");

			bool pause = false;

			try
			{
				var document = await GetGitHubVersionsDocument();

				var responses = new SequenceData[]
				{
					ProgramSequence(document),
					ExtensionSequence(document),
					await ConfigSequence()
				};

				foreach (var item in responses)
				{
					if (item.HasMessage)
					{
						pause = true;
						break;
					}
				}
			}

			catch (Exception error)
			{
				AddLogText(error.Message);
				pause = true;
			}

			StartButton.IsEnabled = true;

			if (pause == false)
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
