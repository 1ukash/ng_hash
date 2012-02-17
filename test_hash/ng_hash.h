
/*
**********************************************************************
*     Заголовочный файл, фильтрующего модуля
*
* (c) Copyright
*---------------------------------------------------------------------
* Файл ng_hash.h
* Автор: Лукашин Алексей
*
* $Id$
**********************************************************************
*/


#ifndef _NG_HASH_H_
#define _NG_HASH_H_

#include <sys/fcntl.h>

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/proc.h>
#include <sys/smp.h>
#include <sys/thr.h>

#include <net/ethernet.h>
#include <netinet/in.h>
#include <netinet/ip_var.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netinet/ip.h>
#include <netinet/tcpip.h>
#include <netgraph/ng_message.h>
#include <netgraph/netgraph.h>
#include <netgraph/ng_parse.h>

/*
 * макрос подсчета разницы времен
 */
#define timersub(tvp, uvp, vvp)                     \
    do {                                \
        (vvp)->tv_sec = (tvp)->tv_sec - (uvp)->tv_sec;      \
        (vvp)->tv_usec = (tvp)->tv_usec - (uvp)->tv_usec;   \
        if ((vvp)->tv_usec < 0) {               \
            (vvp)->tv_sec--;                \
            (vvp)->tv_usec += 1000000;          \
        }                           \
    } while (0)

/* Определение имени узла и идентификатор */
#define NG_HASH_NODE_TYPE	"hash"
#define NGM_HASH_COOKIE	1243937831	


/*Имена хуков*/
#define NG_A 'A'
#define NG_B 'B'
#define NG_C 'C'
#define NG_D 'D'

#define A 0
#define B 1
#define C 2
#define D 3

#define IFNUM 4
#define MAX_RULES 4

/* 
 * Структура статистики хука
 */
struct ng_hash_hookstat {
	u_int64_t	inFrames;
	u_int64_t	outFrames;
	u_int64_t	errors;
	u_int64_t	broadcast;
};

/* 
 * Структура статистики узла
 */
struct ng_hash_nodestat {
	u_int64_t	mac_size;
	u_int64_t	mac_elements;
};


/* Этот макрос необходимо синхронизировать с вышеописанной структурой */
#define NG_HASH_HOOKSTAT_INFO	{				\
	  { "inFrames",		&ng_parse_uint64_type	},	\
	  { "outFrames",		&ng_parse_uint64_type	},	\
	  { "errors",	&ng_parse_uint64_type	},	\
	  { "broadcast",	&ng_parse_uint64_type	},	\
	  { NULL }						\
}

/* Этот макрос необходимо синхронизировать с вышеописанной структурой */
#define NG_HASH_NODESTAT_INFO	{				\
	  { "mac_size",		&ng_parse_uint64_type	},	\
	  { "mac_elements",	&ng_parse_uint64_type	},	\
	  { NULL }						\
}

/*  структура, возвращаемая при запросе NGM_HASH_GET_STATS */
struct ng_hash_stats {
	struct ng_hash_hookstat a,b,c,d;
    struct ng_hash_nodestat mac;
};

/* Этот макрос тоже необходимо синхронизировать с вышеописанной структурой */
#define NG_HASH_STATS_INFO(hstype,nstype)	{			\
	  { "a",		(hstype)		},	\
	  { "b",		(hstype)		},	\
      { "c",      (hstype)        },  \
      { "d",      (hstype)        },  \
      { "mac",      (nstype)        },  \
	  { NULL }						\
}


struct ng_hash_macrule {
	int number;
	u_char		mac_source[6];
	u_char		mac_destination[6];
	int access;
};
/*#define NG_HASH_MACRULE_INFO {        \
    { "number",     &ng_parse_int32_type  },  \
    { "access",     &ng_parse_int32_type  },  \
    { NULL }            \
}*/
#define NG_HASH_MACRULE_INFO {				\
	  { "number",			&ng_parse_int32_type	},	\
	  { "mac_source",		&ng_parse_enaddr_type	},	\
	  { "mac_destination",	&ng_parse_enaddr_type	},	\
	  { "access",			&ng_parse_int32_type	},	\
	  { NULL }						\
}

/* Команды понимаемые узлом */
enum {
	NGM_HASH_GET_STATS = 1,	/* получить статистику */
	NGM_HASH_CLR_STATS,		/* очистить */
	NGM_HASH_CLEAR,
	NGM_HASH_ADD_MACRULE,	/* правила*/
	NGM_HASH_MACRULE_STAT,
};

#endif /* _NETGRAPH_HASH_H_ */
