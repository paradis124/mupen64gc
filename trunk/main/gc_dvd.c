#include "gc_dvd.h"
int last_current_dir = -1;
volatile unsigned long* dvd = (volatile long*)0xCC006000;
unsigned int dvd_read(void* dst, int len, unsigned int offset)
{
	if ((((int)dst) & 0xC0000000) == 0x80000000) // cached?
		dvd[0] = 0x2E;
	dvd[1] = 0;

	dvd[2] = 0xA8000000;
	dvd[3] = offset >> 2;
	dvd[4] = len;
	dvd[5] = (unsigned long)dst;
	dvd[6] = len;
	dvd[7] = 3; // enable reading!
	DCInvalidateRange(dst, len);
	while (dvd[7] & 1);

	if (dvd[0] & 0x4)
		return 1;
		
	return 0;
}
int read_sector(void* buffer, int sector)
{
	return dvd_read(buffer, 2048, sector * 2048);
}
int read_safe(void* dst, int offset, int len)
{
	int ol = len;
	while (len)
	{
		int sector = offset / 2048;
		read_sector(sector_buffer, sector);
		int off = offset & 2047;

		int rl = 2048 - off;
		if (rl > len)
			rl = len;
		memcpy(dst, sector_buffer + off, rl);	

		offset += rl;
		len -= rl;
		dst += rl;
	}
	return ol;
}


int read_direntry(unsigned char* direntry)
{
	int nrb = *direntry++;
	++direntry;

	int sector;

	direntry += 4;
	sector = (*direntry++) << 24;
	sector |= (*direntry++) << 16;
	sector |= (*direntry++) << 8;
	sector |= (*direntry++);	

	int size;

	direntry += 4;

	size = (*direntry++) << 24;
	size |= (*direntry++) << 16;
	size |= (*direntry++) << 8;
	size |= (*direntry++);

	direntry += 7; // skip date

	int flags = *direntry++;
	++direntry; ++direntry; direntry += 4;

	int nl = *direntry++;

	char* name = file[files].name;

	file[files].sector = sector;
	file[files].size = size;
	file[files].flags = flags;

	if ((nl == 1) && (direntry[0] == 1)) // ".."
	{
		file[files].name[0] = 0;
		if (last_current_dir != sector)
			files++;
	}
	else if ((nl == 1) && (direntry[0] == 0))
	{
		last_current_dir = sector;
	}
	else
	{
		if (is_unicode)
		{
			int i;
			for (i = 0; i < (nl / 2); ++i)
				name[i] = direntry[i * 2 + 1];
			name[i] = 0;
			nl = i;
		}
		else
		{
			memcpy(name, direntry, nl);
			name[nl] = 0;
		}

		if (!(flags & 2))
		{
			if (name[nl - 2] == ';')
				name[nl - 2] = 0;

			int i = nl;
			while (i >= 0)
				if (name[i] == '.')
					break;
				else
					--i;

			++i;

		}
		else
		{
			name[nl++] = '/';
			name[nl] = 0;
		}

		files++;
	}

	return nrb;
}



void read_directory(int sector, int len)
{
	read_sector(sector_buffer, sector);

	int ptr = 0;
	files = 0;
	memset(file,0,sizeof(file));
	while (len > 0)
	{
		ptr += read_direntry(sector_buffer + ptr);
		if (!sector_buffer[ptr])
		{
			len -= 2048;
			read_sector(sector_buffer, ++sector);
			ptr = 0;
		}
	}
}

