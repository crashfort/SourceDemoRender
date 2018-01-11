using System;
using System.Runtime.InteropServices;

namespace LauncherUI
{
	static class WindowsAPI
	{
		[DllImport("kernel32.dll")]
		public static extern IntPtr LoadLibrary(string name);

		[DllImport("kernel32.dll")]
		public static extern IntPtr GetProcAddress(IntPtr module, string name);

		[DllImport("kernel32.dll")]
		public static extern bool FreeLibrary(IntPtr module);
	}
}
