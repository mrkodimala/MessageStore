#include "stdafx.h"
#include <winsock2.h>
#include <windows.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
struct node
{
	char msg[128];
	int msg_id;
	node *next;
}*flist,*alist,*printid;

struct bufserv{
	
		int userId;
		int forumId;
		int msgId;
		int commentId;
		int choice;
		char *forumname;
		char msg[128];
}buf1;

bool flag=true;
int mid = 0;
int count1 =0;
char *Data[100];
int count=1;
int values[100];
DWORD WINAPI SocketHandler(void*);
void replyto_client(char *buf, int *csock);

void socket_server() {

	//The port you want the server to listen on
	int host_port= 1101;

	unsigned short wVersionRequested;
	WSADATA wsaData;
	int err;
	wVersionRequested = MAKEWORD( 2, 2 );
 	err = WSAStartup( wVersionRequested, &wsaData );
	if ( err != 0 || ( LOBYTE( wsaData.wVersion ) != 2 ||
		    HIBYTE( wsaData.wVersion ) != 2 )) {
	    fprintf(stderr, "No sock dll %d\n",WSAGetLastError());
		goto FINISH;
	}

	//Initialize sockets and set options
	int hsock;
	int * p_int ;
	hsock = socket(AF_INET, SOCK_STREAM, 0);
	if(hsock == -1){
		printf("Error initializing socket %d\n",WSAGetLastError());
		goto FINISH;
	}
	
	p_int = (int*)malloc(sizeof(int));
	*p_int = 1;
	if( (setsockopt(hsock, SOL_SOCKET, SO_REUSEADDR, (char*)p_int, sizeof(int)) == -1 )||
		(setsockopt(hsock, SOL_SOCKET, SO_KEEPALIVE, (char*)p_int, sizeof(int)) == -1 ) ){
		printf("Error setting options %d\n", WSAGetLastError());
		free(p_int);
		goto FINISH;
	}
	free(p_int);

	//Bind and listen
	struct sockaddr_in my_addr;
	my_addr.sin_family = AF_INET ;
	my_addr.sin_port = htons(host_port);
	
	memset(&(my_addr.sin_zero), 0, 8);
	my_addr.sin_addr.s_addr = INADDR_ANY ;
	
	/* if you get error in bind 
	make sure nothing else is listening on that port */
	if( bind( hsock, (struct sockaddr*)&my_addr, sizeof(my_addr)) == -1 ){
		fprintf(stderr,"Error binding to socket %d\n",WSAGetLastError());
		goto FINISH;
	}
	if(listen( hsock, 10) == -1 ){
		fprintf(stderr, "Error listening %d\n",WSAGetLastError());
		goto FINISH;
	}
	
	//Now lets do the actual server stuff

	int* csock;
	sockaddr_in sadr;
	int	addr_size = sizeof(SOCKADDR);
	
	while(true){
		printf("waiting for a connection\n");
		csock = (int*)malloc(sizeof(int));
		
		if((*csock = accept( hsock, (SOCKADDR*)&sadr, &addr_size))!= INVALID_SOCKET ){
			//printf("Received connection from %s",inet_ntoa(sadr.sin_addr));
			CreateThread(0,0,&SocketHandler, (void*)csock , 0,0);
		}
		else{
			fprintf(stderr, "Error accepting %d\n",WSAGetLastError());
		}
	}

FINISH:
;
}


const int blocksize = 256;
const int datastart = 8192;
struct BitVector{
	int values[1024];
};

int GiveFreeBlock()
{
	int s = 1;
	int flag = 0;
	int count = 0;
	int j = 0;
	struct BitVector b;
	FILE *file = fopen("store.bin", "r+b");
	fread(&b, sizeof(struct BitVector), 1, file);
	for (int i = 0; i < 16384; i++)
	{
		int v = b.values[i];
		j = 1;
		s = 1;
		if (v < 4294967295)
		{
			//printf("%d\t", v);
			do{
				flag = v&s;
				count++;
				if (flag == 0)
				{
					b.values[i] = v^s;
					printf("\nB value=%d\n", b.values[i]);
					fseek(file, 0, SEEK_SET);
					fwrite(&b, sizeof(struct BitVector), 1, file);
					fclose(file);
					return datastart + (count * blocksize);
				}
				s = s << 1;
				j++;
			} while (j < 32);
		}
		else{
			count = count + 31;
		}
	}
	fclose(file);
	return 0;
}


void FreeBlock(int blockno)
{
	FILE *file = fopen("store.bin", "rb+");
	struct BitVector b;
	fread(&b, sizeof(struct BitVector), 1, file);
	blockno = blockno - datastart;
	blockno = blockno / 1024;
	int d = blockno / 32;
	int r = blockno % 32;
	unsigned int s = 1;
	unsigned int v = b.values[d];
	int i;
	if (d == 0)
		i = 2;
	else
		i = 0;
	for (; i <= r; i++)
	{
		s = s << 1;
	}
	b.values[d] = v^s;
	fseek(file, 0, SEEK_SET);
	fwrite(&b, sizeof(struct BitVector), 1, file);
	fclose(file);
}

struct User{
	char username[32];
	int offset[7];
	int count;
};

struct UserData{
	struct User users[31];
	int userscount;
};

struct Category{
	char name[12];
	int msgtableadd;
};

struct CatCollection{
	struct Category cats[127];
	int catcount;
};

struct Message{
	char msg[128];
	char name[32];
	int reptbleoffset;
};

struct MessageTable{
	int messages[10];
	int messagecount;
	int nxttbleadd;
	int prevtbleadd;
};

int current_user;

void AddUser(char *name)
{
	FILE *file = fopen("store.bin", "rb+");
	fseek(file, 4096, SEEK_SET);
	struct UserData u;
	fread(&u, sizeof(struct UserData), 1, file);
	struct User user;
	memset(&user, 0, sizeof(struct User));
	strcpy(user.username, name);
	user.count = 0;
	u.users[u.userscount++] = user;
	current_user = u.userscount - 1;
	fseek(file, 4096, SEEK_SET);
	fwrite(&u, sizeof(struct UserData), 1, file);
	fclose(file);
}

int LoginUser(char *name)
{
	FILE *file = fopen("store.bin", "rb+");
	fseek(file, 4096, SEEK_SET);
	struct UserData u;
	fread(&u, sizeof(struct UserData), 1, file);
	struct User user;
	int i;
	int flag;
	for (i = 0; i < u.userscount; i++)
	{
		user = u.users[i];
		flag = strcmp(user.username, name);
		//printf("In the login\n%s\t%s\t%d\n", user.username, name,flag);
		if (flag == 0)
		{
			//printf("%s\t%d\t%d\n",user.username, user.offset[0], user.offset[1]);
			return i;
		}
	}
	fclose(file);
	return -1;
}


void ProcessMainScreen(char *command)
{
	int i=1;
	for (; command[i] != '$'; i++);
	i++;
	char option[10];
	int offset = 0;
	for (; command[i] != '$'; i++)
		option[offset++] = command[i];
	int choice = atoi(option);
	char username[32];
	offset = 0;
	i++;
	for (; command[i] != '$'; i++)
		username[offset++] = command[i];
	username[offset] = '\0';
	//printf("%s\t%d\n", username, choice);
	if (choice == 2)
	{
		AddUser(username);
		strcpy(command, "@loginscreen$\n\t1.Add Cateogry\n\t2.View Categories\n\t3.View your Categories\n\t$1$3$$#");
	}
	else if(choice==1){
		int k = LoginUser(username);
		printf("k=%d\n", k);
		if (k == -1)
			strcpy(command, "Invalid Username#");
		else{
			current_user = k;
			strcpy(command, "@loginscreen$\n\t1.Add Cateogry\n\t2.View Categories\n\t3.View your Categories\n\t$1$3$$#");
		}
	}
}


void ViewCategories(char *command)
{
	FILE *file = fopen("store.bin", "rb+");
	fseek(file, 6144, SEEK_SET);
	struct CatCollection c;
	fread(&c, sizeof(struct CatCollection), 1, file);
	strcpy(command,"@viewcats$");
	char temp[5];
	for (int i = 0; i < c.catcount; i++)
	{
		itoa(i + 1, temp, 10);
		strcat(command, "\n\t");
		strcat(command, temp);
		strcat(command, ".");
		strcat(command, c.cats[i].name);
	}
	strcat(command, "\n\t$");
	itoa(1, temp, 10);
	strcat(command,temp);
	strcat(command, "$");
	itoa(c.catcount, temp, 10);
	strcat(command, temp);
	strcat(command, "$$#");
	fclose(file);
}


void ViewYourCategories(char *command)
{
	FILE *file = fopen("store.bin", "rb+");
	struct CatCollection c;
	fseek(file, 6144, SEEK_SET);
	fread(&c, sizeof(struct CatCollection), 1, file);
	struct UserData u;
	fseek(file, 4096, SEEK_SET);
	fread(&u, sizeof(struct UserData), 1, file);
	struct User user;
	user = u.users[current_user];
	if (user.count <= 0)
	{
		strcpy(command, "No categories Go back and Create one#");
		return;
	}
	strcpy(command, "@viewyourcats$");
	char temp[5];
	for (int i = 0; i <user.count; i++)
	{
		itoa(i + 1, temp, 10);
		strcat(command, "\n\t");
		strcat(command, temp);
		strcat(command, ".");
		strcat(command, c.cats[user.offset[i]].name);
	}
	strcat(command, "\n\t$");
	itoa(1, temp, 10);
	strcat(command, temp);
	strcat(command, "$");
	itoa(user.count, temp, 10);
	strcat(command, temp);
	strcat(command, "$$#");
}

void ProcessLoginScreen(char *command)
{
	int i;
	for (i = 1; command[i] != '$'; i++);
	char option[10];
	int offset = 0;
	i++;
	for (; command[i] != '$'; i++)
		option[offset++] = command[i];
	option[offset] = '\0';
	int choice = atoi(option);
	printf("choice=%d", choice);
	printf("%s", command);
	if (choice == 1)
	{
		strcpy(command,"%addcategory$categoryname$#");
	}
	else if (choice == 2)
	{
		ViewCategories(command);
	}
	else {
		ViewYourCategories(command);
	}
}

void ProcessAddCategory(char *command)
{
	int i;
	for (i = 1; command[i] != '$'; i++);
	char name[32];
	int offset = 0;
	i++;
	for (; command[i] != '$'; i++)
		name[offset++] = command[i];
	name[offset] = '\0';
	struct UserData u;
	FILE *file = fopen("store.bin", "rb+");
	fseek(file, 4096, SEEK_SET);
	fread(&u, sizeof(struct UserData), 1, file);
	struct User user = u.users[current_user];
	struct CatCollection c;
	fseek(file, 6144, SEEK_SET);
	fread(&c, sizeof(struct CatCollection), 1, file);
	struct Category catgry;
	strcpy(catgry.name, name);
	catgry.msgtableadd = 0;
	c.cats[c.catcount++] = catgry;
	user.offset[user.count++] = c.catcount - 1;
	u.users[current_user] = user;
	fseek(file, 4096, SEEK_SET);
	fwrite(&u, sizeof(struct UserData), 1, file);
	fseek(file, 6144, SEEK_SET);
	fwrite(&c, sizeof(struct CatCollection), 1, file);
	fclose(file);
}


int cur_msg_tble_add;
int cur_category;

void ViewMessages(char *command)
{
	if (cur_msg_tble_add <= 0)
	{
		strcpy(command, "@addmessage$\n\tNo Messages Yet\n\t1.Add New Message\n\t$1$1$message$#");
		return;
	}
	FILE *file = fopen("store.bin", "rb+");
	fseek(file, cur_msg_tble_add, SEEK_SET);
	struct MessageTable mstble;
	struct Message msg;
	fread(&mstble, sizeof(struct MessageTable), 1, file);
	strcpy(command, "@messages$");
	char temp[10];
	for (int i = 0; i < mstble.messagecount; i++)
	{
		int msgoffset = mstble.messages[i];
		fseek(file, msgoffset, SEEK_SET);
		fread(&msg, sizeof(struct Message), 1, file);
		itoa(i + 1, temp, 10);
		strcat(command, "\n\t");
		strcat(command, temp);
		strcat(command, " . ");
		strcat(command, msg.msg);
		strcat(command, "\t");
		strcat(command, msg.name);
	}
	if (mstble.messagecount == 10)
		strcat(command,"\n\t11. Prev Table\n\t12. Next Table\n\t$1$12$$#");
	else{
		strcat(command, "\n\t Enter 0 to Add Message\n\t11 . Prev Table\n\t$0$11$$#");
	}
}

void AddMessage(char *command)
{
	int i;
	for (i = 1; command[i] != '$'; i++);
	i++;
	for (; command[i] != '$'; i++);
	i++;
	char message[128];
	int offset = 0;
	for (; command[i] != '$'; i++)
		message[offset++] = command[i];
	message[offset] = '\0';
	int msgtableoffset;
	int flag = 0; int prevpointer=0;
	FILE *file = fopen("store.bin", "rb+");
	printf("current message table address=%d\n", cur_msg_tble_add);
	if (cur_msg_tble_add <= 0)
	{
		msgtableoffset = GiveFreeBlock();
		printf("\nmessage table offset dynamically allocated=%d\n", msgtableoffset);
		struct CatCollection c;
		struct MessageTable mstble;
		fseek(file, 6144, SEEK_SET);
		fread(&c, sizeof(struct CatCollection), 1, file);
		struct Category cat = c.cats[cur_category];
		if (cat.msgtableadd > 0)
		{
			int offset = cat.msgtableadd;
			printf("message table addresss in categories=%d\n", offset);
			int tempoffset=offset;
			while (offset > 0)
			{
				fseek(file, offset, SEEK_SET);
				fread(&mstble, sizeof(struct MessageTable), 1, file);
				tempoffset = offset;
				offset = mstble.nxttbleadd;
				printf("In the while next table adress=%d,tempoffset=%d\n", offset, tempoffset);
			}
			mstble.nxttbleadd = msgtableoffset;
			printf("next message table address=%d\n", mstble.nxttbleadd);
			fseek(file,tempoffset , SEEK_SET);
			fwrite(&mstble, sizeof(struct MessageTable), 1, file);
			flag = 1;
			prevpointer = tempoffset;
			printf("Prev pointer = %d\n", prevpointer);
		}
		else{
			printf("in the else cat adress=%d and assigned value=%d\n", cat.msgtableadd,msgtableoffset);
			cat.msgtableadd = msgtableoffset;
			c.cats[cur_category] = cat;
			fseek(file, 6144, SEEK_SET);
			fwrite(&c, sizeof(struct CatCollection), 1, file);
		}
	}
	else{
		msgtableoffset = cur_msg_tble_add;
	}
	fseek(file, msgtableoffset, SEEK_SET);
	struct MessageTable mstble;
	fread(&mstble, sizeof(struct MessageTable), 1, file);
	struct Message msg;
	memset(&msg, 0, sizeof(struct Message));
	strcpy(msg.msg, message);
	struct UserData u;
	fseek(file, 4096, SEEK_SET);
	fread(&u, sizeof(struct UserData), 1, file);
	strcpy(msg.name, u.users[current_user].username);
	int msgoffset = GiveFreeBlock();
	fseek(file, msgoffset, SEEK_SET);
	fwrite(&msg, sizeof(struct Message), 1, file);
	mstble.messages[mstble.messagecount++] = msgoffset;
	if (flag == 1)
	{
		mstble.prevtbleadd = prevpointer;
	}
	fseek(file, msgtableoffset, SEEK_SET);
	fwrite(&mstble, sizeof(struct MessageTable), 1, file);
	fclose(file);
}

int cur_message_number;
int cur_reply_table;

void ProcessMessages(char *command)
{
	int i;
	for (i = 1; command[i] != '$'; i++);
	i++;
	char option[10];
	int offset = 0;
	for (; command[i] != '$'; i++)
		option[offset++] = command[i];
	option[offset] = '\0';
	int choice = atoi(option);
	FILE *file = fopen("store.bin", "rb+");
	struct MessageTable msgtble;
	fseek(file, cur_msg_tble_add, SEEK_SET);
	fread(&msgtble, sizeof(struct MessageTable), 1, file);
	fclose(file);
	if (choice == 0)
		strcpy(command,"@addmessage$\n\t1 . Add Message \n\t$1$1$message$#");
	else if (choice > 0 && choice <= 10)
	{
		if (choice>msgtble.messagecount)
			strcpy(command, "Invalid Choice#");
		else
		{
			struct Message msg;
			fseek(file, msgtble.messages[choice - 1], SEEK_SET);
			fread(&msg, sizeof(struct Message), 1, file);
			cur_reply_table = msg.reptbleoffset;
			cur_message_number = choice-1;
			strcpy(command, "@messageselect$\n\t1.View  Replies\n\t2.Delete Message\n\t$1$2$$#");
		}
	}
	else if (choice == 11)
	{
		if (msgtble.prevtbleadd == 0)
			strcpy(command, "No Prev Table#");
		else if (msgtble.prevtbleadd > 0)
		{
			cur_msg_tble_add = msgtble.prevtbleadd;
			ViewMessages(command);
		}
	}
	else if (choice == 12)
	{
		printf("\ncurrent msg table addresss=%d\t", cur_msg_tble_add);
		cur_msg_tble_add = msgtble.nxttbleadd;
		printf("current msg table addresss=%d\n", cur_msg_tble_add);
		ViewMessages(command);
	}
}


void DeleteMessage()
{
	FILE *file = fopen("store.bin", "rb+");
	int temp_cur_table = cur_msg_tble_add;
	struct MessageTable msgtble,mstble1,mstble2;
	fseek(file, temp_cur_table, SEEK_SET);
	fread(&msgtble, sizeof(struct MessageTable), 1, file);
	FreeBlock(msgtble.messages[cur_message_number]);
	msgtble.messages[cur_message_number] = -999;
	int i = cur_message_number;
	for (; i < msgtble.messagecount-1; i++)
		msgtble.messages[i] = msgtble.messages[i + 1];
	msgtble.messagecount--;
	fseek(file, temp_cur_table, SEEK_SET);
	fwrite(&msgtble, sizeof(struct MessageTable), 1, file);
	while (msgtble.messagecount == 9)
	{
		int next = msgtble.nxttbleadd;
		if (next>0)
		{
			fseek(file, next, SEEK_SET);
			fread(&mstble1, sizeof(struct MessageTable), 1, file);
			if (mstble1.messagecount > 0)
			{
				msgtble.messages[msgtble.messagecount++] = mstble1.messages[0];
				mstble1.messages[0] = -999;
				fseek(file, temp_cur_table, SEEK_SET);
				fwrite(&msgtble, sizeof(struct MessageTable), 1, file);
			}
			for (int j = 0; j < mstble1.messagecount - 1; j++)
				mstble1.messages[j] = mstble1.messages[j + 1];
			mstble1.messagecount--;
			fseek(file, next, SEEK_SET);
			fwrite(&mstble1, sizeof(struct MessageTable), 1, file);
			fseek(file, next, SEEK_SET);
			fread(&msgtble, sizeof(struct MessageTable), 1, file);
		}
	}
	fclose(file);
}

int cur_replytable;
int cur_message_offset;

void ProcessAddReply(char *command)
{
	printf("current Reply table address=%d\tcurrent Message offset=%d\n", cur_replytable, cur_message_offset);
	printf("command=%s\n", command);
	int i;
	for (i = 1; command[i] != '$'; i++);
	char option[10];
	int offset = 0;
	i++;
	for (; command[i] != '$'; i++)
		option[offset++] = command[i];
	option[offset] = '\0';
	char message[128];
	offset = 0;
	i++;
	for (; command[i] != '$'; i++)
		message[offset++] = command[i];
	message[offset] = '\0';
	printf("%s\t%s\n", message, option);
	FILE *file = fopen("store.bin", "rb+");
	struct UserData u;
	fseek(file, 4096, SEEK_SET);
	fread(&u, sizeof(struct UserData), 1, file);
	char username[32];
	strcpy(username, u.users[current_user].username);
	printf("\n\n%s\t%s\t%s\n",username, message, option);
	struct Message msg;
	struct MessageTable msgtable;
	int prevpointer = 0;
	if (cur_replytable == 0)
	{
		int replytbleoffset = GiveFreeBlock();
		cur_replytable = replytbleoffset;
		printf("in the if curreplytable=%d\n", cur_replytable);
		fseek(file, cur_message_offset, SEEK_SET);
		fread(&msg, sizeof(struct Message), 1, file);
		printf("Message=%s\nreplyoffset=%d\n", msg.msg,msg.reptbleoffset);
		int tempoffset = msg.reptbleoffset;
		if (tempoffset == 0)
		{
			msg.reptbleoffset = replytbleoffset;
			fseek(file, cur_message_offset, SEEK_SET);
			fwrite(&msg, sizeof(struct Message), 1, file);
		}
		else{
			prevpointer=tempoffset;
			while (tempoffset > 0)
			{
				fseek(file, tempoffset, SEEK_SET);
				fread(&msgtable, sizeof(struct MessageTable), 1, file);
				prevpointer = tempoffset;
				tempoffset = msgtable.nxttbleadd;
			}
			msgtable.nxttbleadd = replytbleoffset;
			fseek(file, prevpointer, SEEK_SET);
			fwrite(&msgtable, sizeof(struct MessageTable), 1, file);
		}
	}
	int messageoffset = GiveFreeBlock();
	memset(&msg, 0, sizeof(struct Message));
	strcpy(msg.msg, message);
	strcpy(msg.name, username);
	msg.reptbleoffset = 0;
	fseek(file, messageoffset, SEEK_SET);
	fwrite(&msg, sizeof(struct Message), 1, file);
	fseek(file, cur_replytable, SEEK_SET);
	fread(&msgtable, sizeof(struct MessageTable), 1, file);
	msgtable.messages[msgtable.messagecount++] = messageoffset;
	if (prevpointer > 0)
	{
		msgtable.prevtbleadd = prevpointer;
	}
	fseek(file, cur_replytable, SEEK_SET);
	fwrite(&msgtable, sizeof(struct MessageTable), 1, file);
	fclose(file);
	strcpy(command, "Reply Added successfully#");
}



void ProcessViewReplies(char *command)
{
	FILE *file = fopen("store.bin", "rb+");
	if (cur_replytable <= 0)
	{
		strcpy(command, "@addreply$\n\tNo Replies\n\t1.Add Reply\n\t$1$1$reply$#");
	}
	else{
		strcpy(command, "@reply$\n\t");
		struct MessageTable msgtable;
		fseek(file, cur_replytable, SEEK_SET);
		fread(&msgtable, sizeof(struct MessageTable), 1, file);
		struct Message msg;
		char temp[5];
		for (int i = 0; i < msgtable.messagecount; i++)
		{
			fseek(file, msgtable.messages[i], SEEK_SET);
			fread(&msg, sizeof(struct Message), 1, file);
			itoa(i + 1, temp, 10);
			strcat(command, temp);
			strcat(command, " . ");
			strcat(command, msg.msg);
			strcat(command, "\t");
			strcat(command, msg.name);
			strcat(command, "\n\t");
		}
		if (msgtable.messagecount < 10)
			strcat(command, "Enter 0 to Add Reply\n\t1 to Prev Table\n\t $0$1$$#");
		else if (msgtable.messagecount >= 10)
			strcat(command, "Enter 1 to Prev Table\n\t2 to Next Table\n\t$1$2$$#");
	}
	fclose(file);
}

void ViewReplies(char *command)
{
	int msgno = cur_message_number;
	int msgtableoffset = cur_msg_tble_add;
	FILE *file = fopen("store.bin", "rb+");
	struct MessageTable msgtable;
	fseek(file, msgtableoffset, SEEK_SET);
	fread(&msgtable, sizeof(struct MessageTable), 1, file);
	int msgoffset = msgtable.messages[msgno];
	cur_message_offset = msgoffset;
	struct Message msg;
	fseek(file, msgoffset, SEEK_SET);
	fread(&msg, sizeof(struct Message), 1, file);
	int replytableoffset = msg.reptbleoffset;
	cur_replytable = replytableoffset;
	ProcessViewReplies(command);
	fclose(file);
}


void ProcessReplies(char *command)
{
	int i;
	for (i = 1; command[i] != '$'; i++);
	char option[10];
	int offset = 0;
	i++;
	for (; command[i] != '$'; i++)
		option[offset++] = command[i];
	option[offset] = '\0';
	int choice = atoi(option);
	FILE *file = fopen("store.bin", "rb+");
	struct MessageTable msgtable;
	if (choice == 0)
	{
		strcpy(command, "@addreply$\n\t1. Add Reply\n\t$1$1$reply$#");
	}
	else if (choice == 1)
	{
		fseek(file, cur_replytable, SEEK_SET);
		fread(&msgtable, sizeof(struct MessageTable), 1, file);
		if (msgtable.prevtbleadd == 0)
		{
			strcpy(command, "No Prev Tables#");
		}
		else{
			cur_replytable = msgtable.prevtbleadd;
			ProcessViewReplies(command);
		}
	}
	else if (choice == 2)
	{
		fseek(file, cur_replytable, SEEK_SET);
		fread(&msgtable, sizeof(struct MessageTable), 1, file);
		cur_replytable = msgtable.nxttbleadd;
		ProcessViewReplies(command);
	}
}


void ProcessMessagesSelection(char *command)
{
	int i;
	for (i = 1; command[i] != '$'; i++);
	char options[5];
	i++;
	int offset = 0;
	for (; command[i] != '$'; i++)
		options[offset++] = command[i];
	options[offset] = '\0';
	int choice = atoi(options);
	if (choice == 1)
	{
		ViewReplies(command);
	}
	else if (choice == 2)
	{
		DeleteMessage();
		strcpy(command, "Deleted Message  Successfully#");
	}

}


void ProcessViewYourCats(char *command)
{
	int i;
	for (i = 1; command[i] != '$'; i++);
	i++;
	char option[10];
	int offset = 0;
	for (; command[i] != '$'; i++)
		option[offset++] = command[i];
	option[offset] = '\0';
	int choice = atoi(option);
	FILE *file = fopen("store.bin", "rb+");
	fseek(file, 6144, SEEK_SET);
	struct CatCollection c;
	fread(&c, sizeof(struct CatCollection), 1, file);
	fseek(file, 4096, SEEK_SET);
	struct UserData u;
	fread(&u, sizeof(struct UserData), 1, file);
	printf("\n%s", u.users[current_user].username);
	struct User user = u.users[current_user];
	int catno = user.offset[choice-1];
	struct Category ctgry = c.cats[catno];
	cur_msg_tble_add = ctgry.msgtableadd;
	cur_category = catno;
	//printf("category name=%s\n%d\n", ctgry.name,messageoffset);
	fclose(file);
	ViewMessages(command);
	//strcpy(command, "Message table will be available in few hours#");
}


void ProcessViewCats(char *command)
{
	int i;
	for (i = 1; command[i] != '$'; i++);
	i++;
	char option[10];
	int offset = 0;
	for (; command[i] != '$'; i++)
		option[offset++] = command[i];
	option[offset] = '\0';
	int choice = atoi(option);
	FILE *file = fopen("store.bin", "rb+");
	fseek(file, 6144, SEEK_SET);
	struct CatCollection c;
	fread(&c, sizeof(struct CatCollection), 1, file);
	struct Category ctgry;
	ctgry = c.cats[choice - 1];
	int messageoffset = ctgry.msgtableadd;
	cur_msg_tble_add = messageoffset;
	cur_category = choice - 1;
	//printf("category name=%s\n", ctgry.name);
	fclose(file);
	//strcpy(command, "Message table will be available in few hours#");
	ViewMessages(command);
}


int processrecvbuf(char *command)
{
	if (command[0] != '$')
		return 0;
	char buffer[32];
	int offset=0;
	for (int i = 1; command[i] != '$'; i++)
		buffer[offset++] = command[i];
	buffer[offset] = '\0';
	if (!strcmp("opened", buffer))
		return 1;
	else if (!strcmp("mainscreen", buffer))
		return 2;
	else if (!strcmp("loginscreen", buffer))
		return 3;
	else if (!strcmp("addcategory", buffer))
		return 4;
	else if (!strcmp("viewcats", buffer))
		return 5;
	else if (!strcmp("viewyourcats", buffer))
		return 6;
	else if (!strcmp("addmessage", buffer))
		return 7;
	else if (!strcmp("messages", buffer))
		return 8;
	else if (!strcmp("messageselect", buffer))
		return 9;
	else if (!strcmp("addreply", buffer))
		return 10;
	else if (!strcmp("reply", buffer))
		return 11;
}

void process_input(char *recvbuf, int recv_buf_cnt, int* csock) 
{

	char replybuf[1024]={'\0'};
	int k = processrecvbuf(recvbuf);
	if (k == 1)
	{
		printf("Client is started\n");
		strcpy(recvbuf, "@mainscreen$\n\n\tWelcome to Message Store\n\t\t1.Login\n\t\t2.New User\n\t$1$2$name$#");
	}
	else if (k == 2)
	{
		printf("Main screen unser construction");
		ProcessMainScreen(recvbuf);
	}
	else if (k == 3)
	{
		ProcessLoginScreen(recvbuf);
		printf("%s", recvbuf);
	}
	else if (k == 4)
	{
		ProcessAddCategory(recvbuf);
		strcpy(recvbuf, "Category added to Successfully#");
	}
	else if (k == 5)
	{
		ProcessViewCats(recvbuf);
	}
	else if (k == 6)
	{
		ProcessViewYourCats(recvbuf);
	}
	else if (k == 7)
	{
		printf("Add message is called");
		AddMessage(recvbuf);
		strcpy(recvbuf, "Message Added Successfully#");
	}
	else if (k == 8)
	{
		printf("Message Table is called");
		ProcessMessages(recvbuf);
	}
	else if (k == 9)
	{
		ProcessMessagesSelection(recvbuf);
		
	}
	else if (k == 10)
	{
		ProcessAddReply(recvbuf);
		//strcpy(recvbuf, "Replies Will be Added soon#");
	}
	else if (k == 11)
	{
		ProcessReplies(recvbuf);
	}
	replyto_client(recvbuf, csock);
	replybuf[0] = '\0';
}

void replyto_client(char *buf, int *csock) {
	int bytecount;
	
	if((bytecount = send(*csock, buf, strlen(buf), 0))==SOCKET_ERROR){
		fprintf(stderr, "Error sending data %d\n", WSAGetLastError());
		free (csock);
	}
	printf("replied : %s", buf);
}

DWORD WINAPI SocketHandler(void* lp){
    int *csock = (int*)lp;

	char recvbuf[1024];
	int recvbuf_len = 1024;
	int recv_byte_cnt;

	memset(recvbuf, 0, recvbuf_len);
	if((recv_byte_cnt = recv(*csock, recvbuf, recvbuf_len, 0))==SOCKET_ERROR){
		fprintf(stderr, "Error receiving data %d\n", WSAGetLastError());
		free (csock);
		return 0;
	}

	//printf("Received bytes %d\nReceived string \"%s\"\n", recv_byte_cnt, recvbuf);
	process_input(recvbuf, recv_byte_cnt, csock);

    return 0;
}