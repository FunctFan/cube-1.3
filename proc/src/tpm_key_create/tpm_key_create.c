#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <dirent.h>
#include <sys/time.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <openssl/rsa.h>
#include <openssl/evp.h>

#include "data_type.h"
#include "errno.h"
#include "alloc.h"
#include "string.h"
#include "basefunc.h"
#include "struct_deal.h"
#include "crypto_func.h"
#include "memdb.h"
#include "message.h"
#include "ex_module.h"
#include "tesi.h"

#include "file_struct.h"
#include "tesi_key.h"
#include "tesi_aik_struct.h"
#include "tpm_key_create.h"

static struct timeval time_val={0,50*1000};
int print_error(char * str, int result)
{
	printf("%s %s",str,tss_err_string(result));
}

int tpm_key_create_init(void * sub_proc,void * para)
{
	int ret;
	TSS_RESULT result;	
	char local_uuid[DIGEST_SIZE];
	
	result=TESI_Local_ReloadWithAuth("ooo","sss");
	if(result!=TSS_SUCCESS)
	{
		printf("open tpm error %d!\n",result);
		return -ENFILE;
	}
	return 0;
}

int tpm_key_create_start(void * sub_proc,void * para)
{
	int ret;
	int retval;
	void * recv_msg;
	void * send_msg;
	void * context;
	int i;
	int type;
	int subtype;

	char local_uuid[DIGEST_SIZE];
	char proc_name[DIGEST_SIZE];
	
	ret=proc_share_data_getvalue("uuid",local_uuid);
	ret=proc_share_data_getvalue("proc_name",proc_name);

	printf("begin tpm key create start!\n");

	for(i=0;i<300*1000;i++)
	{
		usleep(time_val.tv_usec);
		ret=ex_module_recvmsg(sub_proc,&recv_msg);
		if(ret<0)
			continue;
		if(recv_msg==NULL)
			continue;

 		type=message_get_type(recv_msg);
		subtype=message_get_subtype(recv_msg);
		
		if((type==DTYPE_TESI_KEY_STRUCT)&&(subtype==SUBTYPE_WRAPPED_KEY))
		{
			proc_tpm_key_generate(sub_proc,recv_msg);
		}
	}

	return 0;
};


int proc_tpm_key_generate(void * sub_proc,void * recv_msg)
{
	TSS_RESULT result;
	TSS_HKEY   hKey;
	TSS_HKEY   hWrapKey;
	int i;
	struct vTPM_wrappedkey * key_frame;
	int ret;

	char filename[DIGEST_SIZE*3];
	
	printf("begin tpm key generate!\n");
	char buffer[1024];
	char digest[DIGEST_SIZE];
	int blobsize=0;
	int fd;

	// create a signkey and write its key in localsignkey.key, write its pubkey in localsignkey.pem
	result=TESI_Local_ReloadWithAuth("ooo","sss");

	i=0;

	MSG_HEAD * msghead;
	msghead=message_get_head(recv_msg);
	if(msghead==NULL)
		return -EINVAL;

	void * send_msg=message_create(DTYPE_TESI_KEY_STRUCT,SUBTYPE_WRAPPED_KEY,recv_msg);
	for(i=0;i<msghead->record_num;i++)
	{
		ret=message_get_record(recv_msg,&key_frame,i);
		if(ret<0)
			return -EINVAL;
		if(key_frame==NULL)
			break;
		ret=create_tpm_key(key_frame);
		if(ret<0)
			return ret;
		ret=message_add_record(send_msg,key_frame);
		if(ret<0)
			break;
	};
	
	ret=ex_module_sendmsg(sub_proc,send_msg);
	return ret;
}