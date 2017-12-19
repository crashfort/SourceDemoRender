using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Windows;
using System.Linq;

namespace LauncherUI
{
	public partial class ExtensionsWindow : Window
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

		static class SDR
		{
			[StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
			public struct QueryData
			{
				public IntPtr Name;
				public IntPtr Namespace;
				public IntPtr Author;
				public IntPtr Contact;

				public int Version;
			};

			[UnmanagedFunctionPointer(CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
			public delegate void QueryType(ref QueryData data);
		}

		class ListBoxData
		{
			public string Name;
			public string FileName;
			public string Author;
			public int Version;
		}

		List<ListBoxData> Extensions = new List<ListBoxData>();

		public ExtensionsWindow()
		{
			InitializeComponent();

			PopulateList();
			ResolveFromLocalOrder();
			SyncWithUI();
		}

		void PopulateList()
		{
			var path = System.IO.Path.Combine("Extensions\\Enabled\\");
			var files = System.IO.Directory.GetFiles(path, "*.dll", System.IO.SearchOption.TopDirectoryOnly);

			foreach (var file in files)
			{
				var library = IntPtr.Zero;

				try
				{
					library = WindowsAPI.LoadLibrary(file);

					var address = WindowsAPI.GetProcAddress(library, "SDR_Query");

					if (address != IntPtr.Zero)
					{
						var function = Marshal.GetDelegateForFunctionPointer<SDR.QueryType>(address);

						var result = new SDR.QueryData();
						function(ref result);

						var fileinfo = new System.IO.FileInfo(file);

						var data = new ListBoxData();
						data.Name = Marshal.PtrToStringAnsi(result.Name);
						data.FileName = fileinfo.Name;
						data.Author = Marshal.PtrToStringAnsi(result.Author);
						data.Version = result.Version;

						Extensions.Add(data);
					}
				}

				finally
				{
					WindowsAPI.FreeLibrary(library);
				}
			}
		}

		void ResolveFromLocalOrder()
		{
			if (Extensions.Count < 2)
			{
				return;
			}

			var path = System.IO.Path.Combine("Extensions\\Enabled\\Order.json");

			if (!System.IO.File.Exists(path))
			{
				return;
			}

			var content = System.IO.File.ReadAllText(path, new System.Text.UTF8Encoding(false));
			var document = System.Json.JsonValue.Parse(content);

			var localorder = new List<string>();

			foreach (System.Json.JsonValue item in document)
			{
				localorder.Add(item);
			}

			var copyarray = Extensions.ToArray();
			Extensions.CopyTo(copyarray);

			var copy = copyarray.ToList();

			Extensions.Clear();

			foreach (var item in localorder)
			{
				foreach (var temp in copy)
				{
					if (item == temp.FileName)
					{
						Extensions.Add(temp);
						copy.Remove(temp);

						break;
					}
				}
			}

			foreach (var temp in copy)
			{
				Extensions.Add(temp);
			}
		}

		void SyncWithUI()
		{
			ExtensionsList.Items.Clear();

			foreach (var item in Extensions)
			{
				var content = new System.Windows.Controls.Grid();
				content.Height = 65;

				var index = new System.Windows.Controls.TextBlock();
				index.Text = string.Format("{0}#", ExtensionsList.Items.Count + 1);
				index.FontSize = 30;
				index.FontWeight = FontWeight.FromOpenTypeWeight(100);
				index.Foreground = System.Windows.Media.Brushes.Gray;
				index.HorizontalAlignment = HorizontalAlignment.Right;
				index.VerticalAlignment = VerticalAlignment.Top;
				index.FlowDirection = FlowDirection.RightToLeft;

				var title = new System.Windows.Controls.TextBlock();
				title.Text = item.Name;
				title.FontSize = 30;
				title.FontWeight = FontWeights.Thin;
				title.HorizontalAlignment = HorizontalAlignment.Left;
				title.VerticalAlignment = VerticalAlignment.Top;

				var file = new System.Windows.Controls.TextBlock();
				file.Text = item.FileName;
				file.FontSize = 15;
				file.Foreground = System.Windows.Media.Brushes.Gray;
				file.HorizontalAlignment = HorizontalAlignment.Left;
				file.VerticalAlignment = VerticalAlignment.Bottom;

				var infostr = new System.Windows.Controls.TextBlock();
				infostr.FontSize = 15;
				infostr.HorizontalAlignment = HorizontalAlignment.Right;
				infostr.VerticalAlignment = VerticalAlignment.Bottom;

				var authortitle = new System.Windows.Controls.TextBlock();
				authortitle.Text = "Author: ";
				authortitle.Foreground = System.Windows.Media.Brushes.Black;

				var authorstr = new System.Windows.Controls.TextBlock();
				authorstr.Text = item.Author;
				authorstr.Foreground = System.Windows.Media.Brushes.Gray;

				var versiontitle = new System.Windows.Controls.TextBlock();
				versiontitle.Text = " Version: ";
				versiontitle.Foreground = System.Windows.Media.Brushes.Black;

				var versionstr = new System.Windows.Controls.TextBlock();
				versionstr.Text = item.Version.ToString();
				versionstr.Foreground = System.Windows.Media.Brushes.Gray;

				infostr.Inlines.Add(authortitle);
				infostr.Inlines.Add(authorstr);
				infostr.Inlines.Add(versiontitle);
				infostr.Inlines.Add(versionstr);

				content.Children.Add(title);
				content.Children.Add(file);
				content.Children.Add(infostr);
				content.Children.Add(index);

				ExtensionsList.Items.Add(content);
			}
		}

		void SwapItems(int index, int newindex)
		{
			var temp = Extensions[index];
			Extensions[index] = Extensions[newindex];
			Extensions[newindex] = temp;

			SyncWithUI();

			ExtensionsList.SelectedIndex = newindex;
			ExtensionsList.Focus();
			ExtensionsList.ScrollIntoView(ExtensionsList.SelectedItem);
		}

		void MoveUpButton_Click(object sender, RoutedEventArgs args)
		{
			var index = ExtensionsList.SelectedIndex;
			var newindex = index - 1;

			if (index == 0)
			{
				ExtensionsList.Focus();
				return;
			}

			SwapItems(index, newindex);
		}

		void MoveDownButton_Click(object sender, RoutedEventArgs args)
		{
			var index = ExtensionsList.SelectedIndex;
			var newindex = index + 1;

			if (index == ExtensionsList.Items.Count - 1)
			{
				ExtensionsList.Focus();
				return;
			}

			SwapItems(index, newindex);
		}

		void MoveTopButton_Click(object sender, RoutedEventArgs args)
		{
			var index = ExtensionsList.SelectedIndex;
			var newindex = 0;

			if (index == 0)
			{
				ExtensionsList.Focus();
				return;
			}

			SwapItems(index, newindex);
		}

		void MoveBottomButton_Click(object sender, RoutedEventArgs args)
		{
			var index = ExtensionsList.SelectedIndex;
			var newindex = ExtensionsList.Items.Count - 1;

			if (index == ExtensionsList.Items.Count - 1)
			{
				ExtensionsList.Focus();
				return;
			}

			SwapItems(index, newindex);
		}

		void OKButton_Click(object sender, RoutedEventArgs args)
		{
			var path = System.IO.Path.Combine("Extensions\\Enabled\\Order.json");

			var saverestore = new List<string>();

			foreach (var item in Extensions)
			{
				saverestore.Add(item.FileName);
			}

			var json = Newtonsoft.Json.JsonConvert.SerializeObject(saverestore);

			System.IO.File.WriteAllText(path, json, new System.Text.UTF8Encoding(false));

			Close();
		}

		void ExtensionsList_SelectionChanged(object sender, System.Windows.Controls.SelectionChangedEventArgs args)
		{
			var index = ExtensionsList.SelectedIndex;

			var topfree = index > 0;
			var bottomfree = index < ExtensionsList.Items.Count - 1;

			MoveUpButton.IsEnabled = false;
			MoveTopButton.IsEnabled = false;

			MoveDownButton.IsEnabled = false;
			MoveBottomButton.IsEnabled = false;

			if (bottomfree)
			{
				MoveDownButton.IsEnabled = true;
				MoveBottomButton.IsEnabled = true;
			}

			if (topfree)
			{
				MoveUpButton.IsEnabled = true;
				MoveTopButton.IsEnabled = true;
			}

			if (!topfree && !bottomfree)
			{
				MoveUpButton.IsEnabled = false;
				MoveTopButton.IsEnabled = false;

				MoveDownButton.IsEnabled = false;
				MoveBottomButton.IsEnabled = false;
			}
		}
	}
}
