#!/usr/bin/env bash
if [ $# -ne 3 ]
then
	echo "Usage: $0 mp4_filename timestamp image_filename"
	echo "       \"mp4_filename\" should be the filename of a 4K/6K Photo video created by a Panasonic Lumix DC-GH5."
	echo "       \"timestamp\" must be rounded down to be properly recognized by ffmpeg."
	echo "       Rounded-up timestamps will result in seeking to the next frame, so to be safe, subtract 0.001."
	echo "       \"image_filename\" is the output filename of the image you want, with extension .jpg, .png, or .tiff."
	echo "       ffmpeg and exiftool must be in your PATH."
	echo "Examples:"
	echo "       $0 P1231001.MP4 4.904 framegrab.png"
	echo "       $0 \"P1153210 - interesting video.MP4\" 0:20.253 \"interesting frame.jpg\""
	echo "       $0 \"~/videos/P1271414 - very long video.MP4\" 1:07:18.250 \"~/video frames/needle in a haystack.tiff\""
else
	ffmpeg -seek_timestamp 1 -ss "$2" -i "$1" -ss 0 -frames:v 1 -nostats -vf showinfo -pix_fmt rgb48le -qmin 1 -qmax 1 "$3" 2>&1 | "$(dirname $0)/EXIFfromMP4" -frame "$1" "$2" "$3"
fi
