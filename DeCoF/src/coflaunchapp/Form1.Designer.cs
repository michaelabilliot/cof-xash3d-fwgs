namespace CoFLaunchApp
{
    partial class Form1
    {
        /// <summary>
        /// Required designer variable.
        /// </summary>
        private System.ComponentModel.IContainer components = null;

        /// <summary>
        /// Clean up any resources being used.
        /// </summary>
        /// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null))
            {
                components.Dispose();
            }
            base.Dispose(disposing);
        }

        #region Windows Form Designer generated code

        /// <summary>
        /// Required method for Designer support - do not modify
        /// the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(Form1));
            this.sound = new System.Media.SoundPlayer();
            this.sound2 = new System.Media.SoundPlayer();
            this.sound3 = new System.Media.SoundPlayer();
            this.label1 = new System.Windows.Forms.Label();
            this.comboBox1 = new System.Windows.Forms.ComboBox();
            this.button1 = new System.Windows.Forms.Button();
            this.checkBox1 = new System.Windows.Forms.CheckBox();
            this.checkBox2 = new System.Windows.Forms.CheckBox();
            this.checkBox3 = new System.Windows.Forms.CheckBox();
            this.button2 = new System.Windows.Forms.Button();
            this.button3 = new System.Windows.Forms.Button();
            this.button4 = new System.Windows.Forms.Button();
            this.button5 = new System.Windows.Forms.Button();
            this.label5 = new System.Windows.Forms.Label();
            this.aombutton = new System.Windows.Forms.Button();
            this.fbfollowbutton = new System.Windows.Forms.Button();
            this.label2 = new System.Windows.Forms.Label();
            this.SuspendLayout();
            // 
            // sound
            // 
            this.sound.LoadTimeout = 10000;
            this.sound.SoundLocation = "";
            this.sound.Stream = CoFLaunchApp.Properties.Resources.bg_music;
            this.sound.Tag = null;
            // 
            // sound2
            // 
            this.sound2.LoadTimeout = 10000;
            this.sound2.SoundLocation = "";
            this.sound2.Stream = CoFLaunchApp.Properties.Resources._3d_rollover;
            this.sound2.Tag = null;
            // 
            // sound3
            // 
            this.sound3.LoadTimeout = 10000;
            this.sound3.SoundLocation = "";
            this.sound3.Stream = CoFLaunchApp.Properties.Resources._3d_click;
            this.sound3.Tag = null;
            // 
            // label1
            // 
            this.label1.AutoSize = true;
            this.label1.BackColor = System.Drawing.Color.Transparent;
            this.label1.Font = new System.Drawing.Font("Microsoft Sans Serif", 9.75F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.label1.ForeColor = System.Drawing.Color.Snow;
            this.label1.Location = new System.Drawing.Point(12, 65);
            this.label1.Name = "label1";
            this.label1.Size = new System.Drawing.Size(107, 16);
            this.label1.TabIndex = 1;
            this.label1.Text = "Set resolution:";
            // 
            // comboBox1
            // 
            this.comboBox1.BackColor = System.Drawing.SystemColors.InfoText;
            this.comboBox1.ForeColor = System.Drawing.Color.White;
            this.comboBox1.FormattingEnabled = true;
            this.comboBox1.Items.AddRange(new object[] {
            "800x600",
            "1024x768",
            "1152x864",
            "1280x960",
            "1280x1024",
            "- Widescreen",
            "1080x720",
            "1280x720",
            "1280x800",
            "1440x900",
            "1600x900",
            "1680x1050",
            "1920x1080"});
            this.comboBox1.Location = new System.Drawing.Point(24, 95);
            this.comboBox1.Name = "comboBox1";
            this.comboBox1.Size = new System.Drawing.Size(121, 21);
            this.comboBox1.TabIndex = 2;
            this.comboBox1.Text = "...";
            // 
            // button1
            // 
            this.button1.BackColor = System.Drawing.Color.Transparent;
            this.button1.BackgroundImage = CoFLaunchApp.Properties.Resources.launch;
            this.button1.BackgroundImageLayout = System.Windows.Forms.ImageLayout.Center;
            this.button1.DialogResult = System.Windows.Forms.DialogResult.OK;
            this.button1.Dock = System.Windows.Forms.DockStyle.Bottom;
            this.button1.FlatAppearance.BorderSize = 0;
            this.button1.FlatAppearance.MouseDownBackColor = System.Drawing.Color.Transparent;
            this.button1.FlatAppearance.MouseOverBackColor = System.Drawing.Color.Transparent;
            this.button1.FlatStyle = System.Windows.Forms.FlatStyle.Flat;
            this.button1.Location = new System.Drawing.Point(0, 505);
            this.button1.Name = "button1";
            this.button1.Size = new System.Drawing.Size(584, 57);
            this.button1.TabIndex = 3;
            this.button1.UseVisualStyleBackColor = false;
            this.button1.Click += new System.EventHandler(this.button1_Click);
            this.button1.MouseLeave += new System.EventHandler(this.button1_MouseLeave);
            this.button1.MouseHover += new System.EventHandler(this.button1_MouseOver);
            // 
            // checkBox1
            // 
            this.checkBox1.AutoSize = true;
            this.checkBox1.BackColor = System.Drawing.Color.Transparent;
            this.checkBox1.ForeColor = System.Drawing.Color.White;
            this.checkBox1.Location = new System.Drawing.Point(24, 169);
            this.checkBox1.Name = "checkBox1";
            this.checkBox1.Size = new System.Drawing.Size(70, 17);
            this.checkBox1.TabIndex = 4;
            this.checkBox1.Text = "2D Menu";
            this.checkBox1.UseVisualStyleBackColor = false;
            // 
            // checkBox2
            // 
            this.checkBox2.AutoSize = true;
            this.checkBox2.BackColor = System.Drawing.Color.Transparent;
            this.checkBox2.ForeColor = System.Drawing.Color.White;
            this.checkBox2.Location = new System.Drawing.Point(24, 192);
            this.checkBox2.Name = "checkBox2";
            this.checkBox2.Size = new System.Drawing.Size(77, 17);
            this.checkBox2.TabIndex = 5;
            this.checkBox2.Text = "Windowed";
            this.checkBox2.UseVisualStyleBackColor = false;
            // 
            // checkBox3
            // 
            this.checkBox3.AutoSize = true;
            this.checkBox3.BackColor = System.Drawing.Color.Transparent;
            this.checkBox3.ForeColor = System.Drawing.Color.White;
            this.checkBox3.Location = new System.Drawing.Point(24, 215);
            this.checkBox3.Name = "checkBox3";
            this.checkBox3.Size = new System.Drawing.Size(189, 17);
            this.checkBox3.TabIndex = 6;
            this.checkBox3.Text = "16 Bit (low res - not recommended)";
            this.checkBox3.UseVisualStyleBackColor = false;
            // 
            // button2
            // 
            this.button2.Anchor = System.Windows.Forms.AnchorStyles.Bottom;
            this.button2.BackColor = System.Drawing.Color.Transparent;
            this.button2.BackgroundImage = CoFLaunchApp.Properties.Resources.unlocks;
            this.button2.BackgroundImageLayout = System.Windows.Forms.ImageLayout.Center;
            this.button2.DialogResult = System.Windows.Forms.DialogResult.OK;
            this.button2.FlatAppearance.BorderColor = System.Drawing.Color.Black;
            this.button2.FlatAppearance.BorderSize = 0;
            this.button2.FlatAppearance.MouseDownBackColor = System.Drawing.Color.Transparent;
            this.button2.FlatAppearance.MouseOverBackColor = System.Drawing.Color.Transparent;
            this.button2.FlatStyle = System.Windows.Forms.FlatStyle.Flat;
            this.button2.Location = new System.Drawing.Point(50, 413);
            this.button2.Name = "button2";
            this.button2.Size = new System.Drawing.Size(484, 40);
            this.button2.TabIndex = 7;
            this.button2.UseVisualStyleBackColor = false;
            this.button2.Click += new System.EventHandler(this.button2_Click);
            this.button2.MouseLeave += new System.EventHandler(this.button2_MouseLeave);
            this.button2.MouseHover += new System.EventHandler(this.button2_MouseOver);
            // 
            // button3
            // 
            this.button3.Anchor = System.Windows.Forms.AnchorStyles.Bottom;
            this.button3.BackColor = System.Drawing.Color.Transparent;
            this.button3.BackgroundImage = CoFLaunchApp.Properties.Resources.saves;
            this.button3.BackgroundImageLayout = System.Windows.Forms.ImageLayout.Center;
            this.button3.DialogResult = System.Windows.Forms.DialogResult.OK;
            this.button3.FlatAppearance.BorderSize = 0;
            this.button3.FlatAppearance.MouseDownBackColor = System.Drawing.Color.Transparent;
            this.button3.FlatAppearance.MouseOverBackColor = System.Drawing.Color.Transparent;
            this.button3.FlatStyle = System.Windows.Forms.FlatStyle.Flat;
            this.button3.Location = new System.Drawing.Point(50, 459);
            this.button3.Name = "button3";
            this.button3.Size = new System.Drawing.Size(484, 40);
            this.button3.TabIndex = 8;
            this.button3.UseVisualStyleBackColor = false;
            this.button3.Click += new System.EventHandler(this.button3_Click);
            this.button3.MouseLeave += new System.EventHandler(this.button3_MouseLeave);
            this.button3.MouseHover += new System.EventHandler(this.button3_MouseOver);
            // 
            // button4
            // 
            this.button4.BackColor = System.Drawing.Color.Transparent;
            this.button4.BackgroundImage = CoFLaunchApp.Properties.Resources.header;
            this.button4.BackgroundImageLayout = System.Windows.Forms.ImageLayout.Center;
            this.button4.DialogResult = System.Windows.Forms.DialogResult.OK;
            this.button4.Dock = System.Windows.Forms.DockStyle.Top;
            this.button4.FlatAppearance.BorderSize = 0;
            this.button4.FlatAppearance.MouseDownBackColor = System.Drawing.Color.Transparent;
            this.button4.FlatAppearance.MouseOverBackColor = System.Drawing.Color.Transparent;
            this.button4.FlatStyle = System.Windows.Forms.FlatStyle.Flat;
            this.button4.Location = new System.Drawing.Point(0, 0);
            this.button4.Name = "button4";
            this.button4.Size = new System.Drawing.Size(584, 57);
            this.button4.TabIndex = 13;
            this.button4.UseVisualStyleBackColor = false;
            this.button4.Click += new System.EventHandler(this.button4_Click);
            // 
            // button5
            // 
            this.button5.Anchor = System.Windows.Forms.AnchorStyles.Bottom;
            this.button5.BackColor = System.Drawing.Color.Transparent;
            this.button5.BackgroundImage = CoFLaunchApp.Properties.Resources.help;
            this.button5.BackgroundImageLayout = System.Windows.Forms.ImageLayout.Center;
            this.button5.DialogResult = System.Windows.Forms.DialogResult.OK;
            this.button5.FlatAppearance.BorderColor = System.Drawing.Color.Black;
            this.button5.FlatAppearance.BorderSize = 0;
            this.button5.FlatAppearance.MouseDownBackColor = System.Drawing.Color.Transparent;
            this.button5.FlatAppearance.MouseOverBackColor = System.Drawing.Color.Transparent;
            this.button5.FlatStyle = System.Windows.Forms.FlatStyle.Flat;
            this.button5.Location = new System.Drawing.Point(50, 367);
            this.button5.Name = "button5";
            this.button5.Size = new System.Drawing.Size(484, 40);
            this.button5.TabIndex = 14;
            this.button5.UseVisualStyleBackColor = false;
            this.button5.Click += new System.EventHandler(this.button5_Click);
            this.button5.MouseLeave += new System.EventHandler(this.button5_MouseLeave);
            this.button5.MouseHover += new System.EventHandler(this.button5_MouseOver);
            // 
            // label5
            // 
            this.label5.AutoSize = true;
            this.label5.BackColor = System.Drawing.Color.Transparent;
            this.label5.Font = new System.Drawing.Font("Microsoft Sans Serif", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.label5.ForeColor = System.Drawing.Color.White;
            this.label5.Location = new System.Drawing.Point(47, 270);
            this.label5.Name = "label5";
            this.label5.Size = new System.Drawing.Size(118, 13);
            this.label5.TabIndex = 20;
            this.label5.Text = "Get news on Facebook";
            // 
            // aombutton
            // 
            this.aombutton.BackColor = System.Drawing.Color.Transparent;
            this.aombutton.BackgroundImage = CoFLaunchApp.Properties.Resources.aom;
            this.aombutton.BackgroundImageLayout = System.Windows.Forms.ImageLayout.Center;
            this.aombutton.DialogResult = System.Windows.Forms.DialogResult.OK;
            this.aombutton.FlatAppearance.BorderColor = System.Drawing.Color.Black;
            this.aombutton.FlatAppearance.BorderSize = 0;
            this.aombutton.FlatAppearance.MouseDownBackColor = System.Drawing.Color.Transparent;
            this.aombutton.FlatAppearance.MouseOverBackColor = System.Drawing.Color.Transparent;
            this.aombutton.FlatStyle = System.Windows.Forms.FlatStyle.Flat;
            this.aombutton.Location = new System.Drawing.Point(24, 237);
            this.aombutton.Name = "aombutton";
            this.aombutton.Size = new System.Drawing.Size(20, 20);
            this.aombutton.TabIndex = 21;
            this.aombutton.UseVisualStyleBackColor = false;
            this.aombutton.Click += new System.EventHandler(this.aombutton_Click);
            this.aombutton.MouseHover += new System.EventHandler(this.fbfollowbutton_MouseOver);
            // 
            // fbfollowbutton
            // 
            this.fbfollowbutton.BackColor = System.Drawing.Color.Transparent;
            this.fbfollowbutton.BackgroundImage = CoFLaunchApp.Properties.Resources.facebook;
            this.fbfollowbutton.BackgroundImageLayout = System.Windows.Forms.ImageLayout.Center;
            this.fbfollowbutton.DialogResult = System.Windows.Forms.DialogResult.OK;
            this.fbfollowbutton.FlatAppearance.BorderColor = System.Drawing.Color.Black;
            this.fbfollowbutton.FlatAppearance.BorderSize = 0;
            this.fbfollowbutton.FlatAppearance.MouseDownBackColor = System.Drawing.Color.Transparent;
            this.fbfollowbutton.FlatAppearance.MouseOverBackColor = System.Drawing.Color.Transparent;
            this.fbfollowbutton.FlatStyle = System.Windows.Forms.FlatStyle.Flat;
            this.fbfollowbutton.Location = new System.Drawing.Point(24, 263);
            this.fbfollowbutton.Name = "fbfollowbutton";
            this.fbfollowbutton.Size = new System.Drawing.Size(20, 20);
            this.fbfollowbutton.TabIndex = 22;
            this.fbfollowbutton.UseVisualStyleBackColor = false;
            this.fbfollowbutton.Click += new System.EventHandler(this.fbfollowbutton_Click);
            this.fbfollowbutton.MouseHover += new System.EventHandler(this.fbfollowbutton_MouseOver);
            // 
            // label2
            // 
            this.label2.AutoSize = true;
            this.label2.BackColor = System.Drawing.Color.Transparent;
            this.label2.Font = new System.Drawing.Font("Microsoft Sans Serif", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.label2.ForeColor = System.Drawing.Color.White;
            this.label2.Location = new System.Drawing.Point(47, 244);
            this.label2.Name = "label2";
            this.label2.Size = new System.Drawing.Size(246, 13);
            this.label2.TabIndex = 23;
            this.label2.Text = "Download Team Psykskallar's \"Afraid of Monsters\"";
            // 
            // Form1
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.BackColor = System.Drawing.SystemColors.Control;
            this.BackgroundImage = CoFLaunchApp.Properties.Resources.bg;
            this.ClientSize = new System.Drawing.Size(584, 562);
            this.Controls.Add(this.label2);
            this.Controls.Add(this.fbfollowbutton);
            this.Controls.Add(this.aombutton);
            this.Controls.Add(this.label5);
            this.Controls.Add(this.button5);
            this.Controls.Add(this.button4);
            this.Controls.Add(this.button3);
            this.Controls.Add(this.button2);
            this.Controls.Add(this.checkBox3);
            this.Controls.Add(this.checkBox2);
            this.Controls.Add(this.checkBox1);
            this.Controls.Add(this.button1);
            this.Controls.Add(this.comboBox1);
            this.Controls.Add(this.label1);
            this.DoubleBuffered = true;
            this.Icon = CoFLaunchApp.Properties.Resources.game;
            this.Name = "Form1";
            this.Text = "Cry of Fear Launcher";
            this.Load += new System.EventHandler(this.Form1_Load);
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private System.Media.SoundPlayer sound;
        private System.Media.SoundPlayer sound2;
        private System.Media.SoundPlayer sound3;
        private System.Windows.Forms.Label label1;
        private System.Windows.Forms.ComboBox comboBox1;
        private System.Windows.Forms.Button button1;
        private System.Windows.Forms.CheckBox checkBox1;
        private System.Windows.Forms.CheckBox checkBox2;
        private System.Windows.Forms.CheckBox checkBox3;
        private System.Windows.Forms.Button button2;
        private System.Windows.Forms.Button button3;
        private System.Windows.Forms.Button button4;
        private System.Windows.Forms.Button button5;
        private System.Windows.Forms.Label label5;
        private System.Windows.Forms.Button aombutton;
        private System.Windows.Forms.Button fbfollowbutton;
        private System.Windows.Forms.Label label2;
    }
}
