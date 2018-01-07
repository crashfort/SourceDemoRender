using System.Collections.Generic;

namespace LauncherUI.SDR
{
	public static class VerifiedExtensions
	{
		static List<string> Files = new List<string>();

		public static void Add(string filename)
		{
			Files.Add(filename);
		}

		public static bool IsVerified(string filename)
		{
			return Files.Find(name => name == filename) != null;
		}
	}
}
