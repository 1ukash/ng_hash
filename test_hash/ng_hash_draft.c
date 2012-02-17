/*
**********************************************************************
*     Фильтрующий модуль в ядре netgraph
*
* (c) Copyright
*---------------------------------------------------------------------
* Файл ng_hash.c 
* Автор: Лукашин Алексей
*
* $Id$
**********************************************************************
*/
#include "hash_tcp.h"

struct hookinfo {
	hook_p			hook;
	int linknum;
	struct ng_hash_hookstat	stats;
};
typedef struct hookinfo *hi_p;

struct privdata {
	struct hookinfo* links[IFNUM];
	struct hash_table* mactable;
	int atime;
	int cnt;
};
typedef struct privdata *sc_p;

static ng_constructor_t	ng_hash_constructor;
static ng_rcvmsg_t	ng_hash_rcvmsg;
static ng_shutdown_t	ng_hash_shutdown;
static ng_close_t	ng_hash_close;
static ng_newhook_t	ng_hash_newhook;
static ng_rcvdata_t	ng_hash_queue_rcvdata;//ng_hash_rcvdata;
static ng_disconnect_t	ng_hash_disconnect;

const u_char bcast_addr[ETHER_ADDR_LEN] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
u_char tmp[ETHER_ADDR_LEN] = { 0x01, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC };

static const struct ng_parse_struct_field ng_hash_hookstat_type_fields[] = NG_HASH_HOOKSTAT_INFO;

static const struct ng_parse_struct_field ng_hash_nodestat_type_fields[] = NG_HASH_NODESTAT_INFO;

static const struct ng_parse_type ng_hash_hookstat_type = {
	&ng_parse_struct_type,
	&ng_hash_hookstat_type_fields
};

static const struct ng_parse_type ng_hash_nodestat_type = {
	&ng_parse_struct_type,
	&ng_hash_nodestat_type_fields
};

static const struct ng_parse_struct_field ng_hash_stats_type_fields[] = NG_HASH_STATS_INFO(&ng_hash_hookstat_type, &ng_hash_nodestat_type);

static const struct ng_parse_type ng_hash_stats_type = {
	&ng_parse_struct_type,
	&ng_hash_stats_type_fields
};

static const struct ng_cmdlist ng_hash_cmds[] = {
	{
	  NGM_HASH_COOKIE,
	  NGM_HASH_GET_STATS,
	  "getstats",
	  NULL,
	  &ng_hash_stats_type
	},
	{
	  NGM_HASH_COOKIE,
	  NGM_HASH_CLR_STATS,
	  "clrstats",
	  NULL,
	  NULL
	},
	{
	  NGM_HASH_COOKIE,
	  NGM_HASH_CLEAR,
	  "clear",
	  NULL,
	  NULL
	},
	{ 0 }
};

static struct ng_type ng_hash_typestruct = {
	.version =	NG_ABI_VERSION,
	.name =		NG_HASH_NODE_TYPE,
	.constructor =  ng_hash_constructor,
	.rcvmsg =	ng_hash_rcvmsg,
	.shutdown =	ng_hash_shutdown,
	.close =	ng_hash_close,
	.newhook =	ng_hash_newhook,
	.rcvdata =	ng_hash_queue_rcvdata,		//ng_hash_rcvdata,
	.disconnect =	ng_hash_disconnect,
	.cmdlist =	ng_hash_cmds,
};
NETGRAPH_INIT(hash, &ng_hash_typestruct);

static struct mtx mutex[8], send_mtx;

struct queue_element {
  struct mbuf *m;
	hook_p hook;
	//item_p item;
};

struct frame{
  struct queue_element *q_el;
  struct frame *next;
};


static struct frame **first, **last;
static struct proc **queue_proc;
static int atime = 0, count = 0, cpus = 4;
static struct tcp_hash_table *tcpht;

void put(struct queue_element *q_el, int num){
  struct frame *tmp = (struct frame*)malloc(sizeof(struct frame), M_NETGRAPH, M_NOWAIT|M_ZERO);
  tmp->next = NULL;
  tmp->q_el = q_el;
  if (first[num]->next == NULL) {
    first[num]->next = tmp;
  }
  last[num]->next = tmp;
  last[num] = tmp;
};

struct queue_element* get(int num){
  struct frame *tmp = first[num]->next;
  struct queue_element *q_el = tmp->q_el;
  first[num]->next = tmp->next;
  if (first[num]->next == NULL) {
    last[num] = first[num];
  }
  free(tmp, M_NETGRAPH);
  return q_el;
};

int isEmpty(int num) {
  return first[num]->next == NULL;
}

static void test_func(void *arg) {
  int n = (int) arg;
  while (1) {
  mtx_lock(&mutex[n]);
    if (isEmpty(n)) {
			mtx_unlock(&mutex[n]);
			tsleep(queue_proc[n], 0, "sleeping", 0);
    } else {
			struct queue_element *data = get(n);
 			mtx_unlock(&mutex[n]);			
			ng_hash_rcvdata(data->hook, data->m);
    }
  }
}

static int ng_hash_queue_rcvdata(hook_p hook, item_p item) {

  struct queue_element *data = (struct queue_element*)malloc(sizeof(struct queue_element), M_NETGRAPH, M_NOWAIT|M_ZERO);
	
  struct ether_header *eh;
	int error = 0;
	struct mbuf *m;
	struct ip *ip;
	struct tcphdr *tcp;
	int ip_off, poff;
	NGI_GET_M(item, m);
	NG_FREE_ITEM(item);
		
  if (m->m_pkthdr.len < ETHER_HDR_LEN) {
    NG_FREE_M(m);
    printf("invalid packet length\n");
    return (EINVAL);
  }
  
  if ((m->m_len < ETHER_HDR_LEN) && !(m=m_pullup(m,ETHER_HDR_LEN))) {
    printf("cannot pullup()\n");
    return (ENOBUFS);
  }
	
	eh = mtod(m, struct ether_header *);
	
	u_int32_t hash = 0;
	int queue_num;
	
	if (eh->ether_type == htons(ETHERTYPE_IP)) {
		ip_off = sizeof(struct ether_header);
		if ((m = m_pullup(m, ip_off + sizeof(struct ip)))==NULL) {
			printf("cannot pullup()\n");
			return(ENOBUFS);
		}
		ip = (struct ip *)(mtod(m, char *) + ip_off);		
		//проверяем TCP ли это
		if (ip->ip_p == 6){
			poff = ip_off + (ip->ip_hl << 2);
			if ((m = m_pullup(m, poff + sizeof(struct tcphdr)))==NULL) {
				printf("cannot pullup()\n");
				return(ENOBUFS);
			}
			tcp = (struct tcphdr *)(mtod(m, char *) + poff);
			hash = TCP_HASH(ip->ip_src.s_addr,tcp->th_sport,ip->ip_dst.s_addr,tcp->th_dport,tcpht->hashMask);
			
			queue_num = hash % cpus;
		
		} else {
				queue_num = 1;
		}
	} else {
			queue_num = 2;
	}
  	
  data->hook = hook;
  data->m = m;
	
	mtx_lock(&mutex[queue_num]);
	put(data, queue_num);
	mtx_unlock(&mutex[queue_num]);
	wakeup(queue_proc[queue_num]);
	
  return 0;
  
}

static int
ng_hash_constructor(node_p node)
{
	sc_p	privdata;

	MALLOC(privdata, sc_p, sizeof(*privdata), M_NETGRAPH, M_NOWAIT|M_ZERO);

	if (privdata == NULL)
		return (ENOMEM);

	MALLOC(privdata->mactable, struct hash_table*, sizeof(struct hash_table), M_NETGRAPH, M_NOWAIT|M_ZERO);
	
	if (privdata->mactable == NULL)
		return (ENOMEM);
	
	MALLOC(tcpht, struct tcp_hash_table*, sizeof(struct tcp_hash_table), M_NETGRAPH, M_NOWAIT|M_ZERO);
	
	if (tcpht == NULL)
		return (ENOMEM);
	
	int error;
	error=init_default(privdata->mactable);
	if(error != 0) {
		return (ENOMEM);
	}
	
	error=init_default_tcp_hash_table(tcpht);
	if(error != 0) {
		return (ENOMEM);
	}

	int i = 0;
	for(;i<IFNUM;i++) {
		privdata->links[i] = NULL;
	}

	privdata->atime = 0;
	privdata->cnt = 0;

	NG_NODE_SET_PRIVATE(node, privdata);
	
	if ((N_MALLOC(queue_proc, struct proc** , cpus * sizeof(struct proc *)) ) == NULL) {
		return -1;
	}
	
	if ((N_MALLOC(first, struct frame** , cpus * sizeof(struct frame *)) ) == NULL) {
		return -1;
	}

	if ((N_MALLOC(last, struct frame** , cpus * sizeof(struct frame *)) ) == NULL) {
		return -1;
	}
	
	printf("threads = %d\n", cpus);
	mtx_init(&send_mtx, "mutex", NULL, MTX_DEF);
	for(i = 0; i < cpus; i++) {
	  
	  mtx_init(&mutex[i], "mutex", NULL, MTX_DEF);
	  queue_proc[i] = (struct proc*)malloc(sizeof(struct proc), M_NETGRAPH, M_NOWAIT|M_ZERO);
	  /*создание пустой очереди*/
	  first[i] = (struct frame*)malloc(sizeof(struct frame), M_NETGRAPH, M_NOWAIT|M_ZERO);
	  first[i]->next = NULL;
	  first[i]->q_el = NULL;
	  last[i] = first[i];
	
	  if (kthread_create(test_func, (void *)i, NULL, 0, 0, "queue%d", i))
		break;
 	}	
 	
	return (0);
}


static int
ng_hash_newhook(node_p node, hook_p hook, const char *name)
{
	sc_p	privdata = NG_NODE_PRIVATE(node);
	hi_p	hinfo = NULL;
	
	if (strlen(name) != 1) {
		return (EINVAL);
	}

	if (*name == NG_A) {
		MALLOC(privdata->links[A], struct hookinfo*, sizeof(struct hookinfo), M_NETGRAPH, M_NOWAIT|M_ZERO);
		hinfo = privdata->links[A];
		hinfo->linknum = 0;
	} else if (*name == NG_B) {
		MALLOC(privdata->links[B], struct hookinfo*, sizeof(struct hookinfo), M_NETGRAPH, M_NOWAIT|M_ZERO);
		hinfo = privdata->links[B];
		hinfo->linknum = 1;
	}
 	else if (*name == NG_C) {
		MALLOC(privdata->links[C], struct hookinfo*, sizeof(struct hookinfo), M_NETGRAPH, M_NOWAIT|M_ZERO);
		hinfo = privdata->links[C];
		hinfo->linknum = C;
	} else if (*name == NG_D) {
		MALLOC(privdata->links[D], struct hookinfo*, sizeof(struct hookinfo), M_NETGRAPH, M_NOWAIT|M_ZERO);
		hinfo = privdata->links[D];
		hinfo->linknum = D;
	} 

	else {
		return (EINVAL);
	}

	hinfo->hook = hook;	
	NG_HOOK_SET_PRIVATE(hook, hinfo);

	bzero(&hinfo->stats, sizeof(hinfo->stats));
	return (0);
}

static int
ng_hash_rcvmsg(node_p node, item_p item, hook_p lasthook)
{
	const sc_p sc = NG_NODE_PRIVATE(node);
	struct ng_mesg *resp = NULL;
	int error = 0;

	struct ng_mesg *msg;
	NGI_GET_MSG(item, msg);

	switch (msg->header.typecookie) {
	case NGM_HASH_COOKIE:
		switch (msg->header.cmd) {
		case NGM_HASH_CLEAR:
		{
			remove_all_elements(sc->mactable);
			printf("mac table was cleared\n");
			break;
		}
		case NGM_HASH_GET_STATS: 
		{
			struct ng_hash_stats *stats;
           	NG_MKRESPONSE(resp, msg, sizeof(*stats), M_NOWAIT);
			if (resp == NULL) { 
				error = ENOMEM;
				goto done;
			}
			stats = (struct ng_hash_stats *)resp->data;
			if(sc->links[A] != NULL) {
				bcopy(&sc->links[A]->stats, &stats->a, sizeof(stats->a));
			} else {
				bzero(&stats->a, sizeof(stats->a));
			}

			if(sc->links[B] != NULL) {
				bcopy(&sc->links[B]->stats, &stats->b, sizeof(stats->b));
			} else {
				bzero(&stats->b, sizeof(stats->b));
			}

			if(sc->links[C] != NULL) {
				bcopy(&sc->links[C]->stats, &stats->c, sizeof(stats->c));
			} else {
				bzero(&stats->c, sizeof(stats->c));
			}

			if(sc->links[D] != NULL) {
				bcopy(&sc->links[D]->stats, &stats->d, sizeof(stats->d));
			} else {
				bzero(&stats->d, sizeof(stats->d));
			}

			stats->mac.mac_size=sc->mactable->hash_size;			
			stats->mac.mac_elements=sc->mactable->num_elements;
			break;
		}
		case NGM_HASH_CLR_STATS: 
		{
			if(sc->links[A] != NULL)
				bzero(&sc->links[A]->stats, sizeof(sc->links[A]->stats));
			if(sc->links[B] != NULL)
				bzero(&sc->links[B]->stats, sizeof(sc->links[B]->stats));
			if(sc->links[C] != NULL)
				bzero(&sc->links[C]->stats, sizeof(sc->links[C]->stats));
			if(sc->links[D] != NULL)
				bzero(&sc->links[D]->stats, sizeof(sc->links[D]->stats));
			break;
		}
		default:
			error = EINVAL;
			break;
		}
		break;
	case NGM_FLOW_COOKIE:
		break;
	default:
		error = EINVAL;
		break;
	}

done:
	NG_RESPOND_MSG(error, node, item, resp);
	NG_FREE_MSG(msg);
	return (error);
}

int
ng_hash_rcvdata(hook_p hook, struct mbuf *m) 
{
  
	const node_p node = NG_HOOK_NODE(hook);
	const sc_p priv = NG_NODE_PRIVATE(node);
	const hi_p hinfo = NG_HOOK_PRIVATE(hook);

	struct ether_header *eh;
	int	error = 0;
	
	//struct mbuf *m;
	//NGI_GET_M(item, m);
	//NG_FREE_ITEM(item);
	
// 	if (m->m_pkthdr.len < ETHER_HDR_LEN) {
// 		NG_FREE_M(m);
// 		printf("invalid packet length\n");
// 		return (EINVAL);
// 	}
// 	if ((m->m_len < ETHER_HDR_LEN) && !(m=m_pullup(m,ETHER_HDR_LEN))) {
// 		printf("cannot pullup()\n");
// 		return (ENOBUFS);
// 	}
	
	eh = mtod(m, struct ether_header *);

	if ((eh->ether_shost[0] & 1) != 0) {
		NG_FREE_M(m);
		printf("invalid packet\n");
		return (EINVAL);
	}
	
	int manycast;
	manycast = eh->ether_dhost[0] & 1;
	
	int res = get_element(priv->mactable, eh->ether_shost);
	if (res == -1) { 
		
		res = add_element(priv->mactable, eh->ether_shost, hinfo->linknum);
		if (res == -1) { 
			hinfo->stats.errors++;
		} else if (res == -2) { 
			printf("replaced element by another with same mac and other link\n");
		}
	} 

	if (!manycast) {
		int link = get_element(priv->mactable, eh->ether_dhost);
		if (link != -1) { 
			if(link == hinfo->linknum) { 

				NG_FREE_M(m);
				return (0);
			}
			hinfo->stats.outFrames++;
			//Если нашли нужный интерфейс, отправляем пакет по назначению
			struct hookinfo *dest = priv->links[link];
			dest->stats.inFrames++;
			NG_SEND_DATA_ONLY(error, dest->hook, m);

			return (error);
		}
	} else { //Manycast, update stats
		if(ETHER_EQUAL(eh->ether_dhost, bcast_addr)) {
			hinfo->stats.broadcast++;
		}
	}

	//Здесь если широковещательный пакет или неизвестен источник
	//Рассылаем пакеты по всем подключенным интерфейсам
	struct hookinfo* firstFound = NULL; //первый найденный "валидный" интерфейс. 
									//Мы на него пошлем оригинал.
	int i;
	for(i = 0; i < IFNUM; i++) {
		//Если интерфей подключен и не является интерфейсом источником
		if (priv->links[i] != NULL && priv->links[i]->linknum != hinfo->linknum) { 
			//Нашли первый, запомнили, пропустили
			if(firstFound == NULL) {
				firstFound = priv->links[i];
				continue;
			}

			//Сделаем копию mbuf
			struct mbuf *m2;
			m2 = m_dup(m, M_DONTWAIT);
			
			if(m2 == NULL) { //Если mbuf не скопировался
				//NG_FREE_ITEM(item);
				NG_FREE_M(m);
				return (ENOBUFS);
			}
			//Отсылаем копию на очередной интерфейс
			priv->links[i]->stats.inFrames++;
			NG_SEND_DATA_ONLY(error, (priv->links[i])->hook, m2);
			
		}
	}
	//Если-таки нашелся хоть один интерфейс, то отправим оригинал.
	if (firstFound != NULL) {
		hinfo->stats.outFrames++;
		firstFound->stats.inFrames++;
		NG_SEND_DATA_ONLY(error, firstFound->hook, m);
	} else { //А если никого не нашли, то отпустим mbuf с богом
		NG_FREE_M(m);
	}

	return (error);
}

/*
 * Выключение узла
 */
static int
ng_hash_shutdown(node_p node)
{
	const sc_p privdata = NG_NODE_PRIVATE(node);
//Отцепим структуру узла
	NG_NODE_SET_PRIVATE(node, NULL);
//Подчищаем память
	int i = 0;
	for(;i < IFNUM; i++) {
		if(privdata->links[i] != NULL)
			FREE(privdata->links[i], M_NETGRAPH);
	}
	delete_table(privdata->mactable);
	FREE(privdata->mactable, M_NETGRAPH);
	FREE(privdata, M_NETGRAPH);

	NG_NODE_UNREF(node);
	return (0);
}

/*
 * Отсоединение хука
 */
static int
ng_hash_disconnect(hook_p hook)
{
	sc_p	sc = NG_NODE_PRIVATE(NG_HOOK_NODE(hook));
	hi_p const hinfo = NG_HOOK_PRIVATE(hook);

	KASSERT(hinfo != NULL, ("%s: null info", __func__));
	hinfo->hook = NULL;

	int i = 0;
	for(;i < IFNUM; i++) {
		if (sc->links[i] == hinfo) {
			FREE(sc->links[i], M_NETGRAPH);
			sc->links[i] = NULL;
			break;
		}
	}

	/* Откинуться, когда последний хук отсоединен */
	if ((NG_NODE_NUMHOOKS(NG_HOOK_NODE(hook)) == 0) &&
	    NG_NODE_IS_VALID(NG_HOOK_NODE(hook)))
		ng_rmnode_self(NG_HOOK_NODE(hook));
	return (0);
}

/*
 *Закрыть узел. 
 */
static int
ng_hash_close(node_p node)
{
	return (0);
}

