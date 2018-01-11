using System;
using System.Runtime.InteropServices;

namespace LauncherUI.SDR
{
	public static class LibraryAPI
	{
		[UnmanagedFunctionPointer(CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
		public delegate int SDR_LibraryVersion();

		public static int GetLibraryVersion()
		{
			var library = IntPtr.Zero;

			try
			{
				library = WindowsAPI.LoadLibrary("SourceDemoRender.dll");

				var address = WindowsAPI.GetProcAddress(library, "SDR_LibraryVersion");
				var function = Marshal.GetDelegateForFunctionPointer<SDR_LibraryVersion>(address);

				return function();
			}

			finally
			{
				WindowsAPI.FreeLibrary(library);
			}

			throw new Exception("Could not retrieve library version");
		}
	}
}
