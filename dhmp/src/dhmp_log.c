#include <sys/time.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "dhmp_log.h"

#define LOG_TIME_FMT "%04d/%02d/%02d-%02d:%02d:%02d.%05ld"

enum dhmp_log_level global_log_level=DHMP_LOG_LEVEL_WARN;

const char *const level_str[]=
{
	"ERROR", "WARN", "INFO", "DEBUG","TRACE"
};
	
void dhmp_log_impl(const char * file, unsigned line, const char * func, unsigned log_level, const char * fmt, ...)
{
	va_list args;
	char mbuf[2048];
	int mlength=0;

	const char *short_filename;
	char filebuf[270];//filename at most 256

	struct timeval tv_now;
	struct tm tm_now;
	time_t tt_now;
	
	/*get the args,fill in the string*/
	va_start(args,fmt);
	mlength=vsnprintf(mbuf,sizeof(mbuf),fmt,args);
	va_end(args);
	mbuf[mlength]=0;

	/*get short file name*/
	short_filename=strrchr(file,'/');
	short_filename=(!short_filename)?file:short_filename+1;


	snprintf(filebuf,sizeof(filebuf),"%s:%u",short_filename,line);

	/*get the time*/
	gettimeofday(&tv_now,NULL);
	tt_now=(time_t)tv_now.tv_sec;
	localtime_r(&tt_now,&tm_now);


	fprintf(stderr,
		"[" LOG_TIME_FMT "] %-28s [%-5s] - %s\n",
		tm_now.tm_year + 1900, tm_now.tm_mon + 1, tm_now.tm_mday,
		tm_now.tm_hour, tm_now.tm_min, tm_now.tm_sec, tv_now.tv_usec,
		filebuf,
		level_str[log_level], mbuf);

	fflush(stderr);
}


