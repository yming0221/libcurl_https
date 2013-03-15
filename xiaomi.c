/*************************************************************
 *FILE : xiaomi.c
 *Author:YanM
 *E-mail:yming0221@gmail.com
 *
 *Modify history:
 *	1.修改日志的输出格式，将acstime()先转换成字符串后去掉换行符号
 *				----YanM 09/26/2012
 *	2.添加了小米第四轮抢购注册许愿函数MakeWish()
 *              ----YanM 10/01/2012
 *  3.添加小米第四轮抢购注册函数Mi_4_Register()
 *              ----YanM 10/01/2012
 *	4.修改3中提到的函数，电话号码自动增加，批量注册需要指定第一个电话号码，
 *	  并将注册信息存储在文件user_register中,目前实现了批量注册和许愿
 *				----YanM 10/01/2012
 *	5.添加保存电话号码的文件，用于自动注册.
 *				----YanM 10/02/2012
 *	6.添加小米抢购的链接参数变量，在文件commodityinfo中读取.
 *				----YanM 10/02/2012
 *	7.添加抢购函数，并将是否需要身份验证使用宏定义作开关调整
 *				----YanM 10/03/2012
 *	8.修改许愿函数中的许愿内容为免单手机
 *				----YanM 10/06/2012
 *	9.修改程序为多线程，加快程序的并发执行速度
 *				----YanM 10/07/2012
 *	10.由于程序运行修改为了多线程，故将电话信息和用户名密码放置在同一个文件中
 *				----YanM 10/07/2012
 *	11.修改了程序在多线程下使用同一个cookie文件造成的BUG，这里使每个线程使用自己的cookie文件，存储在cookies文件夹下
 *				----YanM 10/07/2012
 *
 *
***************************************************************/
#include <stdio.h>
#include <string.h>
#include <curl/curl.h>
#include <stdlib.h>
#include <time.h>
#include "switch.h"
#include <pthread.h>


#define MAX_LINE	512
#define COOKIE_NAME_LEN 40
//Globle variable area

pthread_t pt_id[PTHREAD_NUMBER];
//pthread_mutex_t pt_lock[PTHREAD_NUMBER];

FILE *info;//the infomation of the users
FILE *login_out;//to record the login and logout of the user
FILE *prebuy;//to record the users operation
FILE *commodity;//the infomation of the commodity


//the url variable read from the file commodityinfo
char add_to_cart[MAX_LINE];
char commodity_10_dian[MAX_LINE];
char mi1s_4_url[MAX_LINE];
char mi1s_4_refer_url[MAX_LINE];
//end of url variable
long long unsigned tel;
FILE *writedata;//the data the server response
FILE *writeheader;//the header the server response

//End of golable variable area

/* we have this global to let the callback get easy access to it */ 
static pthread_mutex_t *lockarray;

#ifdef USE_OPENSSL
#include <openssl/crypto.h>
static void lock_callback(int mode, int type, char *file, int line)
{
	(void)file;
	(void)line;
	if (mode & CRYPTO_LOCK) 
	{
		pthread_mutex_lock(&(lockarray[type]));		   
	}
	else
	{
	    pthread_mutex_unlock(&(lockarray[type]));
	}
}
 
static unsigned long thread_id(void)
{
	  unsigned long ret;   
	  ret=(unsigned long)pthread_self();
	  return(ret);
}
 
static void init_locks(void)
{
	int i;   
	lockarray=(pthread_mutex_t *)OPENSSL_malloc(CRYPTO_num_locks() * sizeof(pthread_mutex_t));
	for (i=0; i<CRYPTO_num_locks(); i++)
	{
		pthread_mutex_init(&(lockarray[i]),NULL);
	}
	   
	CRYPTO_set_id_callback((unsigned long (*)())thread_id);
	CRYPTO_set_locking_callback((void (*)())lock_callback);
}
 
static void kill_locks(void)
{
	int i;
	CRYPTO_set_locking_callback(NULL);
	for (i=0; i<CRYPTO_num_locks(); i++)
		pthread_mutex_destroy(&(lockarray[i]));
		   
	OPENSSL_free(lockarray);
}
#endif

/****************************************
 *Func Name:put_err_msg
 *Description:This fun is used to put the mean of CURL's error number
 *
****************************************/
static void inline put_err_msg(int errno)
{
#ifdef HTTPS_DEBUG
	fprintf(stderr,"%s\n",curl_easy_strerror(errno));
#endif
}
/***************************************
 *Func Name:GetTime
 *Description:This func is used to obtain the time of system to record
 *
****************************************/
static struct tm *GetTime()
{
	time_t now;
	struct tm *timenow;
	time(&now);
	timenow = localtime(&now);
	return timenow;
}
/****************************************
 *Func Name:WriteData
 *
 *
*****************************************/
static size_t WriteData(void * buffer,size_t size,size_t nmemb,void * stream)
{
	int nsize = size * nmemb;
	FILE *fp = (FILE *)stream;
	if(fp)
	{
		fwrite(buffer,size,nmemb,fp);
	}
	return nsize;
}
/****************************************
  *Func Name:CookieFileName
  *Description:This func return the cookie file name,against by the thread_id
  *The memery space of the str must has been allocated
****************************************/
static void CookieFileName(char *str,int pth_id)
{
	if(str == NULL)
		return;
	sprintf(str,"%s_thread_%d","cookies/cookie",pth_id);
}
/****************************************
 *Func Name:Login
 *Description:This func is used to login the server and save the session
 *
****************************************/
static int Login(int pth_id,const char *FORM_DATA)
{

#define HOST_LOGINAUTH_URL	"https://account.xiaomi.com/pass/serviceLoginAuth/"
#define HOST_LOGIN_URL		"https://account.xiaomi.com/pass/serviceLogin"
	CURL *curl;
	CURLcode res;
	struct curl_slist *headerlist=NULL;
	static const char buf[] = "Expect:";
	curl = curl_easy_init();
	headerlist = curl_slist_append(headerlist, buf);
	if(curl) 
	{
		char cookie_name[COOKIE_NAME_LEN];
		CookieFileName(cookie_name,pth_id);
	    
		res = curl_easy_setopt(curl,CURLOPT_URL,HOST_LOGINAUTH_URL);//设置请求的URL
		put_err_msg(res);
	    res = curl_easy_setopt(curl,CURLOPT_HTTPHEADER, headerlist);
		put_err_msg(res);
	    res = curl_easy_setopt(curl,CURLOPT_COOKIEFILE,cookie_name);//提交cookie
		put_err_msg(res);
		res = curl_easy_setopt(curl,CURLOPT_POSTFIELDS,FORM_DATA);//设定要提交的表单
		put_err_msg(res);
		res = curl_easy_setopt (curl, CURLOPT_POSTFIELDSIZE, strlen (FORM_DATA));//提交表单数据长度
		put_err_msg(res);
      	res = curl_easy_setopt (curl, CURLOPT_POST, 1L);//指定使用POST
      	put_err_msg(res);
     	res = curl_easy_setopt (curl, CURLOPT_FAILONERROR, 1L);
     	put_err_msg(res);
	    res = curl_easy_setopt(curl,CURLOPT_COOKIEJAR,cookie_name);//保存登陆后的cookie
		put_err_msg(res);
		res = curl_easy_setopt(curl,CURLOPT_FOLLOWLOCATION,1L);//设置自动跳转
		put_err_msg(res);
		res = curl_easy_setopt(curl,CURLOPT_REFERER,HOST_LOGIN_URL);//设置referer
		put_err_msg(res);
		res = curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,WriteData);//设置写数据回调函数
		put_err_msg(res);
		res = curl_easy_setopt(curl,CURLOPT_WRITEDATA,writedata);//设置写数据的文件描述符
		put_err_msg(res);
		res = curl_easy_setopt(curl,CURLOPT_WRITEHEADER,writeheader);//设置写头部文件描述符
		put_err_msg(res);
	#ifdef SKIP_PEER_VERIFICATION
	    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
	#endif
	#ifdef SKIP_HOSTNAME_VERFICATION
	    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
	#endif
	#ifdef HTTPS_DEBUG
		curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);//Use for debug
	#endif
	    res = curl_easy_perform(curl);

	    /* always cleanup */
	    curl_easy_cleanup(curl);
	    /* free slist */
	    curl_slist_free_all(headerlist);
	}
	return res;
}

#if (defined S_ADD_TO_CART || defined S_EMPTY_CART)
/*********************************************
 *Func Name:Add_to_Cart
 *Description:This func is used to add a commodity to your cart
 *INPUT:The pointer of the commodity URL
*********************************************/
static int Add_to_Cart(const char *mi,int pth_id)
{
	CURL *curl;
	CURLcode res;
	struct curl_slist *headerlist=NULL;
	static const char buf[] = "Expect:";
	curl = curl_easy_init();
	headerlist = curl_slist_append(headerlist, buf);
	if(curl) 
	{
		char cookie_name[COOKIE_NAME_LEN];
		CookieFileName(cookie_name,pth_id);
		
		res = curl_easy_setopt(curl,CURLOPT_URL,mi);//设置要加入购物车的商品URL
		put_err_msg(res);
	    res = curl_easy_setopt(curl,CURLOPT_HTTPHEADER, headerlist);
		put_err_msg(res);
	    res = curl_easy_setopt(curl,CURLOPT_COOKIEFILE,cookie_name);//提交cookie
		put_err_msg(res);
	    res = curl_easy_setopt(curl,CURLOPT_COOKIEJAR,cookie_name);//保存cookie
		put_err_msg(res);
		res = curl_easy_setopt(curl,CURLOPT_FOLLOWLOCATION,1L);//设置自动跳转
		put_err_msg(res);
		res = curl_easy_setopt(curl,CURLOPT_AUTOREFERER,1L);
		put_err_msg(res);
		res = curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,WriteData);//设置写数据回调函数
		put_err_msg(res);
		res = curl_easy_setopt(curl,CURLOPT_WRITEDATA,writedata);//设置写数据的文件描述符
		put_err_msg(res);
		res = curl_easy_setopt(curl,CURLOPT_WRITEHEADER,writeheader);//设置写头部文件描述符
		put_err_msg(res);
	#ifdef SKIP_PEER_VERIFICATION
	    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
	#endif
	#ifdef SKIP_HOSTNAME_VERFICATION
	    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
	#endif
	#ifdef HTTPS_DEBUG
		curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);//Use for debug
	#endif
	    res = curl_easy_perform(curl);
	    /* always cleanup */
	    curl_easy_cleanup(curl);
	    /* free slist */
	    curl_slist_free_all(headerlist);
	}
	return res;

}
#endif

#ifdef S_10_DIAN
/*********************************************
 *Func Name:Commodity_10_Dian
 *Description:
 *INPUT:The URL of the commodity
*********************************************/
static int Commodity_10_Dian(const char *commodity,int pth_id)
{
	CURL *curl;
	CURLcode res;
	struct curl_slist *headerlist=NULL;
	static const char buf[] = "Expect:";
	curl = curl_easy_init();

	headerlist = curl_slist_append(headerlist, buf);
	if(curl) 
	{
		char cookie_name[COOKIE_NAME_LEN];
		CookieFileName(cookie_name,pth_id);
	 
	    res = curl_easy_setopt(curl,CURLOPT_URL,commodity);//设置要10点秒杀URL
		put_err_msg(res);
	    res = curl_easy_setopt(curl,CURLOPT_HTTPHEADER, headerlist);
		put_err_msg(res);
	    res = curl_easy_setopt(curl,CURLOPT_COOKIEFILE,cookie_name);//提交cookie
		put_err_msg(res);
	    res = curl_easy_setopt(curl,CURLOPT_COOKIEJAR,cookie_name);//保存cookie
		put_err_msg(res);
		res = curl_easy_setopt(curl,CURLOPT_REFERER,"http://www.xiaomi.com");//设置referer
		put_err_msg(res);
		res = curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,WriteData);//设置写数据回调函数
		put_err_msg(res);
		res = curl_easy_setopt(curl,CURLOPT_WRITEDATA,writedata);//设置写数据的文件描述符
		put_err_msg(res);
	#ifdef SKIP_PEER_VERIFICATION
	    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
	#endif
	#ifdef SKIP_HOSTNAME_VERFICATION
	    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
	#endif
	#ifdef HTTPS_DEBUG
		curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);//Use for debug
	#endif
	    res = curl_easy_perform(curl);
	    /* always cleanup */
	    curl_easy_cleanup(curl);
	    /* free slist */
	    curl_slist_free_all(headerlist);
	}
	return res;

}
#endif

#ifdef S_MAKE_WISH
/*********************************************
 *Func Name:MakeWish
 *Description:This function is used for the 4 round of MI1S register
 *This function must be used after Register  eg.Mi_4_Register()
*********************************************/
static int MakeWish(int pth_id)
{
#define MAKEWISHURL "http://t.hd.xiaomi.com/index.php?_a=20120928&_op=dowish"
//我要1S F码
//#define SUBMITFORM  "wishType=5&selText=%E6%88%91%E8%A6%811S+F%E7%A0%81%EF%BC%88100%E4%B8%AA%EF%BC%89"
//我要免单手机
#define SUBMITFORM	"wishType=6&selText=%E6%88%91%E8%A6%81%E5%85%8D%E5%8D%95%E6%89%8B%E6%9C%BA%EF%BC%88%E4%BB%8510%E5%8F%B0%EF%BC%89"
	CURL *curl;
	CURLcode res;
	struct curl_slist *headerlist=NULL;
	static const char buf[] = "Expect:";
	curl = curl_easy_init();
	headerlist = curl_slist_append(headerlist, buf);
	if(curl) 
	{
		char cookie_name[COOKIE_NAME_LEN];
		CookieFileName(cookie_name,pth_id);
	 
	    res = curl_easy_setopt(curl,CURLOPT_URL,MAKEWISHURL);//设置请求的URL
	    put_err_msg(res);
	    res = curl_easy_setopt(curl,CURLOPT_HTTPHEADER, headerlist);
		put_err_msg(res);
	    res = curl_easy_setopt(curl,CURLOPT_COOKIEFILE,cookie_name);//提交cookie
		put_err_msg(res);
		res = curl_easy_setopt(curl,CURLOPT_POSTFIELDS,SUBMITFORM);//设定要提交的表单
		put_err_msg(res);
		res = curl_easy_setopt (curl, CURLOPT_POSTFIELDSIZE, strlen (SUBMITFORM));//提交表单数据长度
		put_err_msg(res);
      	res = curl_easy_setopt (curl, CURLOPT_POST, 1L);//指定使用POST
      	put_err_msg(res);
     	res = curl_easy_setopt (curl, CURLOPT_FAILONERROR, 1L);
     	put_err_msg(res);
		res = curl_easy_setopt(curl,CURLOPT_REFERER,"http://t.hd.xiaomi.com/index.php?_a=20120928&_op=wish");//设置referer
		put_err_msg(res);
        res = curl_easy_setopt(curl,CURLOPT_COOKIEJAR,cookie_name);//保存登陆后的cookie
		put_err_msg(res);
		res = curl_easy_setopt(curl,CURLOPT_FOLLOWLOCATION,1L);//设置自动跳转
		put_err_msg(res);
		res = curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,WriteData);//设置写数据回调函数
		put_err_msg(res);
		res = curl_easy_setopt(curl,CURLOPT_WRITEDATA,writedata);//设置写数据的文件描述符
		put_err_msg(res);
	#ifdef SKIP_PEER_VERIFICATION
	    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
	#endif
	#ifdef SKIP_HOSTNAME_VERFICATION
	    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
	#endif
	#ifdef HTTPS_DEBUG
		curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);//Use for debug
	#endif
	    res = curl_easy_perform(curl);
	    /* always cleanup */
	    curl_easy_cleanup(curl);
	    /* free slist */
	    curl_slist_free_all(headerlist);
	}
	return res;

}
#endif
/********************************************
 *Func Name:Mi_4_Register
 *Description:The 4th time per-buy order
 *INPUT:telnum,email address
*********************************************/
#ifdef S_MI_4_REGISTER
static int Mi_4_Register(long long unsigned tel,const char *email,int pth_id)
{
#define REGISTER_URL    "http://t.hd.xiaomi.com/index.php?_a=20120928&_op=dosubscribe"
	char SUBMITFORM[MAX_LINE];

	sprintf(SUBMITFORM,"username=%s&tel=%llu&email=%s","%E6%9D%8E%E5%88%9A",tel,email);//Produce the form to register,username为李刚
	CURL *curl;
	CURLcode res;
	struct curl_slist *headerlist=NULL;
	static const char buf[] = "Expect:";
	curl = curl_easy_init();
	headerlist = curl_slist_append(headerlist, buf);
	if(curl) 
	{
		char cookie_name[COOKIE_NAME_LEN];
		CookieFileName(cookie_name,pth_id);
	 
	    res = curl_easy_setopt(curl,CURLOPT_URL,REGISTER_URL);//设置请求的URL
	    put_err_msg(res);
	    res = curl_easy_setopt(curl,CURLOPT_HTTPHEADER, headerlist);
		put_err_msg(res);
	    res = curl_easy_setopt(curl,CURLOPT_COOKIEFILE,cookie_name);//提交cookie
		put_err_msg(res);
		res = curl_easy_setopt(curl,CURLOPT_POSTFIELDS,SUBMITFORM);//设定要提交的表单
		put_err_msg(res);
		res = curl_easy_setopt (curl, CURLOPT_POSTFIELDSIZE, strlen (SUBMITFORM));//提交表单数据长度
		put_err_msg(res);
      	res = curl_easy_setopt (curl, CURLOPT_POST, 1L);//指定使用POST
      	put_err_msg(res);
     	res = curl_easy_setopt (curl, CURLOPT_FAILONERROR, 1L);
     	put_err_msg(res);
	   	res = curl_easy_setopt(curl,CURLOPT_REFERER,"http://t.hd.xiaomi.com/index.php?_a=20120928&_op=wish");//设置referer
		put_err_msg(res);
        res = curl_easy_setopt(curl,CURLOPT_COOKIEJAR,cookie_name);//保存登陆后的cookie
		put_err_msg(res);
		res = curl_easy_setopt(curl,CURLOPT_FOLLOWLOCATION,1L);//设置自动跳转
		put_err_msg(res);
		res = curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,WriteData);//设置写数据回调函数
		put_err_msg(res);
		res = curl_easy_setopt(curl,CURLOPT_WRITEDATA,writedata);//设置写数据的文件描述符
		put_err_msg(res);
	#ifdef SKIP_PEER_VERIFICATION
	    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
	#endif
	#ifdef SKIP_HOSTNAME_VERFICATION
	    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
	#endif
	#ifdef HTTPS_DEBUG
		curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);//Use for debug
	#endif
	    res = curl_easy_perform(curl);

	    /* always cleanup */
	    curl_easy_cleanup(curl);
	    /* free slist */
	    curl_slist_free_all(headerlist);
	}
	return res;

}
#endif
/*********************************************
 *Func Name:Logout
 *Description:This func is used for logout and delete the session saved local
 *
*********************************************/
static int Logout(int pth_id)
{
#define HOST_LOGOUT_URL		"http://order.xiaomi.com/site/logout"

	CURL *curl;
	CURLcode res;
	struct curl_slist *headerlist=NULL;
	static const char buf[] = "Expect:";
	curl = curl_easy_init();
	headerlist = curl_slist_append(headerlist, buf);
	if(curl) 
	{
		char cookie_name[COOKIE_NAME_LEN];
		CookieFileName(cookie_name,pth_id);
	 
	    res = curl_easy_setopt(curl,CURLOPT_URL,HOST_LOGOUT_URL);//设置请求的URL
		put_err_msg(res);
	    res = curl_easy_setopt(curl,CURLOPT_HTTPHEADER, headerlist);
		put_err_msg(res);
	    res = curl_easy_setopt(curl,CURLOPT_COOKIEFILE,cookie_name);//提交第一步保存的cookie
		put_err_msg(res);
	    res = curl_easy_setopt(curl,CURLOPT_COOKIEJAR,cookie_name);//保存登陆后的cookie
		put_err_msg(res);
		res = curl_easy_setopt(curl,CURLOPT_FOLLOWLOCATION,1L);//设置自动跳转
		put_err_msg(res);
		res = curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,WriteData);//设置写数据回调函数
		put_err_msg(res);
		res = curl_easy_setopt(curl,CURLOPT_WRITEDATA,writedata);//设置写数据的文件描述符
		put_err_msg(res);
		res = curl_easy_setopt(curl,CURLOPT_WRITEHEADER,writeheader);//设置写头部文件描述符
		put_err_msg(res);
	#ifdef SKIP_PEER_VERIFICATION
	    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
	#endif
	#ifdef SKIP_HOSTNAME_VERFICATION
	    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
	#endif
	#ifdef HTTPS_DEBUG
		curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);//Use for debug
	#endif
	    res = curl_easy_perform(curl);

	    /* always cleanup */
	    curl_easy_cleanup(curl);
	    /* free slist */
	    curl_slist_free_all(headerlist);
	}
	return res;
}
#ifdef S_PANIC_BUYING
/*********************************************
 *Func Name:PanicBuying
 *Description:I don't kown the detail of the process until now,use POST or GET,and don't know the Referrence
 *INPUT:mi_4_url and mi_4_refer_url is URL,username and tel is used for authentication
 *
**********************************************/
static int PanicBuying(const char *mi_4_url,const char *mi_4_refer_url,const char *username,long long unsigned tel,int pth_id)
{
	CURL *curl;
	CURLcode res;
	struct curl_slist *headerlist=NULL;
	static const char buf[] = "Expect:";
	curl = curl_easy_init();
#ifdef S_PANIC_BUYING_USE_AUTH
	char SUBMITFORM[MAX_LINE];
	sprintf(SUBMITFORM,"username=%s&tel=%llu&email=%s","%E6%9D%8E%E5%88%9A",tel,username);
#endif
	headerlist = curl_slist_append(headerlist, buf);
	if(curl) 
	{
		char cookie_name[COOKIE_NAME_LEN];
		CookieFileName(cookie_name,pth_id);
	 
	    res = curl_easy_setopt(curl,CURLOPT_URL,mi_4_url);//设置抢购的URL
		put_err_msg(res);
	    res = curl_easy_setopt(curl,CURLOPT_HTTPHEADER, headerlist);
		put_err_msg(res);
	    res = curl_easy_setopt(curl,CURLOPT_COOKIEFILE,cookie_name);//提交cookie
		put_err_msg(res);
	    res = curl_easy_setopt(curl,CURLOPT_COOKIEJAR,cookie_name);//保存cookie
		put_err_msg(res);
		res = curl_easy_setopt(curl,CURLOPT_FOLLOWLOCATION,1L);//设置自动跳转
		put_err_msg(res);
#ifndef S_PANIC_BUYING_AUTOREFERER
		res = curl_easy_setopt(curl,CURLOPT_REFERER,mi_4_refer_url);//设置referer
#else
		res = curl_easy_setopt(curl,CURLOPT_AUTOREFERER,1L);
#endif
		put_err_msg(res);
		res = curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,WriteData);//设置写数据回调函数
		put_err_msg(res);
		res = curl_easy_setopt(curl,CURLOPT_WRITEDATA,writedata);//设置写数据的文件描述符
		put_err_msg(res);
#ifdef S_PANIC_BUYING_USE_AUTH
		res = curl_easy_setopt (curl, CURLOPT_POSTFIELDSIZE, strlen(SUBMITFORM));//提交表单数据长度
		put_err_msg(res);
		res = curl_easy_setopt(curl,CURLOPT_POSTFIELDS,SUBMITFORM);//设定要提交的表单
		put_err_msg(res);
#endif
		res = curl_easy_setopt (curl, CURLOPT_POSTFIELDSIZE, strlen(PANIC_FORM));//提交表单数据长度
		put_err_msg(res);
		res = curl_easy_setopt(curl,CURLOPT_POSTFIELDS,PANIC_FORM);//设定要提交的表单
		put_err_msg(res);
     	res = curl_easy_setopt (curl, CURLOPT_POST, 1L);//指定使用POST
      	put_err_msg(res);

	#ifdef SKIP_PEER_VERIFICATION
	    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
	#endif
	#ifdef SKIP_HOSTNAME_VERFICATION
	    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
	#endif
	#ifdef HTTPS_DEBUG
		curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);//Use for debug
	#endif
	    res = curl_easy_perform(curl);
	    /* always cleanup */
	    curl_easy_cleanup(curl);
	    /* free slist */
	    curl_slist_free_all(headerlist);
	}
	return res;

}
#endif
/**********************************************
 *Func Name:SetPara
 *Description:Read the URL paramater from the FILE fp
 *
***********************************************/
static int SetPara(FILE *fp)
{
	char para[MAX_LINE];
	char url[MAX_LINE];
	char cache[MAX_LINE * 2];
	while(fgets(cache,MAX_LINE * 2,fp) != NULL)
	{
		if('#' == cache[0] || '\0' == cache[0]) 
			continue;
		sscanf(cache,"%s %s",para,url);
		if(strcmp(para,"ADD_TO_CART") == 0) 
			strcpy(add_to_cart,url);
		else if(strcmp(para,"COMMODITY_10_DIAN") == 0) 
			strcpy(commodity_10_dian,url);
		else if(strcmp(para,"MI1S_4_URL") == 0)
			strcpy(mi1s_4_url,url);
		else if(strcmp(para,"MI1S_4_REFER_URL") == 0)
			strcpy(mi1s_4_refer_url,url);

	}
	return 0;
}


/*********************************************
  *Func Name:single_run
  *Description:this is the function run as threads
  *PARAMETER:the thread id,used to identify the cookie file the thread use.
**********************************************/
static void single_run(void *pth_id)
{
	pthread_detach(pthread_self());

	int thread_id = *(int *)pth_id;
	char username[MAX_LINE];
	char passwd[MAX_LINE];
	__int64 regist_tel;
	char form_data[MAX_LINE];//save the users login form data to login

	int res;
	
	while(fscanf(info,"%s %s %llu",username,passwd,&regist_tel) != EOF)
	{
		if(username[0] == '#') //This is used for the info file note
			continue;
		if(username[0] == '*')//This is used for stop
			break;
		sprintf(form_data,"passToken=""&user=%s&pwd=%s",username,passwd);
		
		res = Login(thread_id,form_data);
		if(res != CURLE_OK)
		{
			fprintf(stderr,"Login() is failed!\n");
			put_err_msg(res);
			goto err_e;
		}
		time = GetTime();
		strcpy(time_str,asctime(time));
		time_str_len = strlen(time_str);
		time_str[time_str_len - 1] = '\0';

		fprintf(login_out,"%s	user:%s login!\n",time_str,username);
#ifdef S_ADD_TO_CART
		res = Add_to_Cart(add_to_cart,thread_id);
		if(res != CURLE_OK)
		{
			fprintf(stderr,"Add_to_Cart() is failed!\n");
			put_err_msg(res);
			goto err_e;
		}
		time = GetTime();
		strcpy(time_str,asctime(time));
		time_str_len = strlen(time_str);
		time_str[time_str_len - 1] = '\0';
		fprintf(prebuy,"%s	user:%s prebuy successfully!\n",time_str,username);
		printf("thread:%d	user:%s	prebuy successfully!\n",thread_id,username);
#endif
#ifdef S_EMPTY_CART
		res = Add_to_Cart("http://order.xiaomi.com/cart/empty",thread_id);
		res = Add_to_Cart("http://order.xiaomi.com/cart/empty",thread_id);
		if(res != CURLE_OK)
		{
			fprintf(stderr,"Empty the cart is failed!\n");
			put_err_msg(res);
			goto err_e;
		}
		printf("thread:%d	user:%s	cart is empty!\n",thread_id,username);
#endif
#ifdef S_10_DIAN
		res = Commodity_10_Dian(commodity_10_dian,thread_id);
		res = Commodity_10_Dian(commodity_10_dian,thread_id);
		if(res != CURLE_OK)
		{
			fprintf(stderr,"Commodity_10_Dian() is failed!\n");
			put_err_msg(res);
			goto err_e;
		}
		printf("user:%s 10 dian run successfully!\n",username);
#endif
#ifdef S_MAKE_WISH
        res = MakeWish(thread_id);
        res = MakeWish(thread_id);//Don't know why! Run this function twice will be proper.Maybe relavent with the cookie.
        if(res != CURLE_OK)
        {
            fprintf(stderr,"MakeWish() is failed!\n");
            put_err_msg(res);
            goto err_e;
        }
		printf("Thread:%d	user:%s	make a wish successfully!\n",thread_id,username);
#endif
#ifdef S_MI_4_REGISTER
        res = Mi_4_Register(tel,username,thread_id);
        res = Mi_4_Register(tel,username,thread_id);//Same to above.
        if(res != CURLE_OK)
        {
            fprintf(stderr,"Mi_4_Register() is failed!\n");
            put_err_msg(res);
            goto err_e;
        }
		fprintf(user_register,"用户名:	%s,密码:	%s,姓名:	%s,电话:	%llu,邮件:	%s\n",username,passwd,"李刚",tel,username);
		
		printf("%s registered successfully!\n",username);

		tel++;//号码自增1
#endif

#ifdef S_PANIC_BUYING
		res = PanicBuying(mi1s_4_url,mi1s_4_refer_url,username,regist_tel,thread_id);
		res = PanicBuying(mi1s_4_url,mi1s_4_refer_url,username,regist_tel,thread_id);//we run this function twice in case of failure.
		if(res != CURLE_OK)
		{
			fprintf(stderr,"PanicBuying() is failed!\n");
			put_err_msg(res);
			goto err_e;
		}
		printf("Thread : %d		user:%s		panic buying over!\n",thread_id,username);
#endif//end of S_PANIC_BUYING
		res = Logout(thread_id);
		if(res != CURLE_OK)
		{
			fprintf(stderr,"Logout() is failed!\n");
			put_err_msg(res);
			goto err_e;
		}
		time = GetTime();
		strcpy(time_str,asctime(time));
		time_str_len = strlen(time_str);
		time_str[time_str_len - 1] = '\0';
		fprintf(login_out,"%s	user:%s logout!\n",time_str,username);
	}
	//pthread_mutex_unlock(&pt_lock[thread_id]);

err_e:;
	pthread_exit(NULL);
}


int main(int argc, char *argv[])
{

	curl_global_init(CURL_GLOBAL_ALL);

#ifdef S_MI_4_REGISTER
	FILE *nexttelnum;//this is file is used to save the next telephone number to register
	
	if(NULL == (nexttelnum = fopen("nexttelephone","a+")))
	{
		fprintf(stderr,"File nexttelephone open failed!\n");
		goto err_next_tel_num;
	}

	if(EOF == fscanf(nexttelnum,"%llu",&tel))//Read the telephone number from the file
	{
		fprintf(stderr,"File nexttelphone is null!\n");
		goto err_next_tel_num;
	}

	fopen("nexttelephone","w");//清空文件内容
	
	FILE *user_register;//the register infomation of the file userinfo
	
	if(NULL == (user_register = fopen("user_register","a+")))
	{
		fprintf(stderr,"File user_register open failed!\n");
		goto err_user_register;
	}
#endif

	if(NULL == (info = fopen("user_info_tel","r")))
	{
		fprintf(stderr,"File user_info_tel open failed!\n");
		goto err_info;
	}
	if(NULL == (login_out = fopen("login_out.log","a+")))
	{
		fprintf(stderr,"File login_out.log open failed!\n");
		goto err_login_out;
	}
	if(NULL == (prebuy = fopen("prebuy.log","a+")))
	{
		fprintf(stderr,"File prebuy.log open failed!\n");
		goto err_prebuy;
	}
	if(NULL == (writeheader = fopen("writeheader.dat","a+")))
	{
		fprintf(stderr,"File writeheader open failed!\n");
		goto err_writeheader;
	}
	if(NULL == (writedata = fopen("writedata.dat","a+")))
	{
		fprintf(stderr,"File writedata.dat open failed!\n");
		goto err_writedata;
	}
    if(NULL == (commodity = fopen("commodityinfo","r")))
	{
		fprintf(stderr,"File commodityinfo open failed!\n");
		goto err_commodity;
	}
	
	//Setup the paramater
	if(0 != SetPara(commodity))
	{
		fprintf(stderr,"SetPara failed!\n");
		goto err_commodity;
	}
#ifdef HTTPS_DEBUG
	printf("Para: \n%s\n%s\n%s\n%s\n",add_to_cart,commodity_10_dian,mi1s_4_url,mi1s_4_refer_url);
#endif

	//下面是多线程处理
	int i;
	const int map[]={0,1,2,3,4,5,6,7,8,9,10,11,12,15,16,17,18,19,20,\
					21,22,23,24,25,25,27,28,29,30,31,32,33,34,35,36,37,38,39};//i can not transmit as a parameter.
/*	for(i = 0;i < PTHREAD_NUMBER;i++)
	{
		pthread_mutex_init(&pt_lock[i],NULL);//inatialize the mutex
	}
*/
#ifdef USE_OPENSSL
	init_locks();
#endif
	for(i = 0;i < PTHREAD_NUMBER;i++)
	{
		if(pthread_create(&pt_id[i],NULL,(void *)single_run,(void *)&map[i]) != 0)
		{
			fprintf(stderr,"Thread create error!\n");
			continue;
		}
	}

	for(i = 0;i < PTHREAD_NUMBER;i++)
	{
		pthread_join(pt_id[i],NULL);
	}

/*	for(i = 0;i < PTHREAD_NUMBER;i++)
	{
		pthread_mutex_destroy(&pt_lock[i]);//destory the mutex
	}
*/
#ifdef USE_OPENSSL
	kill_locks();
#endif

err_commodity:
	if(commodity)
		fclose(commodity);
err_writedata:
	if(writedata)
		fclose(writedata);
err_writeheader:
	if(writeheader)
		fclose(writeheader);
err_prebuy:
	if(prebuy)
		fclose(prebuy);
err_login_out:
	if(login_out)
		fclose(login_out);
err_info:
	if(info)
		fclose(info);

#ifdef S_MI_4_REGISTER
err_user_register:
	fprintf(nexttelnum,"%llu",tel);//Save the next telephone number to the file
	if(user_register)
		fclose(user_register);
err_next_tel_num:
	if(nexttelnum)
		fclose(nexttelnum);
#endif
	curl_global_cleanup();
	return 0;
}
