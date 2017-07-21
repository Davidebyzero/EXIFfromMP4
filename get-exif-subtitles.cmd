@REM Usage: get-exif mp4_filename srt_filename
@REM        ffprobe.exe must be in your PATH.
@REM This implementation uses "-show_entries packet=..." even though this shows the packets in decoding order, which is different than presentation order for IPB videos, because "-show_entries frame=..." is much slower as currently implemented in ffmpeg.
@(ffprobe.exe -hide_banner -i %1 -select_streams v:0 -of csv -show_entries stream=time_base&&ffprobe.exe -hide_banner -i %1 -select_streams v:0 -of csv -show_entries packet=pts,pos) 2>NUL|"%~dp0\EXIFfromMP4.exe" -srt %1 %2
