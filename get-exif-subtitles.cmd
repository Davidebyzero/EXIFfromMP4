@echo off

set argc=0
for %%i in (%*) do set /a argc+=1
if %argc%==2 goto good
echo Usage: %0 mp4_filename srt_filename
echo        where "mp4_filename" is a 4K/6K Photo video from the Panasonic Lumix DC-GH5, and "srt_filename" is the subtitle
echo        file to create (which in most cases should have the same base filename as the video file).
echo        ffprobe.exe must be in your PATH.
echo Examples:
echo        %0 P1231001.MP4 P1231001.srt
echo        %0 "P1153210 - interesting video.MP4" "P1153210 - interesting video.srt"
echo        %0 "D:\videos\P1153210 - interesting video.MP4" "D:\videos\P1153210 - interesting video.srt"
goto:eof

REM This implementation uses "-show_entries packet=..." even though this shows the packets in decoding order, which is different
REM than presentation order for IPB videos, because "-show_entries frame=..." is much slower as currently implemented in ffmpeg.
REM It is left to EXIFfromMP4 to properly order the packets.
REM Note, ffprobe is invoked twice in a row, because if "-show_entries stream=..." and "-show_entries packet=..." are combined in
REM a single invokation, it shows them in the wrong order, always packet data first and stream data afterward. But EXIFfromMP4
REM requires the stream data first to properly interpret the packet data.
:good
(ffprobe.exe -hide_banner -i %1 -select_streams v:0 -of csv -show_entries stream=time_base&&ffprobe.exe -hide_banner -i %1 -select_streams v:0 -of csv -show_entries packet=pts,pos) 2>NUL|"%~dp0\EXIFfromMP4.exe" -srt %1 %2
