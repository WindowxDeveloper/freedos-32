#include "fat.h"
void nls_init(void);


int main()
{
	int res;
	Volume *v;
	Channel *c;
	char buf[2048];
	int k;
	nls_init();
	res = fat_mount("../img/floppy.img", &v);
	printf("Mount: %i (%s)\n", res, strerror(-res));
	#if 1
	res = fat_open(v, "\\autoexec.bat", NULL, O_RDWR, 0, &c);
	printf("Open: %i (%s)\n", res, strerror(-res));
	#if 0 /* write */
	res = fat_lseek(c, 0, SEEK_SET);
	printf("Seek: %i (%s)\n", res, strerror(-res));
	for (k = 0; k < sizeof(buf); k++) buf[k] = (k % 10) + '0';
	res = fat_write(c, buf, sizeof(buf));
	printf("Write: %i (%s)\n", res, strerror(-res));
	#elif 0 /* read */
	res = fat_read(c, buf, sizeof(buf));
	printf("Read: %i (%s)\n", res, strerror(-res));
	for (k = 0; k < res; k++) fputc(buf[k], stdout);
	#elif 1 /* truncate */
	res = fat_ftruncate(c, 65432);
	printf("Truncate: %i (%s)\n", res, strerror(-res));
	#endif
	res = fat_close(c);
	printf("Close: %i (%s)\n", res, strerror(-res));
	res = fat_unmount(v);
	printf("Unmount: %i (%s)\n", res, strerror(-res));
	#endif
	#if 0
	res = fat_open(v, "\\", NULL, O_RDONLY | O_DIRECTORY, 0, &c);
//	res = fat_open(v, "\\command.exe", O_RDONLY, 0, &c);
	printf("Open: %i (%s)\n", res, strerror(-res));
	//res = fat_read(c, buf, sizeof(buf));
	//printf("Read: %i (%s)\n", res, strerror(-res));
	//for (k = 0; k < res; k++) fputc(buf[k], stdout);
	const char *fn = "*.*";
	fd32_fs_lfnfind_t lfnfind;
	for (;;)
	{
		res = fat_findfile(c, fn, strlen(fn), (FAT_ANONE << 16) | FAT_ANOVOLID, &lfnfind);
		printf("findfile: \"%s\" %i (%s)\n", lfnfind.LongName, res, strerror(-res));
		if (res < 0) break;
	}
	#endif
	#if 0
	fd32_fs_dosfind_t df;
	res = fat_findfirst(v, "\\grub\\menu.lst", FAT_ANOVOLID, &df);
	if (res >= 0) printf("'%s'\n", df.Name);
	do
	{
		res = fat_findnext(v, &df);
		if (res >= 0) printf("'%s'\n", df.Name);
	}
	while (res >= 0);
	#endif
	return 0;
}
