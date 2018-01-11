using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;

namespace LauncherUI.SDR
{
	[StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
	public struct QueryDataPtr
	{
		public IntPtr Name;
		public IntPtr Namespace;
		public IntPtr Author;
		public IntPtr Contact;

		public int Version;

		public IntPtr Dependencies;
	}

	[UnmanagedFunctionPointer(CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
	public delegate void SDR_Query(ref QueryDataPtr data);

	public class QueryData
	{
		public static void FillNullStrings(ref QueryData data)
		{
			data.Name = data.Name ?? "N/A";
			data.Namespace = data.Namespace ?? "N/A";
			data.Author = data.Author ?? "N/A";
			data.Contact = data.Contact ?? "N/A";
		}

		public static QueryData FromPtr(QueryDataPtr ptr)
		{
			var ret = new QueryData();

			ret.Name = Marshal.PtrToStringAnsi(ptr.Name);
			ret.Namespace = Marshal.PtrToStringAnsi(ptr.Namespace);
			ret.Author = Marshal.PtrToStringAnsi(ptr.Author);
			ret.Contact = Marshal.PtrToStringAnsi(ptr.Contact);

			ret.Version = ptr.Version;

			var depstr = Marshal.PtrToStringAnsi(ptr.Dependencies);

			if (depstr != null)
			{
				ret.Dependencies = new List<string>();

				var parts = depstr.Split(',');

				foreach (var item in parts)
				{
					ret.Dependencies.Add(item.Trim());
				}
			}

			return ret;
		}

		public string Name;
		public string Namespace;
		public string Author;
		public string Contact;

		public int Version;

		public List<string> Dependencies;
	}

	public class ExtensionData
	{
		public string RelativePath;
		public string FileName;

		public QueryData Query;
	}

	public static class ExtensionLoader
	{
		static public List<ExtensionData> LoadAll(string path)
		{
			var files = System.IO.Directory.GetFiles(path, "*.dll", System.IO.SearchOption.TopDirectoryOnly);

			var ret = new List<ExtensionData>();

			foreach (var file in files)
			{
				var library = IntPtr.Zero;

				var current = new ExtensionData();

				try
				{
					library = WindowsAPI.LoadLibrary(file);

					var address = WindowsAPI.GetProcAddress(library, "SDR_Query");

					if (address != IntPtr.Zero)
					{
						var function = Marshal.GetDelegateForFunctionPointer<SDR_Query>(address);

						var result = new QueryDataPtr();
						function(ref result);

						var fileinfo = new System.IO.FileInfo(file);

						current.RelativePath = file;
						current.FileName = fileinfo.Name;
						current.Query = QueryData.FromPtr(result);

						ret.Add(current);
					}
				}

				finally
				{
					WindowsAPI.FreeLibrary(library);
				}
			}

			return ret;
		}
	}
}
