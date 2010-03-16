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


void arr_conn (char **args, char *name, char *ch);
void interface (char **buf_con, char **msg, char **msg2, char *arg, char *ch);
 
int main (int argc, char **argv)
{
	char name[NG_NODESIZ];
	int args_count = 5;
	int res = 0;
	
	char buf_mkp[10], buf_name[15],buf_msg[10];
	
	strcpy (buf_mkp,argv[1]);
	strcat (buf_mkp,":");
	
	strcpy (buf_name,buf_mkp);
	strcat (buf_name,"lower");
	
	strcpy (buf_msg,buf_mkp);
	
	char *args_mkp[] = {"mkpeer", buf_mkp, "hash", "lower", "A"};
	char *args_name[] = {"name", buf_name, "HASH"};
	char *args_msg[] = {"msg", buf_msg, "setpromisc", "1"};
	char *args_msg2[] = {"msg", buf_msg, "setautosrc", "0"};
	
	/* Управляющий сокет и сокет данных */
	int	csock, dsock;
	
	/* Задание имени узла по умолчанию */
	snprintf(name, sizeof(name), "ngctl%d", getpid());
	
	/* Проверка числа переданных аргументов */
	if (argc < 2)
	{
		printf ("%s:HASH no interface name specified", argv[0]);
		exit(1);
	}
	
	csock = socket (PF_NETGRAPH, SOCK_DGRAM, NG_CONTROL);
	dsock = socket (PF_NETGRAPH, SOCK_DGRAM, NG_DATA);
	
	/* Создание нового узла сокета */
	if (NgMkSockNode(name, &csock, &dsock) < 0)
	{
		fprintf (stderr, "Error of creating node - %s", (char *)strerror(errno));
		exit(1);
	}
	
	/* Описание MkPeer*/
	if (!(res = MkPeerCmd(args_count, args_mkp)))
	{
		printf("Create node successful");
	}
	else
	{
		printf("Error of creating node");
	}
	
	args_count = 3;
	/* Описание Name*/
	if (!(res = NameCmd(args_count, args_name)))
	{
		printf("Name is changed");
	}
	else
	{
		printf("Error of changing name");
	}
	
	args_count = 5;
	/* Описание Msg*/
	if (!(res = MsgCmd(args_count, args_msg)))
	{
		printf("Message 'setpromisc' was sent");
	}
	else
	{
		printf("Error of sending message 'setpromisc'");
	}
	if (!(res = MsgCmd(args_count, args_msg2)))
	{
		printf("Message 'setautosrc' was sent");
	}
	else
	{
		printf("Error of sending message 'setautosrc'");
	}
	
	if (argc > 2)
	{
		char *args_conn[5];
		char hook[1] = "B";
		for (int i=2;i<argc; i++)
		{
			interface (args_conn, args_msg, args_msg2, argv[i], hook);
			hook[0]++;
		}
	}
}

void arr_conn (char **args, char *name, char *ch)
{
	char buf_connect[10];
	strcpy (buf_connect,name);
	strcat (buf_connect,":");
	strcpy (args[0],"connect");
	strcpy (args[1],"HASH:");
	strcpy (args[2],buf_connect);
	strcpy (args[3],ch);
	strcpy (args[4],"lower");
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
		printf("Connect is successful!");
	}
	else
	{
		printf("Error of connecting");
	}
	
	/* Описание Msg*/
	if (!(res = MsgCmd(args_count, msg)))
	{
		printf("Message 'setpromisc' was sent");
	}
	else
	{
		printf("Error of sending message 'setpromisc'");
	}
	if (!(res = MsgCmd(args_count, msg2)))
	{
		printf("Message 'setautosrc' was sent");
	}
	else
	{
		printf("Error of sending message 'setautosrc'");
	}
}
