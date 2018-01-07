using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Windows;
using System.Linq;

namespace LauncherUI
{
	public partial class ExtensionsWindow : Window
	{
		class ListBoxData
		{
			public bool Enabled;
			public int Index;

			public SDR.ExtensionData Info;

			public System.Windows.Controls.ListBoxItem BoxItem;
			public System.Windows.Controls.Grid ContentGrid;
			public System.Windows.Controls.CheckBox StatusCheckBox;
			public System.Windows.Controls.TextBlock TitleBlock;
		}

		List<ListBoxData> Extensions = new List<ListBoxData>();

		public ExtensionsWindow()
		{
			InitializeComponent();

			CreateDirectoriesIfMissing();

			PopulateList();
			ResolveFromLocalOrder();
			SyncWithUI();
		}

		void CreateDirectoriesIfMissing()
		{
			if (!System.IO.Directory.Exists("Extensions"))
			{
				System.IO.Directory.CreateDirectory("Extensions");
			}

			if (!System.IO.Directory.Exists("Extensions\\Enabled"))
			{
				System.IO.Directory.CreateDirectory("Extensions\\Enabled");
			}

			if (!System.IO.Directory.Exists("Extensions\\Disabled"))
			{
				System.IO.Directory.CreateDirectory("Extensions\\Disabled");
			}
		}

		void LoadExtensionsFromPath(string path, bool enabled)
		{
			var exts = SDR.Loader.LoadAll(path);

			foreach (var item in exts)
			{
				var data = new ListBoxData();
				data.Info = item;
				data.Enabled = enabled;

				SDR.QueryData.FillNullStrings(ref data.Info.Query);

				Extensions.Add(data);
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
						if (item == temp.Info.FileName)
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

		void CreateConflictToolTip(ListBoxData item, string text)
		{
			if (item.BoxItem.ToolTip == null)
			{
				item.BoxItem.ToolTip = "";
			}

			else
			{
				item.BoxItem.ToolTip += "\n";
			}

			item.BoxItem.ToolTip += text;
		}

		void CreateStatusConflict(ListBoxData item, ListBoxData dep)
		{
			item.TitleBlock.Foreground = System.Windows.Media.Brushes.Red;

			var text = string.Format("\"{0}\" has \"{1}\" listed as a dependency but it's not enabled.", item.Info.Query.Name, dep.Info.Query.Name);
			CreateConflictToolTip(item, text);
		}

		void CreateExistanceConflict(ListBoxData item, string dep)
		{
			item.TitleBlock.Foreground = System.Windows.Media.Brushes.Red;

			var text = string.Format("\"{0}\" has \"{1}\" listed as a dependency but it doesn't exist.", item.Info.Query.Name, dep);
			CreateConflictToolTip(item, text);
		}

		void CreateOrderConflict(ListBoxData item, ListBoxData dep)
		{
			item.TitleBlock.Foreground = System.Windows.Media.Brushes.Red;

			var text = string.Format("\"{0}\" must be after \"{1}\" because it's listed as a dependency.", item.Info.Query.Name, dep.Info.Query.Name);
			CreateConflictToolTip(item, text);
		}

		void CheckConflicts()
		{
			bool conflicts = false;

			foreach (var item in Extensions)
			{
				item.TitleBlock.Foreground = System.Windows.Media.Brushes.Black;
				item.BoxItem.ToolTip = null;

				if (!item.Enabled)
				{
					continue;
				}

				if (item.Info.Query.Dependencies == null)
				{
					continue;
				}

				foreach (var dep in item.Info.Query.Dependencies)
				{
					var them = Extensions.Find(other => other.Info.FileName == dep);

					if (them == null)
					{
						CreateExistanceConflict(item, dep);
						conflicts = true;

						continue;
					}

					if (!them.Enabled)
					{
						CreateStatusConflict(item, them);
						conflicts = true;
					}

					if (them.Index > item.Index)
					{
						CreateOrderConflict(item, them);
						conflicts = true;
					}
				}
			}

			OKButton.IsEnabled = !conflicts;
		}

		bool ShowDisabled = true;

		void SyncWithUI()
		{
			ExtensionsList.SelectedItem = null;
			ExtensionsList.Items.Clear();

			SetMoveUpState(false);
			SetMoveDownState(false);

			foreach (var item in Extensions)
			{
				if (!item.Enabled && !ShowDisabled)
				{
					continue;
				}

				item.Index = ExtensionsList.Items.Count;

				item.ContentGrid = new System.Windows.Controls.Grid();
				item.ContentGrid.Height = 70;

				var enabledseq = new System.Windows.Controls.TextBlock();
				enabledseq.FontSize = 15;
				enabledseq.HorizontalAlignment = HorizontalAlignment.Left;
				enabledseq.VerticalAlignment = VerticalAlignment.Bottom;

				var enabledtitle = new System.Windows.Controls.TextBlock();
				enabledtitle.Text = "Enable ";
				enabledtitle.Foreground = System.Windows.Media.Brushes.Black;

				var enabledstr = new System.Windows.Controls.TextBlock();
				enabledstr.Text = item.Info.FileName;
				enabledstr.Foreground = System.Windows.Media.Brushes.Gray;

				enabledseq.Inlines.Add(enabledtitle);
				enabledseq.Inlines.Add(enabledstr);

				item.StatusCheckBox = new System.Windows.Controls.CheckBox();
				item.StatusCheckBox.Content = enabledseq;
				item.StatusCheckBox.FontSize = 15;
				item.StatusCheckBox.HorizontalAlignment = HorizontalAlignment.Left;
				item.StatusCheckBox.VerticalAlignment = VerticalAlignment.Bottom;
				item.StatusCheckBox.Foreground = System.Windows.Media.Brushes.Gray;
				item.StatusCheckBox.VerticalContentAlignment = VerticalAlignment.Center;

				item.StatusCheckBox.IsChecked = item.Enabled;

				item.StatusCheckBox.DataContext = item;
				item.StatusCheckBox.Checked += ExtensionEnabledCheck_Checked;
				item.StatusCheckBox.Unchecked += ExtensionEnabledCheck_Unchecked;

				item.TitleBlock = new System.Windows.Controls.TextBlock();
				item.TitleBlock.Text = item.Info.Query.Name;
				item.TitleBlock.FontSize = 30;
				item.TitleBlock.FontWeight = FontWeights.Thin;
				item.TitleBlock.HorizontalAlignment = HorizontalAlignment.Left;
				item.TitleBlock.VerticalAlignment = VerticalAlignment.Top;

				var index = new System.Windows.Controls.TextBlock();
				index.Text = string.Format("{0}#", ExtensionsList.Items.Count + 1);
				index.FontSize = 30;
				index.FontWeight = FontWeights.Thin;
				index.Foreground = System.Windows.Media.Brushes.Gray;
				index.HorizontalAlignment = HorizontalAlignment.Right;
				index.VerticalAlignment = VerticalAlignment.Top;
				index.FlowDirection = FlowDirection.RightToLeft;

				var infoseq = new System.Windows.Controls.TextBlock();
				infoseq.FontSize = 15;
				infoseq.HorizontalAlignment = HorizontalAlignment.Right;
				infoseq.VerticalAlignment = VerticalAlignment.Bottom;

				var authortitle = new System.Windows.Controls.TextBlock();
				authortitle.Text = "Author ";
				authortitle.Foreground = System.Windows.Media.Brushes.Black;

				var authorstr = new System.Windows.Controls.TextBlock();
				authorstr.Text = item.Info.Query.Author;
				authorstr.Foreground = System.Windows.Media.Brushes.Gray;

				var versiontitle = new System.Windows.Controls.TextBlock();
				versiontitle.Text = " Version ";
				versiontitle.Foreground = System.Windows.Media.Brushes.Black;

				var versionstr = new System.Windows.Controls.TextBlock();
				versionstr.Text = item.Info.Query.Version.ToString();
				versionstr.Foreground = System.Windows.Media.Brushes.Gray;

				infoseq.Inlines.Add(authortitle);
				infoseq.Inlines.Add(authorstr);
				infoseq.Inlines.Add(versiontitle);
				infoseq.Inlines.Add(versionstr);

				item.ContentGrid.Children.Add(item.StatusCheckBox);
				item.ContentGrid.Children.Add(item.TitleBlock);
				item.ContentGrid.Children.Add(infoseq);
				item.ContentGrid.Children.Add(index);

				item.BoxItem = new System.Windows.Controls.ListBoxItem();
				item.BoxItem.Content = item.ContentGrid;
				item.BoxItem.BorderThickness = new Thickness(0);
				item.BoxItem.Margin = new Thickness(0);
				item.BoxItem.Padding = new Thickness(10);

				ExtensionsList.Items.Add(item.BoxItem);
			}

			CheckConflicts();
		}

		void SetExtensionStatusFromEvent(object sender, bool value)
		{
			var control = sender as System.Windows.Controls.CheckBox;
			var data = (ListBoxData)control.DataContext;

			data.Enabled = value;

			var oldindex = ExtensionsList.SelectedIndex;

			ExtensionsList.SelectedIndex = data.Index;

			if (oldindex == data.Index)
			{
				UpdateSelectionButtonsState(ExtensionsList.SelectedIndex);
			}

			CheckConflicts();
		}

		void ExtensionEnabledCheck_Checked(object sender, RoutedEventArgs args)
		{
			SetExtensionStatusFromEvent(sender, true);
		}

		void ExtensionEnabledCheck_Unchecked(object sender, RoutedEventArgs args)
		{
			SetExtensionStatusFromEvent(sender, false);
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
					saverestore.Add(item.Info.FileName);

					var newlocation = System.IO.Path.Combine(enabledpath, item.Info.FileName);

					if (item.Info.RelativePath != newlocation)
					{
						System.IO.File.Move(item.Info.RelativePath, newlocation);
					}
				}

				else
				{
					var newlocation = System.IO.Path.Combine(disabledpath, item.Info.FileName);

					if (item.Info.RelativePath != newlocation)
					{
						System.IO.File.Move(item.Info.RelativePath, newlocation);
					}
				}
			}

			var json = Newtonsoft.Json.JsonConvert.SerializeObject(saverestore);

			var orderpath = System.IO.Path.Combine(enabledpath, "Order.json");
			System.IO.File.WriteAllText(orderpath, json, new System.Text.UTF8Encoding(false));

			Close();
		}

		void UpdateSelectionButtonsState(int index)
		{
			var topfree = index > 0;
			var bottomfree = index < ExtensionsList.Items.Count - 1;

			SetMoveUpState(topfree);
			SetMoveDownState(bottomfree);
		}

		void ExtensionsList_SelectionChanged(object sender, System.Windows.Controls.SelectionChangedEventArgs args)
		{
			if (ExtensionsList.SelectedItem != null)
			{
				UpdateSelectionButtonsState(ExtensionsList.SelectedIndex);
			}
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
