using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Windows.Forms;
using System.Diagnostics;
using System.IO;
using System.Net;
using Microsoft.Win32;

namespace CoFLaunchApp
{
    public partial class Form1 : Form
    {
        public string RegistryRead(string KeyName, bool dword)
        {
            RegistryKey rk = Registry.CurrentUser;
            RegistryKey sk1 = rk.OpenSubKey("SOFTWARE\\Valve\\Cry of Fear\\Settings");

            if (sk1 == null)
            {
                MessageBox.Show("Returned NULL");
                return null;
            }
            else
            {
                try
                {
                    string value;
                    object regValue = sk1.GetValue(KeyName.ToUpper());

                    switch (sk1.GetValueKind(KeyName))
                    {
                        case RegistryValueKind.String:
                        case RegistryValueKind.ExpandString:
                            value = (string)regValue;
                            break;
                        case RegistryValueKind.DWord:
                            value = Convert.ToString((int)regValue);
                            break;
                        default:
                            MessageBox.Show("Unsupported registry type");
                            return null;
                    }

                    sk1.Close();
                    return value;
                }
                catch (Exception)
                {
                    return null;
                }
            }
        }

        public bool RegistryWrite(string KeyName, object Value, bool dword)
        {
            try
            {
                RegistryKey rk = Registry.CurrentUser;
                RegistryKey sk1 = rk.CreateSubKey("SOFTWARE\\Valve\\Cry of Fear\\Settings");

                if (dword)
                    sk1.SetValue(KeyName.ToUpper(), Value, RegistryValueKind.DWord);
                else
                    sk1.SetValue(KeyName.ToUpper(), Value);

                sk1.Close();
                return true;
            }
            catch (Exception e)
            {
                MessageBox.Show("Error Writing registry " + KeyName.ToUpper() + " e: " + e.ToString());
                return false;
            }
        }

        public Form1()
        {
            InitializeComponent();
        }

        private void Form1_Load(object sender, EventArgs e)
        {
            this.Text = "Cry of Fear Launcher - Cry of Fear version 1.6";

            if (RegistryRead("no3dmenucof", true) == null)
                RegistryWrite("no3dmenucof", "false", false);

            if (RegistryRead("no3dmenucof", true) == "true")
                checkBox1.Checked = true;

            if (RegistryRead("ScreenWindowed", true) == "1")
                checkBox2.Checked = true;

            if (RegistryRead("ScreenBPP", true) == "16")
                checkBox3.Checked = true;

            if (RegistryRead("ScreenWidth", true) == "800" && RegistryRead("ScreenHeight", true) == "600")
                comboBox1.SelectedIndex = 0;
            if (RegistryRead("ScreenWidth", true) == "1024" && RegistryRead("ScreenHeight", true) == "768")
                comboBox1.SelectedIndex = 1;
            if (RegistryRead("ScreenWidth", true) == "1152" && RegistryRead("ScreenHeight", true) == "864")
                comboBox1.SelectedIndex = 2;
            if (RegistryRead("ScreenWidth", true) == "1280" && RegistryRead("ScreenHeight", true) == "960")
                comboBox1.SelectedIndex = 3;
            if (RegistryRead("ScreenWidth", true) == "1280" && RegistryRead("ScreenHeight", true) == "1024")
                comboBox1.SelectedIndex = 4;
            if (RegistryRead("ScreenWidth", true) == "1080" && RegistryRead("ScreenHeight", true) == "720")
                comboBox1.SelectedIndex = 6;
            if (RegistryRead("ScreenWidth", true) == "1280" && RegistryRead("ScreenHeight", true) == "720")
                comboBox1.SelectedIndex = 7;
            if (RegistryRead("ScreenWidth", true) == "1280" && RegistryRead("ScreenHeight", true) == "800")
                comboBox1.SelectedIndex = 8;
            if (RegistryRead("ScreenWidth", true) == "1440" && RegistryRead("ScreenHeight", true) == "900")
                comboBox1.SelectedIndex = 9;
            if (RegistryRead("ScreenWidth", true) == "1600" && RegistryRead("ScreenHeight", true) == "900")
                comboBox1.SelectedIndex = 10;
            if (RegistryRead("ScreenWidth", true) == "1680" && RegistryRead("ScreenHeight", true) == "1050")
                comboBox1.SelectedIndex = 11;
            if (RegistryRead("ScreenWidth", true) == "1920" && RegistryRead("ScreenHeight", true) == "1080")
                comboBox1.SelectedIndex = 12;
        }

        private void button5_MouseLeave(object sender, EventArgs e)
        {
            button5.BackgroundImage = Properties.Resources.help;
        }

        private void button5_MouseOver(object sender, EventArgs e)
        {
            button5.BackgroundImage = Properties.Resources.helpmo;
            sound2.Play();
        }

        private void fbfollowbutton_MouseOver(object sender, EventArgs e)
        {
            sound2.Play();
        }

        private void aombutton_MouseOver(object sender, EventArgs e)
        {
            sound2.Play();
        }

        private void button1_MouseOver(object sender, EventArgs e)
        {
            button1.BackgroundImage = Properties.Resources.launchmo;
            sound2.Play();
        }

        private void button1_MouseLeave(object sender, EventArgs e)
        {
            button1.BackgroundImage = Properties.Resources.launch;
        }

        private void button2_MouseOver(object sender, EventArgs e)
        {
            button2.BackgroundImage = Properties.Resources.unlocksmo;
            sound2.Play();
        }

        private void button2_MouseLeave(object sender, EventArgs e)
        {
            button2.BackgroundImage = Properties.Resources.unlocks;
        }

        private void button3_MouseOver(object sender, EventArgs e)
        {
            button3.BackgroundImage = Properties.Resources.mo_test;
            sound2.Play();
        }

        private void button3_MouseLeave(object sender, EventArgs e)
        {
            button3.BackgroundImage = Properties.Resources.saves;
        }

        private void checkBox4_Click(object sender, EventArgs e)
        {

        }

        static string ConvertStringArrayToString(string[] array)
        {
            StringBuilder builder = new StringBuilder();
            int i = 0;
            foreach (string value in array)
            {
                if (i > 0)
                {
                    builder.Append(' ');
                    builder.Append(value);
                }
                i++;
            }
            return builder.ToString();
        }

        private void button1_Click(object sender, EventArgs e)
        {
            Process p = new Process();
            p.StartInfo.FileName = "cof.exe";
            p.StartInfo.Arguments = " -basedir cryoffear -game cryoffear -hw -heapsize 1536000 -no3dmenu";

            string args = ConvertStringArrayToString(Environment.GetCommandLineArgs());
            p.StartInfo.Arguments += args;

            if (checkBox1.Checked)
            {
                RegistryWrite("NO3DMENUCOF", "true", false);
                p.StartInfo.Arguments += " -no3dmenu";
            }
            else
            {
                RegistryWrite("NO3DMENUCOF", "false", false);
            }

            if (checkBox2.Checked)
            {
                p.StartInfo.Arguments += " -windowed";
                RegistryWrite("ScreenWindowed", "1", true);
            }
            else
            {
                p.StartInfo.Arguments += " -fullscreen";
                RegistryWrite("ScreenWindowed", "0", true);
            }

            if (checkBox3.Checked)
                RegistryWrite("ScreenBPP", "16", true);
            else
                RegistryWrite("ScreenBPP", "32", true);

            if (comboBox1.SelectedIndex > 5)
                p.StartInfo.Arguments += " -widescreen";

            switch (comboBox1.SelectedIndex)
            {
                case 0:
                    p.StartInfo.Arguments += " -w 800 -h 600";
                    RegistryWrite("ScreenWidth", "800", true);
                    RegistryWrite("ScreenHeight", "600", true);
                    break;
                case 1:
                    p.StartInfo.Arguments += " -w 1024 -h 768";
                    RegistryWrite("ScreenWidth", "1024", true);
                    RegistryWrite("ScreenHeight", "768", true);
                    break;
                case 2:
                    p.StartInfo.Arguments += " -w 1152 -h 864";
                    RegistryWrite("ScreenWidth", "1152", true);
                    RegistryWrite("ScreenHeight", "864", true);
                    break;
                case 3:
                    p.StartInfo.Arguments += " -w 1280 -h 960";
                    RegistryWrite("ScreenWidth", "1280", true);
                    RegistryWrite("ScreenHeight", "960", true);
                    break;
                case 4:
                    p.StartInfo.Arguments += " -w 1280 -h 1024";
                    RegistryWrite("ScreenWidth", "1280", true);
                    RegistryWrite("ScreenHeight", "1024", true);
                    break;
                case 6:
                    p.StartInfo.Arguments += " -w 1080 -h 720";
                    RegistryWrite("ScreenWidth", "1080", true);
                    RegistryWrite("ScreenHeight", "720", true);
                    break;
                case 7:
                    p.StartInfo.Arguments += " -w 1280 -h 720";
                    RegistryWrite("ScreenWidth", "1280", true);
                    RegistryWrite("ScreenHeight", "720", true);
                    break;
                case 8:
                    p.StartInfo.Arguments += " -w 1280 -h 800";
                    RegistryWrite("ScreenWidth", "1280", true);
                    RegistryWrite("ScreenHeight", "800", true);
                    break;
                case 9:
                    p.StartInfo.Arguments += " -w 1440 -h 900";
                    RegistryWrite("ScreenWidth", "1440", true);
                    RegistryWrite("ScreenHeight", "900", true);
                    break;
                case 10:
                    p.StartInfo.Arguments += " -w 1600 -h 900";
                    RegistryWrite("ScreenWidth", "1600", true);
                    RegistryWrite("ScreenHeight", "900", true);
                    break;
                case 11:
                    p.StartInfo.Arguments += " -w 1680 -h 1050";
                    RegistryWrite("ScreenWidth", "1680", true);
                    RegistryWrite("ScreenHeight", "1050", true);
                    break;
                case 12:
                    p.StartInfo.Arguments += " -w 1920 -h 1080";
                    RegistryWrite("ScreenWidth", "1920", true);
                    RegistryWrite("ScreenHeight", "1080", true);
                    break;
            }

            p.EnableRaisingEvents = true;
            p.Start();
            sound3.Play();
            this.Close();
        }

        private void button2_Click(object sender, EventArgs e)
        {
            if (MessageBox.Show("This will reset all your unlockables.\nAre you sure?", "Delete unlockables", MessageBoxButtons.OKCancel) == DialogResult.OK)
            {
                Process p = new Process();
                p.StartInfo.FileName = "StatsReset.exe";
                p.StartInfo.Arguments += " -stats";
                p.EnableRaisingEvents = true;
                p.StartInfo.WorkingDirectory = Directory.GetCurrentDirectory();
                p.StartInfo.WorkingDirectory += "\\cryoffear\\";
                p.Start();
                sound3.Play();
            }
        }

        private void button3_Click(object sender, EventArgs e)
        {
            if (MessageBox.Show("This will delete all your saves.\nAre you sure?", "Delete saves", MessageBoxButtons.OKCancel) == DialogResult.OK)
            {
                Process p = new Process();
                p.StartInfo.FileName = "StatsReset.exe";
                p.StartInfo.Arguments += " -save";
                p.EnableRaisingEvents = true;
                p.StartInfo.WorkingDirectory = Directory.GetCurrentDirectory();
                p.StartInfo.WorkingDirectory += "\\cryoffear\\";
                p.Start();
                sound3.Play();
            }
        }

        private void button4_Click(object sender, EventArgs e)
        {
            Process.Start("www.cry-of-fear.com");
            sound3.Play();
        }

        private void fbfollowbutton_Click(object sender, EventArgs e)
        {
            Process.Start("www.facebook.com/pages/Cry-of-Fear-mod/111800915551808");
            sound3.Play();
        }

        private void aombutton_Click(object sender, EventArgs e)
        {
            Process.Start("www.moddb.com/mods/afraid-of-monsters-dc");
            sound3.Play();
        }

        private void button5_Click(object sender, EventArgs e)
        {
            Process.Start("www.cry-of-fear.com/forum/index.php?board=4.0");
            sound3.Play();
        }

        private bool RemoteFileExists(string url)
        {
            try
            {
                HttpWebRequest request = WebRequest.Create(url) as HttpWebRequest;
                request.Method = "HEAD";
                HttpWebResponse response = request.GetResponse() as HttpWebResponse;

                if (response.StatusCode == HttpStatusCode.OK)
                {
                    response.Close();
                    request.Abort();
                    return true;
                }

                return false;
            }
            catch
            {
                return false;
            }
        }
    }
}
