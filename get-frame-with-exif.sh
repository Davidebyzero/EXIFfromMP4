#!/usr/bin/env bash
# Usage: get-frame-with-exif mp4_filename timestamp image_filename
#        "timestamp" must be rounded down to be properly recognized by ffmpeg.
#        Rounded-up timestamps will result in seeking to the next frame.
#        ffmpeg and exiftool must be in your PATH.
# Examples:
#        get-frame-with-exif P1231001.MP4 4.904 framegrab.png
#        get-frame-with-exif "P1153210 - interesting video.MP4" 0:20.253 "interesting frame.jpg"
#
ffmpeg -seek_timestamp 1 -ss "$2" -i "$1" -ss 0 -frames:v 1 -nostats -vf showinfo -pix_fmt rgb48le -qmin 1 -qmax 1 "$3" 2>&1 | "$(dirname $0)/EXIFfromMP4" -frame "$1" "$2" "$3"
