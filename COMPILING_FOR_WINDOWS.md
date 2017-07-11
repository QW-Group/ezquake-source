## Compiling for Windows

### Using Ubuntu Bash

You can use the new Ubuntu Bash feature in Windows 10 to compile ezQuake for Windows.

To enable Bash for Windows, press the `Start` button and type `Turn Windows f` and select `Turn Windows features on or off`. Scroll down to `Windows Subsystem for Linux (Beta)` and enable it.

Now press WINDOWS+I, go to `Update & security` and then to the `For developers` tab. Enable `Developer mode`.

Now press the `Start` button again and enter `bash`. Click it and install Bash.

Enter the following command to install all required prerequisites to build ezQuake:

```
sudo apt-get install -y git mingw-w64 build-essential
```

Now clone the ezQuake source code:

```
git clone https://github.com/ezQuake/ezquake-source.git ezquake
```

Now build the ezQuake executable:

```
EZ_CONFIG_FILE=.config_windows make
```

### Using a Linux system

1) Make sure you have mingw32 toolchain installed. On Arch Linux it's mingw-w64 (select complete group)

2) Run "EZ_CONFIG_FILE=.config_windows make" (you can add -j5 to make to build in parallell. Use nbr of cpu cores +1)
   without the quotes.

3) You should get an ezquake.exe :-)

