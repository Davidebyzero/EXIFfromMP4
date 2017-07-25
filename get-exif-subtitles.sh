#!/usr/bin/env bash
# Usage: get-exif-subtitles mp4_filename srt_filename
#        ffprobe must be in your PATH.
# Example: get-exif-subtitles P1231001.MP4 P1231001.srt
#
# This implementation uses "-show_entries packet=..." even though this shows the packets in decoding order, which is different
# than presentation order for IPB videos, because "-show_entries frame=..." is much slower as currently implemented in ffmpeg.
# It is left to EXIFfromMP4 to properly order the packets.
# Note, ffprobe is invoked twice in a row, because if "-show_entries stream=..." and "-show_entries packet=..." are combined in
# a single invokation, it shows them in the wrong order, always packet data first and stream data afterward. But EXIFfromMP4
# requires the stream data first to properly interpret the packet data.
#
(ffprobe -hide_banner -i "$1" -select_streams v:0 -of csv -show_entries stream=time_base&&ffprobe -hide_banner -i "$1" -select_streams v:0 -of csv -show_entries packet=pts,pos) 2>/dev/null|"$(dirname $0)/EXIFfromMP4" -srt "$1" "$2"
