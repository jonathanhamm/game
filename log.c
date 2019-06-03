#include "log.h"
#include <stdarg.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>

static FILE *log_file;
static pthread_mutex_t lock;

static void log_format(const char *format, const char *name, va_list args);
static void log_format_errno(const char *format, const char *name, va_list args);
static void log_format_explicit(int err, const char *format, const char *name, va_list args);
static int log_create_directory(void);

int log_init_name(const char *name) {
	int result;
	char namebuf[64];
	FILE *file;

	result = log_create_directory();
	if(result) {
		fputs("Warning: Failed to create log directory: " LOG_PATH " . Failling back on stdout.\n", stderr);
		return -1;
	}
	sprintf(namebuf, LOG_PATH "/%s", name);
	file = fopen(namebuf, "a");	
	if(!file) {
		perror("Failed to Open file");
		fprintf(stderr, "Warning: Failed to open log file %s for writing. Failling back on stdout.\n", name);
		log_file = stdout;
		return -1;
	}
	return log_init(file);
}

int log_init(FILE *file) {
	int result;

	log_file = file;
	result = pthread_mutex_init(&lock, NULL);
	if(result) {
		perror("Failed to initialize log file mutex.");
		if(file != stdout && file != stderr) {
			fclose(file);
		}
		return -1;
	}
	return 0;
}

void log_puts(const char *format, ...) {
	va_list args;

	if(log_file) {
		va_start(args, format);
		pthread_mutex_lock(&lock);
		vfprintf(log_file, format, args);
		fputc('\n', log_file);
		fflush(log_file);
		pthread_mutex_unlock(&lock);
		va_end(args);
	}
}

void log_debug(const char *format, ...) {
	va_list args;

	if(log_file) {
		va_start(args, format);
		log_format(format, "DEBUG", args);
		va_end(args);
	}
}

void log_info(const char *format, ...) {
	va_list args;

	if(log_file) {
		va_start(args, format);
		log_format(format, "INFO", args);
		va_end(args);
	}
}

void log_warn(const char *format, ...) {
	va_list args;

	if(log_file) {
		va_start(args, format);
		log_format(format, "WARN", args);
		va_end(args);
	}
}

void log_error(const char *format, ...) {
	va_list args;

	if(log_file) {
		va_start(args, format);
		log_format(format, "ERROR", args);
		va_end(args);
	}
}

void log_error_errno(const char *format, ...) {
	va_list args;

	if(log_file) {
		va_start(args, format);
		log_format_errno(format, "ERROR", args);
		va_end(args);
	}
}

void log_error_explicit(int err, const char *format, ...) {
	va_list args;

	if(log_file) {
		va_start(args, format);
		log_format_explicit(err, format, "ERROR", args);
		va_end(args);
	}
}

void log_format(const char *format, const char *name, va_list args) {
	time_t now;
	char buffer[64];
	struct tm local_time;

	now = time(NULL);
	localtime_r(&now, &local_time);
	strftime(buffer, 64, "%Y-%m-%dT%H:%M:%S%z", &local_time);
	pthread_mutex_lock(&lock);
	fprintf(log_file, "*%s*\t%s\t", name, buffer);
	vfprintf(log_file, format, args);
	fputc('\n', log_file);
	fflush(log_file);
	pthread_mutex_unlock(&lock);
}

void log_format_errno(const char *format, const char *name, va_list args) {
	log_format_explicit(errno, format, name, args);
}

void log_format_explicit(int err, const char *format, const char *name, va_list args) {
	time_t now;
	int result;
	char buffer[LOG_ERR_BUF_SIZE + 1];
	struct tm local_time;

	now = time(NULL);
	localtime_r(&now, &local_time);
	strftime(buffer, 64, "%Y-%m-%dT%H:%M:%S%z", &local_time);
	pthread_mutex_lock(&lock);
	fprintf(log_file, "*%s*\t%s\t", name, buffer);
	vfprintf(log_file, format, args);
	fputs(" : ", log_file);
	while((result = strerror_r(err, buffer, LOG_ERR_BUF_SIZE)) != 0) {
		fputs(buffer, log_file);
	}
	fputc('\n', log_file);
	fflush(log_file);
	pthread_mutex_unlock(&lock);
}

void log_end(void) {
	int result; 

	fflush(log_file);
	if(log_file != stdout && log_file != stderr) {
		fclose(log_file);
	}
	result = pthread_mutex_destroy(&lock);
	if(result) {
		perror("Failed to destroy log file lock mutex");
	}
	log_file = NULL;
}

int log_create_directory(void) {
	int result;
	struct stat st;

	result = stat(LOG_PATH, &st);
	if(result) {
		if(errno == ENOENT) {
			result = mkdir(LOG_PATH, S_IRUSR | S_IWUSR | S_IXUSR);
			if(result) {
				perror("Error while creating log directory - occured during mkdir()");
				return -1;
			}
		}
		else {
			perror("Error while creating log directory - occured during stat()");
			return -1;
		}
	}
	return 0;
}

