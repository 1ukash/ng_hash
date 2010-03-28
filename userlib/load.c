#include <netgraph.h>
#include "ngctl.h"
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/types.h>
#include <netgraph/ng_socket.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#define DUMP_BYTES_PER_LINE	16

/* Управляющий сокет и сокет данных */
int	csock, dsock;

void interface (char **buf_con, char **msg, char **msg2, char *arg);


int main (int argc, char **argv)
{
	char name[NG_NODESIZ];
	int args_count = 5;
	int res = 0;
	
	char buf_mkp[10], buf_name[15],buf_msg[10];

	/* Проверка числа переданных аргументов */
	if (argc < 2)
	{
		printf ("%s:HASH no interface name specified\n", argv[0]);
		exit(1);
	}
	
	strcpy (buf_mkp,argv[1]);
	strcat (buf_mkp,":");
	
	strcpy (buf_name,buf_mkp);
	strcat (buf_name,"lower");
	
	strcpy (buf_msg,buf_mkp);
	
	char *args_mkp[] = {"mkpeer", buf_mkp, "hash", "lower", "A"};
	char *args_name[] = {"name", buf_name, "HASH"};
	char *args_msg[] = {"msg", buf_msg, "setpromisc", "1"};
	char *args_msg2[] = {"msg", buf_msg, "setautosrc", "0"};
	
	
	
	/* Задание имени узла по умолчанию */
	snprintf(name, sizeof(name), "ngctl%d", getpid());
	
	
	/*csock = socket (PF_NETGRAPH, SOCK_DGRAM, NG_CONTROL);
	dsock = socket (PF_NETGRAPH, SOCK_DGRAM, NG_DATA);*/
	
	/* Создание нового узла сокета */
	if (NgMkSockNode(name, &csock, &dsock) < 0)
	{
		fprintf (stderr, "Error of creating node - %s\n", (char *)strerror(errno));
		exit(1);
	}
	
	/* Описание MkPeer*/
	if (!(res = MkPeerCmd(args_count, args_mkp)))
	{
		printf("Create node successful\n");
	}
	else
	{
		printf("Error of creating node!\n");
	}
	
	args_count = 3;
	/* Описание Name*/
	if (!(res = NameCmd(args_count, args_name)))
	{
		printf("Name is change\n");
	}
	else
	{
		printf("Error of changing name!\n");
	}
	
	args_count = 5;
	/* Описание Msg*/
	if (!(res = MsgCmd(args_count, args_msg)))
	{
		printf("Message 'setpromisc' was sent\n");
	}
	else
	{
		printf("Error of sending message 'setpromisc'\n");
	}
	if (!(res = MsgCmd(args_count, args_msg2)))
	{
		printf("Message 'setautosrc' was sent\n");
	}
	else
	{
		printf("Error of sending message 'setautosrc'\n");
	}
	
	if (argc > 2)
	{
		char buf_connect[10];
		strcpy (buf_connect, argv[2]);
		strcat (buf_connect, ":");
		char *hook = {"B", "C", "D"};
		char *args_conn[] = {"connect", "HASH:", buf_connect, hook[0], "lower"};
		interface (args_conn, args_msg, args_msg2, argv[2]);
		for (int i=3;i<argc; i++)
		{
			strcpy (buf_connect, argv[i]);
			strcat (buf_connect, ":");
			args_conn[2] = buf_connect;
			args_conn[3] = hook[i-2];
			interface (args_conn, args_msg, args_msg2, argv[i]);
		}
	}
}

void interface (char **buf_con, char **msg, char **msg2, char *arg, char *ch)
{
	int res = 0, args_count = 5;
	arr_conn (buf_con, arg,ch);
	
	strcpy (msg[1],arg);
	strcat (msg[1],":");
	strcpy (msg2[1],msg[1]);
	
	/* Описание Connect*/
	if (!(res = ConnectCmd(args_count, buf_con)))
	{
		printf("Connect is successful!\n");
	}
	else
	{
		printf("Error of connecting\n");
	}
	
	/* Описание Msg*/
	if (!(res = MsgCmd(args_count, msg)))
	{
		printf("Message 'setpromisc' was sent\n");
	}
	else
	{
		printf("Error of sending message 'setpromisc'\n");
	}
	if (!(res = MsgCmd(args_count, msg2)))
	{
		printf("Message 'setautosrc' was sent\n");
	}
	else
	{
		printf("Error of sending message 'setautosrc'\n");
	}
}

void
DumpAscii(const u_char *buf, int len)
{
	char ch, sbuf[100];
	int k, count;

	for (count = 0; count < len; count += DUMP_BYTES_PER_LINE) {
		snprintf(sbuf, sizeof(sbuf), "%04x:  ", count);
		for (k = 0; k < DUMP_BYTES_PER_LINE; k++) {
			if (count + k < len) {
				snprintf(sbuf + strlen(sbuf),
				    sizeof(sbuf) - strlen(sbuf),
				    "%02x ", buf[count + k]);
			} else {
				snprintf(sbuf + strlen(sbuf),
				    sizeof(sbuf) - strlen(sbuf), "   ");
			}
		}
		snprintf(sbuf + strlen(sbuf), sizeof(sbuf) - strlen(sbuf), " ");
		for (k = 0; k < DUMP_BYTES_PER_LINE; k++) {
			if (count + k < len) {
				ch = isprint(buf[count + k]) ?
				    buf[count + k] : '.';
				snprintf(sbuf + strlen(sbuf),
				    sizeof(sbuf) - strlen(sbuf), "%c", ch);
			} else {
				snprintf(sbuf + strlen(sbuf),
				    sizeof(sbuf) - strlen(sbuf), " ");
			}
		}
		printf("%s\n", sbuf);
	}
}
