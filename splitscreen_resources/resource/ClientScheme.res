///////////////////////////////////////////////////////////
// Tracker scheme resource file
//
// sections:
//		Colors			- all the colors used by the scheme
//		BaseSettings	- contains settings for app to use to draw controls
//		Fonts			- list of all the fonts used by app
//		Borders			- description of all the borders
//
///////////////////////////////////////////////////////////
Scheme
{
	//////////////////////// COLORS ///////////////////////////
	Colors
	{
		// base colors
		"White"				"255 255 255 255"
		"Grey"				"131 131 131 255"
		"GreyDim"			"150 150 150 255"
		"LightGrey"			"195 195 195 255"
		
		"DarkGrey"			"100 100 100 150"
		"DarkGrey_Opaque"		"100 100 100 255"
		"AlmostBlack"			"40 40 40 255"
		
		"Red"				"192 28 0 255"
		"TransparentRed"		"192 28 0 200"

		"Black"				"0 0 0 255"
		"TransparentBlack"		"0 0 0 200"
		"TransparentLightBlack"		"0 0 0 150"
		"DimBlack"			"0 0 0 60"

		"SB_SelText"			"206 224 204 255"
		"SB_SelBG"			"39 70 44 200"
		"SB_Text"			"153 255 140 255"
		"SB_Labels"			"153 255 140 255"
		"SB_Gold"			"232 180 2 255"
		"SB_White"			"220 220 220 255"
		"SB_Elim_FFA"		"255 0 0 255"
		"SB_Elim_Team"		"0 0 0 255"

		"Orange"			"244 101 14 255"

		"Blank"				"0 0 0 0"

		//Goldeneye
		"LightBrown"		"162 101 35 255"
		"FolderTan"		"220 205 178 255"
		"FolderTan_Trans"	"172 160 138 185"

		"GEChatColor"		"220 220 220 255"
		"GEChatLocation"	"235 118 0 255"
		"GEChatAchievement"	"55 171 16 255"
		"GENoTeamChatColor" "95 185 82 255"
		"GEJanusChatColor"	"206 43 43 255"
		"GEMI6ChatColor"	"94 171 231 255"

		"GEDeathLocal"		"254 224 107 255"
		"GEDeathSuicide"	"255 100 100 255"
		"GEDeathKiller"		"255 100 100 255"
		"GEDeathVictim"		"100 255 100 255"
		"GEDeathWeapon"		"100 100 255 255"

		"GEGameplayInfo"	"30 30 30 255"
		"GEGameplayBG"		"172 160 138 200"

		"RoundTimerSpec"	"0 0 0 235"
		"RoundTimerNorm"	"255 255 255 255"
		"RoundTimerPause"	"120 120 120 255"

		"Scoreboard_Spectators"		"220 220 220 200"

		"SpecGUI_Janus"		"142 37 37 255"
		"SpecGUI_MI6"		"37 50 142 255"
		"SpecGUI_Default"	"0 0 0 255"
		
		"GEVoiceChat"		"255 255 255 255"
	}
	
	///////////////////// BASE SETTINGS ////////////////////////
	//
	// default settings for all panels
	// controls use these to determine their settings
	BaseSettings
	{
		"FgColor"			"200 200 200 255"
		"BgColor"			"0 0 0 76"

		"Panel.FgColor"			"255 255 255 255"
		"Panel.BgColor"			"0 0 0 0"
		
		"IntHigh"		"0 255 0 255"
		"IntLow"		"0 150 0 255"
		
		"BrightFg"		"170 170 170 150"

		"DamagedBg"			"180 0 0 200"
		"DamagedFg"			"180 0 0 230"
		"BrightDamagedFg"		"255 0 0 255"

		// Weapon Selection Colors
		"SelectionNumberFg"		"Black"
		"SelectionTextFg"		"AlmostBlack"
		"SelectionUnselTextFg"		"White"
		"SelectionBoxBg" 		"FolderTan_Trans"
		"SelectionSelectedBoxBg" 	"FolderTan"
		"SelectionUnselectableBoxBg"	"TransparentRed"
		
		"ZoomReticleColor"		"170 170 170 255"

		// HL1-style HUD colors
		"Yellowish"			"255 255 255 225"
		"Normal"			"255 255 255 255"
		"Caution"			"255 255 255 255"
		"Red"				"255 255 255 255"

		// Top-left corner of the "Half-Life 2" on the main screen
		"Main.Title1.X"		"76"
		"Main.Title1.Y"		"184"
		"Main.Title1.Color"	"255 255 255 255"
		
		// Top-left corner of the "DEATHMATCH" on the main screen
		"Main.Title2.X"		"315"
		"Main.Title2.Y"		"222"
		"Main.Title2.Color"	"255 255 255 255"

		// The menu on the main screen: anchored to the bottom left (a Y past the bottom makes the
		// engine pin it above Main.BottomBorder), and far enough left to clear the title in the background
		"Main.Menu.X"		"21"	// 14 x 1.5 (GE_MENU_SCALE_1080); GameUI scales Main.* with the screen height (480 = full height)
		"Main.Menu.Y"		"480"

		// Blank space to leave beneath the menu on the main screen
		"Main.BottomBorder"	"6"	// 4 x 1.5


		// vgui_controls color specifications
		Border.Bright					"LightGrey"		// the lit side of a control
		Border.Dark						"Grey"		// the dark/unlit side of a control
		Border.Selection				"Blank"				// the additional border color for displaying the default/selected button

		Button.TextColor			"LightGrey"
		Button.BgColor				"TransparentBlack"
		Button.ArmedTextColor			"DarkGrey_Opaque"
		Button.ArmedBgColor			"TransparentRed"
		Button.DepressedTextColor		"DarkGrey_Opaque"
		Button.DepressedBgColor			"TransparentRed"

		CheckButton.TextColor			"Black"
		CheckButton.SelectedTextColor	"Black"
		CheckButton.BgColor				"TransparentBlack"
		CheckButton.Border1  			"Border.Dark" 		// the left checkbutton border
		CheckButton.Border2  			"Border.Bright"		// the right checkbutton border
		CheckButton.Check				"Black"				// color of the check itself

		ComboBoxButton.ArrowColor		"Grey"
		ComboBoxButton.ArmedArrowColor	"Grey"
		ComboBoxButton.BgColor			"TransparentBlack"
		ComboBoxButton.DisabledBgColor	"Blank"

		Frame.BgColor					"TransparentBlack"
		Frame.OutOfFocusBgColor			"TransparentBlack"
		Frame.FocusTransitionEffectTime	"0.0"	// time it takes for a window to fade in/out on focus/out of focus
		Frame.TransitionEffectTime		"0.0"	// time it takes for a window to fade in/out on open/close
		Frame.AutoSnapRange				"0"
		FrameGrip.Color1				"Blank"
		FrameGrip.Color2				"Blank"
		FrameTitleButton.FgColor		"Blank"
		FrameTitleButton.BgColor		"Blank"
		FrameTitleButton.DisabledFgColor	"Blank"
		FrameTitleButton.DisabledBgColor	"Blank"
		FrameSystemButton.FgColor		"Blank"
		FrameSystemButton.BgColor		"Blank"
		FrameSystemButton.Icon			""
		FrameSystemButton.DisabledIcon	""
		FrameTitleBar.TextColor			"Grey"
		FrameTitleBar.BgColor			"Blank"
		FrameTitleBar.DisabledTextColor	"Grey"
		FrameTitleBar.DisabledBgColor	"Blank"

		GraphPanel.FgColor				"TransparentBlack"
		GraphPanel.BgColor				"TransparentBlack"

		Label.TextDullColor				"Black"
		Label.TextColor					"Black"
		Label.TextBrightColor			"SB_Labels"
		Label.SelectedTextColor			"White"
		Label.BgColor					"Blank"
		Label.DisabledFgColor1			"Blank"
		Label.DisabledFgColor2			"Grey"

		ListPanel.TextColor				"White"
		ListPanel.BgColor				"TransparentBlack"
		ListPanel.SelectedTextColor			"White"
		ListPanel.SelectedBgColor			"Red"
		ListPanel.SelectedOutOfFocusBgColor		"Red"
		ListPanel.EmptyListInfoTextColor		"Grey"

		// Added for CharSelect Panel
		HorzListPanel.BgColor			"Blank"

		Menu.TextColor				"Black"
		Menu.BgColor					"FolderTan_Trans"
		Menu.ArmedTextColor			"Black"
		Menu.ArmedBgColor				"TransparentRed"
		Menu.TextInset					"6"

		Chat.TypingText					"White"

		Panel.FgColor					"GreyDim"
		Panel.BgColor					"Blank"

		PanelListPanel.TextColor			"White"
		PanelListPanel.BgColor				"TransparentBlack"
		PanelListPanel.SelectedTextColor		"White"
		PanelListPanel.SelectedBgColor			"Red"
		PanelListPanel.SelectedOutOfFocusBgColor	"Red"
		PanelListPanel.EmptyListInfoTextColor		"Grey"

		ProgressBar.FgColor				"Grey"
		ProgressBar.BgColor				"TransparentBlack"

		PropertySheet.TextColor			"Grey"
		PropertySheet.SelectedTextColor	"Grey"
		PropertySheet.TransitionEffectTime	"0.25"	// time to change from one tab to another

		RadioButton.TextColor			"Grey"
		RadioButton.SelectedTextColor	"Grey"

		RichText.TextColor			"Black"
		RichText.BgColor			"Blank"
		RichText.SelectedTextColor		"Black"
		RichText.SelectedBgColor		"Blank"

		ScrollBarButton.FgColor			"Black"
		ScrollBarButton.BgColor			"DarkGrey"
		ScrollBarButton.ArmedFgColor		"Black"
		ScrollBarButton.ArmedBgColor		"DarkGrey"
		ScrollBarButton.DepressedFgColor	"Black"
		ScrollBarButton.DepressedBgColor	"DarkGrey_Opaque"

		ScrollBar.Wide				"10"

		ScrollBarSlider.FgColor				"DarkGrey"		// nob color
		ScrollBarSlider.BgColor				"FolderTan"	// slider background color

		SectionedListPanel.HeaderTextColor		"SB_Text"
		SectionedListPanel.HeaderBgColor		"Blank"
		SectionedListPanel.DividerColor			"Blank"
		SectionedListPanel.TextColor			"SB_Text"
		SectionedListPanel.BrightTextColor		"SB_Text"
		SectionedListPanel.BgColor			"Blank"
		SectionedListPanel.SelectedTextColor		"SB_SelText"
		SectionedListPanel.SelectedBgColor		"SB_SelBG"
		SectionedListPanel.OutOfFocusSelectedTextColor	"Black"
		SectionedListPanel.OutOfFocusSelectedBgColor	"255 255 255 32"

		Slider.NobColor				"108 108 108 255"
		Slider.TextColor			"140 140 140 255"
		Slider.TrackColor			"31 31 31 255"
		Slider.DisabledTextColor1	"117 117 117 255"
		Slider.DisabledTextColor2	"30 30 30 255"

		TextEntry.TextColor			"Black"
		TextEntry.BgColor				"FolderTan_Trans"
		TextEntry.CursorColor		"Grey"
		TextEntry.DisabledTextColor	"Grey"
		TextEntry.DisabledBgColor	"Blank"
		TextEntry.SelectedTextColor	"Black"
		TextEntry.SelectedBgColor		"FolderTan_Trans"
		TextEntry.OutOfFocusSelectedBgColor	"Blank"
		TextEntry.FocusEdgeColor	"TransparentBlack"

		ToggleButton.SelectedTextColor	"Grey"

		Tooltip.TextColor			"TransparentBlack"
		Tooltip.BgColor				"Red"

		TreeView.BgColor			"TransparentBlack"

		WizardSubPanel.BgColor		"Blank"

		// scheme-specific colors
		"FgColor"		"Grey"
		"BgColor"		"TransparentBlack"

		"ViewportBG"		"Blank"

		"HudIcon_Green"		"0 160 0 255"
		"HudIcon_Red"		"160 0 0 255"

		// CHudMenu
		"ItemColor"		"170 170 170 120"	// default 255 167 42 255
		"MenuColor"		"170 170 170 255"
		"MenuBoxBg"		"0 0 0 100"

		// Hint message colors
		"HintMessageFg"			"255 255 255 255"
		"HintMessageBg" 		"0 0 0 60"

		"ProgressBarFg"			"220 220 220 255"

		// Top-left corner of the "Counter-Strike" on the main screen
		"Main.Title1.X"		"32"
		"Main.Title1.Y"		"180"
		"Main.Title1.Color"	"255 255 255 255"

		// Top-left corner of the "SOURCE" on the main screen
		"Main.Title2.X"		"380"	
		"Main.Title2.Y"		"205"
		"Main.Title2.Color"	"255 255 255 80"

		// Top-left corner of the "BETA" on the main screen
		"Main.Title3.X"		"460"
		"Main.Title3.Y"		"-10"
		"Main.Title3.Color"	"220 220 220 255"

		// The menu on the main screen: anchored to the bottom left (a Y past the bottom makes the
		// engine pin it above Main.BottomBorder), and far enough left to clear the title in the background
		"Main.Menu.X"		"21"	// 14 x 1.5 (GE_MENU_SCALE_1080); GameUI scales Main.* with the screen height (480 = full height)
		"Main.Menu.Y"		"480"

		// Blank space to leave beneath the menu on the main screen
		"Main.BottomBorder"	"6"	// 4 x 1.5
	}
	//////////////////////// FONTS /////////////////////////////
	//
	// describes all the fonts
	Fonts
	{
		// fonts are used in order that they are listed
		// fonts are used in order that they are listed
		"DebugFixed"
		{
			"1"
			{
				"name"		"Tahoma"
				"tall"		"14"
				"weight"	"400"
				"antialias" "1"
			}
		}
		// fonts are used in order that they are listed
		"DebugFixedSmall"
		{
			"1"
			{
				"name"		"Tahoma"
				"tall"		"14"
				"weight"	"400"
				"antialias" "1"
			}
		}
		// fonts listed later in the order will only be used if they fulfill a range not already filled
		// if a font fails to load then the subsequent fonts will replace
		"Default"
		{
			"1"
			{
				"name"		"Tahoma"
				"tall"		"11"
				"weight"	"500"
				"antialias" "1"
				"yres"	"1 599"
			}
			"1DropShadow"
			{
				"name"		"Tahoma"
				"tall"		"11"
				"weight"	"500"
				"dropshadow" "1"
				"antialias" "1"
				"yres"	"1 599"
			}
			"2"
			{
				"name"		"Tahoma"
				"tall"		"11"
				"weight"	"600"
				"antialias" "1"
				"yres"	"600 767"
			}
			"2DropShadow"
			{
				"name"		"Tahoma"
				"tall"		"11"
				"weight"	"600"
				"dropshadow" "1"
				"antialias" "1"
				"yres"	"600 767"
			}
			"3"
			{
				"name"		"Tahoma"
				"tall"		"13"
				"weight"	"700"
				"antialias" "1"
				"yres"	"768 1023"
			}
			"4"
			{
				"name"		"Tahoma"
				"tall"		"14"
				"weight"	"800"
				"antialias" "1"
				"yres"	"1024 1199"
			}
			"5"
			{
				"name"		"Tahoma"
				"tall"		"16"
				"weight"	"800"
				"antialias" "1"
				"yres"	"1200 1800"
			}
		}
		"DefaultSmall"
		{
			"1"
			{
				"name"		"Tahoma"
				"tall"		"11"
				"weight"	"0"
				"yres"	"480 599"
			}
			"2"
			{
				"name"		"Tahoma"
				"tall"		"12"
				"weight"	"0"
				"yres"	"600 767"
				"antialias"	"1"
			}
			"3"
			{
				"name"		"Tahoma"
				"tall"		"12"
				"weight"	"0"
				"yres"	"768 1023"
				"antialias"	"1"
			}
			"4"
			{
				"name"		"Tahoma"
				"tall"		"12"
				"weight"	"0"
				"yres"	"1024 1199"
				"antialias"	"1"
			}
			"5"
			{
				"name"		"Tahoma"
				"tall"		"14"
				"weight"	"0"
				"yres"	"1200 1800"
				"antialias"	"1"
			}
		}
		"DefaultVerySmall"
		{
			"1"
			{
				"name"		"Tahoma"
				"tall"		"11"
				"weight"	"0"
				"yres"	"480 599"
			}
			"2"
			{
				"name"		"Tahoma"
				"tall"		"12"
				"weight"	"0"
				"yres"	"600 767"
				"antialias"	"1"
			}
			"3"
			{
				"name"		"Tahoma"
				"tall"		"12"
				"weight"	"0"
				"yres"	"768 1023"
				"antialias"	"1"
			}
			"4"
			{
				"name"		"Tahoma"
				"tall"		"12"
				"weight"	"0"
				"yres"	"1024 1199"
				"antialias"	"1"
			}
			"5"
			{
				"name"		"Tahoma"
				"tall"		"14"
				"weight"	"0"
				"yres"	"1200 1800"
				"antialias"	"1"
			}
		}

		HudNumbers
		{
			"1"
			{
				"name"		"Tahoma"
				"tall"		"22"
				"weight"	"50"
				"antialias" 	"1"
				"additive"	"0"
				"custom"	"1"
			}
		}
		HudNumbersGlow
		{
			"1"
			{
				"name"		"Tahoma"
				"tall"		"22"
				"weight"	"100"
				"blur"		"4"
				"scanlines" 	"1"
				"antialias" 	"1"
				"additive"	"0"
				"custom"	"1"
			}
		}
		HudNumbersSmall
		{
			"1"
			{
				"name"		"Tahoma"
				"tall"		"16"
				"weight"	"1000"
				"additive"	"0"
				"antialias" 	"1"
				"custom"	"1"
			}
		}
		HudSelectionNumbers
		{
			"1"
			{
				"name"		"DS Moster GES"
				"tall"  	"16"
				"weight" 	"700"
				"additive" 	"0"
				"antialias" 	"1"
				"custom"	"1"
			}
		}
		BudgetLabel
		{
			"1"
			{
				"name"		"Tahoma"
				"tall"		"14"
				"weight"	"400"
				"outline"	"1"
			}
		}
		DebugOverlay
		{
			"1"
			{
				"name"		"Tahoma"
				"tall"		"14"
				"weight"	"700"
				"outline"	"1"
			}
		}
		"CloseCaption_Normal"
		{
			"1"
			{
				"name"		"Tahoma"
				"tall"		"26"
				"weight"	"500"
			}
		}
		"CloseCaption_Italic"
		{
			"1"
			{
				"name"		"Tahoma"
				"tall"		"26"
				"weight"	"500"
				"italic"	"1"
			}
		}
		"CloseCaption_Bold"
		{
			"1"
			{
				"name"		"Tahoma"
				"tall"		"26"
				"weight"	"900"
			}
		}
		"CloseCaption_BoldItalic"
		{
			"1"
			{
				"name"		"Tahoma"
				"tall"		"26"
				"weight"	"900"
				"italic"	"1"
			}
		}
		"Trebuchet24"
		{
			"1"
			{
				"name"		"Trebuchet MS"
				"tall"		"24"
				"weight"	"900"
				"antialias" 	"1"
				"additive"	"1"
			}
		}
		"Trebuchet18"
		{
			"1"
			{
				"name"		"Trebuchet MS"
				"tall"		"18"
				"weight"	"900"
			}
		}
		ClientTitleFont
		{
			"1"
			{
				"name"  	"HL2MP"
				"tall"  	"46"
				"weight" 	"0"
				"additive" 	"0"
				"antialias" 	"1"
			}
		}
		CreditsLogo
		{
			"1"
			{
				"name"		"Tahoma"
				"tall"		"128"
				"weight"	"0"
				"antialias" 	"1"
				"additive"	"1"
				"custom"	"1"
			}
		}
		CreditsText
		{
			"1"
			{
				"name"		"Trebuchet MS"
				"tall"		"20"
				"weight"	"900"
				"antialias" 	"1"
				"additive"	"1"
			}
		}
		CreditsOutroLogos
		{
			"1"
			{
				"name"		"Tahoma"
				"tall"		"48"
				"weight"	"0"
				"antialias" 	"1"
				"additive"	"1"
				"custom"	"1"
			}
		}
		CreditsOutroText
		{
			"1"
			{
				"name"		"Tahoma"
				"tall"		"9"
				"weight"	"900"
				"antialias" 	"1"
			}
		}
		CenterPrintText
		{
			// note that this scales with the screen resolution
			"1"
			{
				"name"		"Trebuchet MS"
				"tall"		"18"
				"weight"	"900"
				"antialias" 	"1"
				"additive"	"1"
			}
		}
		"ChatFont"
		{
			"1"
			{
				"name"		"Tahoma"
				"tall"		"10"
				"weight"	"700"
				"yres"		"480 599"
				"antialias" 	"1"
				"dropshadow"	"1"
			}
			"2"
			{
				"name"		"Tahoma"
				"tall"		"12"
				"weight"	"700"
				"yres"		"600 767"
				"antialias" 	"1"
				"dropshadow"	"1"
			}
			"3"
			{
				"name"		"Tahoma"
				"tall"		"16"
				"weight"	"700"
				"yres"		"768 1023"
				"dropshadow"	"1"
			}
			"4"
			{
				"name"		"Tahoma"
				"tall"		"18"
				"weight"	"700"
				"yres"		"1024 1199"
				"dropshadow"	"1"
			}
			"5"
			{
				"name"		"Tahoma"
				"tall"		"20"
				"weight"	"700"
				"yres"		"1200 10000"
				"dropshadow"	"1"
			}
		}
		"InfoTextFont"
		{
			"1"
			{
				"name"		"Tahoma"
				"tall"		"10"
				"weight"	"600"
				"yres"		"480 599"
				"antialias" 	"1"
				"dropshadow"	"1"
			}
			"2"
			{
				"name"		"Tahoma"
				"tall"		"12"
				"weight"	"600"
				"yres"		"600 767"
				"antialias" 	"1"
				"dropshadow"	"1"
			}
			"3"
			{
				"name"		"Tahoma"
				"tall"		"14"
				"weight"	"600"
				"yres"		"768 1023"
				"antialias" 	"1"
				"dropshadow"	"1"
			}
			"4"
			{
				"name"		"Tahoma"
				"tall"		"16"
				"weight"	"600"
				"yres"		"1024 1199"
				"antialias" 	"1"
				"dropshadow"	"1"
			}
			"5"
			{
				"name"		"Tahoma"
				"tall"		"18"
				"weight"	"600"
				"yres"		"1200 10000"
				"antialias" 	"1"
				"dropshadow"	"1"
			}
		}

		HudHintText
		{
			"1"
			{
				"name"		"Verdana"
				"tall"		"11"
				"weight"	"0"
				"antialias" "1"
				"additive"	"1"
			}
		}
		
		// this is the symbol font
		"Marlett"
		{
			"1"
			{
				"name"		"Marlett"
				"tall"		"14"
				"weight"	"0"
				"symbol"	"1"
			}
		}

//
// GE SPECIFIC FONTS!!
//
		
		"TypeWriter_Tiny"
		{
			"1"
			{
				"name"  "Verdana"
				"tall"  "10"
				"weight" "700"
				"additive" "0"
				"yres"	"480 599"
				"antialias" "1"
				"custom"	"1"
			}
			"2"
			{
				"name"  "Verdana"
				"tall"  "12"
				"weight" "700"
				"additive" "0"
				"yres"	"600 767"
				"antialias" "1"
				"custom"	"1"
			}
			"3"
			{
				"name"  "DS Moster GES"
				"tall"  "16"
				"weight" "200"
				"additive" "0"
				"yres"	"768 1023"
				"antialias" "1"
				"custom"	"1"
			}
			"4"
			{
				"name"  "DS Moster GES"
				"tall"  "18"
				"weight" "200"
				"additive" "0"
				"yres"		"1024 1199"
				"antialias" "1"
				"custom"	"1"
			}
			"5"
			{
				"name"  "DS Moster GES"
				"tall"  "20"
				"weight" "200"
				"additive" "0"
				"yres"		"1200 10000"
				"antialias" "1"
				"custom"	"1"
			}
		}
		"TypeWriter_Small"
		{
			"1"
			{
				"name"  "DS Moster GES"
				"tall"  "12"
				"weight" "300"
				"additive" "0"
				"antialias" "1"
				"custom"	"1"
			}
		}
		"TypeWriter"
		{
			"1"
			{
				"name"  "DS Moster GES"
				"tall"  "14"
				"weight" "300"
				"additive" "0"
				"antialias" "1"
				"custom"	"1"
			}
		}
		"TypeWriter_Medium"
		{
			"1"
			{
				"name"  "DS Moster GES"
				"tall"  "20"
				"weight" "400"
				"additive" "0"
				"antialias" "1"
				"custom"	"1"
			}
		}
		"TypeWriter_Large"
		{
			"1"
			{
				"name"  "DS Moster GES"
				"tall"  "30"
				"weight" "400"
				"additive" "0"
				"antialias" "1"
				"custom"	"1"
			}
		}
		"TypeWriter_Header"
		{
			"1"
			{
				"name"  "DS Moster GES"
				"tall"  "16"
				"weight" "400"
				"additive" "0"
				"antialias" "1"
				"custom"	"1"
			}
		}
		"HandWriting"
		{
			"1"
			{
				"name"  "aescrawl"
				"tall"  "24"
				"weight" "300"
				"additive" "0"
				"antialias" "1"
				"custom"	"1"
			}
		}
		"CharSelectName"
		{
			"1"
			{
				"name"  	"Adventure Subtitles GES"
				"tall"  	"7"
				"weight" 	"100"
				"additive" 	"0"
				"antialias" "1"
				"custom"	"1"
			}
		}
		"Scoreboard"
		{
			"1"
			{
				"name"  "Adventure Subtitles GES"
				"tall"  "8"
				"weight" "300"
				"additive" "0"
				"antialias" "1"
				"custom"	"1"
			}
		}
		"Scoreboard_Spec"
		{
			"1"
			{
				"name"  "Adventure Subtitles GES"
				"tall"  "6"
				"weight" "100"
				"additive" "0"
				"antialias" "1"
				"custom"	"1"
			}
		}
		"VoiceChatName"
		{
			"1"
			{
				"name"  "Adventure Subtitles GES"
				"tall"  "10"
				"weight" "10"
				"additive" "0"
				"antialias" "1"
				"custom"	"1"
			}
		}
		"VoiceChatName_Small"
		{
			"1"
			{
				"name"  "Adventure Subtitles GES"
				"tall"  "7"
				"weight" "10"
				"additive" "0"
				"antialias" "1"
				"custom"	"1"
			}
		}
		"PickupNumbers"
		{
			"1"
			{
				"name"  	"Adventure Subtitles GES"
				"tall"  	"10"
				"weight" 	"400"
				"additive" 	"0"
				"antialias" 	"1"
				"custom"	"1"
			}
		}

		"AmmoNumbers"
		{
			"1"
			{
				"name"  	"Adventure Subtitles GES"
				"tall"  	"17"
				"weight" 	"30"
				"additive" 	"0"
				"antialias" 	"1"
				"dropshadow" 	"1"
				"custom"	"1"
			}
		}

		"GETargetID"
		{
			"1"
			{
				"name"		"Trebuchet MS"
				"tall"		"14"
				"weight"	"800"
				"antialias" 	"1"
				"additive"	"0"
			}
		}
		
		"GERespawnText"
		{
			"1"
			{
				"name"		"DS Moster GES"
				"tall"		"18"
				"weight"	"800"
				"antialias" 	"1"
				"additive"	"0"
				"custom"	"1"
			}
		}

		"HUDTeamScore"
		{
			"1"
			{
				"name"		"DS Moster GES"
				"tall"		"11"
				"weight"	"300"
				"antialias" 	"1"
				"additive"	"0"
				"custom"	"1"
			}
		}

		"HUDTeamScorePulse"
		{
			"1"
			{
				"name"		"DS Moster GES"
				"tall"		"11"
				"weight"	"400"
				"blur"		"5"
				"scanlines" 	"1"
				"antialias" 	"1"
				"additive"	"0"
				"custom"	"1"
			}
		}
		
		"GES_Symbols"
		{
			"1"
			{
				"name"  "GESDings"
				"tall"  "12"
				"weight" "300"
				"additive" "0"
				"antialias" "1"
				"custom"	"1"
			}
		}
//
// END GES SPECIFIC FONTS
//

		"Intermission"
		{
			"1"
			{
				"name"		"Tahoma"
				"tall"		"30"
				"weight"	"700"
				"yres"	"480 599"
				"antialias" "1"
				"dropshadow"	"1"
			}
			"2"
			{
				"name"		"Tahoma"
				"tall"		"36"
				"weight"	"700"
				"yres"	"600 767"
				"antialias" "1"
				"dropshadow"	"1"
			}
			"3"
			{
				"name"		"Tahoma"
				"tall"		"42"
				"weight"	"700"
				"yres"	"768 1023"
				"dropshadow"	"1"
			}
			"4"
			{
				"name"		"Tahoma"
				"tall"		"54"
				"weight"	"700"
				"yres"	"1024 1199"
				"dropshadow"	"1"
			}
			"5"
			{
				"name"		"Tahoma"
				"tall"		"60"
				"weight"	"700"
				"yres"	"1200 10000"
				"dropshadow"	"1"
			}
		}		

		"IntermissionSmall"
		{
			"1"
			{
				"name"		"Tahoma"
				"tall"		"15"
				"weight"	"700"
				"yres"	"480 599"
				"antialias" "1"
				"dropshadow"	"1"
			}
			"2"
			{
				"name"		"Tahoma"
				"tall"		"18"
				"weight"	"700"
				"yres"	"600 767"
				"antialias" "1"
				"dropshadow"	"1"
			}
			"3"
			{
				"name"		"Tahoma"
				"tall"		"21"
				"weight"	"700"
				"yres"	"768 1023"
				"dropshadow"	"1"
			}
			"4"
			{
				"name"		"Tahoma"
				"tall"		"27"
				"weight"	"700"
				"yres"	"1024 1199"
				"dropshadow"	"1"
			}
			"5"
			{
				"name"		"Tahoma"
				"tall"		"30"
				"weight"	"700"
				"yres"	"1200 10000"
				"dropshadow"	"1"
			}
		}	
		"AchievementNotification"
		{
			"1"
			{
				"name"		"Trebuchet MS"
				"tall"		"14"
				"weight"	"900"
				"antialias" "1"
			}
		}	
			
	}

		//////////////////// BORDERS //////////////////////////////
	//
	// describes all the border types
	Borders
	{
		BaseBorder
		{
			"inset" "0 0 1 1"
			Left
			{
				"1"
				{
					"color" "Border.Dark"
					"offset" "0 1"
				}
			}

			Right
			{
				"1"
				{
					"color" "Border.Bright"
					"offset" "1 0"
				}
			}

			Top
			{
				"1"
				{
					"color" "Border.Dark"
					"offset" "0 0"
				}
			}

			Bottom
			{
				"1"
				{
					"color" "Border.Bright"
					"offset" "0 0"
				}
			}
		}
		
		TitleButtonBorder
		{
			"inset" "0 0 1 1"
			Left
			{
				"1"
				{
					"color" "Border.Bright"
					"offset" "0 1"
				}
			}

			Right
			{
				"1"
				{
					"color" "Border.Dark"
					"offset" "1 0"
				}
			}

			Top
			{
				"1"
				{
					"color" "Border.Bright"
					"offset" "0 0"
				}
			}

			Bottom
			{
				"1"
				{
					"color" "Border.Dark"
					"offset" "0 0"
				}
			}
		}

		TitleButtonDisabledBorder
		{
			"inset" "0 0 1 1"
			Left
			{
				"1"
				{
					"color" "BgColor"
					"offset" "0 1"
				}
			}

			Right
			{
				"1"
				{
					"color" "BgColor"
					"offset" "1 0"
				}
			}
			Top
			{
				"1"
				{
					"color" "BgColor"
					"offset" "0 0"
				}
			}

			Bottom
			{
				"1"
				{
					"color" "BgColor"
					"offset" "0 0"
				}
			}
		}

		TitleButtonDepressedBorder
		{
			"inset" "1 1 1 1"
			Left
			{
				"1"
				{
					"color" "Border.Dark"
					"offset" "0 1"
				}
			}

			Right
			{
				"1"
				{
					"color" "Border.Bright"
					"offset" "1 0"
				}
			}

			Top
			{
				"1"
				{
					"color" "Border.Dark"
					"offset" "0 0"
				}
			}

			Bottom
			{
				"1"
				{
					"color" "Border.Bright"
					"offset" "0 0"
				}
			}
		}

		ScrollBarButtonBorder
		{
			"inset" "1 0 0 0"
			Left
			{
				"1"
				{
					"color" "Border.Bright"
					"offset" "0 1"
				}
			}

			Right
			{
				"1"
				{
					"color" "Border.Dark"
					"offset" "1 0"
				}
			}

			Top
			{
				"1"
				{
					"color" "Border.Bright"
					"offset" "0 0"
				}
			}

			Bottom
			{
				"1"
				{
					"color" "Border.Dark"
					"offset" "0 0"
				}
			}
		}

		ScrollBarButtonDepressedBorder
		{
			"inset" "2 2 0 0"
			Left
			{
				"1"
				{
					"color" "Border.Dark"
					"offset" "0 1"
				}
			}

			Right
			{
				"1"
				{
					"color" "Border.Bright"
					"offset" "1 0"
				}
			}

			Top
			{
				"1"
				{
					"color" "Border.Dark"
					"offset" "0 0"
				}
			}

			Bottom
			{
				"1"
				{
					"color" "Border.Bright"
					"offset" "0 0"
				}
			}
		}
		
		ButtonBorder
		{
			"inset" "0 0 0 0"
			Left
			{
				"1"
				{
					"color" "Border.Bright"
					"offset" "0 1"
				}
			}

			Right
			{
				"1"
				{
					"color" "Border.Dark"
					"offset" "0 0"
				}
			}

			Top
			{
				"1"
				{
					"color" "Border.Bright"
					"offset" "1 1"
				}
			}

			Bottom
			{
				"1"
				{
					"color" "Border.Dark"
					"offset" "0 0"
				}
			}
		}

		FrameBorder
		{
			"inset" "0 0 1 1"
			Left
			{
				"1"
				{
					"color" "ControlBG"
					"offset" "0 1"
				}
			}

			Right
			{
				"1"
				{
					"color" "ControlBG"
					"offset" "0 0"
				}
			}

			Top
			{
				"1"
				{
					"color" "ControlBG"
					"offset" "0 1"
				}
			}

			Bottom
			{
				"1"
				{
					"color" "ControlBG"
					"offset" "0 0"
				}
			}
		}

		TabBorder
		{
			"inset" "0 0 1 1"
			Left
			{
				"1"
				{
					"color" "Border.Bright"
					"offset" "0 1"
				}
			}

			Right
			{
				"1"
				{
					"color" "Border.Dark"
					"offset" "1 0"
				}
			}

			Top
			{
				"1"
				{
					"color" "Border.Bright"
					"offset" "0 0"
				}
			}

			Bottom
			{
				"1"
				{
					"color" "Border.Bright"
					"offset" "0 0"
				}
			}
		}

		TabActiveBorder
		{
			"inset" "0 0 1 0"
			Left
			{
				"1"
				{
					"color" "Border.Bright"
					"offset" "0 0"
				}
			}

			Right
			{
				"1"
				{
					"color" "Border.Dark"
					"offset" "1 0"
				}
			}

			Top
			{
				"1"
				{
					"color" "Border.Bright"
					"offset" "0 0"
				}
			}

			Bottom
			{
				"1"
				{
					"color" "ControlBG"
					"offset" "6 2"
				}
			}
		}


		ToolTipBorder
		{
			"inset" "0 0 1 0"
			Left
			{
				"1"
				{
					"color" "Border.Dark"
					"offset" "0 0"
				}
			}

			Right
			{
				"1"
				{
					"color" "Border.Dark"
					"offset" "1 0"
				}
			}

			Top
			{
				"1"
				{
					"color" "Border.Dark"
					"offset" "0 0"
				}
			}

			Bottom
			{
				"1"
				{
					"color" "Border.Dark"
					"offset" "0 0"
				}
			}
		}

		// this is the border used for default buttons (the button that gets pressed when you hit enter)
		ButtonKeyFocusBorder
		{
			"inset" "0 0 0 0"
			Left
			{
				"1"
				{
					"color" "Border.Bright"
					"offset" "0 1"
				}
			}

			Right
			{
				"1"
				{
					"color" "Border.Dark"
					"offset" "0 0"
				}
			}

			Top
			{
				"1"
				{
					"color" "Border.Bright"
					"offset" "1 1"
				}
			}

			Bottom
			{
				"1"
				{
					"color" "Border.Dark"
					"offset" "0 0"
				}
			}
		}

		ButtonDepressedBorder
		{
			"inset" "0 0 0 0"
			Left
			{
				"1"
				{
					"color" "Border.Bright"
					"offset" "0 1"
				}
			}

			Right
			{
				"1"
				{
					"color" "Border.Dark"
					"offset" "0 0"
				}
			}

			Top
			{
				"1"
				{
					"color" "Border.Bright"
					"offset" "1 1"
				}
			}

			Bottom
			{
				"1"
				{
					"color" "Border.Dark"
					"offset" "0 0"
				}
			}
		}

		ComboBoxBorder
		{
			"inset" "0 0 1 1"
			Left
			{
				"1"
				{
					"color" "Border.Dark"
					"offset" "0 1"
				}
			}

			Right
			{
				"1"
				{
					"color" "Border.Bright"
					"offset" "1 0"
				}
			}

			Top
			{
				"1"
				{
					"color" "Border.Dark"
					"offset" "0 0"
				}
			}

			Bottom
			{
				"1"
				{
					"color" "Border.Bright"
					"offset" "0 0"
				}
			}
		}

		MenuBorder
		{
			"inset" "1 1 1 1"
			Left
			{
				"1"
				{
					"color" "Border.Bright"
					"offset" "0 1"
				}
			}

			Right
			{
				"1"
				{
					"color" "Border.Dark"
					"offset" "1 0"
				}
			}

			Top
			{
				"1"
				{
					"color" "Border.Bright"
					"offset" "0 0"
				}
			}

			Bottom
			{
				"1"
				{
					"color" "Border.Dark"
					"offset" "0 0"
				}
			}
		}
		BrowserBorder
		{
			"inset" "0 0 0 0"
			Left
			{
				"1"
				{
					"color" "Border.Dark"
					"offset" "0 0"
				}
			}

			Right
			{
				"1"
				{
					"color" "Border.Bright"
					"offset" "0 0"
				}
			}

			Top
			{
				"1"
				{
					"color" "Border.Dark"
					"offset" "0 0"
				}
			}

			Bottom
			{
				"1"
				{
					"color" "Border.Bright"
					"offset" "0 0"
				}
			}
		}
	}

	
	//////////////////////// CUSTOM FONT FILES /////////////////////////////
	//
	// specifies all the custom (non-system) font files that need to be loaded to service the above described fonts
	CustomFontFiles
	{
		"1"		"resource/HALFLIFE2.ttf"
		"2"		"resource/gesource.ttf"
		"3"		"resource/DS Moster.ttf"
		"4"		"resource/Adventure Subtitles.ttf"
		"5"		"resource/aescrawl.ttf"
		"6"		"resource/marlett.ttf"
		"7"		"resource/GESDings.ttf"
	}

}
