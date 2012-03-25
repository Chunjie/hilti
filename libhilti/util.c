
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>
#include <unistd.h>
#include <sys/resource.h>

#include "util.h"
#include "globals.h"
// #include "threading.h"

void hlt_util_nanosleep(uint64_t nsecs)
{
    struct timespec sleep_time;
    sleep_time.tv_sec = (time_t) (nsecs / 1e9);
    sleep_time.tv_nsec = (nsecs - sleep_time.tv_sec * 1e9);
    while ( nanosleep(&sleep_time, &sleep_time) )
        continue;
}

static void _reverse(char* start, char* end)
{
    char tmp;

    while( end > start ) {
        tmp = *end;
        *end-- = *start;
        *start++ = tmp;
    }
}

int hlt_util_uitoa_n(uint64_t value, char* buf, int n, int base, int zerofill)
	{
	static char dig[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
	assert(buf && n > 0);

	int i = 0;

	do {
		buf[i++] = dig[value % base];
		value /= base;
	} while ( value && i < n - 1 );

    while ( zerofill && i < n - 1 )
        buf[i++] = '0';

	buf[i] = '\0';

    _reverse(buf, buf + i - 1);

	return i + 1;
	}

int hlt_util_number_of_cpus()
{
    return sysconf(_SC_NPROCESSORS_ONLN);
}

#if 0
void hlt_pthread_setcancelstate(int state, int *oldstate)
{
    if ( ! __hlt_global_thread_mgr )
        return;

    if (  __hlt_global_thread_mgr->state != HLT_THREAD_MGR_RUN )
        pthread_setcancelstate(state, oldstate);
}
#endif

size_t hlt_util_memory_usage()
{
    struct rusage r;
    getrusage(RUSAGE_SELF, &r);
    return (r.ru_maxrss * 1024);
}

#define _flip(x) \
    ((((x) & 0xff00000000000000LL) >> 56) | \
    (((x) & 0x00ff000000000000LL) >> 40) | \
    (((x) & 0x0000ff0000000000LL) >> 24) | \
    (((x) & 0x000000ff00000000LL) >> 8) | \
    (((x) & 0x00000000ff000000LL) << 8) | \
    (((x) & 0x0000000000ff0000LL) << 24) | \
    (((x) & 0x000000000000ff00LL) << 40) | \
    (((x) & 0x00000000000000ffLL) << 56))

uint64_t hlt_hton64(uint64_t v)
{
#if ! __BIG_ENDIAN__
    return _flip(v);
#else
    return v;
#endif
}

uint32_t hlt_hton32(uint32_t v)
{
    return ntohl(v);
}

uint16_t hlt_hton16(uint16_t v)
{
    return ntohs(v);
}

uint64_t hlt_ntoh64(uint64_t v)
{
#if ! __BIG_ENDIAN__
    return _flip(v);
#else
    return v;
#endif
}

uint32_t hlt_ntoh32(uint32_t v)
{
    return ntohl(v);
}

uint16_t hlt_ntoh16(uint16_t v)
{
    return ntohs(v);
}

void hlt_abort()
{
    fprintf(stderr, "internal HILTI error: hlt_abort() called");
    abort();
}