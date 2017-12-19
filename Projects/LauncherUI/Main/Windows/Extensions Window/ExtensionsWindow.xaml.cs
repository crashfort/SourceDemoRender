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
			public bool Enabled;

			public string Name;
			public string FileName;
			public string RelativePath;
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

		void LoadExtensionsFromPath(string path, bool enabled)
		{
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
						data.RelativePath = file;
						data.Enabled = enabled;
						data.FileName = fileinfo.Name;

						data.Name = Marshal.PtrToStringAnsi(result.Name);

						if (data.Name == null)
						{
							data.Name = "Unnamed Extension";
						}

						data.Author = Marshal.PtrToStringAnsi(result.Author);

						if (data.Author == null)
						{
							data.Author = "Unnamed Author";
						}

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

		void PopulateList()
		{
			LoadExtensionsFromPath("Extensions\\Enabled\\", true);
			LoadExtensionsFromPath("Extensions\\Disabled\\", false);
		}

		void ResolveFromLocalOrder()
		{
			if (Extensions.Count < 2)
			{
				return;
			}

			var path = "Extensions\\Enabled\\Order.json";

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
					if (temp.Enabled)
					{
						if (item == temp.FileName)
						{
							Extensions.Add(temp);
							copy.Remove(temp);

							break;
						}
					}
				}
			}

			foreach (var temp in copy)
			{
				Extensions.Add(temp);
			}
		}

		void SyncSelection(int index)
		{
			ExtensionsList.SelectedIndex = index;
			ExtensionsList.Focus();
			ExtensionsList.ScrollIntoView(ExtensionsList.SelectedItem);
		}

		void SetMoveUpState(bool state)
		{
			MoveUpButton.IsEnabled = state;
			MoveTopButton.IsEnabled = state;
		}

		void SetMoveDownState(bool state)
		{
			MoveDownButton.IsEnabled = state;
			MoveBottomButton.IsEnabled = state;
		}

		bool ShowDisabled = true;

		void SyncWithUI()
		{
			ExtensionsList.Items.Clear();

			SetMoveUpState(false);
			SetMoveDownState(false);

			foreach (var item in Extensions)
			{
				if (!item.Enabled && !ShowDisabled)
				{
					continue;
				}

				var content = new System.Windows.Controls.Grid();
				content.Height = 70;

				var enabledseq = new System.Windows.Controls.TextBlock();
				enabledseq.FontSize = 15;
				enabledseq.HorizontalAlignment = HorizontalAlignment.Left;
				enabledseq.VerticalAlignment = VerticalAlignment.Bottom;

				var enabledtitle = new System.Windows.Controls.TextBlock();
				enabledtitle.Text = "Enable ";
				enabledtitle.Foreground = System.Windows.Media.Brushes.Black;

				var enabledstr = new System.Windows.Controls.TextBlock();
				enabledstr.Text = item.FileName;
				enabledstr.Foreground = System.Windows.Media.Brushes.Gray;

				enabledseq.Inlines.Add(enabledtitle);
				enabledseq.Inlines.Add(enabledstr);

				var check = new System.Windows.Controls.CheckBox();
				check.Content = enabledseq;
				check.FontSize = 15;
				check.HorizontalAlignment = HorizontalAlignment.Left;
				check.VerticalAlignment = VerticalAlignment.Bottom;
				check.Foreground = System.Windows.Media.Brushes.Gray;
				check.VerticalContentAlignment = VerticalAlignment.Center;

				check.IsChecked = item.Enabled;

				check.DataContext = item;
				check.Checked += ExtensionEnabledCheck_Checked;
				check.Unchecked += ExtensionEnabledCheck_Unchecked;

				var title = new System.Windows.Controls.TextBlock();
				title.Text = item.Name;
				title.FontSize = 30;
				title.FontWeight = FontWeights.Thin;
				title.HorizontalAlignment = HorizontalAlignment.Left;
				title.VerticalAlignment = VerticalAlignment.Top;

				var index = new System.Windows.Controls.TextBlock();
				index.Text = string.Format("{0}#", ExtensionsList.Items.Count + 1);
				index.FontSize = 30;
				index.FontWeight = FontWeight.FromOpenTypeWeight(100);
				index.Foreground = System.Windows.Media.Brushes.Gray;
				index.HorizontalAlignment = HorizontalAlignment.Right;
				index.VerticalAlignment = VerticalAlignment.Top;
				index.FlowDirection = FlowDirection.RightToLeft;

				var infostr = new System.Windows.Controls.TextBlock();
				infostr.FontSize = 15;
				infostr.HorizontalAlignment = HorizontalAlignment.Right;
				infostr.VerticalAlignment = VerticalAlignment.Bottom;

				var authortitle = new System.Windows.Controls.TextBlock();
				authortitle.Text = "Author ";
				authortitle.Foreground = System.Windows.Media.Brushes.Black;

				var authorstr = new System.Windows.Controls.TextBlock();
				authorstr.Text = item.Author;
				authorstr.Foreground = System.Windows.Media.Brushes.Gray;

				var versiontitle = new System.Windows.Controls.TextBlock();
				versiontitle.Text = " Version ";
				versiontitle.Foreground = System.Windows.Media.Brushes.Black;

				var versionstr = new System.Windows.Controls.TextBlock();
				versionstr.Text = item.Version.ToString();
				versionstr.Foreground = System.Windows.Media.Brushes.Gray;

				infostr.Inlines.Add(authortitle);
				infostr.Inlines.Add(authorstr);
				infostr.Inlines.Add(versiontitle);
				infostr.Inlines.Add(versionstr);

				content.Children.Add(check);
				content.Children.Add(title);
				content.Children.Add(infostr);
				content.Children.Add(index);

				ExtensionsList.Items.Add(content);
			}
		}

		void ExtensionEnabledCheck_Checked(object sender, RoutedEventArgs args)
		{
			var control = sender as System.Windows.Controls.CheckBox;
			var data = (ListBoxData)control.DataContext;

			data.Enabled = true;
		}

		void ExtensionEnabledCheck_Unchecked(object sender, RoutedEventArgs args)
		{
			var control = sender as System.Windows.Controls.CheckBox;
			var data = (ListBoxData)control.DataContext;

			data.Enabled = false;
		}

		void SwapItems(int index, int newindex)
		{
			var temp = Extensions[index];
			Extensions[index] = Extensions[newindex];
			Extensions[newindex] = temp;

			SyncWithUI();
			SyncSelection(newindex);
		}

		void MoveUpButton_Click(object sender, RoutedEventArgs args)
		{
			if (Extensions.Count < 2)
			{
				return;
			}

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
			if (Extensions.Count < 2)
			{
				return;
			}

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
			if (Extensions.Count < 2)
			{
				return;
			}

			var index = ExtensionsList.SelectedIndex;
			var newindex = 0;

			if (index == 0)
			{
				ExtensionsList.Focus();
				return;
			}

			var target = Extensions[index];
			Extensions.RemoveAt(index);

			Extensions.Insert(newindex, target);

			SyncWithUI();
			SyncSelection(newindex);
		}

		void MoveBottomButton_Click(object sender, RoutedEventArgs args)
		{
			if (Extensions.Count < 2)
			{
				return;
			}

			var index = ExtensionsList.SelectedIndex;
			var newindex = ExtensionsList.Items.Count - 1;

			if (index == ExtensionsList.Items.Count - 1)
			{
				ExtensionsList.Focus();
				return;
			}

			var target = Extensions[index];
			Extensions.RemoveAt(index);

			Extensions.Insert(newindex, target);

			SyncWithUI();
			SyncSelection(newindex);
		}

		void OKButton_Click(object sender, RoutedEventArgs args)
		{
			var enabledpath = "Extensions\\Enabled\\";
			var disabledpath = "Extensions\\Disabled\\";

			var saverestore = new List<string>();

			foreach (var item in Extensions)
			{
				if (item.Enabled)
				{
					saverestore.Add(item.FileName);

					var newlocation = System.IO.Path.Combine(enabledpath, item.FileName);

					if (item.RelativePath != newlocation)
					{
						System.IO.File.Move(item.RelativePath, newlocation);
					}
				}

				else
				{
					var newlocation = System.IO.Path.Combine(disabledpath, item.FileName);

					if (item.RelativePath != newlocation)
					{
						System.IO.File.Move(item.RelativePath, newlocation);
					}
				}
			}

			var json = Newtonsoft.Json.JsonConvert.SerializeObject(saverestore);

			var orderpath = System.IO.Path.Combine(enabledpath, "Order.json");
			System.IO.File.WriteAllText(orderpath, json, new System.Text.UTF8Encoding(false));

			Close();
		}

		void ExtensionsList_SelectionChanged(object sender, System.Windows.Controls.SelectionChangedEventArgs args)
		{
			var index = ExtensionsList.SelectedIndex;

			var topfree = index > 0;
			var bottomfree = index < ExtensionsList.Items.Count - 1;

			SetMoveUpState(topfree);
			SetMoveDownState(bottomfree);
		}

		void ShowDisabledCheck_Checked(object sender, RoutedEventArgs args)
		{
			if (ExtensionsList == null)
			{
				return;
			}

			ShowDisabled = true;
			SyncWithUI();
		}

		void ShowDisabledCheck_Unchecked(object sender, RoutedEventArgs args)
		{
			if (ExtensionsList == null)
			{
				return;
			}

			ShowDisabled = false;
			SyncWithUI();
		}
	}
}
