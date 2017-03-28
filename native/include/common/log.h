#ifndef _LOG_H_
#define _LOG_H_

#if defined(__ANDROID__)
	#include <android/log.h>
  	#define	LOG_TAG    	"libglappjni"
	#define	LOGI(...)	__android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)
	#define	LOGE(...)	__android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__) 
#elif defined(DESKTOP_APP)
	#include <cstdio>
	#include "glad/glad.h"
	#define MAX_LOG_MESSAGE_LENGTH 4096
	#define	LOGI(...)\
	{\
		char _log_message[MAX_LOG_MESSAGE_LENGTH];\
		snprintf(_log_message, MAX_LOG_MESSAGE_LENGTH, __VA_ARGS__);\
		printf("%s\n", _log_message);\
	}\

	#define	LOGE(...)\
	{\
		char _log_message[MAX_LOG_MESSAGE_LENGTH];\
		snprintf(_log_message, MAX_LOG_MESSAGE_LENGTH, __VA_ARGS__);\
		fprintf(stderr, "%s\n", _log_message);\
	}\

#endif

#endif // _LOG_H_
