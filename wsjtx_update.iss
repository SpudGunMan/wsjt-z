[Setup]
AppName=wsjtz
AppVerName=wsjtz Version 1.2 r3537
AppCopyright=Copyright (C) 2001-2013 by Joe Taylor, K1JT
DefaultDirName=c:\wsjtz1.2
DefaultGroupName=wsjtz1.2

[Files]
Source: "c:\Users\joe\wsjt\wsjtz_install\wsjtz.exe";                     DestDir: "{app}"
Source: "c:\Users\joe\wsjt\wsjtx_install\jt9.exe";                       DestDir: "{app}"
Source: "c:\Users\joe\wsjt\wsjtx\shortcuts.txt";                         DestDir: "{app}"
Source: "c:\Users\joe\wsjt\wsjtx\mouse_commands.txt";                    DestDir: "{app}"
Source: "c:\Users\joe\wsjt\wsjtx\WSJT-X_Users_Guide_v1.2.pdf";           DestDir: "{app}"

[Icons]
Name: "{group}\wsjtz1.2";        Filename: "{app}\wsjtz.exe";   WorkingDir: {app}; IconFilename: {app}\wsjt.ico
Name: "{userdesktop}\wsjtz1.2";  Filename: "{app}\wsjtz.exe";   WorkingDir: {app}; IconFilename: {app}\wsjt.ico

