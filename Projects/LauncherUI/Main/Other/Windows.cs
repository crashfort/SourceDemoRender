using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

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
