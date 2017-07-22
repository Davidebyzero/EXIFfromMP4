@REM Usage: get-exif mp4_filename srt_filename
@REM        ffprobe.exe must be in your PATH.
@REM This implementation uses "-show_entries packet=..." even though this shows the packets in decoding order, which is different
@REM than presentation order for IPB videos, because "-show_entries frame=..." is much slower as currently implemented in ffmpeg.
@REM It is left to EXIFfromMP4 to properly order the packets.
@REM Note, ffprobe is invoked twice in a row, because if "-show_entries stream=..." and "-show_entries packet=..." are combined in
@REM a single invokation, it shows them in the wrong order, always packet data first and stream data afterward. But EXIFfromMP4
@REM requires the stream data first to properly interpret the packet data.
@(ffprobe.exe -hide_banner -i %1 -select_streams v:0 -of csv -show_entries stream=time_base&&ffprobe.exe -hide_banner -i %1 -select_streams v:0 -of csv -show_entries packet=pts,pos) 2>NUL|"%~dp0\EXIFfromMP4.exe" -srt %1 %2
