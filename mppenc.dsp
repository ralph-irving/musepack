# Microsoft Developer Studio Project File - Name="mppenc" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=mppenc - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "mppenc.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "mppenc.mak" CFG="mppenc - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "mppenc - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "mppenc - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "mppenc - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /Gr /Zp4 /W3 /O2 /Ob2 /D "NDEBUG" /D "WIN32" /D "_CONSOLE" /D "_MBCS" /D "MPP_ENCODER" /FD /GM /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x407 /d "NDEBUG"
# ADD RSC /l 0x407 /d "NDEBUG MPP_ENCODER"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib setargv.obj winmm.lib /nologo /subsystem:console /machine:I386

!ELSEIF  "$(CFG)" == "mppenc - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /G5 /W3 /GX /ZI /Od /D "_DEBUG" /D "WIN32" /D "_CONSOLE" /D "_MBCS" /D "MPP_ENCODER" /YX /FD /c
# ADD BASE RSC /l 0x407 /d "_DEBUG"
# ADD RSC /l 0x407 /d "_DEBUG MPP_ENCODER"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib setargv.obj winmm.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept

!ENDIF 

# Begin Target

# Name "mppenc - Win32 Release"
# Name "mppenc - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\ans.c
# End Source File
# Begin Source File

SOURCE=.\bitstream.c
# End Source File
# Begin Source File

SOURCE=.\codetable.c
# End Source File
# Begin Source File

SOURCE=.\codetable_data.c
# End Source File
# Begin Source File

SOURCE=.\codetable_enc.c
# End Source File
# Begin Source File

SOURCE=.\codetablemake.c
# End Source File
# Begin Source File

SOURCE=.\cvd.c
# End Source File
# Begin Source File

SOURCE=.\directory.c
# End Source File
# Begin Source File

SOURCE=.\encode_sv7.c
# End Source File
# Begin Source File

SOURCE=.\fastmath.c
# End Source File
# Begin Source File

SOURCE=.\fft4g.c
# End Source File
# Begin Source File

SOURCE=.\fft4gasm.nas

!IF  "$(CFG)" == "mppenc - Win32 Release"

# Begin Custom Build
InputPath=.\fft4gasm.nas
InputName=fft4gasm

"Release/$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	"C:/PROGRAM FILES/NASM/NASMW" -d WIN32 -f win32 -o Release/$(InputName).obj $(InputPath) -l $(InputName).lst

# End Custom Build

!ELSEIF  "$(CFG)" == "mppenc - Win32 Debug"

# Begin Custom Build
InputPath=.\fft4gasm.nas
InputName=fft4gasm

"Debug/$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	"C:/PROGRAM FILES/NASM/NASMW" -d WIN32 -f win32 -o Debug/$(InputName).obj $(InputPath)

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\fft_routines.c
# End Source File
# Begin Source File

SOURCE=.\fpu.c
# End Source File
# Begin Source File

SOURCE=.\keyboard.c
# End Source File
# Begin Source File

SOURCE=.\mppenc.c
# End Source File
# Begin Source File

SOURCE=.\pipeopen.c
# End Source File
# Begin Source File

SOURCE=.\priority.c
# End Source File
# Begin Source File

SOURCE=.\psy.c
# End Source File
# Begin Source File

SOURCE=.\psy_inc.h
# End Source File
# Begin Source File

SOURCE=.\psy_tab.c
# End Source File
# Begin Source File

SOURCE=.\quant.c
# End Source File
# Begin Source File

SOURCE=.\quantnew.c
# End Source File
# Begin Source File

SOURCE=.\stderr.c
# End Source File
# Begin Source File

SOURCE=.\subband.c
# End Source File
# Begin Source File

SOURCE=.\tags.c
# End Source File
# Begin Source File

SOURCE=.\wave_in.c
# End Source File
# Begin Source File

SOURCE=.\winmsg.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\codetable.h
# End Source File
# Begin Source File

SOURCE=.\codetable_1.h
# End Source File
# Begin Source File

SOURCE=.\codetable_2.h
# End Source File
# Begin Source File

SOURCE=.\codetable_data.h
# End Source File
# Begin Source File

SOURCE=.\codetable_codes.h
# End Source File
# Begin Source File

SOURCE=.\fastmath.h
# End Source File
# Begin Source File

SOURCE=.\Makefile
# End Source File
# Begin Source File

SOURCE=.\minimax.h
# End Source File
# Begin Source File

SOURCE=.\mppenc.h
# End Source File
# Begin Source File

SOURCE=.\mppenc_fade.h
# End Source File
# Begin Source File

SOURCE=.\mppenc_help.h
# End Source File
# Begin Source File

SOURCE=.\mppenc_qual.h
# End Source File
# Begin Source File

SOURCE=.\mppenc_show.h
# End Source File
# Begin Source File

SOURCE=.\predict.h
# End Source File
# Begin Source File

SOURCE=.\version
# End Source File
# End Group
# Begin Group "Design Proposal"

# PROP Default_Filter ""
# Begin Source File

SOURCE=".\A-Block.txt"
# End Source File
# Begin Source File

SOURCE=".\A-codepage.txt"
# End Source File
# Begin Source File

SOURCE=".\A-freq.txt"
# End Source File
# Begin Source File

SOURCE=".\A-LPC.txt"
# End Source File
# Begin Source File

SOURCE=".\A-multichannel.txt"
# End Source File
# Begin Source File

SOURCE=".\A-Notizen.txt"
# End Source File
# Begin Source File

SOURCE=".\A-OldNew.txt"
# End Source File
# Begin Source File

SOURCE=".\A-quant.txt"
# End Source File
# Begin Source File

SOURCE=".\APEtag-FAQ.txt"
# End Source File
# Begin Source File

SOURCE=.\todotodotodo
# End Source File
# End Group
# End Target
# End Project
