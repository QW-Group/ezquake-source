object Form1: TForm1
  Left = 431
  Top = 215
  BorderIcons = [biSystemMenu, biMinimize, biHelp]
  BorderStyle = bsSingle
  Caption = 'ezQuake Starter'
  ClientHeight = 426
  ClientWidth = 592
  Color = clBtnFace
  Font.Charset = DEFAULT_CHARSET
  Font.Color = clWindowText
  Font.Height = -11
  Font.Name = 'MS Sans Serif'
  Font.Style = []
  OldCreateOrder = False
  Position = poScreenCenter
  OnCreate = FormCreate
  OnShow = FormShow
  PixelsPerInch = 96
  TextHeight = 13
  object gs: TGroupBox
    Left = 8
    Top = 160
    Width = 393
    Height = 153
    Caption = 'Graphics settings'
    TabOrder = 5
    object Label1: TLabel
      Left = 16
      Top = 24
      Width = 76
      Height = 13
      Caption = 'Game resolution'
    end
    object Label2: TLabel
      Left = 16
      Top = 48
      Width = 86
      Height = 13
      Caption = 'Console resolution'
    end
    object Label3: TLabel
      Left = 16
      Top = 72
      Width = 121
      Height = 13
      Caption = 'Bits per pixel (color depth)'
    end
    object Label11: TLabel
      Left = 16
      Top = 96
      Width = 90
      Height = 13
      Caption = 'Screen refresh rate'
    end
    object Label16: TLabel
      Left = 16
      Top = 120
      Width = 112
      Height = 13
      Caption = 'Max number of particles'
    end
    object vmode: TComboBox
      Left = 112
      Top = 20
      Width = 89
      Height = 21
      Hint = 'Starts Quake with the specified resuolution'
      CharCase = ecLowerCase
      ItemHeight = 13
      ParentShowHint = False
      ShowHint = True
      TabOrder = 0
    end
    object cmode: TComboBox
      Left = 112
      Top = 44
      Width = 89
      Height = 21
      Hint = 'Starts Quake with the specified console resuolution'
      CharCase = ecLowerCase
      ItemHeight = 13
      ParentShowHint = False
      ShowHint = True
      TabOrder = 1
    end
    object bpp: TComboBox
      Left = 144
      Top = 68
      Width = 57
      Height = 21
      Hint = 'Sets the pixel depth mode the video adapter should swich into'
      Style = csDropDownList
      ItemHeight = 13
      ParentShowHint = False
      ShowHint = True
      TabOrder = 2
      Items.Strings = (
        '16'
        '32')
    end
    object window: TCheckBox
      Left = 232
      Top = 32
      Width = 105
      Height = 17
      Hint = 'Starts Quake in window mode'
      Caption = 'run in window'
      ParentShowHint = False
      ShowHint = True
      TabOrder = 6
      OnClick = windowClick
    end
    object current: TCheckBox
      Left = 232
      Top = 80
      Width = 137
      Height = 17
      Hint = 'Forces Quake to run in current video mode'
      Caption = 'use current video mode'
      ParentShowHint = False
      ShowHint = True
      TabOrder = 9
      OnClick = currentClick
    end
    object dibonly: TCheckBox
      Left = 232
      Top = 16
      Width = 121
      Height = 17
      Hint = 'Forces Quake to not use any direct hardware access video modes'
      Caption = 'use only dib modes'
      ParentShowHint = False
      ShowHint = True
      TabOrder = 5
    end
    object nohwgamma: TCheckBox
      Left = 232
      Top = 96
      Width = 145
      Height = 17
      Hint = 'Disables hardware gamma control'
      Caption = 'disable hw gamma control'
      ParentShowHint = False
      ShowHint = True
      TabOrder = 10
    end
    object nomtex: TCheckBox
      Left = 232
      Top = 112
      Width = 121
      Height = 17
      Hint = 'Disables multitexturing'
      Caption = 'disable multitexturing'
      ParentShowHint = False
      ShowHint = True
      TabOrder = 11
      OnClick = nomtexClick
    end
    object freq: TComboBox
      Left = 144
      Top = 92
      Width = 57
      Height = 21
      Hint = 'Sets the display frequency the video adapter should swich into'
      ItemHeight = 13
      ParentShowHint = False
      ShowHint = True
      TabOrder = 3
    end
    object no24bit: TCheckBox
      Left = 232
      Top = 48
      Width = 129
      Height = 17
      Hint = 'Disables 24bit textures'
      Caption = 'disable 24bit textures'
      ParentShowHint = False
      ShowHint = True
      TabOrder = 7
    end
    object maxtmu2: TCheckBox
      Left = 232
      Top = 128
      Width = 97
      Height = 17
      Hint = 
        'Use this if you get serious fps drops or your textures look wron' +
        'g/deformed'
      Caption = 'limit TMU'#39's to 2'
      ParentShowHint = False
      ShowHint = True
      TabOrder = 12
      OnClick = maxtmu2Click
    end
    object particles: TEdit
      Left = 144
      Top = 116
      Width = 41
      Height = 21
      Hint = 
        'Specifies the maximum number of particles to be rendered on the ' +
        'screen at once'
      CharCase = ecLowerCase
      ParentShowHint = False
      ShowHint = True
      TabOrder = 4
      Text = '4096'
    end
    object UpDown4: TUpDown
      Left = 185
      Top = 116
      Width = 14
      Height = 21
      Associate = particles
      Min = 512
      Max = 32767
      Increment = 512
      Position = 4096
      TabOrder = 13
      Thousands = False
      Wrap = False
    end
    object forcetex: TCheckBox
      Left = 232
      Top = 64
      Width = 121
      Height = 17
      Hint = 'Forces Quake to not use any direct hardware access video modes'
      Caption = 'force textures reload'
      ParentShowHint = False
      ShowHint = True
      TabOrder = 8
    end
  end
  object ws: TGroupBox
    Left = 408
    Top = 160
    Width = 177
    Height = 153
    Caption = 'Sound settings'
    TabOrder = 6
    object Label4: TLabel
      Left = 16
      Top = 24
      Width = 79
      Height = 13
      Caption = 'Frequency (KHz)'
    end
    object wavonly: TCheckBox
      Left = 16
      Top = 80
      Width = 121
      Height = 17
      Hint = 'Disallows Quake to use DirectSound if available'
      Caption = 'disable DirectSound'
      ParentShowHint = False
      ShowHint = True
      TabOrder = 3
    end
    object skhz: TComboBox
      Left = 104
      Top = 20
      Width = 57
      Height = 21
      Hint = 'The frequency you want to interpolate the sound to'
      ItemHeight = 13
      ParentShowHint = False
      ShowHint = True
      TabOrder = 0
      Items.Strings = (
        '11'
        '22'
        '44'
        '48')
    end
    object cdaudio: TCheckBox
      Left = 16
      Top = 128
      Width = 105
      Height = 17
      Hint = 'Enables Quake'#39's ability to play CDs'
      Caption = 'enable CD audio'
      ParentShowHint = False
      ShowHint = True
      TabOrder = 6
    end
    object nosound: TCheckBox
      Left = 16
      Top = 48
      Width = 89
      Height = 17
      Hint = 
        'Disables initializing of the sound system and sound output in Qu' +
        'ake'
      Caption = 'disable sound'
      ParentShowHint = False
      ShowHint = True
      TabOrder = 1
      OnClick = nosoundClick
    end
    object primarysound: TCheckBox
      Left = 16
      Top = 112
      Width = 113
      Height = 17
      Hint = 
        'Performance tweek, can speed up sound a bit, but causes glitches' +
        ' with many sound systems'
      Caption = 'use primary sound'
      ParentShowHint = False
      ShowHint = True
      TabOrder = 5
    end
    object nomp3: TCheckBox
      Left = 16
      Top = 96
      Width = 153
      Height = 17
      Hint = 
        'Disables sound playback but simulates sound playback for code te' +
        'sting'
      Caption = 'disable MP3 volume control'
      ParentShowHint = False
      ShowHint = True
      TabOrder = 4
    end
    object snoforceformat: TCheckBox
      Left = 16
      Top = 64
      Width = 145
      Height = 17
      Hint = 
        'Causes Quake to not force the sound hardware into 11KHz, 16Bit m' +
        'ode'
      Caption = 'disable forcing to 11025Hz'
      ParentShowHint = False
      ShowHint = True
      TabOrder = 2
    end
  end
  object us: TGroupBox
    Left = 8
    Top = 316
    Width = 393
    Height = 103
    Caption = 'User settings'
    TabOrder = 7
    object Label9: TLabel
      Left = 16
      Top = 24
      Width = 36
      Height = 13
      Caption = 'Ruleset'
    end
    object Label12: TLabel
      Left = 16
      Top = 48
      Width = 30
      Height = 13
      Caption = 'Config'
    end
    object Label14: TLabel
      Left = 16
      Top = 72
      Width = 49
      Height = 13
      Caption = 'Other stuff'
    end
    object ruleset: TComboBox
      Left = 80
      Top = 20
      Width = 81
      Height = 21
      Hint = 
        'Disables triggers,truelighting etc and enables to play with up t' +
        'o 77 fps on kteam server'
      CharCase = ecLowerCase
      ItemHeight = 13
      ParentShowHint = False
      ShowHint = True
      Sorted = True
      TabOrder = 0
      Items.Strings = (
        'smackdown')
    end
    object clcfg: TComboBox
      Left = 80
      Top = 44
      Width = 81
      Height = 21
      Hint = 'User configuration file'
      CharCase = ecLowerCase
      ItemHeight = 13
      ParentShowHint = False
      ShowHint = True
      Sorted = True
      TabOrder = 1
    end
    object other: TEdit
      Left = 80
      Top = 68
      Width = 297
      Height = 21
      Hint = 'Other command line parameter can be typed here'
      ParentShowHint = False
      ShowHint = True
      TabOrder = 6
    end
    object nohwtimer: TCheckBox
      Left = 168
      Top = 32
      Width = 105
      Height = 17
      Hint = 'Turns off new precise timer and returns back old behaviour'
      Caption = 'disable hw timer'
      ParentShowHint = False
      ShowHint = True
      TabOrder = 3
    end
    object condebug: TCheckBox
      Left = 168
      Top = 16
      Width = 97
      Height = 17
      Hint = 
        'Enables logging of the console text in the "qconsole.log" file i' +
        'n the quake/qw/ directory'
      Caption = 'console logging'
      ParentShowHint = False
      ShowHint = True
      TabOrder = 2
    end
    object noscripts: TCheckBox
      Left = 168
      Top = 48
      Width = 97
      Height = 17
      Caption = 'don'#39't use scripts'
      ParentShowHint = False
      ShowHint = True
      TabOrder = 4
    end
    object indphys: TCheckBox
      Left = 272
      Top = 16
      Width = 118
      Height = 17
      Caption = 'independent physics'
      ParentShowHint = False
      ShowHint = True
      TabOrder = 5
    end
    object allowmulty: TCheckBox
      Left = 272
      Top = 32
      Width = 113
      Height = 17
      Hint = 'Turns off new precise timer and returns back old behaviour'
      Caption = 'multiple instances'
      ParentShowHint = False
      ShowHint = True
      TabOrder = 7
    end
  end
  object ss: TGroupBox
    Left = 8
    Top = 36
    Width = 209
    Height = 121
    Caption = 'Server settings'
    TabOrder = 2
    object Label6: TLabel
      Left = 16
      Top = 24
      Width = 71
      Height = 13
      Caption = 'Game directory'
    end
    object Label8: TLabel
      Left = 16
      Top = 72
      Width = 52
      Height = 13
      Caption = 'Server port'
    end
    object Label10: TLabel
      Left = 16
      Top = 48
      Width = 63
      Height = 13
      Caption = 'Server config'
    end
    object ded: TCheckBox
      Left = 16
      Top = 96
      Width = 105
      Height = 17
      Hint = 'Runs dedicated server'
      Caption = 'dedicated server'
      ParentShowHint = False
      ShowHint = True
      TabOrder = 3
      OnClick = dedClick
    end
    object gamedirs: TComboBox
      Left = 112
      Top = 20
      Width = 89
      Height = 21
      Hint = 'Sets t'#0'e direcory containing the game that the server uses'
      CharCase = ecLowerCase
      ItemHeight = 13
      ParentShowHint = False
      ShowHint = True
      Sorted = True
      TabOrder = 0
      OnChange = gamedirsChange
    end
    object srvcfg: TComboBox
      Left = 112
      Top = 44
      Width = 89
      Height = 21
      Hint = 'Server configuration file'
      CharCase = ecLowerCase
      ItemHeight = 13
      ParentShowHint = False
      ShowHint = True
      Sorted = True
      TabOrder = 1
    end
    object port: TComboBox
      Left = 112
      Top = 68
      Width = 89
      Height = 21
      Hint = 'Sets the TCP port number the server opens for client connections'
      CharCase = ecLowerCase
      ItemHeight = 13
      ParentShowHint = False
      ShowHint = True
      TabOrder = 2
      Items.Strings = (
        '27501'
        '27502'
        '27503'
        '27504'
        '27505'
        '27506'
        '27507'
        '27508'
        '27509')
    end
  end
  object start: TButton
    Left = 528
    Top = 350
    Width = 57
    Height = 25
    Hint = 'GL & HF !!!'
    Caption = 'Start'
    Default = True
    Font.Charset = DEFAULT_CHARSET
    Font.Color = clWindowText
    Font.Height = -11
    Font.Name = 'MS Sans Serif'
    Font.Style = [fsBold]
    ParentFont = False
    ParentShowHint = False
    ShowHint = True
    TabOrder = 13
    OnClick = startClick
  end
  object cancel: TButton
    Left = 472
    Top = 350
    Width = 49
    Height = 25
    Hint = 'It'#39's too hard for me, i would rather play CS'
    Cancel = True
    Caption = 'Cancel'
    ParentShowHint = False
    ShowHint = True
    TabOrder = 12
    OnClick = cancelClick
  end
  object lang: TRadioGroup
    Left = 224
    Top = 0
    Width = 177
    Height = 33
    Caption = 'Help hints'
    Columns = 2
    ItemIndex = 0
    Items.Strings = (
      'Disable'
      'Enable')
    TabOrder = 1
    OnClick = langClick
  end
  object ver: TRadioGroup
    Left = 8
    Top = 0
    Width = 209
    Height = 33
    Caption = 'ezQuake version'
    Columns = 2
    Font.Charset = DEFAULT_CHARSET
    Font.Color = clWindowText
    Font.Height = -11
    Font.Name = 'MS Sans Serif'
    Font.Style = [fsBold]
    ParentFont = False
    TabOrder = 0
    OnClick = verClick
  end
  object ms: TGroupBox
    Left = 224
    Top = 36
    Width = 177
    Height = 121
    Caption = 'Main settings'
    TabOrder = 3
    object Label5: TLabel
      Left = 16
      Top = 48
      Width = 85
      Height = 13
      Caption = 'Demo cache (Mb)'
    end
    object Label7: TLabel
      Left = 16
      Top = 24
      Width = 89
      Height = 13
      Caption = 'Heap memory (Mb)'
    end
    object Label15: TLabel
      Left = 16
      Top = 72
      Width = 83
      Height = 13
      Caption = 'Alias memory (Kb)'
    end
    object Label17: TLabel
      Left = 16
      Top = 96
      Width = 90
      Height = 13
      Caption = 'Console buffer (Kb)'
    end
    object mem: TEdit
      Left = 120
      Top = 20
      Width = 25
      Height = 21
      Hint = 
        'Specif'#0'es the ammount of memory in megabytes that Quake should a' +
        'llocate'
      CharCase = ecLowerCase
      MaxLength = 3
      ParentShowHint = False
      ShowHint = True
      TabOrder = 0
      Text = '16'
    end
    object democache: TEdit
      Left = 120
      Top = 44
      Width = 25
      Height = 21
      Hint = 
        'Specifies the ammount of memory in megabytes for demo recording ' +
        'cache'
      CharCase = ecLowerCase
      MaxLength = 3
      ParentShowHint = False
      ShowHint = True
      TabOrder = 1
      Text = '0'
    end
    object UpDown2: TUpDown
      Left = 145
      Top = 44
      Width = 14
      Height = 21
      Associate = democache
      Min = 0
      Max = 512
      Position = 0
      TabOrder = 3
      Thousands = False
      Wrap = False
    end
    object UpDown1: TUpDown
      Left = 145
      Top = 20
      Width = 14
      Height = 21
      Associate = mem
      Min = 8
      Max = 512
      Position = 16
      TabOrder = 4
      Thousands = False
      Wrap = False
    end
    object zone: TEdit
      Left = 112
      Top = 68
      Width = 33
      Height = 21
      Hint = 'Allocates additional memory to the alias memory space'
      CharCase = ecLowerCase
      MaxLength = 4
      ParentShowHint = False
      ShowHint = True
      TabOrder = 2
      Text = '128'
    end
    object UpDown3: TUpDown
      Left = 145
      Top = 68
      Width = 14
      Height = 21
      Associate = zone
      Min = 32
      Max = 8192
      Increment = 32
      Position = 128
      TabOrder = 5
      Thousands = False
      Wrap = False
    end
    object conbuf: TEdit
      Left = 112
      Top = 92
      Width = 33
      Height = 21
      Hint = 'Allocates additional memory to the alias memory space'
      CharCase = ecLowerCase
      MaxLength = 4
      ParentShowHint = False
      ShowHint = True
      TabOrder = 6
      Text = '64'
    end
    object UpDown5: TUpDown
      Left = 145
      Top = 92
      Width = 14
      Height = 21
      Associate = conbuf
      Min = 64
      Max = 4096
      Increment = 32
      Position = 64
      TabOrder = 7
      Thousands = False
      Wrap = False
    end
  end
  object is: TGroupBox
    Left = 408
    Top = 0
    Width = 177
    Height = 157
    Caption = 'Mouse settings'
    TabOrder = 4
    object dinput: TCheckBox
      Left = 16
      Top = 52
      Width = 97
      Height = 17
      Hint = 'Uses DirectInput for mouse'
      Caption = 'use DirectInput'
      ParentShowHint = False
      ShowHint = True
      TabOrder = 1
      OnClick = dinputClick
    end
    object m_smooth: TCheckBox
      Left = 16
      Top = 68
      Width = 105
      Height = 17
      Hint = 
        'Smoothes your mouse movements and maximizes mouse responsiveness' +
        ' (don'#39't forget to set m_rate to your real mouse rate)'
      Caption = 'mouse smoothing'
      ParentShowHint = False
      ShowHint = True
      TabOrder = 2
      OnClick = m_smoothClick
    end
    object mw_hook: TCheckBox
      Left = 16
      Top = 84
      Width = 137
      Height = 17
      Hint = 'Supports binding all buttons on Logitech mice'
      Caption = 'Logitech mouse support'
      ParentShowHint = False
      ShowHint = True
      TabOrder = 3
    end
    object noacc: TCheckBox
      Left = 16
      Top = 116
      Width = 153
      Height = 17
      Hint = 'Disables the forcing of mouse acceleration on startup'
      Caption = 'disable acceleration forcing'
      ParentShowHint = False
      ShowHint = True
      TabOrder = 5
      OnClick = noaccClick
    end
    object nospd: TCheckBox
      Left = 16
      Top = 132
      Width = 145
      Height = 17
      Hint = 'Disables the forcing of mouse speed on startup'
      Caption = 'disable speed forcing'
      ParentShowHint = False
      ShowHint = True
      TabOrder = 6
      OnClick = nospdClick
    end
    object nopar: TCheckBox
      Left = 16
      Top = 100
      Width = 153
      Height = 17
      Hint = 'Disables the forcing of mouse parameters on startup'
      Caption = 'disable parameters forcing'
      ParentShowHint = False
      ShowHint = True
      TabOrder = 4
      OnClick = noparClick
    end
    object nomouse: TCheckBox
      Left = 16
      Top = 36
      Width = 137
      Height = 17
      Hint = 'Disables mouse support'
      Caption = 'disable mouse support'
      ParentShowHint = False
      ShowHint = True
      TabOrder = 0
      OnClick = nomouseClick
    end
  end
  object fbat: TComboBox
    Left = 408
    Top = 320
    Width = 97
    Height = 21
    Hint = 'Bat file to load/save'
    ItemHeight = 13
    ParentShowHint = False
    ShowHint = True
    Sorted = True
    TabOrder = 8
    OnChange = fbatChange
  end
  object load: TButton
    Left = 512
    Top = 320
    Width = 33
    Height = 21
    Hint = 'Loads settings from bat file for editing'
    Caption = 'Load'
    Enabled = False
    ParentShowHint = False
    ShowHint = True
    TabOrder = 9
    OnClick = loadClick
  end
  object save: TButton
    Left = 552
    Top = 320
    Width = 33
    Height = 21
    Hint = 'Saves current settings to bat file'
    Caption = 'Save'
    Enabled = False
    ParentShowHint = False
    ShowHint = True
    TabOrder = 10
    OnClick = saveClick
  end
  object clearb: TButton
    Left = 408
    Top = 350
    Width = 57
    Height = 25
    Hint = 'Sets all parameters to defaults'
    Caption = 'Clear'
    ParentShowHint = False
    ShowHint = True
    TabOrder = 11
    OnClick = clearbClick
  end
  object crlnk: TButton
    Left = 408
    Top = 384
    Width = 65
    Height = 25
    Hint = 'Creates link at given place'
    Caption = 'Create link'
    Enabled = False
    ParentShowHint = False
    ShowHint = True
    TabOrder = 14
    OnClick = crlnkClick
  end
  object lnkpath: TComboBox
    Left = 480
    Top = 386
    Width = 105
    Height = 21
    Style = csDropDownList
    ItemHeight = 13
    ParentShowHint = False
    ShowHint = True
    TabOrder = 15
    OnChange = lnkpathChange
    Items.Strings = (
      'at Desktop'
      'at Quick launch'
      'at Start menu')
  end
end
