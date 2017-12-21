#ifndef KY_LIB_DEFINES_H_
#define KY_LIB_DEFINES_H_

#ifndef _WIN32
    #define __STDC_LIMIT_MACROS
#endif
#include <stdint.h>
#include <limits.h>
#include <float.h>
#ifndef UINT32_MAX
#   define UINT32_MAX  0x00000000ffffffffULL   /* maximum unsigned int32 value */
#endif
#ifndef INT64_MAX
#   define INT64_MAX   0x7fffffffffffffffLL    /* maximum signed int64 value */
#endif
#ifndef INT64_MIN
#   define INT64_MIN   0x8000000000000000LL    /* minimum signed int64 value */
#endif

// KY_EXPORTS definition
#ifdef _MSC_VER
    #ifdef KY_EXPORTS
        #define KY_API __declspec(dllexport)
    #else
        #define KY_API __declspec(dllimport)
    #endif
#else
    #define KY_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned char KYBOOL;
#define KYTRUE   1
#define KYFALSE  0

// Parameter camera type
typedef enum _cam_property_type
{
    PROPERTY_TYPE_UNKNOWN	= -1,
    PROPERTY_TYPE_INT		= 0x00,
    PROPERTY_TYPE_BOOL		= 0x01,
	PROPERTY_TYPE_STRING	= 0x02,
	PROPERTY_TYPE_FLOAT		= 0x03,
	PROPERTY_TYPE_ENUM		= 0x04,
    PROPERTY_TYPE_COMMAND	= 0x05
}KY_CAM_PROPERTY_TYPE;


// DEPRECATED definition
#ifdef __GNUC__
#define KY_DEPRECATED(func, text) func __attribute__ ((deprecated))
#elif defined(_MSC_VER)
#define KY_DEPRECATED(func, text) __declspec(deprecated(text)) func
// TODO: specify KYFG_CALLCONV for ALL external API
#else
#pragma message("WARNING: You need to implement DEPRECATED for this compiler")
#define DEPRECATED(func) func
#endif

#ifndef _WIN32
    #include <inttypes.h>
/**/
    #define GCC_VERSION (__GNUC__ * 10000 \
                     + __GNUC_MINOR__ * 100 \
                     + __GNUC_PATCHLEVEL__)

    #if(GCC_VERSION <= 40407)
        #define nullptr 0
    #endif
	// We do NOT want to #define 'nullptr' as 0 when compiling in Visual Studio 2012, which does support keyword 'nullptr' 
	// (as well as other C++ 11 features), but still #defines __cplusplus as 199711L
	#if __cplusplus < 201103L
		#define nullptr 0
	#endif

#endif

#ifdef __GNUC__
#define VARIABLE_IS_NOT_USED __attribute__ ((unused))
#else
#define VARIABLE_IS_NOT_USED
#endif

KY_API const char* KY_DeviceDisplayName(int index);
static const char* VARIABLE_IS_NOT_USED DEVICE_QUEUED_BUFFERS_SUPPORTED = "FW_Dma_Capable_QueuedBuffers_Imp";

#ifdef __cplusplus
} // extern "C" {
#endif

#endif // #ifndef KY_LIB_DEFINES_H_
