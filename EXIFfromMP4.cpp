/*
 *  Copyright 2017 David Ellsworth <davide.by.zero@gmail.com>
 *
 *  EXIFfromMP4 is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  EXIFfromMP4 is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with EXIFfromMP4.  If not, see <http://www.gnu.org/licenses/>.
 */

#define REQUIRE_EXIFTOOL_9_67_OR_LATER

#if defined(_WIN32) || defined(_WIN64)
	#ifndef _WIN32_WINNT		// Allow use of features specific to Windows XP or later.
	#define _WIN32_WINNT 0x0501	// Change this to the appropriate value to target other versions of Windows.
	#endif

	#include <tchar.h>

	#define _TCAT(a,b) (_T(a) _T(b))
#else
	#include <unistd.h>

	#define _T
	#define _TCAT(a,b) (a b)
	#define _TCHAR char
	#define TCHAR char
	#define _tmain main
	#define _tcscpy strcpy
	#define _tcscmp strcmp
	#define _tfopen fopen
	#define _tspawnlp spawnlp
	#define _tspawnvp spawnvp
	#define _ftprintf fprintf
	#define _stprintf sprintf

	#define _write write
	#define _close close
	#define _pipe pipe
	#define _dup dup
	#define _dup2 dup2
	#define _O_BINARY O_BINARY

	#define _fseeki64 fseek
	#define _ftelli64 ftell
	#define __int64 long long

	#ifndef _countof
	#define _countof(array) (sizeof(array)/sizeof(array[0]))
	#endif
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <io.h>
#include <fcntl.h>
#include <process.h>

#define STDIN_FILENO 0

typedef unsigned char     BYTE;
typedef unsigned short    WORD;
typedef unsigned         DWORD;
typedef unsigned long long QWORD;
typedef unsigned long long Uint64;
typedef unsigned int Uint;
typedef unsigned char Uchar;
typedef DWORD FOURCC;

#define WORD_ENDIAN(a) (WORD)\
(\
    ((((WORD)(a)>>(8*0))&0xFF)<<(8*1))+\
    ((((WORD)(a)>>(8*1))&0xFF)<<(8*0)) \
)

#define DWORD_ENDIAN(a) (DWORD)\
(\
    ((((DWORD)(a)>>(8*0))&0xFF)<<(8*3))+\
    ((((DWORD)(a)>>(8*1))&0xFF)<<(8*2))+\
    ((((DWORD)(a)>>(8*2))&0xFF)<<(8*1))+\
    ((((DWORD)(a)>>(8*3))&0xFF)<<(8*0)) \
)
#define FOURCC_ENDIAN(a) (DWORD_ENDIAN(a))

#define QWORD_ENDIAN(a) (QWORD)\
(\
    ((((QWORD)(a)>>(8*0))&0xFF)<<(8*7))+\
    ((((QWORD)(a)>>(8*1))&0xFF)<<(8*6))+\
    ((((QWORD)(a)>>(8*2))&0xFF)<<(8*5))+\
    ((((QWORD)(a)>>(8*3))&0xFF)<<(8*4))+\
    ((((QWORD)(a)>>(8*4))&0xFF)<<(8*3))+\
    ((((QWORD)(a)>>(8*5))&0xFF)<<(8*2))+\
    ((((QWORD)(a)>>(8*6))&0xFF)<<(8*1))+\
    ((((QWORD)(a)>>(8*7))&0xFF)<<(8*0)) \
)

template <size_t size>
char (*__strlength_helper(char const (&_String)[size]))[size];
#define strlength(_String) (sizeof(*__strlength_helper(_String))-1)

//#define DEBUG_PER_FRAME_EXIF

#ifdef DEBUG_PER_FRAME_EXIF
BYTE *getEXIF(FILE *f, Uint64 pos, Uint i)
#else
BYTE *getEXIF(FILE *f, Uint64 pos)
#endif
{
	_fseeki64(f, pos, SEEK_SET);
	const size_t bufsize = 275; // 275 is needed for 6K Photo, but only 206 is needed for 4K Photo
	static BYTE buf[bufsize];
	fread(buf, sizeof(buf), 1, f);
	static BYTE buf_out[bufsize];
	Uint start = 0;
	Uint length = start + sizeof(buf_out);
	Uint length_out = 0;
	for (; start < length;)
	{
		if (start + 3 <= length && buf[start]==0 && buf[start+1]==0 && buf[start+2]==3) // emulation prevention byte
		{
			buf_out[length_out++] = 0;
			buf_out[length_out++] = 0;
			start += 3;
		}
		else
			buf_out[length_out++] = buf[start++];
	}

#ifdef DEBUG_PER_FRAME_EXIF
	char filename[1000];
	sprintf(filename, "exif.%06u.bin", i);
	FILE *fout = fopen(filename, "wb");
	fwrite(buf_out, bufsize, 1, fout);
	fclose(fout);
#endif

	// TODO: Parse the byte stream format, instead of making assumptions on the specifics of its layout as used by the Panasonic Lumix DC-GH5

	Uint skip;
	// buf_out[5] meaning: 0x10=I-frame, 0x30=P-frame, 0x50=B-frame
	bool is_I_frame = buf_out[5]==0x10;
	if ((DWORD&)buf_out[is_I_frame ? 0x4D : 0x2D] == DWORD_ENDIAN('USR2'))
		skip = 0x3A + (is_I_frame ? 0x20 : 0); // 6K Photo (HEVC)
	else
		skip = 0x15 + (is_I_frame ? 0x11 : 0); // 4K Photo (H264)
	return buf_out + skip;
}

void EXIFtoSubtitleOnFrame(FILE *f0, FILE *f1, bool fps60, Uint64 pos, Uint i)
{
#ifdef DEBUG_PER_FRAME_EXIF
	BYTE *exif = getEXIF(f0, pos, i);
#else
	BYTE *exif = getEXIF(f0, pos);
#endif
	Uint ms0, ms1;
	if (fps60)
	{
		ms0 = (Uint)(((Uint64)(i  ) * 1001 + 60/2) / 60);
		ms1 = (Uint)(((Uint64)(i+1) * 1001 + 60/2) / 60);
	}
	else
	{
		ms0 = (Uint)(((Uint64)(i  ) * 1001 + 30/2) / 30);
		ms1 = (Uint)(((Uint64)(i+1) * 1001 + 30/2) / 30);
	}

	if ((DWORD&)exif[0x19] != DWORD_ENDIAN('USR1'))
		return;

	fprintf(f1,
		"%u\n"
		"%02u:%02u:%02u,%03u --> %02u:%02u:%02u,%03u\n",
		i+1,
		ms0/(60*60*1000), ms0/(60*1000)%60, ms0/1000%60, ms0%1000,
		ms1/(60*60*1000), ms1/(60*1000)%60, ms1/1000%60, ms1%1000);

	fputc(exif[0x20]/0x10 + '0', f1);
	fputc(exif[0x20]%0x10 + '0', f1);
	fputc(exif[0x21]/0x10 + '0', f1);
	fputc(exif[0x21]%0x10 + '0', f1);
	fputc('-', f1);
	fputc(exif[0x22]/0x10 + '0', f1);
	fputc(exif[0x22]%0x10 + '0', f1);
	fputc('-', f1);
	fputc(exif[0x24]/0x10 + '0', f1);
	fputc(exif[0x24]%0x10 + '0', f1);
	fputc(' ', f1);
	fputc(exif[0x25]/0x10 + '0', f1);
	fputc(exif[0x25]%0x10 + '0', f1);
	fputc(':', f1);
	fputc(exif[0x26]/0x10 + '0', f1);
	fputc(exif[0x26]%0x10 + '0', f1);
	fputc(':', f1);
	fputc(exif[0x27]/0x10 + '0', f1);
	fputc(exif[0x27]%0x10 + '0', f1);
	fputc('.', f1);
	fputc(exif[0x94]      + '0', f1);
	fputc(exif[0x95]/0x10 + '0', f1);
	fputc(exif[0x95]%0x10 + '0', f1);
	fputc(' ', f1);
	fputc(' ', f1);

	if (WORD_ENDIAN((WORD&)exif[0x35])==10)
		fprintf(f1, "f/%u.%u  ", WORD_ENDIAN((WORD&)exif[0x33])/10, WORD_ENDIAN((WORD&)exif[0x33])%10);
	else
		fprintf(f1, "f/?  ");

	if (WORD_ENDIAN((WORD&)exif[0x2E])==10)
	{
		fprintf(f1, "1/%u", WORD_ENDIAN((WORD&)exif[0x30])/10);
		if (Uint frac = WORD_ENDIAN((WORD&)exif[0x30])%10)
			fprintf(f1, ".%u", frac);
	}
	else
	if (WORD_ENDIAN((WORD&)exif[0x30])==10)
	{
		fprintf(f1, "%u", WORD_ENDIAN((WORD&)exif[0x2E])/10);
		if (Uint frac = WORD_ENDIAN((WORD&)exif[0x2E])%10)
			fprintf(f1, ".%u", frac);
	}
	else
		fprintf(f1, "?");
	fprintf(f1, " s  ISO %u  ", WORD_ENDIAN((WORD&)exif[0x76]));

	if (WORD_ENDIAN((WORD&)exif[0x62])==10)
		fprintf(f1, "%u.%u", WORD_ENDIAN((WORD&)exif[0x60])/10, WORD_ENDIAN((WORD&)exif[0x60])%10);
	else
		fprintf(f1, "?");
	fprintf(f1, " mm\n\n");
}

int EXIFtoSubtitles(FILE *f0, FILE *f1)
{
	char line[100];

	Uint64 timebase;
	if (!fgets(line, sizeof(line), stdin))
		return -1;
	if (strncmp(line, "stream,1/", strlength("stream,1/")) != 0)
		return -1;
	sscanf(line + strlength("stream,1/"), "%llu", &timebase);

	// use a 60fps flag so that divisors can be constant, allowing compiler optimization
	bool fps60;
	if (timebase == 60 * 3000)
		fps60 = true;
	else
	if (timebase == 30 * 3000)
		fps60 = false;
	else
	{
		fputs("expected value of time_base is 90000 or 180000\n", stderr);
		return -1;
	}

	bool consecutive_pts;
	Uint64 pts_next_expected = 0;
	Uint64 pos_buffer[2];
	Uint pos_buffer_i = 0;
	for (Uint i=0;;)
	{
		if (!fgets(line, sizeof(line), stdin))
		{
			if (pos_buffer_i != 0)
			{
				fputs("Error: End of IPB video occurs at a frame number not divisible by 3\n", stderr);
				return -1;
			}
			break;
		}
		if (strncmp(line, "packet,", strlength("packet,")) != 0)
		{
			fputs("Error: Unexpected line from stdin\n", stderr);
			return -1;
		}
		Uint64 pts, pos;
		sscanf(line + strlength("packet,"), "%llu,%llu", &pts, &pos);
		
		if (pts_next_expected == 0 && pos_buffer_i == 0)
			consecutive_pts = pts == 0;
		if (consecutive_pts)
		{
			if (pts != pts_next_expected)
			{
				fputs("pts departs from expected order and/or duration of 3003\n", stderr);
				return -1;
			}
			EXIFtoSubtitleOnFrame(f0, f1, fps60, pos, i++);
			pts_next_expected = pts + 3003;
		}
		else
		{
			if (pts != pts_next_expected + 3003 * (pos_buffer_i==0 ? 2 : pos_buffer_i-1))
			{
				fputs("pts departs from expected order and/or duration of 3003\n", stderr);
				return -1;
			}
			if (pos_buffer_i == 2)
			{
				EXIFtoSubtitleOnFrame(f0, f1, fps60, pos_buffer[1], i);
				EXIFtoSubtitleOnFrame(f0, f1, fps60, pos,           i+1);
				EXIFtoSubtitleOnFrame(f0, f1, fps60, pos_buffer[0], i+2);
				i += 3;
				pos_buffer_i = 0;
				pts_next_expected += 3003 * 3;
			}
			else
				pos_buffer[pos_buffer_i++] = pos;
		}
	}
	return 0;
}

#ifndef REQUIRE_EXIFTOOL_9_67_OR_LATER
BYTE *GetFirstFrameThumbnailWithEXIFfromMP4(FILE *f, Uint &bufferLength)
// pointer returned by this function is either NULL (indictating error) or allocated with malloc() with free() needing to be done on it later
{
	struct AtomHeader
	{
		DWORD length;
		union
		{
			FOURCC name;
			char nameStr[4];
		};
	};
	fseek(f, 0, SEEK_END);
	__int64 fileLength = _ftelli64(f);
	fseek(f, 0, SEEK_SET);
	for (;;)
	{
		AtomHeader atomHeader;
		if (fread(&atomHeader, sizeof(atomHeader), 1, f) == 0)
			return NULL;
		__int64 length = DWORD_ENDIAN(atomHeader.length);
		if (length == 0)
			length = fileLength - _ftelli64(f);
		else
		if (length == 1)
		{
			QWORD actualLength;
			if (fread(&actualLength, sizeof(actualLength), 1, f) == 0)
				return NULL;
			length = QWORD_ENDIAN(actualLength) - sizeof(atomHeader) - sizeof(actualLength);
		}
		else
			length -= sizeof(atomHeader);
		
		//printf("Atom: %c%c%c%c, length 0x%llX\n", atomHeader.nameStr[0], atomHeader.nameStr[1], atomHeader.nameStr[2], atomHeader.nameStr[3], length);
		if (atomHeader.name == FOURCC_ENDIAN('moov'))
		{
			for (;;)
			{
				if (length < sizeof(atomHeader))
					return NULL;
				if (fread(&atomHeader, sizeof(atomHeader), 1, f) == 0)
					return NULL;
				length -= sizeof(atomHeader);
				__int64 movieAtomLength = DWORD_ENDIAN(atomHeader.length);
				if (movieAtomLength == 0)
					movieAtomLength = length;
				else
				if (movieAtomLength == 1)
				{
					QWORD actualLength;
					if (length < sizeof(actualLength))
						return NULL;
					if (fread(&actualLength, sizeof(actualLength), 1, f) == 0)
						return NULL;
					length -= sizeof(actualLength);
					movieAtomLength = QWORD_ENDIAN(actualLength) - sizeof(atomHeader) - sizeof(actualLength);
					if (length < movieAtomLength)
						return NULL;
				}
				else
				{
					movieAtomLength -= sizeof(atomHeader);
					if (length < movieAtomLength)
						return NULL;
				}

				//printf("Movie Atom: %c%c%c%c, length 0x%llX\n", atomHeader.nameStr[0], atomHeader.nameStr[1], atomHeader.nameStr[2], atomHeader.nameStr[3], movieAtomLength);
				if (atomHeader.name == FOURCC_ENDIAN('udta'))
				{
					for (;;)
					{
						if (movieAtomLength < sizeof(atomHeader))
							return NULL;
						if (fread(&atomHeader, sizeof(atomHeader), 1, f) == 0)
							return NULL;
						movieAtomLength -= sizeof(atomHeader);
						__int64 userDataAtomLength = DWORD_ENDIAN(atomHeader.length);
						if (userDataAtomLength == 0)
							userDataAtomLength = movieAtomLength;
						else
						if (userDataAtomLength == 1)
						{
							QWORD actualLength;
							if (movieAtomLength < sizeof(actualLength))
								return NULL;
							if (fread(&actualLength, sizeof(actualLength), 1, f) == 0)
								return NULL;
							movieAtomLength -= sizeof(actualLength);
							userDataAtomLength = QWORD_ENDIAN(actualLength) - sizeof(atomHeader) - sizeof(actualLength);
							if (movieAtomLength < userDataAtomLength)
								return NULL;
						}
						else
						{
							userDataAtomLength -= sizeof(atomHeader);
							if (movieAtomLength < userDataAtomLength)
								return NULL;
						}

						//printf("Movie User Data Atom: %c%c%c%c, length 0x%llX\n", atomHeader.nameStr[0], atomHeader.nameStr[1], atomHeader.nameStr[2], atomHeader.nameStr[3], userDataAtomLength);
						if (atomHeader.name == FOURCC_ENDIAN('PANA'))
						{
							if (userDataAtomLength < 0x1FFF0)
								return NULL;
							BYTE *buf = (BYTE*)malloc(0x1FFF0);
							if (!buf)
								return NULL;
							if (fread(buf, 0x1FFF0, 1, f) == 0)
								return NULL;

							if ((DWORD&)buf[0x74] != DWORD_ENDIAN(0xFFD8FFE0) || (WORD&)buf[0x78] != WORD_ENDIAN(0x10) || (FOURCC&)buf[0x7A] != FOURCC_ENDIAN('JFIF'))
							{
								free(buf);
								return NULL;
							}
							Uint jpegLength = 0;
							for (;;)
							{
								if ((WORD&)buf[0x88 + jpegLength] == WORD_ENDIAN(0xFFD9))
									break;
								if (++jpegLength >= 0x4076)
									return NULL;
							}
							jpegLength += 2;

							if ((DWORD&)buf[0x4080] != DWORD_ENDIAN(0xFFD8FFE1))
								return NULL;
							Uint exifLength = WORD_ENDIAN((WORD&)buf[0x4084]);
							/*if (0x4084 + exifLength > userDataAtomLength) // unnecessary check because we already made sure that userDataAtomLength >= 0x1FFF0
								return NULL;*/

							BYTE *firstFrameThumbnailWithEXIF = (BYTE*)malloc(bufferLength = 4 + exifLength + jpegLength);
							if (!firstFrameThumbnailWithEXIF)
								return NULL;
							memcpy(firstFrameThumbnailWithEXIF, buf + 0x4080, 4 + exifLength);
							memcpy(firstFrameThumbnailWithEXIF + 4 + exifLength, buf + 0x88, jpegLength);

							free(buf);
							return firstFrameThumbnailWithEXIF;
						}

						if (userDataAtomLength > movieAtomLength)
							return NULL;
						__int64 newFilePos = _ftelli64(f) + userDataAtomLength;
						if (newFilePos >= fileLength)
							return NULL;
						if (_fseeki64(f, newFilePos, SEEK_SET) != 0)
							return NULL;
						movieAtomLength -= userDataAtomLength;
						if (movieAtomLength == 0)
							break;
					}
					return 0;
				}

				if (movieAtomLength > length)
					return NULL;
				__int64 newFilePos = _ftelli64(f) + movieAtomLength;
				if (newFilePos >= fileLength)
					return NULL;
				if (_fseeki64(f, newFilePos, SEEK_SET) != 0)
					return NULL;
				length -= movieAtomLength;
				if (length == 0)
					break;
			}
		}

		__int64 newFilePos = _ftelli64(f) + length;
		if (newFilePos >= fileLength)
			return NULL;
		if (_fseeki64(f, newFilePos, SEEK_SET) != 0)
			return NULL;
	}
}
#endif

int __cdecl _tmain(int argc, _TCHAR* argv[])
{
	if (_tcscmp(argv[1], _T("-srt"))==0)
	{
		FILE *f0 = _tfopen(argv[2], _T("rb"));
		FILE *f1 = _tfopen(argv[3], _T("wt"));
		int retval = EXIFtoSubtitles(f0, f1);
		fclose(f0);
		fclose(f1);
		return retval;
	}
	if (_tcscmp(argv[1], _T("-frame"))==0)
	{
#if defined(_WIN32) || defined(_WIN64)
		const TCHAR *exiftool = _T("exiftool.exe");
#else
		const TCHAR *exiftool = _T("exiftool");
#endif
		FILE *f0 = _tfopen(argv[2], _T("rb"));
		char line[512];
		Uint linepos = 0;
		bool done = false;
		Uint64 pos = 0;
		for (;;)
		{
			char ch = fgetc(stdin);
			if (feof(stdin) || ferror(stdin))
				break;
			putchar(ch);
			if (done)
				continue;
			if (linepos < sizeof(line))
				line[linepos++] = ch;
			if (ch == '\n')
			{
				if (strncmp(line, "[Parsed_showinfo_0 @ ", strlength("[Parsed_showinfo_0 @ ")) == 0)
				{
					char *found = strstr(line + strlength("[Parsed_showinfo_0 @ "), " pos:");
					if (found)
					{
						sscanf(found + strlength(" pos:"), "%llu", &pos);
						//printf("pos is %llu\n", pos);
						done = true;
					}
				}
				linepos = 0;
			}
		}
		if (!pos)
			fputs("Failure finding reported packet pos of frame in ffmpeg's output\n", stderr);
		else
		{
#ifdef DEBUG_PER_FRAME_EXIF
			BYTE *exif = getEXIF(f0, pos, 0);
#else
			BYTE *exif = getEXIF(f0, pos);
#endif
			if ((DWORD&)exif[0x19] == DWORD_ENDIAN('USR1'))
			{
#if defined(_WIN32) || defined(_WIN64)
				TCHAR image_filename[32768];
				_stprintf_s(image_filename, _T("\"%s\""), argv[4]); // workaround for flaw in the MSCRT implementation of passing arguments in the spawn functions - enclose filename in quotes in case it has spaces
#else
				const TCHAR *image_filename = argv[4];
#endif

#ifdef REQUIRE_EXIFTOOL_9_67_OR_LATER
	#if defined(_WIN32) || defined(_WIN64)
				TCHAR mp4_filename[32768];
				_stprintf_s(mp4_filename, _T("\"%s\""), argv[2]); // workaround for flaw in the MSCRT implementation of passing arguments in the spawn functions - enclose filename in quotes in case it has spaces
	#else
				const TCHAR *mp4_filename = argv[2];
	#endif
				if (_tspawnlp(_P_WAIT, exiftool, exiftool, _T("-overwrite_original"), _T("-tagsFromFile"), mp4_filename, image_filename, NULL) == -1)
					_ftprintf(stderr, _T("Error starting %s\n"), exiftool);
#else
				Uint initialThumbnailLength;
				BYTE *initialThumbnail = GetFirstFrameThumbnailWithEXIFfromMP4(f0, initialThumbnailLength);
				if (!initialThumbnail)
					fputs("Failure finding initial-frame EXIF in video file\n", stderr);
				else
				{
					int stdin_backup = _dup(STDIN_FILENO);
					int stdin_pipe[2];
	#if defined(_WIN32) || defined(_WIN64)
					if (_pipe(stdin_pipe, initialThumbnailLength, _O_BINARY) < 0)
	#else
					if (_pipe(stdin_pipe) < 0)
	#endif
						return -1;
					_dup2(stdin_pipe[0], STDIN_FILENO);
					_write(stdin_pipe[1], initialThumbnail, initialThumbnailLength);
					_close(stdin_pipe[1]);

					if (_tspawnlp(_P_WAIT, exiftool, exiftool, _T("-overwrite_original"), _T("-tagsFromFile"), _T("-"), image_filename, NULL) == -1)
						_ftprintf(stderr, _T("Error starting %s\n"), exiftool);

					_dup2(stdin_backup, STDIN_FILENO);
				}
#endif

				// since the date+time stamps have spaces in them, they must be enclosed in quotes to work around a flaw in the MSCRT implementation of passing arguments in the spawn functions;
				// exiftool can handle it even if the quotes are passed through as literal quotes
				static TCHAR exiftoolCreateDate      [] = _TCAT("-CreateDate=",       "\"YYYY-YY-YY hh:mm:ss\""); static TCHAR exiftoolSubSecTimeDigitized[] = _TCAT("-SubSecTimeDigitized=", "999");
				static TCHAR exiftoolDateTimeOriginal[] = _TCAT("-DateTimeOriginal=", "\"YYYY-YY-YY hh:mm:ss\""); static TCHAR exiftoolSubSecTimeOriginal [] = _TCAT("-SubSecTimeOriginal=" , "999");
				static TCHAR exiftoolModifyDate      [] = _TCAT("-ModifyDate=",       "\"YYYY-YY-YY hh:mm:ss\""); static TCHAR exiftoolSubSecTime         [] = _TCAT("-SubSecTime="         , "999");
				exiftoolCreateDate[strlength("-CreateDate=\""                  )] = exif[0x20]/0x10 + _TCHAR('0');
				exiftoolCreateDate[strlength("-CreateDate=\"Y"                 )] = exif[0x20]%0x10 + _TCHAR('0');
				exiftoolCreateDate[strlength("-CreateDate=\"YY"                )] = exif[0x21]/0x10 + _TCHAR('0');
				exiftoolCreateDate[strlength("-CreateDate=\"YYY"               )] = exif[0x21]%0x10 + _TCHAR('0');
				exiftoolCreateDate[strlength("-CreateDate=\"YYYY-"             )] = exif[0x22]/0x10 + _TCHAR('0');
				exiftoolCreateDate[strlength("-CreateDate=\"YYYY-M"            )] = exif[0x22]%0x10 + _TCHAR('0');
				exiftoolCreateDate[strlength("-CreateDate=\"YYYY-MM-"          )] = exif[0x24]/0x10 + _TCHAR('0');
				exiftoolCreateDate[strlength("-CreateDate=\"YYYY-MM-D"         )] = exif[0x24]%0x10 + _TCHAR('0');
				exiftoolCreateDate[strlength("-CreateDate=\"YYYY-MM-DD "       )] = exif[0x25]/0x10 + _TCHAR('0');
				exiftoolCreateDate[strlength("-CreateDate=\"YYYY-MM-DD h"      )] = exif[0x25]%0x10 + _TCHAR('0');
				exiftoolCreateDate[strlength("-CreateDate=\"YYYY-MM-DD hh:"    )] = exif[0x26]/0x10 + _TCHAR('0');
				exiftoolCreateDate[strlength("-CreateDate=\"YYYY-MM-DD hh:m"   )] = exif[0x26]%0x10 + _TCHAR('0');
				exiftoolCreateDate[strlength("-CreateDate=\"YYYY-MM-DD hh:mm:" )] = exif[0x27]/0x10 + _TCHAR('0');
				exiftoolCreateDate[strlength("-CreateDate=\"YYYY-MM-DD hh:mm:s")] = exif[0x27]%0x10 + _TCHAR('0');
				exiftoolSubSecTimeDigitized[strlength("-SubSecTimeDigitized="  )] = exif[0x94]      + '0';
				exiftoolSubSecTimeDigitized[strlength("-SubSecTimeDigitized=9" )] = exif[0x95]/0x10 + '0';
				exiftoolSubSecTimeDigitized[strlength("-SubSecTimeDigitized=99")] = exif[0x95]%0x10 + '0';
				memcpy(exiftoolDateTimeOriginal + strlength("-DateTimeOriginal="), exiftoolCreateDate + strlength("-CreateDate="), strlength("\"YYYY-YY-YY hh:mm:ss\"") * sizeof(TCHAR));
				memcpy(exiftoolModifyDate       + strlength("-ModifyDate="      ), exiftoolCreateDate + strlength("-CreateDate="), strlength("\"YYYY-YY-YY hh:mm:ss\"") * sizeof(TCHAR));
				memcpy(exiftoolSubSecTimeOriginal + strlength("-SubSecTimeOriginal="), exiftoolSubSecTimeDigitized + strlength("-SubSecTimeDigitized="), strlength("999") * sizeof(TCHAR));
				memcpy(exiftoolSubSecTime         + strlength("-SubSecTime="        ), exiftoolSubSecTimeDigitized + strlength("-SubSecTimeDigitized="), strlength("999") * sizeof(TCHAR));

				static TCHAR exiftoolFNumber[] = _TCAT("-FNumber=", "65535/65535");
				_stprintf(exiftoolFNumber + strlength("-FNumber="), _T("%u/%u"), WORD_ENDIAN((WORD&)exif[0x33]), WORD_ENDIAN((WORD&)exif[0x35]));

				static TCHAR exiftoolExposureTime[] = _TCAT("-ExposureTime=", "65535/65535");
				_stprintf(exiftoolExposureTime + strlength("-ExposureTime="), _T("%u/%u"), WORD_ENDIAN((WORD&)exif[0x2E]), WORD_ENDIAN((WORD&)exif[0x30]));

				static TCHAR exiftoolISO[] = _TCAT("-ISO=", "65535");
				_stprintf(exiftoolISO + strlength("-ISO="), _T("%u"), WORD_ENDIAN((WORD&)exif[0x76]));

				static TCHAR exiftoolExposureCompensation[] = _TCAT("-ExposureCompensation=", "-327.68");
				{
					short EVcomp = (short&)exif[0x3D];
					if (EVcomp < 0)
						_stprintf(exiftoolExposureCompensation + strlength("-ExposureCompensation="), _T("-%u.%02u\n"), (-EVcomp)/100, (-EVcomp)%100);
					else
						_stprintf(exiftoolExposureCompensation + strlength("-ExposureCompensation="), _T("+%u.%02u\n"), EVcomp/100, EVcomp%100);
				}

				static TCHAR exiftoolFocalLength[] = _TCAT("-FocalLength=", "65535/65535");
				_stprintf(exiftoolFocalLength + strlength("-FocalLength="), _T("%u/%u"), WORD_ENDIAN((WORD&)exif[0x60]), WORD_ENDIAN((WORD&)exif[0x62]));

				static TCHAR exiftoolFocalLengthIn35mmFormat[] = _TCAT("-FocalLengthIn35mmFormat=", "65535");
				_stprintf(exiftoolFocalLengthIn35mmFormat + strlength("-FocalLengthIn35mmFormat="), _T("%u"), WORD_ENDIAN((WORD&)exif[0x47]));

				static TCHAR exiftoolOrientation[] = _TCAT("-Orientation=", "255");
				_stprintf(exiftoolOrientation + strlength("-Orientation="), _T("%u"), WORD_ENDIAN((WORD&)exif[0x72]));

				static TCHAR exiftoolWBRedLevel  [] = _TCAT("-WBRedLevel=",   "65536");
				static TCHAR exiftoolWBGreenLevel[] = _TCAT("-WBGreenLevel=", "65536");
				static TCHAR exiftoolWBBlueLevel [] = _TCAT("-WBBlueLevel=",  "65536");
				_stprintf(exiftoolWBRedLevel   + strlength("-WBRedLevel="  ), _T("%u"), WORD_ENDIAN((WORD&)exif[0x85]));
				_stprintf(exiftoolWBGreenLevel + strlength("-WBGreenLevel="), _T("%u"), WORD_ENDIAN((WORD&)exif[0x8A]));
				_stprintf(exiftoolWBBlueLevel  + strlength("-WBBlueLevel=" ), _T("%u"), WORD_ENDIAN((WORD&)exif[0x8F]));

				static TCHAR exiftoolWhiteBalance[] = _TCAT("-WhiteBalance=", "255");
				{
					switch (exif[0x75])
					{
					case  0: _tcscpy(exiftoolWhiteBalance + strlength("-WhiteBalance="), _T( "1")); break; // AWB, AWBc, Custom, or Custom Kelvin
					case  3: _tcscpy(exiftoolWhiteBalance + strlength("-WhiteBalance="), _T( "4")); break; // incandescent
					case  9: _tcscpy(exiftoolWhiteBalance + strlength("-WhiteBalance="), _T( "2")); break; // sunlight
					case 10: _tcscpy(exiftoolWhiteBalance + strlength("-WhiteBalance="), _T( "1")); break; // cloudy
					case 11: _tcscpy(exiftoolWhiteBalance + strlength("-WhiteBalance="), _T("12")); break; // shade
					default: _tcscpy(exiftoolWhiteBalance + strlength("-WhiteBalance="), _T( "0")); break;
					}
				}

				// TODO: set "ShootingMode" too, and handle Intelligent Auto mode correctly
				static TCHAR exiftoolExposureProgram[] = _TCAT("-ExposureProgram=", "255");
				_stprintf(exiftoolExposureProgram + strlength("-ExposureProgram="), _T("%u"), exif[0x39]);
				// 1 = Manual mode
				// 2 = Program mode
				// 3 = Aperture-priority mode
				// 4 = Shutter-priority mode
				// 8 = Intelligent Auto mode

				static const TCHAR *args[] =
				{
					exiftool,
					_T("-n"),
					exiftoolCreateDate,       exiftoolSubSecTimeDigitized,
					exiftoolDateTimeOriginal, exiftoolSubSecTimeOriginal,
					exiftoolModifyDate,       exiftoolSubSecTime,
					exiftoolFNumber,
					exiftoolExposureTime,
					exiftoolISO,
					exiftoolFocalLength,
					exiftoolFocalLengthIn35mmFormat,
					exiftoolExposureCompensation,
					exiftoolWBRedLevel,
					exiftoolWBGreenLevel,
					exiftoolWBBlueLevel,
					exiftoolWhiteBalance,
					exiftoolExposureProgram,
					_T("-ProgramISO=0"),
					_T("-AccelerometerX=0"),
					_T("-AccelerometerY=0"),
					_T("-AccelerometerZ=0"),
					_T("-RollAngle="),
					_T("-PitchAngle="),
					_T("-overwrite_original"), image_filename, NULL
				};

				for (Uint i=0;; i++)
				{
					if (!argv[3][i])
					{
						memmove(args + _countof(args)-(6+3), args + _countof(args)-3, 3 * sizeof(*args));
						break;
					}
					if (argv[3][i] != _TCHAR('0') &&
						argv[3][i] != _TCHAR(':') &&
						argv[3][i] != _TCHAR('.'))
						break;
				}

				if (_tspawnvp(_P_WAIT, exiftool, args) == -1)
					_ftprintf(stderr, _T("Error starting %s\n"), exiftool);
			}
		}
		fclose(f0);
		return 0;
	}
	fputs("Unrecognized command line\n", stderr);
	return -1;
}
