@REM Usage: get-exif-at-frame mp4_filename timestamp image_filename
@REM        "timestamp" must be rounded down to be properly recognized by ffmpeg.
@REM        Rounded-up timestamps will result in seeking to the next frame.
@REM        ffmpeg.exe and exiftool.exe must be in your PATH.
@REM Examples:
@REM        get-exif-at-frame P1231001.MP4 4.904 framegrab.png
@REM        get-exif-at-frame "P1153210 - interesting video.MP4" 0:20.253 "interesting frame.jpg"
@REM
@ffmpeg.exe -seek_timestamp 1 -ss %2 -i %1 -ss 0 -frames:v 1 -nostats -vf showinfo -pix_fmt rgb48le -qmin 1 -qmax 1 %3 2>&1 | "%~dp0\EXIFfromMP4.exe" -frame %1 %2 %3
