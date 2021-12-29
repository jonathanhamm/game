#include "log.h"
#include <stdarg.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>

typedef struct lqueue_node_s lqueue_node_s;
typedef struct lqueue_s lqueue_s;

struct lqueue_node_s {
  char *message;
  lqueue_node_s *next;
};

struct lqueue_s  {
  lqueue_node_s *head;
  lqueue_node_s *tail;
};

static FILE *log_file;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
static lqueue_s lqueue;
static pthread_t logthread;
static bool loggeractive;

static void log_format(const char *format, const char *name, va_list args);
static void log_format_errno(const char *format, const char *name, va_list args);
static void log_format_explicit(int err, const char *format, const char *name, va_list args);
static void *log_listener(void *arg);
static void lqueue_message(char *message);
static int log_create_directory(void);

int log_init_name(const char *name) {
	int result;
	char namebuf[64];
	FILE *file;

  loggeractive = true;
  result = pthread_create(&logthread, NULL, log_listener, NULL);
  if (result) {
		fputs("Error: Failed to start logger consumer thread.", stderr);
		return -1;
  }

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

  loggeractive = true;
  result = pthread_create(&logthread, NULL, log_listener, NULL);
  if (result) {
		fputs("Error: Failed to start logger consumer thread.", stderr);
		if(file != stdout && file != stderr) {
			fclose(file);
		}
		return -1;
    loggeractive = false;
  }
	log_file = file;
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
   int size, index;
	char buffer[64];
  char *message;
	struct tm local_time;
	va_list args_copy;

	va_copy(args_copy, args);
	
	now = time(NULL);
	localtime_r(&now, &local_time);
	strftime(buffer, 64, "%Y-%m-%dT%H:%M:%S%z", &local_time);

  size = snprintf(NULL, 0, "*%s*\t%s\t", name, buffer);
  size += vsnprintf(NULL, 0, format, args);

  message = malloc(size + 1);
  if (!message) {
    fputs("FATAL: failed to allocate memory for log message", log_file);
    exit(1);
  }

  index = sprintf(message, "*%s*\t%s\t", name, buffer);

  vsprintf(&message[index], format, args_copy);
  va_end(args_copy);
  lqueue_message(message);
}

void log_format_errno(const char *format, const char *name, va_list args) {
	log_format_explicit(errno, format, name, args);
}

void log_format_explicit(int err, const char *format, const char *name, va_list args) {
	time_t now;
	int result, size, index, newsize;
  //placing \n in string so full memory is allocated, despite this not being the sequence it is printed out. 
  const int separatorlen = sizeof(" : \n"); 
	char buffer[LOG_ERR_BUF_SIZE + 1];
  char *message;
	struct tm local_time;
  va_list args_copy;

  va_copy(args_copy, args);

	now = time(NULL);
	localtime_r(&now, &local_time);
	strftime(buffer, 64, "%Y-%m-%dT%H:%M:%S%z", &local_time);

  size = snprintf(NULL, 0, "*%s*\t%s\t", name, buffer);
  size += vsnprintf(NULL, 0, format, args);
  size += separatorlen;
  message = malloc(size);
  if (!message) {
    fputs("FATAL: failed to allocate memory for log message", log_file);
    exit(1);
  }
  index = sprintf(message, "*%s*\t%s\t", name, buffer);
	vsprintf(&message[index], format, args_copy);
  va_end(args_copy);
  //+2 to factor in newline and null terminator
  strcpy(&message[size - separatorlen + 2], " : ");

	while((result = strerror_r(err, buffer, LOG_ERR_BUF_SIZE)) != 0) {
    newsize += size + strlen(buffer);
    message = realloc(message, newsize);
    if (!message) {
      fputs("FATAL: failed to reallocate memory for log message", log_file);
      exit(1);
    }
    strcpy(&message[size - 1], buffer);
    size = newsize;
	}
  message[size] = '\n';
  message[size + 1] = '\0';
  lqueue_message(message);
}

void *log_listener(void *arg) {
  pthread_mutex_lock(&lock);
  while (loggeractive) {
    while (lqueue.head) {
      lqueue_node_s *head = lqueue.head;
      fputs(head->message, log_file);
      fputc('\n', log_file);
      fflush(log_file);
      lqueue.head = head->next;
      free(head->message);
      free(head);
    }
    pthread_cond_wait(&cond, &lock);
  }
  while (lqueue.head) {
    lqueue_node_s *head = lqueue.head;
    fputs(head->message, log_file);
    fputc('\n', log_file);
    fflush(log_file);
    lqueue.head = head->next;
    free(head->message);
    free(head);
  }
  pthread_mutex_unlock(&lock);
  pthread_exit(NULL);
}

void lqueue_message(char *message) {
    lqueue_node_s *nmessage = malloc(sizeof *nmessage);
    if (!nmessage) {
      fputs("FATAL: failed to allocate memory for log queue", log_file);
      exit(1);
    }
    nmessage->message = message;
    nmessage->next = NULL;
    pthread_mutex_lock(&lock);
    if (lqueue.head) {
      lqueue.tail->next = nmessage;
    } else {
      lqueue.head = nmessage;
    }
    lqueue.tail = nmessage;
    pthread_mutex_unlock(&lock);
    pthread_cond_signal(&cond);
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
  loggeractive = NULL;
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

