# GameUI.dll objects

Link map of GameUI.dll (2026-10-24): every row of docs/progress.csv for gameui has a source file or class LIB. Addresses are the first and last progress.csv row of each object, so a range includes the object's COMDAT tail and any LIB rows inside it.

Method (as for hw.dll, docs/hw_regions.md): each object is contiguous. Its regular code is one section that starts 16-byte aligned after int3 padding, the functions inside it follow each other without padding, and its COMDATs (inline and template functions, compiler-generated destructors, vgui2 message map helpers) follow, each 16-byte aligned. A gap between two attributed files is the COMDAT tail of the first plus the head of the next; a second run of unpadded functions inside a gap is a separate object, identified by its strings, its RTTI vtables (`python tools/rtti.py gameui`, in object order) and the Linux gameui.so DWARF (build/match/gameui_linux_files.csv). Objects follow the project's folder order: the career files, game_shared, then the GameUI files mostly alphabetical (common, HLTV and public files sort in by name), the GameUI controls, the dialogs, the options pages, then the vgui2 controls library (with MemPool, filesystem_helpers and characterset) and the static CRT from 100AD188.

Notes go to progress.csv as "linux: <symbol>" where the number of functions of an object equals the Linux DWARF count (D0/D2 destructor pairs and GCC .part clones merged). Names were given only where the decoration is certain (VC6 cl checked); the other new rows have a file and a note.

Link (2026-10-25): the objects from vgui2/controls/controls.cpp to the end come from a static library, vgui_controls.lib in the retail project. Retail has the import thunks of kernel32, user32 and wsock32 (10058B28-10058C66) between OptionsSubVoice.obj and controls.obj, and putting those objects into a library linked after the import libraries (`"staticlibs"` in the gameui link section) makes VC6 link pull them in exactly the retail object order and start addresses.

| File | Range | Rows | Evidence for new attributions |
|---|---|---|---|
| GameUI/CareerBaseFrame.cpp | 10001000-10003710 | 82 |  |
| GameUI/CareerBotFrame.cpp | 10003730-10009D40 | 179 |  |
| GameUI/CareerBox.cpp | 10009D60-1000B3F0 | 53 |  |
| GameUI/CareerDefinitions.cpp | 1000B400-1000F4C0 | 147 |  |
| GameUI/CareerGame.cpp | 1000F4F0-10014320 | 176 |  |
| GameUI/CareerGameTasks.cpp | 10014350-10014EC0 | 21 |  |
| GameUI/CareerMapFrame.cpp | 10014EE0-10018760 | 35 |  |
| GameUI/CareerProfile.cpp | 10018780-1001B860 | 112 |  |
| GameUI/CareerProfileButton.cpp | 1001B880-1001BE70 | 11 |  |
| GameUI/CareerProfileFrame.cpp | 1001BE90-1001DCB0 | 48 |  |
| game_shared/bot/bot_profile.cpp | 1001DCD0-1001FD80 | 63 | new: "WARNING: Cannot access bot profile database"; 14 text functions, then a long COMDAT tail (STL, LIB candidates) |
| game_shared/shared_util.cpp | 1001FDC0-100201E6 | 10 | new, named: same 10 functions and sizes as hw.dll shared_util.cpp |
| GameUI/BackgroundMenuButton.cpp | 10020240-100204A0 | 6 |  |
| GameUI/BasePanel.cpp | 10020500-10021210 | 32 |  |
| common/BaseSystemModule.cpp | 10021220-100217A0 | 22 |  |
| HLTV/common/BitBuffer.cpp | 100218B0-10022CE0 | 60 | new: RTTI BitBuffer, 59 functions = Linux DWARF count (notes) |
| GameUI/BitmapButton.cpp | 10022D10-10023180 | 8 |  |
| HLTV/common/byteorder.cpp | 10023190-1002336E | 12 | new: 12 functions = Linux DWARF count (notes) |
| GameUI/CDKeyEntryDialog.cpp | 10023380-10024710 | 49 |  |
| GameUI/ChangeGameDialog.cpp | 10024730-10024CD0 | 6 | new (Windows only): RTTI CChangeGameDialog, "ChangeGameDialog", "#GameUI_ChangeGame" |
| public/checksum_md5.cpp | 10024D00-10025E72 | 5 |  |
| HLTV/common/common.cpp | 10025EF0-10026945 | 20 |  |
| GameUI/CreateMultiplayerGameBotPage.cpp | 10026970-10027670 | 11 |  |
| GameUI/CreateMultiplayerGameDialog.cpp | 100276A0-10028300 | 8 |  |
| GameUI/CreateMultiplayerGameGameplayPage.cpp | 10028330-100290E0 | 14 |  |
| GameUI/CreateMultiplayerGameServerPage.cpp | 10029100-10029E60 | 22 |  |
| GameUI/CvarLatch.cpp | 10029E90-10029F11 | 2 | new, named: Linux DWARF constructor and destructor |
| GameUI/DemoEditDialog.cpp | 10029F80-1002BDC0 | 29 |  |
| GameUI/DemoEventsDialog.cpp | 1002BE40-1002CA10 | 21 |  |
| GameUI/DemoPlayerDialog.cpp | 1002CA30-1002E7F0 | 28 |  |
| GameUI/DemoPlayerFileDialog.cpp | 1002E900-1002ED10 | 6 |  |
| HLTV/common/DirectorCmd.cpp | 1002ED40-10030240 | 43 | new: RTTI DirectorCmd, 42 functions = Linux DWARF count (notes) |
| GameUI/DottedLabel.cpp | 10030270-100305F0 | 4 | new: RTTI CDottedBgLabel, "BorderBright" |
| GameUI/GameConsole.cpp | 10030610-10030AC0 | 24 |  |
| GameUI/GameUI_Interface.cpp | 10030AD0-100321C0 | 57 |  |
| GameUI/ImageButton.cpp | 100321E0-100325A0 | 9 |  |
| public/interface.cpp | 100325D0-10032730 | 9 |  |
| GameUI/labelslidercombo.cpp | 10032750-10032A30 | 10 |  |
| GameUI/LoadingDialog.cpp | 10032A40-100346E0 | 38 |  |
| GameUI/LogoFile.cpp | 100346F0-100360B0 | 17 | new: "LOGO", "tempdecal.wad", 8 functions = Linux DWARF (notes) |
| GameUI/MasterVersionChk_guess.cpp | 100360D0-10036330 | 8 | new (Windows only, file name unknown): RTTI CMasterVersionChkMsgHandler, QuitMessageBox, "#GameUI_OutOfDate_Msg" |
| GameUI/ModInfo.cpp | 10036350-100366DC | 18 |  |
| GameUI/MsgBuffer_guess.cpp | 100367C0-10036E30 | 30 | new (Windows only, file name unknown): RTTI CMsgBuffer, "CMsgBuffer(%s), no room for %i" |
| GameUI/MusicManager.cpp | 10036E60-100374A0 | 22 |  |
| GameUI/NetAPI_guess.cpp | 100374C0-10037940 | 12 | new (Windows only, file name unknown): RTTI CNetAPI, "loopback", "%i.%i.%i.%i:%i" |
| common/ObjectList.cpp | 10037960-10037EE0 | 19 |  |
| common/parsemsg.cpp | 10037F10-100383CC | 19 | new, named: refs/halflife common/parsemsg.cpp order, 19 functions |
| common/Random.cpp | 100383F0-10038614 | 5 | new: Linux DWARF (5 functions); ran1 and RandomLong unnamed (int32 decoration unknown) |
| GameUI/RefreshSteamLogin.cpp | 10038690-10038FE0 | 12 |  |
| GameUI/RunGameEngine.cpp | 10038FF0-10039360 | 26 |  |
| GameUI/ScriptObject.cpp | 10039390-1003B1F0 | 30 |  |
| GameUI/Socket_guess.cpp | 1003B220-1003CBF0 | 61 | new (Windows only, file name unknown): RTTI CSocket, CSocketThread, CMsgHandler |
| GameUI/Sys_Utils.cpp | 1003CC10-1003CCE6 | 13 |  |
| GameUI/Taskbar.cpp | 1003CCF0-100402F0 | 89 |  |
| vgui2/controls/MenuItem.cpp | 10040300-10040340 | 2 | COMDATs named after the vgui2 class, inside the GameUI object before them |
| GameUI/Taskbar.cpp | 10040350-10040390 | 2 | Taskbar.cpp COMDATs after the MenuItem ones |
| GameUI/TaskButton.cpp | 100403C0-10040AE0 | 19 |  |
| GameUI/TextEntryBox.cpp | 10040B00-10040EB0 | 7 |  |
| public/UtlBuffer.cpp | 10040EE0-100418C0 | 25 |  |
| public/utlsymbol.cpp | 100418F0-100432B0 | 85 |  |
| GameUI/VGuiSystemModuleLoader.cpp | 100432E0-100444F0 | 53 |  |
| GameUI/BitmapImagePanel.cpp | 10044510-10044750 | 7 |  |
| GameUI/CommandCheckButton.cpp | 10044770-10044960 | 6 | new: RTTI CCommandCheckButton, 3 functions = Linux DWARF (notes) |
| GameUI/CvarNegateCheckButton.cpp | 10044970-10044E60 | 17 |  |
| GameUI/CvarSlider.cpp | 10044EF0-10045500 | 18 |  |
| GameUI/CvarTextEntry.cpp | 10045590-10045A50 | 16 |  |
| GameUI/CvarToggleCheckButton.cpp | 10045AE0-10045F60 | 17 |  |
| GameUI/KeyToggleCheckButton.cpp | 10045FE0-10046270 | 7 |  |
| GameUI/LabeledCommandComboBox.cpp | 100462A0-10046D00 | 42 |  |
| GameUI/PanelListPanel.cpp | 10046D90-100477B0 | 20 |  |
| vgui2/controls/ScrollBar.cpp | 100477D0-100477D0 | 1 | COMDATs named after the vgui2 class, inside the GameUI object before them |
| GameUI/PanelListPanel.cpp | 100477F0-10047A30 | 11 |  |
| vgui2/controls/ScrollBar.cpp | 10047B10-10047B50 | 2 | COMDATs named after the vgui2 class, inside the GameUI object before them |
| GameUI/PanelListPanel.cpp | 10047B60-10047C90 | 6 |  |
| GameUI/vcontrolslistpanel.cpp | 10047D20-100484C0 | 21 |  |
| GameUI/ContentControlDialog.cpp | 100484E0-10048EE0 | 16 |  |
| GameUI/GameConsoleDialog.cpp | 10048F10-1004BA70 | 101 |  |
| GameUI/HelpDialog.cpp | 1004BAB0-1004BCD0 | 5 |  |
| GameUI/LoadGameDialog.cpp | 1004BD00-1004C610 | 8 |  |
| GameUI/MultiplayerAdvancedDialog.cpp | 1004C640-1004DB50 | 22 |  |
| GameUI/NewGameDialog.cpp | 1004DB60-1004E000 | 5 | new: RTTI CNewGameDialog, "NewGameDialog", 4 functions = Linux DWARF (notes) |
| GameUI/PlayerListDialog.cpp | 1004E030-1004EB60 | 16 |  |
| GameUI/SaveGameDialog.cpp | 1004EBE0-1004FA30 | 11 |  |
| GameUI/OptionsDialog.cpp | 1004FA60-1004FDC0 | 7 |  |
| GameUI/OptionsSubAdvanced.cpp | 1004FDF0-10050390 | 17 |  |
| GameUI/OptionsSubAudio.cpp | 100503B0-10050990 | 14 |  |
| GameUI/OptionsSubKeyboard.cpp | 10050A10-100531E0 | 64 |  |
| GameUI/OptionsSubMouse.cpp | 10053210-10053A50 | 19 |  |
| GameUI/OptionsSubMultiplayer.cpp | 10053AC0-10056A10 | 44 |  |
| GameUI/OptionsSubVideo.cpp | 10056A40-10057A40 | 27 |  |
| GameUI/OptionsSubVoice.cpp | 10057A70-10058AB0 | 29 |  |
| vgui2/controls/controls.cpp | 10058C70-10058F07 | 13 |  |
| vgui2/controls/Button.cpp | 10058F20-1005AC50 | 74 |  |
| vgui2/controls/Label.cpp | 1005AD10-1005D750 | 87 |  |
| vgui2/controls/Panel.cpp | 1005D770-10065560 | 316 |  |
| vgui2/controls/TextImage.cpp | 10065590-10066A90 | 46 |  |
| vgui2/controls/BitmapImagePanel.cpp | 10066AB0-10067530 | 13 |  |
| vgui2/controls/Frame.cpp | 10067550-1006BE20 | 117 |  |
| vgui2/controls/EditablePanel.cpp | 1006BE40-1006E0D0 | 60 |  |
| vgui2/controls/BuildGroup.cpp | 1006E170-10071530 | 89 |  |
| vgui2/controls/PanelHorizListPanel.cpp | 10071550-10072390 | 36 |  |
| public/KeyValues.cpp | 10072470-100744D0 | 54 |  |
| vgui2/controls/RichText.cpp | 100744F0-10079610 | 164 |  |
| vgui2/controls/ScrollBar.cpp | 10079630-1007A520 | 39 |  |
| vgui2/controls/MessageBox.cpp | 1007A5B0-1007AF80 | 23 |  |
| vgui2/controls/ComboBox.cpp | 1007B010-1007CD60 | 63 |  |
| vgui2/controls/URLLabel.cpp | 1007CE00-1007D2C0 | 15 |  |
| vgui2/controls/TextEntry.cpp | 1007D2D0-100835C0 | 134 |  |
| vgui2/controls/ListPanel.cpp | 100835E0-1008BF10 | 277 |  |
| vgui2/controls/PropertyPage.cpp | 1008BF30-1008C620 | 26 |  |
| vgui2/controls/PropertyDialog.cpp | 1008C690-1008D080 | 29 |  |
| vgui2/controls/CheckButton.cpp | 1008D110-1008D8D0 | 20 |  |
| vgui2/controls/ToggleButton.cpp | 1008D960-1008DCB0 | 13 |  |
| vgui2/controls/Slider.cpp | 1008DCC0-1008F2D0 | 35 |  |
| vgui2/controls/TestScriptController.cpp | 1008F2E0-10091FB0 | 103 |  |
| vgui2/controls/ProgressBar.cpp | 10091FD0-10092A10 | 25 |  |
| vgui2/controls/HTML.cpp | 10092A80-10094A60 | 68 |  |
| vgui2/controls/QueryBox.cpp | 10094A70-10095210 | 16 |  |
| vgui2/controls/Menu.cpp | 10095220-100992B0 | 138 |  |
| vgui2/controls/MenuItem.cpp | 100992D0-1009A560 | 49 |  |
| vgui2/controls/Image.cpp | 1009A5E0-1009AC50 | 30 |  |
| vgui2/controls/SectionedListPanel.cpp | 1009AC80-100A0B10 | 241 |  |
| vgui2/controls/RadioButton.cpp | 100A0B20-100A1840 | 31 |  |
| vgui2/controls/ImagePanel.cpp | 100A18D0-100A1FD0 | 14 |  |
| vgui2/controls/Tooltip.cpp | 100A1FE0-100A27E0 | 21 |  |
| public/MemPool.cpp | 100A2820-100A2A51 | 5 | new: CMemoryPool (names by bytematch with hw.dll) |
| vgui2/controls/MenuButton.cpp | 100A2A90-100A3340 | 22 |  |
| vgui2/controls/FocusNavGroup.cpp | 100A33D0-100A3FA0 | 18 |  |
| vgui2/controls/BuildModeDialog.cpp | 100A3FC0-100A7590 | 82 |  |
| vgui2/controls/Divider.cpp | 100A75E0-100A7790 | 9 |  |
| vgui2/controls/AnimatingImagePanel.cpp | 100A77A0-100A8230 | 40 |  |
| vgui2/controls/ScrollBarSlider.cpp | 100A8240-100A8EC0 | 30 |  |
| vgui2/controls/ImageList.cpp | 100A8ED0-100A9140 | 17 | starts at the constructor 100A8ED0 |
| vgui2/controls/PropertySheet.cpp | 100A9160-100AB790 | 85 | starts at the constructor 100A9160 |
| public/filesystem_helpers.cpp | 100AB8B0-100ABAF6 | 3 | new, named: ParseFile x2, InitializeCharacterSets ("{}()'") |
| public/characterset.cpp | 100ABB20-100ABB20 | 1 | new, named: CharacterSetBuild |
| vgui2/controls/PanelListPanel.cpp | 100ABB80-100AD100 | 63 |  |

## Open points

- The file names of MsgBuffer_guess.cpp, NetAPI_guess.cpp, Socket_guess.cpp and MasterVersionChk_guess.cpp are not known (no Linux counterpart, no __FILE__ string); the stems come from their RTTI classes and sort into the link position. ChangeGameDialog.cpp is named after its panel name and class.
- The COMDAT tails of the career files and bot_profile.cpp hold many std::vector and std::string instantiations (for example a std::string copy with an EH catch block at 1001ADF0, inside CareerProfile.cpp's tail); they are LIB candidates but stay UNKNOWN with their object's file until identified.
- Boundary cases decided by content: TaskButton.cpp starts at its constructor 100403C0 and CvarSlider.cpp at 10044EF0 (both end exactly on a 16-byte boundary before the next function); 1004DB50 (`ret 4`, 16 bytes) is left in MultiplayerAdvancedDialog.cpp's tail before NewGameDialog.cpp; RunGameEngine.cpp's regular code is only its initializers (10038FF0), its CRunGameEngine methods are inline COMDATs.
- MenuItem.cpp (10040300, 10040340) and ScrollBar.cpp (100477D0, 10047B10, 10047B50) rows are vgui2 inline functions placed in Taskbar.cpp's and GameUI/PanelListPanel.cpp's COMDAT tails; they keep the vgui2 file because their unit lists claim them.
- common/Random.cpp: ran1 and RandomLong are unnamed, the Linux prototypes use int32, which VC6 decorates as _H when it is __int32 (refs/halflife steamtypes.h) and as H or J otherwise. byteorder.cpp has the same question for BigLong and friends.
- public/MemPool.cpp is named (bytematch with hw.dll) but not registered as a GameUI unit.
